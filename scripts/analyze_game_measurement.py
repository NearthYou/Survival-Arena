"""Summarize opt-in frame and process-memory measurements from an actual game run."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics


def percentile(values, fraction):
    position = (len(values) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    return values[lower] + (values[upper] - values[lower]) * (position - lower)


def analyze(path, warmup=30, minimum_seconds=0):
    with Path(path).open(encoding='utf-8', newline='') as stream:
        samples = [{name: float(value) for name, value in row.items()} for row in csv.DictReader(stream)]
    if not samples or warmup < 0 or minimum_seconds < 0:
        raise ValueError('Measurement is empty or its time bounds are invalid')
    previous = 0
    for sample in samples:
        if (any(not math.isfinite(value) or value < 0 for value in sample.values())
                or sample['frame_ms'] <= 0 or sample['elapsed_seconds'] <= previous):
            raise ValueError('Measurement contains invalid or unordered samples')
        previous = sample['elapsed_seconds']
    duration = samples[-1]['elapsed_seconds']
    if duration < minimum_seconds:
        raise ValueError(f'Measurement ended early: {duration:.3f} < {minimum_seconds:.3f} seconds')
    if abs(sum(row['frame_ms'] for row in samples) / 1000 - duration) > 0.1:
        raise ValueError('Frame intervals do not match the measured wall time')
    rows = [row for row in samples if row['elapsed_seconds'] >= warmup]
    if not rows:
        raise ValueError('No samples remain after warmup')
    times = sorted(row['frame_ms'] for row in rows)
    slowest = times[-max(1, math.ceil(len(times) * 0.01)):]
    metadata = Path(str(path) + '.meta.txt')
    conditions = dict(line.split('=', 1) for line in metadata.read_text().splitlines()) if metadata.exists() else {}
    return {'duration_seconds': duration, 'warmup_seconds': warmup, 'frames': len(times),
            'non_presented_frames': sum(row.get('present_result', 0) != 0 for row in rows),
            'frame_statistics_valid': all(row.get('present_result', 0) == 0 for row in rows),
            'average_fps': 1000 / statistics.fmean(times),
            'one_percent_low_fps': 1000 / statistics.fmean(slowest),
            'frame_ms_p50': percentile(times, 0.5), 'frame_ms_p95': percentile(times, 0.95),
            'frame_ms_p99': percentile(times, 0.99), 'frame_ms_max': times[-1],
            'working_set_peak_bytes': int(max(row['working_set_bytes'] for row in rows)),
            'private_peak_bytes': int(max(row['private_bytes'] for row in rows)),
            'private_start_bytes': int(rows[0]['private_bytes']),
            'private_end_bytes': int(rows[-1]['private_bytes']),
            'private_growth_bytes': int(rows[-1]['private_bytes'] - rows[0]['private_bytes']),
            'handle_growth': int(rows[-1]['handles'] - rows[0]['handles']),
            'conditions': conditions, 'source': str(Path(path).absolute())}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--warmup-seconds', type=float, default=30)
    parser.add_argument('--minimum-seconds', type=float, default=0)
    args = parser.parse_args()
    try:
        result = analyze(args.input, args.warmup_seconds, args.minimum_seconds)
        Path(args.output).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(result))
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(1, f'Game measurement failed: {error}\n')
