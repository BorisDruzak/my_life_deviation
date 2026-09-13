from __future__ import annotations

from pathlib import Path
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class MsvcPortabilityTests(unittest.TestCase):
    def test_mind_declares_the_iterator_header_for_back_inserter(self) -> None:
        source = (REPOSITORY_ROOT / "engine/src/life/mind.cpp").read_text(
            encoding="utf-8"
        )

        self.assertIn("std::back_inserter", source)
        self.assertIn("#include <iterator>", source)


if __name__ == "__main__":
    unittest.main()
