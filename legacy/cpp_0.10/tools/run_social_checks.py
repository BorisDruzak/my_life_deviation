#!/usr/bin/env python3
"""Run the actual CLI, retain its JSONL and expose each command and return code."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import time


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--binary", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--long", action="store_true")
    args = p.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        p.error("binary does not exist")
    args.out.mkdir(parents=True, exist_ok=True)
    cases = [(x, x, 8, 900, 42, 0) for x in (
        "personal-boundaries", "friendly", "romantic", "romantic-norm",
        "recipient-refuses", "pleasure-mismatch", "borrow", "information",
        "status", "status-norm", "mixed")]
    cases.append(("borrow-novice", "borrow-novice", 8, 3600, 42, 0))
    if args.long:
        cases += [("mixed-day", "mixed", 16, 86400, 42, 1),
                  ("mixed-seed7", "mixed", 16, 7200, 7, 1),
                  ("mixed-seed101", "mixed", 16, 7200, 101, 1)]
    records = []
    for name, scene, count, seconds, seed, trace_actor in cases:
        root = args.out / name
        root.mkdir(exist_ok=True)
        cmd = [str(binary), "--population", str(count), "--seed", str(seed),
               "--seconds", str(seconds), "--social-scene", scene,
               "--summary", str(root / "summary.json"),
               "--social-state", str(root / "social-state.json"),
               "--thoughts", str(root / "thoughts.jsonl"),
               "--trace", str(root / "events.jsonl")]
        if trace_actor:
            cmd += ["--trace-actor", str(trace_actor)]
        start = time.perf_counter()
        with (root / "process.log").open("w", encoding="utf-8") as log:
            done = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT,
                                  check=False, timeout=180)
        record = {"name": name, "scene": scene, "population": count,
                  "seconds": seconds, "seed": seed, "trace_actor": trace_actor,
                  "command": cmd, "exit_code": done.returncode,
                  "wall_seconds": time.perf_counter() - start}
        (root / "run.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n")
        records.append(record)
        print(name, done.returncode, round(record["wall_seconds"], 3), flush=True)
        if done.returncode:
            break
    (args.out / "runs.json").write_text(json.dumps(records, ensure_ascii=False, indent=2) + "\n")
    return int(any(x["exit_code"] for x in records))


if __name__ == "__main__":
    raise SystemExit(main())
