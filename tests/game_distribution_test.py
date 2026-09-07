import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))


class DistributionContract(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('distribution', SCRIPTS / 'package_game_release.py')
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / 'Binaries').mkdir()
        (self.root / 'Resources').mkdir()
        (self.root / 'Binaries/dxa_game.exe').write_bytes(b'release executable')
        (self.root / 'Resources/model.dat').write_bytes(b'private model')
        (self.root / 'Binaries/dxa_game.pdb').write_bytes(b'developer symbols')
        (self.root / 'Binaries/probe-run.json').write_bytes(b'instrumentation output')
        self.sha = hashlib.sha256(b'release executable').hexdigest()
        digest = hashlib.sha256(b'private model').hexdigest()
        (self.root / 'content.sha256').write_text(digest + '\tResources/model.dat\n')

    def tearDown(self):
        self.temp.cleanup()

    def test_package_contains_only_sealed_runtime_and_executable(self):
        files = self.module.collect_runtime(self.root, self.sha)
        self.assertEqual(set(files), {'Binaries/dxa_game.exe', 'Resources/model.dat', 'content.sha256'})

    def test_changed_payload_and_executable_are_rejected(self):
        (self.root / 'Resources/model.dat').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            self.module.collect_runtime(self.root, self.sha)
        with self.assertRaises(ValueError):
            self.module.collect_runtime(self.root, '0' * 64)

    def test_manifest_path_cannot_escape_runtime(self):
        (self.root / 'content.sha256').write_text('0' * 64 + '\t../outside.dat\n')
        with self.assertRaises(ValueError):
            self.module.collect_runtime(self.root, self.sha)

    def test_empty_manifest_path_is_rejected_cleanly(self):
        (self.root / 'content.sha256').write_text('0' * 64 + '\t\n')
        with self.assertRaises(ValueError):
            self.module.collect_runtime(self.root, self.sha)


if __name__ == '__main__':
    unittest.main()
