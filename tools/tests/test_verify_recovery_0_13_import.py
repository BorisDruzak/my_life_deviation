from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.verify_recovery_0_13_import import verify_evidence, verify_manifest


class RecoveryImportManifestTests(unittest.TestCase):
    def test_accepts_delivery_paths_relative_to_evidence_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = root / "provenance/incoming/recovery_0.13/evidence/baseline/0.log"
            evidence.parent.mkdir(parents=True)
            evidence.write_text("passed\n", encoding="utf-8")
            delivery = root / "DELIVERY.json"
            delivery.write_text(
                json.dumps(
                    {
                        "evidence_files": [
                            {
                                "path": "baseline/0.log",
                                "sha256": hashlib.sha256(evidence.read_bytes()).hexdigest(),
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(verify_evidence(root, delivery), [])

    def test_accepts_a_mapped_file_with_its_recorded_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "engine" / "include" / "life" / "resources.hpp"
            target.parent.mkdir(parents=True)
            target.write_text("resources\n", encoding="utf-8")
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "entries": [
                            {
                                "target": "engine/include/life/resources.hpp",
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
                                "target": "engine/src/life/phone_world.cpp",
                                "sha256": "0" * 64,
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(
                verify_manifest(root, manifest),
                ["missing: engine/src/life/phone_world.cpp"],
            )


if __name__ == "__main__":
    unittest.main()
