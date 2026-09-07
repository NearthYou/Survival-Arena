"""Stage an independent runtime from verified packages and repository shaders."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

from game_packages import inventory, package_lock_path, reject_link, sha256, verify


def write_if_changed(path, data):
    reject_link(path, allow_missing=True)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def stage(repository, build_root, sdk, assets, configuration='Debug'):
    repository, build_root, sdk, assets = map(Path, (repository, build_root, sdk, assets))
    reject_link(build_root, allow_missing=True)
    lock = json.loads(package_lock_path(repository, configuration).read_text(encoding='utf-8'))
    verify(sdk, lock['sdk'])
    verify(assets, lock['assets'])
    runtime = build_root / 'runtime'
    reject_link(runtime, allow_missing=True)
    runtime.mkdir(parents=True, exist_ok=True)
    reject_link(runtime)
    entries = []
    for item in json.loads((assets / 'manifest.json').read_text(encoding='utf-8'))['files']:
        entries.append((assets / item['path'], item['path'], item['sha256']))
    for item in json.loads((sdk / 'manifest.json').read_text(encoding='utf-8'))['files']:
        if item['path'].startswith('Runtime/'):
            entries.append((sdk / item['path'], 'Binaries/' + Path(item['path']).name, item['sha256']))
    for path in sorted((repository / 'game/Shaders').rglob('*')):
        if path.is_file():
            reject_link(path)
            relative = 'Shaders/' + path.relative_to(repository / 'game/Shaders').as_posix()
            entries.append((path, relative, sha256(path)))
    presentation = repository / 'game/Presentation'
    if presentation.is_dir():
        for path in sorted(presentation.rglob('*')):
            if path.is_file():
                reject_link(path)
                if path.suffix.lower() not in ('.png', '.jpg', '.dds'):
                    raise ValueError(f'unsupported presentation asset: {path}')
                relative = 'Resources/ArenaUI/' + path.relative_to(presentation).as_posix()
                entries.append((path, relative, sha256(path)))
    for source, relative, digest in entries:
        destination = runtime / relative
        reject_link(destination, allow_missing=True)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file() or sha256(destination) != digest:
            shutil.copyfile(source, destination)
    # An unlisted file can affect directory-enumerating loaders. Do not silently
    # delete user files from an existing runtime to make the inventory match.
    for folder in ('Resources', 'Shaders'):
        actual = set(folder + '/' + name for name in inventory(runtime / folder, exclude_manifest=False))
        expected = set(relative for _, relative, _ in entries if relative.startswith(folder + '/'))
        if actual != expected:
            raise ValueError(f'unexpected files in runtime/{folder}; use a fresh build directory')
    content = ''.join(f'{digest}\t{relative}\n' for _, relative, digest in sorted(entries, key=lambda entry: entry[1])).encode('utf-8')
    write_if_changed(runtime / 'content.sha256', content)
    digest = hashlib.sha256(content).hexdigest()
    header = ('#pragma once\n#define DXA_GAME_CONTENT_SHA256 "' + digest + '"\n').encode('ascii')
    write_if_changed(build_root / 'include/GameContentHash.h', header)
    print(json.dumps({'runtime': str(runtime), 'content_files': len(entries), 'content_sha256': digest}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repository', 'build-root', 'sdk-root', 'asset-root'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Debug')
    args = parser.parse_args()
    try:
        stage(args.repository, args.build_root, args.sdk_root, args.asset_root, args.configuration)
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'Game runtime staging failed: {error}\n')
