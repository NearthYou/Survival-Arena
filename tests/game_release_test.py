import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('receipt', SCRIPTS / 'write_game_build_receipt.py')
receipt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(receipt)


class ReleaseConfiguration(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows extended paths')
    def test_extended_paths_keep_the_same_input_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            allowed = root / 'game'
            allowed.mkdir()
            source = allowed / 'Arena.manifest'
            source.write_text('manifest', encoding='utf-8')
            extended = '\\\\?\\' + str(source)
            self.assertEqual(receipt.normalized_path(extended), source)
            self.assertTrue(receipt.normalized_path(extended).is_relative_to(allowed))
            outside = '\\\\?\\' + str(root / 'outside.manifest')
            self.assertFalse(receipt.normalized_path(outside).is_relative_to(allowed))

    def test_optimized_dynamic_release_runtime_is_accepted(self):
        receipt.verify_compile_configuration('/c /O2 /MD /D "NDEBUG" main.cpp', 'Release')

    def test_debug_word_in_a_directory_is_not_a_compiler_define(self):
        receipt.verify_compile_configuration('/c /O2 /MD /D "NDEBUG" /I "C:/game_DEBUG/include" main.cpp', 'Release')

    def test_debug_or_unoptimized_commands_cannot_be_labeled_release(self):
        for command in ['/c /Od /MDd /D "_DEBUG" main.cpp',
                        '/c /O2 /MDd /D "NDEBUG" main.cpp',
                        '/c /Od /MD /D "NDEBUG" main.cpp']:
            with self.subTest(command=command), self.assertRaises(ValueError):
                receipt.verify_compile_configuration(command, 'Release')

    def test_mixed_release_and_debug_compilation_is_rejected(self):
        with self.assertRaises(ValueError):
            receipt.verify_compile_configuration('/c /O2 /MD /D "NDEBUG" a.cpp\n/c /Od /MDd b.cpp', 'Release')


if __name__ == '__main__':
    unittest.main()
