"""Validate repository navigation maps without third-party dependencies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


VALID_STATUSES = {"normative", "implemented", "planned", "historical"}
MAP_NAMES = ("code-map.json", "doc-map.json")


def load_map(root: Path, name: str) -> tuple[dict, list[str]]:
    path = root / "navigation" / name
    if not path.is_file():
        return {}, [f"missing navigation map: navigation/{name}"]
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        return {}, [f"invalid JSON in navigation/{name}: {error.msg}"]
    if not isinstance(data, dict):
        return {}, [f"navigation/{name} must contain an object"]
    return data, []


def validate_map(root: Path, name: str, data: dict) -> list[str]:
    errors: list[str] = []
    nodes = data.get("nodes")
    if not isinstance(nodes, list) or not nodes:
        return [f"navigation/{name}: nodes must be a non-empty list"]

    ids: set[str] = set()
    for index, node in enumerate(nodes):
        label = f"navigation/{name} node {index}"
        if not isinstance(node, dict):
            errors.append(f"{label}: node must be an object")
            continue
        node_id = node.get("id")
        if not isinstance(node_id, str) or not node_id:
            errors.append(f"{label}: id must be a non-empty string")
        elif node_id in ids:
            errors.append(f"{label}: duplicate id {node_id}")
        else:
            ids.add(node_id)

        status = node.get("status")
        if status not in VALID_STATUSES:
            errors.append(f"{label}: status must be one of {sorted(VALID_STATUSES)}")

        relative = node.get("path")
        if not isinstance(relative, str) or not relative:
            errors.append(f"{label}: path must be a non-empty string")
            continue
        candidate = (root / relative).resolve()
        try:
            candidate.relative_to(root)
        except ValueError:
            errors.append(f"{label}: path escapes repository: {relative}")
            continue
        if not candidate.exists():
            errors.append(f"{label}: target does not exist: {relative}")

    for index, node in enumerate(nodes):
        if not isinstance(node, dict):
            continue
        dependencies = node.get("depends_on", [])
        if not isinstance(dependencies, list) or not all(isinstance(item, str) for item in dependencies):
            errors.append(f"navigation/{name} node {index}: depends_on must be a list of ids")
            continue
        for dependency in dependencies:
            if dependency not in ids:
                errors.append(f"navigation/{name} node {index}: unknown dependency {dependency}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    errors: list[str] = []
    for name in MAP_NAMES:
        data, load_errors = load_map(root, name)
        errors.extend(load_errors)
        if not load_errors:
            errors.extend(validate_map(root, name, data))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Navigation maps valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
