"""Check that the preserved SELF 0.12 CMake entrypoint references files it owns."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


SOURCE_PATTERN = re.compile(r"\b(?:engine/(?:src|tests)/life/[\w.-]+|apps/simulate/[\w.-]+)\b")


def verify_cmake_sources(root: Path, cmake_path: Path) -> list[str]:
    sources = SOURCE_PATTERN.findall(cmake_path.read_text(encoding="utf-8"))
    return [f"missing CMake source: {source}" for source in sources if not (root / source).is_file()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("legacy/cpp_0.12"))
    args = parser.parse_args()
    root = args.root.resolve()
    errors = verify_cmake_sources(root, root / "CMakeLists.txt")
    if errors:
        for error in errors:
            print(error)
        return 1
    print("SELF 0.12 legacy CMake sources valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
