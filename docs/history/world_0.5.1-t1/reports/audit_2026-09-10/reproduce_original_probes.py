#!/usr/bin/env python3
"""Reproduce four ORIGINAL WORLD-0.5 observations; not new-feature acceptance.

Usage from project root:
  python3 reports/audit_2026-09-10/reproduce_original_probes.py \
    --exe build/world_sim --source . --out reports/reproduced_original
"""
from __future__ import annotations
import argparse
import collections
import json
import math
from pathlib import Path
import subprocess

def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))

def read_jsonl(path: Path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    exe, source, out = args.exe.resolve(), args.source.resolve(), args.out.resolve()
    if not exe.is_file() or not (source / "data/parameters.json").is_file():
        parser.error("Executable or source data directory does not exist")
    if out.exists() and any(out.iterdir()):
        parser.error("Output directory must be absent or empty; nothing is overwritten")
    out.mkdir(parents=True, exist_ok=True)
    fixtures = Path(__file__).resolve().parent / "fixtures"
    summaries = {}
    for name in ("completed_joint_retry", "interrupted_joint_retry", "unregistered_topic", "waiting_overrides_hunger"):
        target = out / name
        command = [str(exe), "run", "--data", str(source / "data"), "--scenario",
                   str(fixtures / (name + ".scenario.json")), "--minutes", "20", "--out", str(target)]
        run = subprocess.run(command, capture_output=True, text=True, check=False, timeout=120)
        (out / (name + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
        if run.returncode:
            raise RuntimeError(f"{name}: executable exited {run.returncode}; see log")
        summary = read_json(target / "summary.json")
        decisions = read_jsonl(target / "decisions.jsonl")
        state = read_json(target / "state.json")["state"]
        counts = summary["event_counts"]
        goals = collections.Counter(d["goal"] for d in decisions)
        observed = {"counts": counts, "decision_goals": dict(goals), "state_hash": summary["state_hash"]}
        if name == "completed_joint_retry":
            assert counts.get("joint_started", 0) == 1
            assert counts.get("joint_ended", 0) == 1
            assert counts.get("command_rejected", 0) == 0
        elif name == "interrupted_joint_retry":
            assert counts.get("participation_withdrawn", 0) == 1
            assert counts.get("command_rejected", 0) == 32
            assert goals["attend_agreement"] == 34
        elif name == "unregistered_topic":
            assert counts.get("joint_started", 0) == 1
            assert "unregistered_topic_for_audit" in state["actors"]["a"]["repetition"]
        else:
            assert goals["await_response"] == 19
            assert counts.get("food_consumed", 0) == 0
            assert state["items"]["meal"]["remaining_units"] == 15
            hunger = state["actors"]["a"]["needs"]["N01"]["value"]
            assert math.isclose(hunger, 15.5 - 20*3.2/60, abs_tol=1e-10)
            observed["N01_end"] = hunger
        summaries[name] = observed
        print(f"REPRODUCED ORIGINAL: {name}")
    (out / "original_observations.json").write_text(json.dumps(summaries, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print("4/4 original observations reproduced. This does NOT mean defects are fixed.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
