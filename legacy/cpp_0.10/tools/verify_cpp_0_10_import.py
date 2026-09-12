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


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate tracked C++ 0.10 import provenance.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("provenance/incoming/cpp_0.10.0_community/supplied-files.json"),
    )
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = args.manifest if args.manifest.is_absolute() else root / args.manifest
    errors = verify_manifest(root, manifest)
    if errors:
        for error in errors:
            print(error)
        return 1
    print("C++ 0.10 import provenance valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
