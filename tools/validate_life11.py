#!/usr/bin/env python3
"""Reproduce the paired LIFE-0.11 experimental matrix; no external packages.

Every run starts from a freshly generated cohort. Time statistics from parallel
runs are diagnostic wall times, not a performance benchmark.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, default=Path('build/life_sim'))
    p.add_argument('--output', type=Path, default=Path('reports/life11/matrix'))
    p.add_argument('--population', type=int, default=16)
    p.add_argument('--days', type=int, default=7)
    p.add_argument('--seeds', type=int, nargs='+', default=[42, 7, 101])
    p.add_argument('--jobs', type=int, default=1)
    p.add_argument('--timeout', type=int, default=600)
    args = p.parse_args()
    if args.jobs < 1 or args.days < 1 or args.timeout < 1:
        p.error('jobs, days and timeout must be positive')
    binary = args.binary.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=True)
    metadata = {'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'platform': platform.platform(), 'logical_cpus': os.cpu_count(),
                'population': args.population, 'days': args.days, 'seeds': args.seeds,
                'concurrent_processes': args.jobs,
                'control': 'same executable, --community; familiarity fix shared',
                'warning': 'wall time includes contention; coefficients are experimental'}
    (args.output / 'environment.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    variants = {'control': ['--community'], 'life': ['--life-projects'],
                'life_no_satiation_forecast': ['--life-projects', '--no-satiation-forecast']}
    def run(job: tuple[str, int]) -> dict:
        variant, seed = job
        folder = args.output / f'{variant}_seed{seed}'
        folder.mkdir(exist_ok=True)
        cmd = [str(binary), *variants[variant], '--population', str(args.population),
               '--days', str(args.days), '--seed', str(seed)]
        for flag, name in [('summary','summary.json'), ('life-state','life.json'),
                           ('community-state','community.json'), ('social-state','social.json'),
                           ('actors','actors.csv')]:
            cmd.extend(['--'+flag, str(folder / name)])
        start = time.monotonic()
        result = {'variant': variant, 'seed': seed, 'command': cmd}
        with (folder / 'stdout.log').open('w') as out, (folder / 'stderr.log').open('w') as err:
            try:
                proc = subprocess.run(cmd, stdout=out, stderr=err, timeout=args.timeout, check=False)
                result['returncode'] = proc.returncode
            except subprocess.TimeoutExpired:
                result['returncode'] = 124
                result['error'] = 'test timeout; not a successful simulation'
        if result['returncode'] == 0:
            summary = json.loads((folder / 'summary.json').read_text(encoding='utf-8'))
            result['all_alive'] = summary['alive'] == args.population
            result['completed_duration'] = summary['seconds'] == args.days*86400
            if not result['all_alive'] or not result['completed_duration']:
                result['error'] = 'normal-world survival/duration acceptance gate failed'
        result['elapsed_seconds'] = time.monotonic()-start
        (folder / 'execution.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps({k: result[k] for k in ['variant','seed','returncode','elapsed_seconds']}), flush=True)
        return result
    jobs = [(variant, seed) for variant in variants for seed in args.seeds]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(run, jobs))
    (args.output / 'executions.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return int(any(r['returncode'] != 0 or 'error' in r for r in results))
if __name__ == '__main__':
    raise SystemExit(main())
