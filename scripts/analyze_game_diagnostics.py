"""Summarize capture-free rendering, navigation and collision measurements."""
import argparse
import csv
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import statistics


def percentile(values, fraction):
    values = sorted(values)
    offset = (len(values) - 1) * fraction
    lower = math.floor(offset)
    upper = math.ceil(offset)
    return values[lower] + (values[upper] - values[lower]) * (offset - lower)


def read_rows(path):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        rows = [{key: float(value) for key, value in row.items()} for row in csv.DictReader(stream)]
    if not rows or any(not math.isfinite(value) or value < 0 for row in rows for value in row.values()):
        raise ValueError(f'Invalid or empty measurement: {path.name}')
    return rows


def analyze(build):
    build = Path(build)
    binaries = build / 'runtime/Binaries'
    result = json.loads((binaries / 'diagnostics.json').read_text(encoding='utf-8'))
    receipt = json.loads((build / 'game-build.json').read_text(encoding='utf-8'))
    run = json.loads((build / 'probe-run.json').read_text(encoding='utf-8-sig'))
    if result.get('passed') is not True or result.get('recording') is not False or result.get('vsync') is not False:
        raise ValueError('Only successful capture-free diagnostics with VSync off can supply performance figures')
    if receipt['configuration'] != 'Release|x64' or run['exit_code'] != 0:
        raise ValueError('Diagnostics must be a successful Release run')
    if not result['paths_valid'] or not result['quadtree_results_equal'] or result['player_walked'] <= 1:
        raise ValueError('Gameplay or geometry verification failed')
    render = read_rows(binaries / 'diagnostics-render.csv')
    if {int(row['phase']) for row in render} != set(range(9)):
        raise ValueError('A rendering comparison phase is missing')
    if any(row['frame_ms'] <= 0 or row['previous_present_result'] != 0 for row in render):
        raise ValueError('Rendering includes invalid intervals or unsuccessful presents')
    summaries = []
    for count in (0, 128, 512, 1024):
        controls = {}
        for individual in ((0,) if count == 0 else (0, 1)):
            rows = [row for row in render if row['objects'] == count and row['individual'] == individual]
            if len(rows) < 20:
                raise ValueError('Insufficient rendering samples')
            times = [row['frame_ms'] for row in rows]
            item = {
                'objects': count, 'mode': 'individual' if individual else 'instanced',
                'samples': len(rows), 'average_fps': 1000 / statistics.fmean(times),
                'average_frame_ms': statistics.fmean(times),
                'frame_ms_p95': percentile(times, 0.95),
                'draw_calls_median': statistics.median(row['draw_calls'] for row in rows),
                'submitted_elements_median': statistics.median(row['submitted_elements'] for row in rows),
            }
            summaries.append(item)
            controls[individual] = item
        if count:
            if controls[0]['submitted_elements_median'] != controls[1]['submitted_elements_median']:
                raise ValueError('Rendering controls did not submit the same amount of geometry')
            if controls[0]['draw_calls_median'] >= controls[1]['draw_calls_median']:
                raise ValueError('The instancing control did not reduce draw submissions')
    paths = read_rows(binaries / 'diagnostics-paths.csv')
    path_summary = []
    for query in range(6):
        rows = [row for row in paths if row['query'] == query]
        if len(rows) != 100 or any(row['valid'] != 1 for row in rows):
            raise ValueError('A fixed navigation query is missing or invalid')
        times = [row['elapsed_us'] / 1000 for row in rows]
        path_summary.append({
            'query': query, 'samples': len(rows),
            'median_ms': statistics.median(times), 'p95_ms': percentile(times, 0.95),
            'expanded_nodes': int(statistics.median(row['expanded_nodes'] for row in rows)),
            'points': int(statistics.median(row['points'] for row in rows)),
            'length_world_units': rows[0]['length'], 'direct_distance_world_units': rows[0]['direct_distance'],
        })
    quads = read_rows(binaries / 'diagnostics-quadtree.csv')
    quad_summary = []
    for count in (128, 512, 1024):
        rows = [row for row in quads if row['objects'] == count]
        if len(rows) != 15 or any(row['results_equal'] != 1 or row['hits'] <= 0 for row in rows):
            raise ValueError('Quadtree and all-pairs overlap sets do not match')
        total = count * (count - 1) // 2
        if any(row['all_pair_tests'] != total or row['tree_pair_tests'] > total for row in rows):
            raise ValueError('Invalid collision candidate count')
        tests = statistics.median(row['tree_pair_tests'] for row in rows)
        build_us = statistics.median(row['tree_build_us'] for row in rows)
        collision_us = statistics.median(row['tree_collision_us'] for row in rows)
        quad_summary.append({
            'objects': count, 'samples': len(rows), 'all_pair_tests': total,
            'tree_pair_tests': int(tests), 'candidate_reduction_percent': (1 - tests / total) * 100,
            'all_pairs_median_ms': statistics.median(row['all_pairs_us'] for row in rows) / 1000,
            'tree_build_median_ms': build_us / 1000, 'tree_collision_median_ms': collision_us / 1000,
            'tree_rebuild_and_collision_median_ms': statistics.median(
                row['tree_build_us'] + row['tree_collision_us'] for row in rows) / 1000,
            'overlap_pairs': int(statistics.median(row['hits'] for row in rows)),
            'overlap_sets_equal': True,
        })
    sources = {}
    for name in ('diagnostics-render.csv', 'diagnostics-paths.csv', 'diagnostics-quadtree.csv', 'diagnostics.json'):
        sources[name] = hashlib.sha256((binaries / name).read_bytes()).hexdigest()
    return {
        'schema': 'game-diagnostics-v1',
        'date': datetime.fromtimestamp((binaries / 'diagnostics.json').stat().st_mtime).astimezone().date().isoformat(),
        'resolution': run['resolution'], 'configuration': receipt['configuration'],
        'vsync': False, 'capture_enabled': False,
        'executable_sha256': run['executable_sha256'].lower(),
        'source_tree_sha256': receipt['source_tree_sha256'],
        'nav_triangles': result['nav_triangles'], 'static_model_indices': result['model_indices'],
        'static_model_shader_pass': result['model_pass'],
        'render': summaries, 'navigation': path_summary, 'quadtree': quad_summary,
        'input_sha256': sources,
        'conditions': {
            'render': 'Same opaque cemetery prop, material, shader pass, transforms and resolution; 8 seconds per phase, first 2 seconds excluded. Actors frozen in this diagnostic scene.',
            'navigation': 'Six fixed routes on the actual game NavMesh, 5 warmup calls then 100 timed calls each. The six routes were sampled for NavMesh membership at 0.25 world-unit intervals.',
            'quadtree': 'Same sphere collider set with intentional overlaps, 2 warmups then 15 comparisons. All-pairs uses i<j; tree time includes its normal duplicate suppression and collision state bookkeeping. Rebuild cost reported separately.',
        },
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    try:
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        summary = analyze(args.build_root)
        output.write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'output': str(output), 'render_groups': len(summary['render']),
                          'navigation_queries': len(summary['navigation']),
                          'quadtree_cases': len(summary['quadtree'])}))
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(1, f'Diagnostic measurement rejected: {error}\n')
