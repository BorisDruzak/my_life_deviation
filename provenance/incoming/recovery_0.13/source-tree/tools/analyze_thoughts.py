#!/usr/bin/env python3
"""Audit emitted thought traces; never invent narrative or simulate NPC reasoning."""
from __future__ import annotations
import argparse
import collections
import json
from pathlib import Path


def analyze(path: Path) -> dict:
    by_actor: dict[int, dict[int, dict]] = collections.defaultdict(dict)
    kinds: collections.Counter = collections.Counter()
    previous_ms: dict[int, int] = {}
    errors: list[str] = []
    latency: list[int] = []
    examples: dict[str, list[dict]] = collections.defaultdict(list)
    max_slots = max_spent = checked_intents = 0
    rows = 0
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            rows += 1
            actor, ident = row["actor"], row["thought"]
            kind, now = row["kind"], row["ms"]
            prefix = f"{path.name}:{line_number} actor={actor} thought={ident}: "
            if ident in by_actor[actor]:
                errors.append(prefix + "duplicate thought identifier")
            if now < previous_ms.get(actor, 0) or row["started_ms"] > now:
                errors.append(prefix + "invalid chronological order")
            if kind not in ("notice", "stale") and now-row["started_ms"] < 125:
                errors.append(prefix + "cognitive result earlier than maximum possible operation rate")
            if not 0 <= row["slots"] <= 12 or not 0 <= row["spent"] <= 32:
                errors.append(prefix + "model budget exceeded")
            if kind == "intent" and row["method"] != "idle":
                basis = by_actor[actor].get(row["basis"])
                if not basis or basis["kind"] != "forecast" or basis["method"] != row["method"]:
                    errors.append(prefix + "intent has no matching completed personal forecast")
                elif basis["ms"] > now:
                    errors.append(prefix + "intent used a future forecast")
                checked_intents += 1
            if kind == "forecast" and row["origin"] != 3:
                errors.append(prefix + "forecast not marked Imagined")
            if kind not in ("notice", "stale"):
                latency.append(now - row["started_ms"])
            if len(examples[kind]) < 2:
                examples[kind].append(row)
            kinds[kind] += 1
            max_slots, max_spent = max(max_slots, row["slots"]), max(max_spent, row["spent"])
            previous_ms[actor] = now
            by_actor[actor][ident] = row
    return {"file": path.name, "rows": rows, "actors": len(by_actor),
            "kinds": dict(kinds), "checked_intents": checked_intents,
            "max_slots": max_slots, "max_spent": max_spent,
            "minimum_operation_ms": min(latency) if latency else None,
            "errors": errors, "examples": dict(examples)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("traces", nargs="+", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    try:
        reports = [analyze(path) for path in args.traces]
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(2, f"Trace could not be audited: {exc}\n")
    content = json.dumps(reports, ensure_ascii=False, indent=2)
    if args.out:
        args.out.write_text(content + "\n", encoding="utf-8")
    else:
        print(content)
    for report in reports:
        print(f'{report["file"]}: {report["rows"]} rows, '
              f'{report["checked_intents"]} intents checked, {len(report["errors"])} errors')
    return int(any(report["errors"] for report in reports))


if __name__ == "__main__":
    raise SystemExit(main())
