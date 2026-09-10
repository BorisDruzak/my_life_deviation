from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "tools" / "verify_navigation.py"


class NavigationValidationTests(unittest.TestCase):
    def run_validator(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(VALIDATOR), "--root", str(root)],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_accepts_repository_navigation_maps(self) -> None:
        """Repository maps with real targets must be accepted."""
        result = self.run_validator(ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Navigation maps valid", result.stdout)

    def test_rejects_missing_target(self) -> None:
        """A missing file target must not be accepted as a navigable node."""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            navigation = root / "navigation"
            navigation.mkdir()
            for name in ("code-map.json", "doc-map.json"):
                (navigation / name).write_text(
                    json.dumps({"nodes": [{"id": "missing", "path": "does/not/exist"}]}),
                    encoding="utf-8",
                )
            result = self.run_validator(root)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does/not/exist", result.stderr)


if __name__ == "__main__":
    unittest.main()
