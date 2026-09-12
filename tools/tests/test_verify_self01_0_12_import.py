from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.verify_self01_0_12_import import verify_manifest


class Self01ImportManifestTests(unittest.TestCase):
    def test_accepts_a_mapped_file_with_its_recorded_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "engine" / "include" / "life" / "self_model.hpp"
            target.parent.mkdir(parents=True)
            target.write_text("self model\n", encoding="utf-8")
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "entries": [
                            {
                                "target": "engine/include/life/self_model.hpp",
                                "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(verify_manifest(root, manifest), [])

    def test_rejects_a_missing_mapped_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "entries": [
                            {
                                "target": "engine/src/life/self_model.cpp",
                                "sha256": "0" * 64,
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(
                verify_manifest(root, manifest),
                ["missing: engine/src/life/self_model.cpp"],
            )


if __name__ == "__main__":
    unittest.main()
