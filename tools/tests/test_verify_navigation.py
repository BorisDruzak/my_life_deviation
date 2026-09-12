from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "tools" / "verify_navigation.py"


class Self01NavigationTests(unittest.TestCase):
    def run_validator(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(VALIDATOR), "--root", str(root)],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_navigation_validator_accepts_the_repository_maps(self) -> None:
        result = self.run_validator(ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Navigation maps valid", result.stdout)

    def test_navigation_validator_rejects_a_missing_target(self) -> None:
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

    def test_behavior_verification_scripts_still_run(self) -> None:
        scripts = sorted((ROOT / "docs" / "verification" / "behavior_0.3").glob("*.py"))
        self.assertEqual(len(scripts), 4)
        for script in scripts:
            with self.subTest(script=script.name):
                result = subprocess.run(
                    [sys.executable, str(script)],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_current_maps_identify_self01_entrypoints(self) -> None:
        code_map = json.loads((ROOT / "navigation" / "code-map.json").read_text(encoding="utf-8"))
        doc_map = json.loads((ROOT / "navigation" / "doc-map.json").read_text(encoding="utf-8"))
        code_paths = {node["id"]: node["path"] for node in code_map["nodes"]}
        doc_paths = {node["id"]: node["path"] for node in doc_map["nodes"]}

        self.assertEqual(code_paths["life_self_model"], "engine/src/life/self_model.cpp")
        self.assertEqual(code_paths["self_behavior_scenario"], "tools/self_behavior_scenario.cpp")
        self.assertEqual(code_paths["life_sim"], "apps/simulate/life_main.cpp")
        self.assertEqual(doc_paths["self_model_rules"], "docs/rules/self_model_0.1/SELF-MODEL-0.1.md")
        self.assertEqual(doc_paths["self_model_profile"], "game/profiles/self_model_0.1/profile.json")


if __name__ == "__main__":
    unittest.main()
