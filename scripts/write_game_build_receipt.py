"""Check MSBuild's actual compile/link inputs and record a local game build."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path

from game_packages import REVISION, package_lock_path, save_json, sha256


def read_log(path):
    data = path.read_bytes()
    return data.decode('utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')


def normalized_path(value):
    path = Path(value).resolve()
    text = str(path)
    # MSBuild's manifest tracker writes extended-length Windows paths.
    if os.name == 'nt':
        if text.startswith('\\\\?\\UNC\\'):
            return Path('\\\\' + text[8:])
        if text.startswith('\\\\?\\') and re.match(r'[A-Za-z]:\\', text[4:]):
            return Path(text[4:])
    return path


def source_tree_digest(repository):
    root = Path(repository) / 'game'
    extensions = {'.cpp', '.h', '.inl', '.fx', '.props', '.vcxproj', '.rc', '.json', '.manifest'}
    entries = [sha256(path) + '\t' + path.relative_to(root).as_posix()
               for path in sorted(root.rglob('*')) if path.is_file()
               and (path.suffix in extensions or path.is_relative_to(root / 'Presentation'))
               and path.name != 'source-provenance.json']
    return hashlib.sha256('\n'.join(entries).encode('utf-8')).hexdigest()


def verify_compile_configuration(command, configuration):
    if configuration not in ('Debug', 'Release'):
        raise ValueError('unsupported compilation configuration')
    if configuration == 'Release':
        commands = [line for line in command.splitlines() if re.search(r'(?i)(?:^|\s)/c(?=\s|$)', line)]
        if not commands:
            raise ValueError('Release compilation commands are missing')
        for line in commands:
            has = lambda flag: re.search(r'(?i)(?:^|\s)/' + flag + r'(?=\s|$)', line) is not None
            debug_define = re.search(r'(?i)(?:^|\s)/D\s*"?(?:_DEBUG|DEBUG)(?="|\s|=|$)', line)
            if not has('O2') or not has('MD') or has('Od') or has('MDd') or debug_define:
                raise ValueError('Release requires optimized compilation and the Release CRT')


def record(repository, build_root, sdk_root, scenario=None, configuration='Debug'):
    repository, build_root, sdk_root = map(normalized_path, (repository, build_root, sdk_root))
    provenance = json.loads((repository / 'game/build-inputs.json').read_text(encoding='utf-8-sig'))
    system_roots = [normalized_path(os.environ[name]) for name in ('ProgramFiles', 'ProgramFiles(x86)', 'SystemRoot') if name in os.environ]
    allowed_inputs = [repository / 'game', repository / 'scripts/reference_oracle', build_root, sdk_root, *system_roots]
    read_inputs = set()
    units = []
    for project in provenance['build_inputs']:
        name = project['project']
        tlogs = build_root / 'obj' / name / (name + '.tlog')
        actual = {}
        for line in read_log(tlogs / 'Cl.items.tlog').splitlines():
            if not line:
                continue
            source, obj = line.split(';')[:2]
            source, obj = normalized_path(source), normalized_path(obj)
            if not source.is_relative_to(repository / 'game' / name) or not obj.is_relative_to(build_root):
                raise ValueError(f'compile input/output outside the internal build: {line}')
            if not obj.is_file() or obj.stat().st_mtime_ns < source.stat().st_mtime_ns:
                raise ValueError(f'object is missing or older than its source: {obj}')
            actual[source.name.lower()] = source
        if set(actual) != {filename.lower() for filename in project['compile']}:
            raise ValueError(f'{name} compilation inventory differs from the original project')
        units.append({'project': name, 'compile_count': len(actual), 'sources': [
            {'path': path.relative_to(repository).as_posix(), 'sha256': sha256(path)}
            for path in sorted(actual.values())]})
        command = read_log(tlogs / 'CL.command.1.tlog')
        verify_compile_configuration(command, configuration)
        if ('GameProbeConfig.h'.lower() in command.lower()) != bool(scenario):
            raise ValueError('normal/probe compilation identity mismatch')
        for log in tlogs.glob('*.read.*.tlog'):
            for line in read_log(log).splitlines():
                for value in line.removeprefix('^').split('|'):
                    if not value:
                        continue
                    path = normalized_path(value)
                    if not any(path.is_relative_to(root) for root in allowed_inputs):
                        raise ValueError(f'build read an undeclared input outside repository/packages/toolchain: {path}')
                    read_inputs.add(str(path))
    engine = build_root / 'lib/dxa_game_engine.lib'
    link_log = build_root / 'obj/Client/Client.tlog/link.read.1.tlog'
    libraries = [normalized_path(line) for line in read_log(link_log).splitlines()
                 if not line.startswith('^') and line.lower().endswith('.lib')]
    if engine not in libraries:
        raise ValueError('linker did not consume the newly built repository engine library')
    for library in libraries:
        if configuration == 'Release' and library.name.lower() in (
                'effects11d.lib', 'directxtex_debug.lib', 'assimp-vc143-mtd.lib',
                'msvcprtd.lib', 'msvcrtd.lib', 'vcruntimed.lib', 'ucrtd.lib', 'vcompd.lib'):
            raise ValueError(f'debug library in Release build: {library}')
        if library == engine or library.is_relative_to(sdk_root) or any(library.is_relative_to(root) for root in system_roots):
            continue
        raise ValueError(f'unexpected external link input: {library}')
    for path in (repository / 'game').rglob('*'):
        if path.is_file() and path.suffix.lower() in ('.obj', '.exe', '.lib', '.pdb', '.cso', '.tlog', '.idb'):
            raise ValueError(f'build output leaked into source: {path}')
    exe = build_root / 'runtime/Binaries' / ('dxa_game_probe.exe' if scenario else 'dxa_game.exe')
    result = {'schema': 'dxa-game-build-v1', 'package_revision': REVISION,
              'repository': str(repository), 'configuration': configuration + '|x64', 'instrumentation': scenario,
              'release_optimization_verified': configuration == 'Release',
              'source_tree_sha256': source_tree_digest(repository),
              'compile_units': units, 'engine_library': str(engine), 'engine_sha256': sha256(engine),
              'executable': str(exe), 'executable_sha256': sha256(exe),
              'link_libraries': [str(path) for path in libraries],
              'read_input_count': len(read_inputs),
              'read_inputs_restricted_to_repository_packages_toolchain': True,
              'package_lock_sha256': sha256(package_lock_path(repository, configuration)),
              'content_manifest_sha256': sha256(build_root / 'runtime/content.sha256'),
              'manual_visual_approval': 'pending'}
    save_json(build_root / 'game-build.json', result)
    print(json.dumps({'engine_compile': units[0]['compile_count'], 'client_compile': units[1]['compile_count'],
                      'internal_engine_linked': True, 'instrumentation': scenario, 'receipt': str(build_root / 'game-build.json')}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repository', 'build-root', 'sdk-root'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--scenario')
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Debug')
    args = parser.parse_args()
    try:
        record(args.repository, args.build_root, args.sdk_root, args.scenario, args.configuration)
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'Game build provenance verification failed: {error}\n')
