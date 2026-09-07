import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/analyze_game_diagnostics.py'
spec = importlib.util.spec_from_file_location('diagnostics', SCRIPT)
diagnostics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnostics)


class DiagnosticMeasurements(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.binaries = self.root / 'runtime/Binaries'
        self.binaries.mkdir(parents=True)
        self.result = dict(passed=True, recording=False, vsync=False, paths_valid=1,
                           quadtree_results_equal=1, player_walked=10, nav_triangles=709,
                           model_indices=5403, model_pass=1)
        self.write_json(self.binaries / 'diagnostics.json', self.result)
        self.write_json(self.root / 'game-build.json', dict(configuration='Release|x64', source_tree_sha256='b'*64))
        self.write_json(self.root / 'probe-run.json', dict(exit_code=0, resolution='1920x1080', executable_sha256='a'*64))
        cases = [(0,0),(128,0),(128,1),(512,1),(512,0),(1024,0),(1024,1),(128,1),(128,0)]
        self.render = [dict(phase=phase, objects=count, individual=mode, frame_ms=10+mode,
                            draw_calls=20+count*mode, submitted_instances=count,
                            submitted_elements=100+count*5403, previous_present_result=0)
                       for phase, (count, mode) in enumerate(cases) for _ in range(25)]
        self.write_rows('diagnostics-render.csv', self.render)
        self.write_rows('diagnostics-paths.csv', [
            dict(query=q, repeat=r, elapsed_us=100, expanded_nodes=20, points=4,
                 length=30, direct_distance=25, valid=1) for q in range(6) for r in range(100)])
        self.quads = [dict(objects=n, repeat=r, all_pair_tests=n*(n-1)//2, tree_pair_tests=n,
                           all_pairs_us=1000, tree_build_us=2000, tree_collision_us=500,
                           hits=16, results_equal=1, nodes=10)
                      for n in (128,512,1024) for r in range(15)]
        self.write_rows('diagnostics-quadtree.csv', self.quads)

    def tearDown(self):
        self.temporary.cleanup()

    def write_json(self, path, value):
        path.write_text(json.dumps(value), encoding='utf-8')

    def write_rows(self, name, rows):
        with (self.binaries / name).open('w', newline='', encoding='utf-8') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

    def test_reports_frame_cost_and_includes_tree_construction(self):
        result = diagnostics.analyze(self.root)
        self.assertEqual(result['render'][0]['average_fps'], 100)
        self.assertEqual(result['navigation'][0]['median_ms'], 0.1)
        self.assertEqual(result['quadtree'][0]['tree_rebuild_and_collision_median_ms'], 2.5)
        self.assertGreater(result['quadtree'][0]['tree_rebuild_and_collision_median_ms'],
                           result['quadtree'][0]['all_pairs_median_ms'])

    def test_recording_is_not_accepted_as_performance_evidence(self):
        self.result['recording'] = True
        self.write_json(self.binaries / 'diagnostics.json', self.result)
        with self.assertRaisesRegex(ValueError, 'capture-free'):
            diagnostics.analyze(self.root)

    def test_different_render_work_is_rejected(self):
        for row in self.render:
            if row['objects'] == 1024 and row['individual']:
                row['submitted_elements'] += 1
        self.write_rows('diagnostics-render.csv', self.render)
        with self.assertRaisesRegex(ValueError, 'same amount'):
            diagnostics.analyze(self.root)

    def test_missing_contacts_and_non_presented_frames_are_rejected(self):
        self.quads[0]['results_equal'] = 0
        self.write_rows('diagnostics-quadtree.csv', self.quads)
        with self.assertRaisesRegex(ValueError, 'overlap sets'):
            diagnostics.analyze(self.root)
        self.render[0]['previous_present_result'] = 1
        self.write_rows('diagnostics-render.csv', self.render)
        with self.assertRaisesRegex(ValueError, 'unsuccessful presents'):
            diagnostics.analyze(self.root)


if __name__ == '__main__':
    unittest.main()
