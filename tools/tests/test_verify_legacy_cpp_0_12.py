from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

from tools.verify_legacy_cpp_0_12 import verify_cmake_sources


class LegacyCpp012ValidationTests(unittest.TestCase):
    def test_accepts_a_cmake_source_that_exists_below_legacy_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "engine/src/life/self_model.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// legacy\n", encoding="utf-8")
            cmake = root / "CMakeLists.txt"
            cmake.write_text(
                "add_library(life_core engine/src/life/self_model.cpp)\n",
                encoding="utf-8",
            )

            self.assertEqual(verify_cmake_sources(root, cmake), [])

    def test_rejects_a_missing_cmake_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cmake = root / "CMakeLists.txt"
            cmake.write_text(
                "add_library(life_core engine/src/life/phone_world.cpp)\n",
                encoding="utf-8",
            )

            self.assertEqual(
                verify_cmake_sources(root, cmake),
                ["missing CMake source: engine/src/life/phone_world.cpp"],
            )


if __name__ == "__main__":
    unittest.main()
