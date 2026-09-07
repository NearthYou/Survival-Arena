import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/analyze_game_measurement.py'


class MeasurementContract(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('measurement', SCRIPT)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.temp = tempfile.TemporaryDirectory()
        self.csv = Path(self.temp.name) / 'frames.csv'
        self.csv.write_text('elapsed_seconds,frame_ms,working_set_bytes,private_bytes,handles\n'
                            '0.01,10,100,50,10\n0.03,20,100,60,10\n'
                            '0.06,30,110,70,11\n0.10,40,120,80,11\n')

    def tearDown(self):
        self.temp.cleanup()

    def test_frame_statistics_are_computed_from_measured_time(self):
        result = self.module.analyze(self.csv, warmup=0, minimum_seconds=0.1)
        self.assertAlmostEqual(result['average_fps'], 40)
        self.assertAlmostEqual(result['frame_ms_p95'], 38.5)
        self.assertAlmostEqual(result['one_percent_low_fps'], 25)
        self.assertEqual(result['frames'], 4)
        self.assertEqual(result['private_growth_bytes'], 30)

    def test_warmup_is_excluded_from_frame_and_memory_statistics(self):
        result = self.module.analyze(self.csv, warmup=0.04, minimum_seconds=0.1)
        self.assertEqual(result['frames'], 2)
        self.assertEqual(result['private_growth_bytes'], 10)

    def test_early_exit_and_invalid_samples_fail(self):
        with self.assertRaises(ValueError):
            self.module.analyze(self.csv, warmup=0, minimum_seconds=30)
        self.csv.write_text(self.csv.read_text().replace('0.06,30', '0.06,nan'))
        with self.assertRaises(ValueError):
            self.module.analyze(self.csv, warmup=0, minimum_seconds=0.1)


if __name__ == '__main__':
    unittest.main()
