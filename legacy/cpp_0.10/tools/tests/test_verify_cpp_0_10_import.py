from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.verify_cpp_0_10_import import verify_manifest


class ImportManifestTests(unittest.TestCase):
    def test_accepts_files_matching_recorded_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "provenance" / "sample.txt"
            target.parent.mkdir(parents=True)
            target.write_text("community\n", encoding="utf-8")
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "entries": [
                            {
                                "target": "provenance/sample.txt",
                                "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(verify_manifest(root, manifest), [])

    def test_reports_missing_or_changed_recorded_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps({"entries": [{"target": "missing.txt", "sha256": "0" * 64}]}),
                encoding="utf-8",
            )

            self.assertEqual(verify_manifest(root, manifest), ["missing: missing.txt"])


if __name__ == "__main__":
    unittest.main()
