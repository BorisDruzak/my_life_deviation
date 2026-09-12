#!/usr/bin/env python3
"""Reproduce laboratory experiments. Requires a built mld_sim, not network access."""
import argparse
import json
from pathlib import Path
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, default=Path('build/mld_sim'))
    parser.add_argument('--output', type=Path, default=Path('experiment-output'))
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f'Build the CMake project first; missing executable: {binary}')
    args.output.mkdir(parents=True, exist_ok=True)
    cases = {
        'week-seed42': ['--npcs', '128', '--seed', '42', '--days', '7'],
        'seed43-indexed': ['--npcs', '128', '--seed', '43', '--days', '1'],
        'seed43-reference': ['--npcs', '128', '--seed', '43', '--days', '1', '--reference'],
        'temptation-low': ['--npcs', '16', '--days', '1', '--temptation', '--norm', '0'],
        'temptation-high': ['--npcs', '16', '--days', '1', '--temptation', '--norm', '1'],
        'scarcity': ['--npcs', '16', '--days', '2', '--scarce'],
        'scale1024': ['--npcs', '1024', '--seconds', '21600'],
    }
    results = {}
    for name, options in cases.items():
        print(f'Running {name}', flush=True)
        output = subprocess.run([str(binary), *options], capture_output=True, text=True, check=True)
        results[name] = json.loads(output.stdout)
        (args.output / f'{name}.json').write_text(output.stdout, encoding='utf-8')
    ignored = {'generation_wall_seconds', 'simulation_wall_seconds', 'search_mode'}
    first, second = (results[key] for key in ('seed43-indexed', 'seed43-reference'))
    if {k: v for k, v in first.items() if k not in ignored} != {k: v for k, v in second.items() if k not in ignored}:
        raise RuntimeError('Reference/indexed semantic divergence')
    if results['temptation-low']['unauthorized_takes'] == 0 or results['temptation-high']['unauthorized_takes'] != 0:
        raise RuntimeError('Norm contrast regression')
    print('Exact differential match and norm contrast verified.')


if __name__ == '__main__':
    main()
