"""T0 regressions against real CLI output, including deterministic resume.

Usage: python3 tests/autonomy_cli_test.py build/world_sim . [--out EMPTY_DIRECTORY]
Without --out all reports are temporary; with --out evidence is retained.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def read_jsonl(path: Path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def require(condition: bool, explanation: str) -> None:
    if not condition:
        raise AssertionError(explanation)


class Suite:
    def __init__(self, exe: Path, source: Path, output: Path):
        self.exe = exe
        self.source = source
        self.output = output
        self.fixtures = source / "reports/audit_2026-09-10/fixtures"
        self.observations: dict[str, dict] = {}

    def run(self, name: str, scenario: str | Path, minutes: int = 20,
            snapshot: Path | None = None) -> Path:
        scenario_path = (self.fixtures / (scenario + ".scenario.json")
                         if isinstance(scenario, str) else scenario)
        target = self.output / name
        command = [str(self.exe), "resume" if snapshot else "run", "--data",
                   str(self.source / "game/rules"), "--scenario", str(scenario_path),
                   "--minutes", str(minutes), "--check-every", "1", "--out", str(target)]
        if snapshot:
            command += ["--snapshot", str(snapshot)]
        result = subprocess.run(command, cwd=self.source, text=True, capture_output=True, timeout=120)
        (self.output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        require(result.returncode == 0, f"{name}: CLI exit {result.returncode}: {result.stderr}")
        summary = read_json(target / "summary.json")
        self.observations[name] = {
            "event_counts": summary["event_counts"], "state_hash": summary["state_hash"],
            "A15_times": [d["time"] for d in read_jsonl(target / "decisions.jsonl")
                          if (d.get("command") or {}).get("type") == "A15"],
            "A07_times": [d["time"] for d in read_jsonl(target / "decisions.jsonl")
                          if (d.get("command") or {}).get("type") == "A07"],
        }
        return target

    def no_replayed_joint(self, output: Path) -> None:
        for decision in read_jsonl(output / "decisions.jsonl"):
            command = decision.get("command")
            if decision["time"] > 3 and command and command["type"] == "A15":
                require(command["args"]["proposal"] != "a@1/proposal",
                        f"Interrupted agreement selected again: {decision}")
        counts = read_json(output / "summary.json")["event_counts"]
        require(counts.get("command_rejected", 0) == 0, f"Unexpected command rejection: {counts}")

    def waiting_food(self) -> None:
        target = self.run("waiting_food", "waiting_overrides_hunger")
        decisions = read_jsonl(target / "decisions.jsonl")
        eats = [d for d in decisions if d["actor"] == "a"
                and (d.get("command") or {}).get("type") == "A07"]
        require(bool(eats), "Owned food was never eaten while waiting for a reply")
        require(eats[0]["time"] == 1, f"Food should be chosen at the first free boundary: {eats[0]}")
        counts = read_json(target / "summary.json")["event_counts"]
        require(counts.get("food_consumed", 0) > 0, "No actual food consumption")
        require(counts.get("need_critical", 0) == 0, "Hunger crossed its critical threshold before eating")
        state = read_json(target / "state.json")["state"]
        require(state["items"]["meal"]["remaining_units"] == 0, "Food effect was not executed")

    def interrupted(self) -> None:
        target = self.run("interrupted", "interrupted_joint_retry")
        counts = read_json(target / "summary.json")["event_counts"]
        require(counts.get("joint_started", 0) == 1, "Fixture never started the joint action")
        require(counts.get("participation_withdrawn", 0) == 1, "Fixture never withdrew participation")
        require(counts.get("action_interrupted", 0) == 1, "Fixture never interrupted the joint action")
        self.no_replayed_joint(target)

    def completed(self) -> None:
        target = self.run("completed", "completed_joint_retry")
        counts = read_json(target / "summary.json")["event_counts"]
        require(counts.get("joint_started", 0) == 1, "Expected exactly one joint session")
        require(counts.get("joint_ended", 0) == 1, "Expected exactly one normal completion")
        require(counts.get("command_rejected", 0) == 0, "Completed session was retried")
        commands = [d for d in read_jsonl(target / "decisions.jsonl")
                    if (d.get("command") or {}).get("type") == "A15"]
        require(len(commands) == 2, f"Expected one independent readiness per participant: {commands}")
        require(all(d["time"] == 2 for d in commands), "Completed session selected again")

    def resume_interrupted(self) -> None:
        full = self.run("resume_full", "interrupted_joint_retry")
        first = self.run("resume_first", "interrupted_joint_retry", minutes=4)
        resumed = self.run("resume_second", "interrupted_joint_retry", minutes=16,
                           snapshot=first / "state.json")
        require(read_json(full / "state.json") == read_json(resumed / "state.json"),
                "Resume changed the deterministic state")
        self.no_replayed_joint(resumed)

    def retry_readiness(self) -> None:
        scenario = read_json(self.fixtures / "completed_joint_retry.scenario.json")
        # At t=2 only a submits readiness; at t=3 both are free and may try together.
        scenario["script"].append({"at": 2, "command": {"actor": "b", "sequence": 3,
            "type": "A01", "args": {"minutes": 1}}})
        fixture = self.output / "retry_readiness.scenario.json"
        fixture.write_text(json.dumps(scenario, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        target = self.run("retry_readiness", fixture)
        counts = read_json(target / "summary.json")["event_counts"]
        require(counts.get("command_rejected", 0) == 1, "Expected only the initial unmatched readiness to fail")
        require(counts.get("joint_started", 0) == 1 and counts.get("joint_ended", 0) == 1,
                "Temporary rejection must not make the session terminal")
        rows = [d for d in read_jsonl(target / "decisions.jsonl")
                if d["time"] == 3 and (d.get("command") or {}).get("type") == "A15"]
        require({d["actor"] for d in rows} == {"a", "b"}, "Both participants must independently retry")

    def execute(self) -> int:
        results = []
        for case in ("waiting_food", "interrupted", "completed", "resume_interrupted", "retry_readiness"):
            try:
                getattr(self, case)()
            except (AssertionError, OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
                results.append({"case": case, "status": "FAIL", "reason": str(error)})
                print(f"FAIL T0_CLI_{case}: {error}")
            else:
                results.append({"case": case, "status": "PASS"})
                print(f"PASS T0_CLI_{case}")
        failed = sum(result["status"] == "FAIL" for result in results)
        report = {"cases": results, "passed": len(results) - failed, "failed": failed,
                  "observations": self.observations}
        (self.output / "results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                                   encoding="utf-8")
        print(f"RESULT passed={len(results) - failed} failed={failed}")
        return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    exe, source = args.exe.resolve(), args.source.resolve()
    if not exe.is_file() or not (source / "game/rules/parameters.json").is_file():
        parser.error("Executable or source data is missing")
    if args.out is not None:
        output = args.out.resolve()
        if output.exists() and (not output.is_dir() or any(output.iterdir())):
            parser.error("--out must be absent or an empty directory; nothing is overwritten")
        output.mkdir(parents=True, exist_ok=True)
        return Suite(exe, source, output).execute()
    with tempfile.TemporaryDirectory(prefix="npc-t0-") as temporary:
        return Suite(exe, source, Path(temporary)).execute()


if __name__ == "__main__":
    raise SystemExit(main())
