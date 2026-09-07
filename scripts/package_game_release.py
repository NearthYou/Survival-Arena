"""Create and relocation-test a local-only offline Release package. Never uploads files."""
import argparse
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import zipfile

from game_packages import reject_link, save_json, sha256
from write_game_build_receipt import source_tree_digest


def collect_runtime(runtime, executable_sha256):
    runtime = Path(runtime).absolute()
    reject_link(runtime)
    executable = runtime / 'Binaries/dxa_game.exe'
    reject_link(executable)
    reject_link(runtime / 'content.sha256')
    if sha256(executable) != executable_sha256.lower():
        raise ValueError('Executable does not match the verified Release build')
    files = {'Binaries/dxa_game.exe': executable, 'content.sha256': runtime / 'content.sha256'}
    for line in (runtime / 'content.sha256').read_text(encoding='utf-8').splitlines():
        digest, relative = line.split('\t', 1)
        path = PurePosixPath(relative)
        if (not path.parts or path.is_absolute() or '..' in path.parts or ':' in relative or '\\' in relative
                or path.parts[0] not in ('Binaries', 'Resources', 'Shaders') or relative in files):
            raise ValueError('Unsafe or duplicate runtime manifest path')
        source = runtime / relative
        reject_link(source)
        if sha256(source) != digest:
            raise ValueError(f'Runtime payload changed: {relative}')
        files[relative] = source
    return files


def package(repository, build_root, output, notices):
    repository, build_root, output, notices = map(
        lambda p: Path(p).absolute(), (repository, build_root, output, notices))
    for path in (build_root, notices):
        reject_link(path)
    reject_link(output, allow_missing=True)
    receipt = json.loads((build_root / 'game-build.json').read_text(encoding='utf-8'))
    if (receipt['configuration'] != 'Release|x64' or receipt['instrumentation'] is not None
            or not receipt['release_optimization_verified']):
        raise ValueError('Only a verified, uninstrumented Release build can be packaged')
    if receipt['source_tree_sha256'] != source_tree_digest(repository):
        raise ValueError('Game sources changed after the verified build')
    runtime = build_root / 'runtime'
    files = collect_runtime(runtime, receipt['executable_sha256'])
    name = 'Survival-Arena-offline-private-' + receipt['executable_sha256'][:12]
    folder = output / name
    archive = output / (name + '.zip')
    relocated = output / (name + '-relocated')
    if any(path.exists() for path in (folder, archive, relocated)):
        raise ValueError('Package evidence already exists; choose a fresh output directory')
    subprocess.run([str(runtime / 'Binaries/dxa_game.exe'), '--validate-runtime'],
                   cwd=tempfile.gettempdir(), check=True, capture_output=True, timeout=90)
    folder.mkdir(parents=True)
    for relative, source in files.items():
        target = folder / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    notice_files = [path for path in notices.rglob('*') if path.is_file()]
    if not notice_files:
        raise ValueError('Third-party notices are required for the local package')
    for source in notice_files:
        reject_link(source)
        target = folder / 'Notices' / source.relative_to(notices)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    (folder / 'README.txt').write_text(
        'Survival Arena - Offline Release\n\n'
        'Extract the entire ZIP and run Binaries/dxa_game.exe.\n'
        'Keep Binaries, Resources and Shaders together. No server is required.\n'
        'Controls: right-click to move/attack, QWER skills, Ctrl+QWER to learn, Z to craft, F3 collision display.\n'
        'Nicky W counters an incoming attack during the guard window.\n\n'
        'LOCAL ONLY: this package contains private extracted resources.\n'
        'Do not upload this ZIP or its Resources folder to a public repository or public release.\n',
        encoding='utf-8')
    save_json(folder / 'build-info.json', {
        'application': 'Survival Arena', 'configuration': receipt['configuration'],
        'automatic_test_input': False, 'distribution': 'local-only',
        'executable_sha256': receipt['executable_sha256'],
        'source_tree_sha256': receipt['source_tree_sha256'],
        'content_manifest_sha256': receipt['content_manifest_sha256'],
    })
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for path in sorted(folder.rglob('*')):
            if path.is_file():
                zipped.write(path, path.relative_to(output).as_posix())
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise ValueError('Package ZIP integrity check failed')
        for entry in zipped.infolist():
            relative = PurePosixPath(entry.filename)
            if relative.is_absolute() or '..' in relative.parts or relative.parts[0] != name:
                raise ValueError('Package ZIP contains an unexpected path')
        zipped.extractall(relocated)
    extracted = relocated / name
    collect_runtime(extracted, receipt['executable_sha256'])
    subprocess.run([str(extracted / 'Binaries/dxa_game.exe'), '--validate-runtime'],
                   cwd=tempfile.gettempdir(), check=True, capture_output=True, timeout=90)
    result = {'local_only': True, 'archive': str(archive), 'sha256': sha256(archive),
              'bytes': archive.stat().st_size, 'runtime_files': len(files),
              'executable_sha256': receipt['executable_sha256'],
              'relocated_runtime': str(extracted), 'relocation_verified': True}
    save_json(output / (name + '-package.json'), result)
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repository', 'build-root', 'output', 'notices'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    try:
        package(args.repository, args.build_root, args.output, args.notices)
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Local Release packaging failed: {error}\n')
