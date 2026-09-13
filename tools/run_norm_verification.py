#!/usr/bin/env python3
"""Run reproducible NORM-MEMORY causal pairs and free-world matrices."""
from __future__ import annotations

import argparse
import concurrent.futures
import gzip
import hashlib
import json
import math
import os
import shutil
import statistics
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

PAIR_CASES = (
    "clothing_history_pair",
    "privacy_history_pair",
    "return_history_pair",
    "help_history_pair",
    "work_history_pair",
)
PRIMARY_METRICS = {
    "clothing_history_pair": ("actor1_garment_tier", 1),
    "privacy_history_pair": ("actor1_disclosures", -1),
    "return_history_pair": ("actor1_returns", 1),
    "help_history_pair": ("actor1_gifts_received", 1),
    "work_history_pair": ("actor1_work_hours", 1),
}
PAIR_SEEDS = (
    42, 7, 101, 2026, 3, 11, 19, 29,
    37, 47, 59, 71, 83, 97, 109, 127,
    149, 173, 197, 223, 251, 281, 313, 347,
    383, 421, 463, 509, 557, 607, 659, 719,
)
FREE_SEEDS = (42, 7, 101, 2026)
FREE_MODES = ("legacy", "enabled", "no-effects", "frozen-learning")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def executable(build_dir: Path, name: str) -> Path:
    candidates = (build_dir / name, build_dir / f"{name}.exe", build_dir / "Release" / f"{name}.exe")
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError(f"cannot find {name} in {build_dir}")


def ctest_executable() -> str | None:
    found = shutil.which("ctest")
    if found:
        return found
    if os.name == "nt":
        for variable in ("ProgramFiles", "ProgramFiles(x86)"):
            root = os.environ.get(variable)
            if root and (candidate := Path(root) / "CMake" / "bin" / "ctest.exe").is_file():
                return str(candidate)
    return None


def discover_ctest(build_dir: Path, explicit: Path | None) -> str | None:
    if explicit:
        return str(explicit.resolve()) if explicit.is_file() else None
    cache = build_dir / "CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("CMAKE_COMMAND:INTERNAL="):
                candidate = Path(line.split("=", 1)[1]).with_name("ctest.exe" if os.name == "nt" else "ctest")
                if candidate.is_file():
                    return str(candidate)
    return ctest_executable()


def source_snapshot(repo: Path, binaries: dict[str, Path]) -> dict[str, Any]:
    roots = [
        repo / "CMakeLists.txt",
        repo / "CMakePresets.json",
        repo / "cmake",
        repo / "apps" / "simulate",
        repo / "engine" / "include",
        repo / "engine" / "src",
        repo / "engine" / "tests",
        repo / "tools" / "norm_behavior_scenarios.cpp",
        repo / "tools" / "norm_perf_probe.cpp",
        repo / "tools" / "run_norm_verification.py",
        repo / "game" / "profiles" / "norm_memory_0.1",
    ]
    files: list[Path] = []
    for root in roots:
        if root.is_file():
            files.append(root)
        elif root.is_dir():
            files.extend(path for path in root.rglob("*") if path.is_file())
    source = {path.relative_to(repo).as_posix(): sha256(path) for path in sorted(set(files))}
    return {"source": source, "executables": {name: sha256(path) for name, path in binaries.items()}}


def git_head(repo: Path) -> str | None:
    try:
        return subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=repo, text=True,
            encoding="utf-8", capture_output=True, check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def artifact_inventory(directory: Path) -> list[dict[str, Any]]:
    rows = []
    if directory.exists():
        for path in sorted(p for p in directory.rglob("*") if p.is_file()):
            rows.append({"path": path.relative_to(directory).as_posix(), "bytes": path.stat().st_size, "sha256": sha256(path)})
    return rows


def run_one(command: list[str], run_dir: Path, name: str) -> dict[str, Any]:
    run_dir.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    completed = subprocess.run(command, text=True, encoding="utf-8", errors="replace", capture_output=True, check=False)
    elapsed = time.perf_counter() - started
    stdout_path = run_dir / f"{name}.stdout.log"
    stderr_path = run_dir / f"{name}.stderr.log"
    stdout_path.write_text(completed.stdout, encoding="utf-8")
    stderr_path.write_text(completed.stderr, encoding="utf-8")
    compressed = []
    for path in sorted(run_dir.rglob("*.jsonl")):
        if path.stat().st_size < 1024 * 1024:
            continue
        target = path.with_suffix(path.suffix + ".gz")
        with path.open("rb") as source, gzip.open(target, "wb", compresslevel=6) as destination:
            shutil.copyfileobj(source, destination)
        path.unlink()
        compressed.append({
            "from": path.relative_to(run_dir).as_posix(),
            "to": target.relative_to(run_dir).as_posix(),
            "bytes": target.stat().st_size,
            "sha256": sha256(target),
        })
    return {
        "name": name,
        "command": command,
        "returncode": completed.returncode,
        "wall_seconds": elapsed,
        "stdout": stdout_path.name,
        "stderr": stderr_path.name,
        "compressed": compressed,
    }


def exact_sign_p(better: int, worse: int) -> float:
    count = better + worse
    if not count:
        return 1.0
    tail = sum(math.comb(count, i) for i in range(min(better, worse) + 1)) / (2**count)
    return min(1.0, 2 * tail)


def wilson(successes: int, count: int) -> list[float]:
    if not count:
        return [0.0, 1.0]
    z = 1.959963984540054
    p = successes / count
    denominator = 1 + z * z / count
    centre = (p + z * z / (2 * count)) / denominator
    radius = z * math.sqrt(p * (1 - p) / count + z * z / (4 * count * count)) / denominator
    return [max(0.0, centre - radius), min(1.0, centre + radius)]


def percentile(values: list[float], probability: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, min(len(ordered) - 1, math.ceil(probability * len(ordered)) - 1))
    return ordered[index]


def pair_analysis(pair_files: list[Path]) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = {case: [] for case in PAIR_CASES}
    for path in pair_files:
        try:
            row = json.loads(path.read_text(encoding="utf-8"))
            grouped[row["case"]].append(row)
        except (OSError, ValueError, KeyError) as error:
            grouped.setdefault("invalid", []).append({"path": str(path), "error": str(error)})
    report: dict[str, Any] = {}
    for case in PAIR_CASES:
        rows = grouped[case]
        integrity = [
            bool(row.get("same_physical_initial_state")) and
            isinstance(row.get("common_physical_initial_hash"), str) and
            len(row["common_physical_initial_hash"]) == 64 and
            isinstance(row.get("control", {}).get("history_initial_hash"), str) and
            len(row["control"]["history_initial_hash"]) == 64 and
            isinstance(row.get("treatment", {}).get("history_initial_hash"), str) and
            len(row["treatment"]["history_initial_hash"]) == 64 and
            row["control"]["history_initial_hash"] != row["treatment"]["history_initial_hash"]
            for row in rows
        ]
        signed = [float(row["signed_delta"]) for row in rows]
        better = sum(value > 0 for value in signed)
        worse = sum(value < 0 for value in signed)
        ties = sum(value == 0 for value in signed)
        discordant = better + worse
        report[case] = {
            "predeclared_primary_metric": PRIMARY_METRICS[case][0],
            "predeclared_expected_direction": PRIMARY_METRICS[case][1],
            "pairs": len(rows),
            "better": better,
            "worse": worse,
            "ties": ties,
            "discordant": discordant,
            "two_sided_exact_sign_p": exact_sign_p(better, worse),
            "better_share_wilson95": wilson(better, discordant),
            "mean_signed_delta": statistics.fmean(signed) if signed else None,
            "initial_state_integrity": all(integrity),
            "initial_hashes": [
                {
                    "common_physical": row.get("common_physical_initial_hash"),
                    "control_history": row.get("control", {}).get("history_initial_hash"),
                    "treatment_history": row.get("treatment", {}).get("history_initial_hash"),
                }
                for row in rows
            ],
            "gate_pass": len(rows) == len(PAIR_SEEDS) and all(integrity) and better > worse and exact_sign_p(better, worse) <= 0.05,
        }
    return report


def physical_analysis(directory: Path, expected_seconds: int, expected_population: int) -> dict[str, Any]:
    errors: list[str] = []
    warnings: list[str] = []
    reports: dict[str, Any] = {}
    for name in ("summary", "community", "life", "self"):
        path = directory / f"{name}.json"
        try:
            reports[name] = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            errors.append(f"{name}_report:{error}")
    if errors:
        return {"hard_gate_pass": False, "errors": errors, "warnings": warnings}

    summary, community, life, self_report = (
        reports["summary"], reports["community"], reports["life"], reports["self"]
    )
    if summary.get("seconds") != expected_seconds or community.get("ms") != expected_seconds * 1000:
        errors.append("duration_mismatch")
    if summary.get("population") != expected_population or len(community.get("actors", [])) != expected_population:
        errors.append("population_mismatch")
    all_alive = summary.get("alive") == expected_population and all(
        actor.get("alive") is True for actor in community.get("actors", [])
    )
    if not all_alive:
        errors.append("not_all_actors_alive")

    time_residuals: list[float] = []
    for actor in community.get("actors", []):
        values = [float(value) for value in actor.get("time_ms", {}).values()]
        if not values or any(not math.isfinite(value) or value < 0 for value in values):
            errors.append(f"bad_primary_time:{actor.get('id')}")
            continue
        residual = sum(values) - expected_seconds * 1000
        time_residuals.append(residual)
        if abs(residual) > 1001:
            errors.append(f"primary_time_coverage:{actor.get('id')}")

    failure_reasons = sum(
        int(count)
        for actor in self_report.get("actors", [])
        for count in actor.get("failures_by_reason", {}).values()
    )
    failures = int(summary.get("failures", -1))
    if failures < 0 or failure_reasons != failures:
        errors.append("failure_count_mismatch")
    if failures:
        warnings.append("action_failures_nonzero")

    critical_by_need = [0.0, 0.0, 0.0]
    longest_by_need = [0.0, 0.0, 0.0]
    for actor in self_report.get("actors", []):
        for index, value in enumerate(actor.get("critical_seconds", [])[:3]):
            critical_by_need[index] += float(value)
        for index, value in enumerate(actor.get("critical_longest_seconds", [])[:3]):
            longest_by_need[index] = max(longest_by_need[index], float(value))
    critical_actor_seconds = float(summary.get("critical_actor_seconds", 0))
    if critical_actor_seconds:
        warnings.append("critical_needs_nonzero")
    if float(summary.get("max_damage", 0)) > 0:
        warnings.append("damage_nonzero")

    secondary_hours = float(life.get("secondary_social_hours", 0))
    if not math.isfinite(secondary_hours) or secondary_hours < 0:
        errors.append("invalid_secondary_social_time")
    return {
        "hard_gate_pass": not errors,
        "errors": errors,
        "warnings": warnings,
        "alive": summary.get("alive"),
        "population": summary.get("population"),
        "failures": failures,
        "failure_reasons_total": failure_reasons,
        "critical_actor_seconds": critical_actor_seconds,
        "critical_by_need_seconds": critical_by_need,
        "critical_longest_by_need_seconds": longest_by_need,
        "max_damage": summary.get("max_damage"),
        "max_abs_primary_time_residual_ms": max(map(abs, time_residuals), default=None),
        "secondary_social_hours": secondary_hours,
        "actions": summary.get("actions", {}),
    }


def life_command(sim: Path, out: Path, seed: int, mode: str, policy: str, full: bool, population: int = 16, days: int = 14) -> list[str]:
    command = [str(sim), "--recovery", "--norm-mode", mode, "--budget-policy", policy, "--population", str(population), "--seed", str(seed)]
    command += ["--days", str(days)] if full else ["--seconds", "600"]
    command += [
        "--summary", str(out / "summary.json"), "--self-state", str(out / "self.json"),
        "--career-state", str(out / "career.json"), "--recovery-state", str(out / "recovery.json"),
        "--norm-state", str(out / "norms.json"), "--norm-trace", str(out / "norms.jsonl"),
        "--thoughts", str(out / "thoughts.jsonl"), "--save", str(out / "final.save"),
        "--actors", str(out / "actors.csv"), "--trace", str(out / "events.jsonl"),
        "--community-state", str(out / "community.json"), "--life-state", str(out / "life.json"),
    ]
    if full:
        command += ["--trace-actor", "1"]
    return command


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--quick", action="store_true", help="one pair seed and ten-minute free-world smoke runs")
    mode.add_argument("--full", action="store_true", help="the predeclared 32-seed pairs and complete free-world/load matrix")
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--pairs-only", action="store_true")
    scope.add_argument("--free-only", action="store_true")
    parser.add_argument("--case", choices=PAIR_CASES, help="run one causal case")
    parser.add_argument("--pair-seed", type=int, help="run one predeclared pair seed")
    parser.add_argument("--skip-tests", action="store_true")
    parser.add_argument("--jobs", type=int, default=1, help="parallel independent runs, 1..4")
    parser.add_argument("--ctest", type=Path, help="explicit ctest executable")
    parser.add_argument("--config", default="Release", help="CTest multi-config configuration")
    parser.add_argument("--build-dir", type=Path, default=Path("build-norm"))
    parser.add_argument("--out", type=Path, default=Path("out/norm-verification"))
    args = parser.parse_args()
    if not 1 <= args.jobs <= 4:
        parser.error("--jobs must be 1..4")
    if args.pair_seed is not None and args.pair_seed not in PAIR_SEEDS:
        parser.error("--pair-seed must be one of the predeclared pair seeds")
    if args.free_only and (args.case is not None or args.pair_seed is not None):
        parser.error("--case and --pair-seed select causal pairs and cannot be used with --free-only")
    full = bool(args.full)
    root = args.out.resolve();root.mkdir(parents=True, exist_ok=True)
    repo = Path(__file__).resolve().parents[1]
    try:
        scenario = executable(args.build_dir.resolve(), "norm_behavior_scenarios")
        sim = executable(args.build_dir.resolve(), "life_sim")
        tests = executable(args.build_dir.resolve(), "life_tests")
    except FileNotFoundError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    binaries = {"norm_behavior_scenarios": scenario, "life_sim": sim, "life_tests": tests}
    source_head_before = git_head(repo)
    before_snapshot = source_snapshot(repo, binaries)
    (root / "inputs-before.json").write_text(json.dumps(before_snapshot, indent=2) + "\n", encoding="utf-8")
    commands: list[dict[str, Any]] = []
    test_dir = root / "tests"
    if not args.skip_tests:
        ctest = discover_ctest(args.build_dir.resolve(), args.ctest)
        if ctest:
            ctest_command = [ctest, "--test-dir", str(args.build_dir.resolve()), "--output-on-failure", "-C", args.config]
            if not full:
                ctest_command += ["-R", "^norm_"]
            record = run_one(ctest_command, test_dir, "ctest");record.update({"kind": "regression"});commands.append(record)
        else:
            commands.append({"name": "ctest", "kind": "regression", "command": ["ctest"], "returncode": 127, "wall_seconds": 0, "stdout": None, "stderr": "ctest executable not found", "compressed": []})
        if full:
            record = run_one([str(tests)], test_dir, "life_tests_all");record.update({"kind": "regression"});commands.append(record)

    pair_files: list[Path] = []
    pair_cases = (args.case,) if args.case else PAIR_CASES
    pair_seeds = (args.pair_seed,) if args.pair_seed is not None else PAIR_SEEDS if full else PAIR_SEEDS[:1]
    run_pairs = not args.free_only
    run_free = not args.pairs_only and args.case is None and args.pair_seed is None
    specifications: list[tuple[list[str],Path,str,dict[str,Any]]] = []
    if run_pairs:
        for case in pair_cases:
            for seed in pair_seeds:
                directory = root / "pairs" / case / str(seed)
                scenario_command = [str(scenario), "--case", case, "--seed", str(seed), "--out", str(directory)]
                if not full:
                    scenario_command += ["--seconds", "600"]
                specifications.append((scenario_command,directory,"run",{"kind":"causal_pair","case":case,"seed":seed}))

    if run_free:
        for seed in FREE_SEEDS:
            for norm_mode in FREE_MODES:
                directory = root / "free" / "guarded" / norm_mode / str(seed)
                specifications.append((life_command(sim,directory,seed,norm_mode,"guarded",full),directory,"run",{"kind":"free_world","seed":seed,"norm_mode":norm_mode,"budget_policy":"guarded","expected_seconds":14*86400 if full else 600,"population":16}))
            directory = root / "free" / "deliberative" / "enabled" / str(seed)
            specifications.append((life_command(sim,directory,seed,"enabled","deliberative",full),directory,"run",{"kind":"free_world","seed":seed,"norm_mode":"enabled","budget_policy":"deliberative","expected_seconds":14*86400 if full else 600,"population":16}))
        if full:
            directory = root / "load" / "enabled-128x7"
            specifications.append((life_command(sim,directory,42,"enabled","guarded",True,population=128,days=7),directory,"run",{"kind":"load","seed":42,"population":128,"days":7,"expected_seconds":7*86400}))

    def execute(specification: tuple[list[str],Path,str,dict[str,Any]]) -> dict[str, Any]:
        command,directory,name,metadata=specification
        record=run_one(command,directory,name);record.update(metadata)
        if metadata["kind"] in {"free_world","load"} and record["returncode"]==0:
            record["physical_analysis"]=physical_analysis(directory,metadata["expected_seconds"],metadata["population"])
        return record

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        for record in executor.map(execute,specifications):
            commands.append(record)
            if record["kind"]=="causal_pair" and record["returncode"]==0:
                directory=root/"pairs"/record["case"]/str(record["seed"])
                if (directory/"pair.json").is_file():pair_files.append(directory/"pair.json")

    analysis = pair_analysis(pair_files)
    free_times = [float(item["wall_seconds"]) for item in commands if item["kind"] in {"free_world", "load"} and item["returncode"] == 0]
    all_commands_passed = all(item["returncode"] == 0 for item in commands)
    physical_gates = [item["physical_analysis"]["hard_gate_pass"] for item in commands if "physical_analysis" in item]
    physical_gates_passed = bool(physical_gates) and all(physical_gates) if run_free else None
    causal_matrix_complete=full and run_pairs and tuple(pair_cases)==PAIR_CASES and tuple(pair_seeds)==PAIR_SEEDS
    complete_matrix=causal_matrix_complete and run_free
    causal_gates_passed = all(item["gate_pass"] for item in analysis.values()) if causal_matrix_complete else None
    after_snapshot=source_snapshot(repo,binaries);snapshot_unchanged=before_snapshot==after_snapshot
    (root/"inputs-after.json").write_text(json.dumps(after_snapshot,indent=2)+"\n",encoding="utf-8")
    source_head_after = git_head(repo)
    stage_passed=all_commands_passed and snapshot_unchanged and causal_gates_passed is not False and physical_gates_passed is not False
    if complete_matrix:
        status="verification_passed" if stage_passed else "verification_failed"
        success: bool | None=stage_passed
    elif full:
        status="stage_completed" if stage_passed else "stage_failed"
        success=None
    else:
        status="smoke_completed" if stage_passed else "smoke_failed"
        success=None
    result = {
        "schema": "norm-verification-manifest-0.1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "profile": "full" if full else "quick",
        "scope": "pairs" if run_pairs and not run_free else "free" if run_free and not run_pairs else "all",
        "jobs": args.jobs,
        "predeclared_pair_seeds": list(PAIR_SEEDS),
        "free_world_seeds": list(FREE_SEEDS),
        "free_world_modes": list(FREE_MODES),
        "source_head": source_head_before,
        "source_head_after": source_head_after,
        "binary_sha256": {"norm_behavior_scenarios": sha256(scenario), "life_sim": sha256(sim), "life_tests": sha256(tests)},
        "tool_source_sha256": sha256(Path(__file__).resolve()),
        "input_snapshot_unchanged": snapshot_unchanged,
        "commands": commands,
        "causal_analysis": analysis,
        "free_world_wall_seconds": {"p50": percentile(free_times, .5), "p95": percentile(free_times, .95)},
        "all_commands_passed": all_commands_passed,
        "causal_gates_passed": causal_gates_passed,
        "physical_gates_passed": physical_gates_passed,
        "success": success,
        "status": status,
        "artifacts": [],
    }
    analysis_path = root / "analysis.json";analysis_path.write_text(json.dumps(analysis, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    result["artifacts"] = artifact_inventory(root)
    manifest_path = root / "manifest.json";manifest_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"manifest": str(manifest_path), "commands": len(commands), "status": result["status"]}, ensure_ascii=False))
    return 0 if stage_passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
