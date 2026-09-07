"""Assemble the offline Release SDK from pinned upstream inputs and a local Assimp build."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

from game_packages import (REVISION, SDK_COMPONENTS, SDK_RELEASE_LIBS, SDK_RELEASE_RUNTIME,
                           VC_RUNTIME, check_path, reject_link, save_json, seal, verify)

ASSIMP_COMMIT = '9519a62dd20799c5493c638d1ef5a6f484e5faf1'


def create(source, assimp_source, assimp_build, redist, output, asset_lock, lock_path):
    source, assimp_source, assimp_build, redist, output = map(
        lambda p: Path(p).absolute(), (source, assimp_source, assimp_build, redist, output))
    for p in (source, assimp_source, assimp_build, redist):
        reject_link(p)
    reject_link(output, allow_missing=True)
    if output.exists() or any(output.is_relative_to(p) or p.is_relative_to(output)
                              for p in (source, assimp_source, assimp_build, redist)):
        raise ValueError('Release SDK output must be new and independent of all inputs')
    subprocess.run(['git', '-C', str(source), 'cat-file', '-e', REVISION + '^{commit}'], check=True)
    actual = subprocess.check_output(['git', '-C', str(assimp_source), 'rev-parse', 'HEAD']).decode().strip()
    if actual != ASSIMP_COMMIT:
        raise ValueError('Assimp source revision does not match v5.2.5')
    subprocess.run(['git', '-C', str(assimp_source), 'diff', '--quiet', 'HEAD'], check=True)
    files = {
        'Lib/Assimp/assimp-vc143-mt.lib': assimp_build / 'lib/Release/assimp-vc143-mt.lib',
        'Runtime/assimp-vc143-mt.dll': assimp_build / 'bin/Release/assimp-vc143-mt.dll',
        'Include/Assimp/config.h': assimp_build / 'include/assimp/config.h',
    }
    for name in VC_RUNTIME:
        component = 'Microsoft.VC143.OpenMP' if name == 'vcomp140.dll' else 'Microsoft.VC143.CRT'
        files['Runtime/' + name] = redist / 'x64' / component / name
    for path in (assimp_source / 'include/assimp').rglob('*'):
        if path.is_file() and path.suffix in ('.h', '.hpp', '.inl'):
            files['Include/Assimp/' + path.relative_to(assimp_source / 'include/assimp').as_posix()] = path
    for path in files.values():
        reject_link(path)
        if not path.is_file():
            raise ValueError(f'Release SDK input missing: {path}')
    assets = json.loads(Path(asset_lock).read_text(encoding='utf-8'))['assets']
    output.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix='dxa-release-sdk-') as temporary:
        archive_path = Path(temporary) / 'sdk.zip'
        subprocess.run(['git', '-C', str(source), 'archive', '--format=zip',
                        '--output=' + str(archive_path), REVISION, 'Libraries'], check=True)
        with zipfile.ZipFile(archive_path) as archive:
            for entry in archive.infolist():
                name = entry.filename
                relative = None
                if any(name.startswith('Libraries/Include/' + component + '/')
                       for component in SDK_COMPONENTS if component != 'Assimp'):
                    relative = name.removeprefix('Libraries/')
                elif name in ['Libraries/Lib/' + lib for lib in SDK_RELEASE_LIBS if not lib.startswith('Assimp/')]:
                    relative = name.removeprefix('Libraries/')
                elif name == 'Libraries/Lib/FMOD/fmod.dll':
                    relative = 'Runtime/fmod.dll'
                if relative and not entry.is_dir():
                    check_path(relative, 'sdk', 'Release|x64')
                    data = archive.read(entry)
                    if data.startswith(b'version https://git-lfs.github.com/spec/v1'):
                        raise ValueError(f'SDK archive contains an LFS pointer: {name}')
                    path = output / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
    for relative, source_file in files.items():
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_file, destination)
    for relative in [*('Lib/' + lib for lib in SDK_RELEASE_LIBS), *('Runtime/' + dll for dll in SDK_RELEASE_RUNTIME)]:
        if not (output / relative).is_file():
            raise ValueError(f'Release SDK input missing: {relative}')
    lock = {'schema': 'dxa-game-packages-v1', 'upstream_commit': REVISION,
            'sdk': seal(output, 'sdk', configuration='Release|x64'), 'assets': assets,
            'release_dependencies': {
                'assimp': {'repository': 'https://github.com/assimp/assimp', 'version': '5.2.5',
                           'commit': ASSIMP_COMMIT, 'built': 'VS2022 x64 Release, shared library, bundled zlib'},
                'vc_runtime': {'toolchain': 'Visual Studio 2022', 'redist_version': redist.name},
            }}
    verify(output, lock['sdk'])
    save_json(lock_path, lock)
    print(json.dumps(lock, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'assimp-source', 'assimp-build', 'redist', 'output', 'asset-lock', 'lock-path'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    try:
        create(args.source, args.assimp_source, args.assimp_build, args.redist,
               args.output, args.asset_lock, args.lock_path)
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Release SDK creation failed: {error}\n')
