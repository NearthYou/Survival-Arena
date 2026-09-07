"""Create and verify the pinned, private SDK and resource packages.

Creation is a separate maintainer operation. Normal configure/build/run only
verifies independent packages and never reads the upstream checkout.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile
import zipfile

REVISION = '01b820a3ebcfd473a898dec1f5bc67c4ac77261e'
SDK_COMPONENTS = ('Assimp', 'DirectXTex', 'FMOD', 'FX11')
SDK_LIBS = (
    'Assimp/assimp-vc143-mtd.lib', 'DirectXTex/DirectXTex_debug.lib',
    'FMOD/fmod_vc.lib', 'FX11/Effects11d.lib',
)
SDK_RUNTIME = ('assimp-vc143-mtd.dll', 'fmod.dll')
SDK_RELEASE_LIBS = (
    'Assimp/assimp-vc143-mt.lib', 'DirectXTex/DirectXTex.lib',
    'FMOD/fmod_vc.lib', 'FX11/Effects11.lib',
)
VC_RUNTIME = ('concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
              'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll', 'vccorlib140.dll',
              'vcruntime140.dll', 'vcruntime140_1.dll', 'vcruntime140_threads.dll', 'vcomp140.dll')
SDK_RELEASE_RUNTIME = ('assimp-vc143-mt.dll', 'fmod.dll', *VC_RUNTIME)


def package_lock_path(repository, configuration='Debug'):
    if configuration not in ('Debug', 'Release'):
        raise ValueError(f'unsupported game configuration: {configuration}')
    name = 'packages.release.lock.json' if configuration == 'Release' else 'packages.lock.json'
    return Path(repository) / 'game' / name


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def reject_link(path, allow_missing=False):
    path = Path(path).absolute()
    for item in (path, *path.parents):
        try:
            info = item.lstat()
        except FileNotFoundError:
            if allow_missing:
                continue
            raise
        if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError(f'link or reparse point is not an independent package: {item}')


def inventory(root, exclude_manifest=True):
    root = Path(root).absolute()
    if not root.is_dir():
        raise ValueError(f'package directory missing: {root}')
    reject_link(root)
    paths = []
    for directory, folders, files in os.walk(root, followlinks=False):
        for name in folders + files:
            reject_link(Path(directory) / name)
        for name in files:
            path = Path(directory) / name
            relative = path.relative_to(root).as_posix()
            if not exclude_manifest or relative != 'manifest.json':
                paths.append(relative)
    return sorted(paths)


def check_path(relative, kind, configuration='Debug|x64'):
    path = PurePosixPath(relative)
    if path.is_absolute() or '..' in path.parts or ':' in relative or '\\' in relative:
        raise ValueError(f'unsafe package path: {relative}')
    if kind == 'sdk':
        if configuration not in ('Debug|x64', 'Release|x64'):
            raise ValueError(f'unsupported SDK configuration: {configuration}')
        libraries = SDK_RELEASE_LIBS if configuration == 'Release|x64' else SDK_LIBS
        runtime = SDK_RELEASE_RUNTIME if configuration == 'Release|x64' else SDK_RUNTIME
        allowed = (len(path.parts) >= 3 and path.parts[0] == 'Include'
                   and path.parts[1] in SDK_COMPONENTS)
        allowed |= relative in [f'Lib/{name}' for name in libraries]
        allowed |= relative in [f'Runtime/{name}' for name in runtime]
        if not allowed:
            raise ValueError(f'file not allowed in SDK: {relative}')
    elif kind == 'assets':
        if len(path.parts) < 2 or path.parts[0] != 'Resources':
            raise ValueError(f'file not allowed in assets: {relative}')
    else:
        raise ValueError(f'unknown package kind: {kind}')


def save_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def seal(root, kind, configuration='Debug|x64'):
    root = Path(root)
    files = []
    for relative in inventory(root):
        check_path(relative, kind, configuration)
        path = root / relative
        files.append({'path': relative, 'bytes': path.stat().st_size, 'sha256': sha256(path)})
    if not files:
        raise ValueError('empty package')
    manifest = {'schema': 'dxa-game-package-v1', 'kind': kind,
                'upstream_commit': REVISION, 'configuration': configuration, 'files': files}
    save_json(root / 'manifest.json', manifest)
    pin = {'kind': kind, 'manifest_sha256': sha256(root / 'manifest.json'),
           'files': len(files), 'bytes': sum(item['bytes'] for item in files)}
    if configuration != 'Debug|x64':
        pin['configuration'] = configuration
    return pin


def verify(root, pin):
    root = Path(root).absolute()
    paths = inventory(root)
    manifest_path = root / 'manifest.json'
    if not manifest_path.is_file():
        raise ValueError(f'package manifest missing: {manifest_path}')
    if sha256(manifest_path) != pin['manifest_sha256']:
        raise ValueError(f'package manifest hash mismatch: {manifest_path}')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if (manifest['schema'] != 'dxa-game-package-v1' or manifest['kind'] != pin['kind']
            or manifest['upstream_commit'] != REVISION
            or manifest['configuration'] != pin.get('configuration', 'Debug|x64')):
        raise ValueError('package identity mismatch')
    entries = manifest['files']
    if len(entries) != pin['files'] or sorted(item['path'] for item in entries) != paths:
        raise ValueError(f'package inventory mismatch: {root}')
    for item in entries:
        check_path(item['path'], pin['kind'], manifest['configuration'])
        path = root / item['path']
        if path.stat().st_size != item['bytes'] or sha256(path) != item['sha256']:
            raise ValueError(f'package file hash mismatch: {path}')
    if sum(item['bytes'] for item in entries) != pin['bytes']:
        raise ValueError('package size mismatch')
    return {'kind': pin['kind'], 'root': str(root), 'files': len(entries), 'verified': True}


def create(source, output, lock_path):
    source, output = Path(source).absolute(), Path(output).absolute()
    reject_link(source)
    reject_link(output, allow_missing=True)
    if output.exists():
        raise ValueError(f'package output already exists: {output}')
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError('package output must be independent of the upstream checkout')
    subprocess.run(['git', '-C', str(source), 'cat-file', '-e', REVISION + '^{commit}'], check=True)
    subprocess.run(['git', '-C', str(source), 'diff', '--quiet', REVISION, '--', 'Resources'], check=True)
    resources = subprocess.check_output([
        'git', '-C', str(source), 'ls-tree', '-rz', '--name-only', REVISION, 'Resources'
    ]).decode('utf-8').strip('\0').split('\0')
    output.mkdir(parents=True)
    sdk, assets = output / 'sdk', output / 'assets'
    sdk.mkdir()
    assets.mkdir()
    with tempfile.TemporaryDirectory(prefix='dxa-sdk-archive-') as temp:
        archive_path = Path(temp) / 'sdk.zip'
        subprocess.run(['git', '-C', str(source), 'archive', '--format=zip',
                        '--output=' + str(archive_path), REVISION, 'Libraries',
                        'Binaries/assimp-vc143-mtd.dll'], check=True)
        with zipfile.ZipFile(archive_path) as archive:
            for item in archive.infolist():
                if item.is_dir():
                    continue
                name = item.filename
                relative = None
                if any(name.startswith('Libraries/Include/' + component + '/') for component in SDK_COMPONENTS):
                    relative = name.removeprefix('Libraries/')
                elif name in ['Libraries/Lib/' + lib for lib in SDK_LIBS]:
                    relative = name.removeprefix('Libraries/')
                elif name == 'Binaries/assimp-vc143-mtd.dll':
                    relative = 'Runtime/assimp-vc143-mtd.dll'
                elif name == 'Libraries/Lib/FMOD/fmod.dll':
                    relative = 'Runtime/fmod.dll'
                if relative:
                    check_path(relative, 'sdk')
                    data = archive.read(item)
                    if data.startswith(b'version https://git-lfs.github.com/spec/v1'):
                        raise ValueError(f'SDK archive contains an LFS pointer: {name}')
                    destination = sdk / relative
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(data)
    for relative in resources:
        check_path(relative, 'assets')
        path = source / relative
        reject_link(path)
        with path.open('rb') as stream:
            if stream.read(80).startswith(b'version https://git-lfs.github.com/spec/v1'):
                raise ValueError(f'Resource is not hydrated: {relative}')
        destination = assets / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
    for relative in [*('Lib/' + lib for lib in SDK_LIBS), *('Runtime/' + dll for dll in SDK_RUNTIME)]:
        if not (sdk / relative).is_file():
            raise ValueError(f'pinned SDK input missing: {relative}')
    lock = {'schema': 'dxa-game-packages-v1', 'upstream_commit': REVISION,
            'sdk': seal(sdk, 'sdk'), 'assets': seal(assets, 'assets')}
    for kind in ('sdk', 'assets'):
        verify(output / kind, lock[kind])
    save_json(lock_path, lock)
    return lock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    verify_parser = commands.add_parser('verify')
    verify_parser.add_argument('--sdk-root', required=True)
    verify_parser.add_argument('--asset-root', required=True)
    verify_parser.add_argument('--lock', required=True)
    create_parser = commands.add_parser('create')
    create_parser.add_argument('--source-root', required=True)
    create_parser.add_argument('--output', required=True)
    create_parser.add_argument('--lock', required=True)
    output_parser = commands.add_parser('check-output')
    output_parser.add_argument('--path', required=True)
    args = parser.parse_args()
    try:
        if args.command == 'create':
            result = create(args.source_root, args.output, args.lock)
        elif args.command == 'check-output':
            reject_link(args.path, allow_missing=True)
            result = {'output_path_checked': str(Path(args.path).absolute())}
        else:
            lock = json.loads(Path(args.lock).read_text(encoding='utf-8'))
            if lock['schema'] != 'dxa-game-packages-v1' or lock['upstream_commit'] != REVISION:
                raise ValueError('repository package lock identity mismatch')
            result = [verify(args.sdk_root, lock['sdk']), verify(args.asset_root, lock['assets'])]
        print(json.dumps(result, ensure_ascii=False))
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Game package verification failed: {error}\n')


if __name__ == '__main__':
    main()
