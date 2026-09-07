"""Exercise the normal game's startup validation without a diagnostic build."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(executable, cwd, success, expected=''):
    environment = os.environ.copy()
    environment.update(DXA_REFERENCE_SOURCE_ROOT='Z:/unavailable-upstream',
                       DXA_GAME_SDK_ROOT='Z:/unavailable-sdk', DXA_GAME_ASSET_ROOT='Z:/unavailable-assets')
    result = subprocess.run([str(executable), '--validate-runtime'], cwd=cwd, env=environment,
                            capture_output=True, timeout=90)
    text = (result.stdout + result.stderr).decode('utf-8', errors='replace')
    if (result.returncode == 0) != success or expected not in text:
        raise RuntimeError(f'unexpected startup result {result.returncode}: {text}')
    return result.returncode


def verify(executable, output):
    executable, output = Path(executable).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='game-runtime-test-', dir=output))
    try:
        moved = work / 'relocated runtime with spaces'
        shutil.copytree(executable.parent.parent, moved)
        exe = moved / 'Binaries/dxa_game.exe'
        outcomes = {'relocated_without_package_or_reference_environment': run(exe, work, True)}
        shader = next((moved / 'Shaders').glob('*.fx'))
        original = shader.read_bytes()
        shader.write_bytes(original + b'\ncorruption')
        outcomes['corrupt_shader'] = run(exe, work, False, 'hash mismatch')
        shader.write_bytes(original)
        hidden = shader.with_suffix('.temporarily-missing')
        shader.rename(hidden)
        outcomes['missing_shader'] = run(exe, work, False, 'missing')
        hidden.rename(shader)
        extra = moved / 'Resources/unlisted.dat'
        extra.write_bytes(b'extra resource')
        outcomes['extra_resource'] = run(exe, work, False, 'Unexpected files')
        extra.unlink()
        manifest = moved / 'content.sha256'
        manifest.write_bytes(manifest.read_bytes() + b'\n')
        outcomes['changed_manifest'] = run(exe, work, False, 'manifest does not match')
        print(json.dumps(outcomes))
    finally:
        if work.parent != output or not work.name.startswith('game-runtime-test-'):
            raise RuntimeError('runtime test cleanup boundary mismatch')
        shutil.rmtree(work)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    verify(args.executable, args.output)
