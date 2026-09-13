"""Validate the tracked RECOVERY 0.13 source mapping and delivery evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_manifest(root: Path, manifest_path: Path) -> list[str]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    for entry in manifest["entries"]:
        target = Path(entry["target"])
        if target.is_absolute() or ".." in target.parts:
            errors.append(f"unsafe target: {entry['target']}")
            continue
        path = root / target
        if not path.is_file():
            errors.append(f"missing: {entry['target']}")
        elif sha256(path) != entry["sha256"].lower():
            errors.append(f"hash mismatch: {entry['target']}")
    return errors


def verify_evidence(root: Path, delivery_path: Path) -> list[str]:
    delivery = json.loads(delivery_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    for entry in delivery.get("evidence_files", []):
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts:
            errors.append(f"unsafe evidence: {entry['path']}")
            continue
        path = root / "provenance/incoming/recovery_0.13/evidence" / relative
        if not path.is_file():
            errors.append(f"missing evidence: {entry['path']}")
        elif sha256(path) != entry["sha256"].lower():
            errors.append(f"evidence hash mismatch: {entry['path']}")
    return errors


def validate_import(root: Path) -> list[str]:
    provenance = root / "provenance/incoming/recovery_0.13"
    return verify_manifest(root, provenance / "mapped-source-files.json") + verify_evidence(
        root, provenance / "DELIVERY.json"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_import(args.root.resolve())
    if errors:
        for error in errors:
            print(error)
        return 1
    print("RECOVERY 0.13 import provenance valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
