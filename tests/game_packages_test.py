import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / 'scripts/game_packages.py'
spec = importlib.util.spec_from_file_location('game_packages', SCRIPT)
packages = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packages)


class PackageContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='dxa-package-test-')
        self.root = pathlib.Path(self.temp.name) / 'package with spaces'
        (self.root / 'Resources').mkdir(parents=True)
        (self.root / 'Resources/map.dat').write_bytes(b'original asset')
        self.lock = packages.seal(self.root, 'assets')

    def tearDown(self):
        self.temp.cleanup()

    def test_independent_copy_is_relocatable(self):
        moved = self.root.with_name('relocated package')
        self.root.rename(moved)
        self.assertEqual(packages.verify(moved, self.lock)['files'], 1)

    def test_missing_or_changed_asset_fails(self):
        asset = self.root / 'Resources/map.dat'
        asset.write_bytes(b'changed asset!')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            packages.verify(self.root, self.lock)
        asset.unlink()
        with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
            packages.verify(self.root, self.lock)

    def test_resealing_changed_asset_cannot_replace_repository_pin(self):
        (self.root / 'Resources/map.dat').write_bytes(b'changed')
        packages.seal(self.root, 'assets')
        with self.assertRaisesRegex(ValueError, 'manifest hash mismatch'):
            packages.verify(self.root, self.lock)

    def test_unlisted_file_fails(self):
        (self.root / 'Resources/unlisted.dat').touch()
        with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
            packages.verify(self.root, self.lock)

    def test_source_engine_outputs_cannot_enter_sdk(self):
        sdk = self.root.parent / 'sdk'
        (sdk / 'Lib/Engine').mkdir(parents=True)
        (sdk / 'Lib/Engine/Engine.lib').write_bytes(b'stale engine')
        with self.assertRaisesRegex(ValueError, 'not allowed in SDK'):
            packages.seal(sdk, 'sdk')

    def test_release_sdk_identity_cannot_be_used_as_debug(self):
        sdk = self.root.parent / 'release sdk'
        (sdk / 'Runtime').mkdir(parents=True)
        (sdk / 'Runtime/assimp-vc143-mt.dll').write_bytes(b'release runtime')
        pin = packages.seal(sdk, 'sdk', configuration='Release|x64')
        self.assertTrue(packages.verify(sdk, pin)['verified'])
        wrong = dict(pin, configuration='Debug|x64')
        with self.assertRaisesRegex(ValueError, 'identity mismatch'):
            packages.verify(sdk, wrong)

    def test_release_sdk_rejects_debug_runtime(self):
        sdk = self.root.parent / 'release sdk'
        (sdk / 'Runtime').mkdir(parents=True)
        (sdk / 'Runtime/assimp-vc143-mtd.dll').write_bytes(b'debug runtime')
        with self.assertRaisesRegex(ValueError, 'not allowed in SDK'):
            packages.seal(sdk, 'sdk', configuration='Release|x64')

    def test_junction_to_original_assets_is_rejected(self):
        if not packages.os.name == 'nt':
            self.skipTest('Windows junction contract')
        import subprocess
        link = self.root.parent / 'asset junction'
        subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(link), str(self.root)],
                       check=True, capture_output=True)
        try:
            with self.assertRaisesRegex(ValueError, 'link or reparse point'):
                packages.verify(link, self.lock)
        finally:
            link.rmdir()

    def test_staging_rejects_output_junction_before_creating_directories(self):
        if packages.os.name != 'nt':
            self.skipTest('Windows junction contract')
        base = self.root.parent
        sdk = base / 'sdk'
        (sdk / 'Include/FMOD').mkdir(parents=True)
        (sdk / 'Include/FMOD/fixture.h').write_bytes(b'fixture SDK header')
        sdk_pin = packages.seal(sdk, 'sdk')
        repository = base / 'repository'
        (repository / 'game/Shaders').mkdir(parents=True)
        (repository / 'game/Shaders/fixture.fx').write_bytes(b'fixture shader')
        packages.save_json(repository / 'game/packages.lock.json', {
            'sdk': sdk_pin, 'assets': self.lock,
        })
        protected = base / 'protected'
        protected.mkdir()
        link = base / 'build junction'
        subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(link), str(protected)],
                       check=True, capture_output=True)
        try:
            result = subprocess.run([
                sys.executable, str(SCRIPT.with_name('stage_game_runtime.py')),
                '--repository', str(repository), '--build-root', str(link / 'new-build'),
                '--sdk-root', str(sdk), '--asset-root', str(self.root),
            ], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('link or reparse point', result.stderr)
            self.assertEqual(list(protected.iterdir()), [], 'Staging wrote through the junction before rejection')
        finally:
            link.rmdir()


if __name__ == '__main__':
    unittest.main()
