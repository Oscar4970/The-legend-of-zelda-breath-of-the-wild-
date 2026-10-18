"""Pruebas de recopilacion: solo GPL libre + base abierta, NUNCA datos de juego."""
from pathlib import Path
import importlib.util
import json
import tempfile
import unittest
from unittest import mock
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "port_botw" / "collect_eden_reference.py"
spec = importlib.util.spec_from_file_location("eden_reference_kit", SCRIPT)
kit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kit)


class KitTests(unittest.TestCase):
    def test_manifest_hashes_and_no_game_data(self):
        manifest = kit.load_manifest()
        self.assertEqual(manifest["source_commit"], "de7e6e01d43e8cf2ec193d74fa053b0d7949955f")
        joined = " ".join(x["path"] for x in manifest["files"]).lower()
        for forbidden in ["romfs", "exefs", ".nsp", ".xci", ".nso", "prod.keys", "firmware"]:
            self.assertNotIn(forbidden, joined)
        self.assertIn("vulkan_device.cpp", joined)

    def test_git_blob_sha1(self):
        import hashlib
        blob = b"test\\n"
        digest = hashlib.sha1(b"blob 5\\0" + blob).hexdigest()
        self.assertEqual(kit.git_blob_sha1(blob), digest)

    def test_mismatched_commit_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / ".git").mkdir()
            manifest = kit.load_manifest()
            with mock.patch.object(kit.subprocess, "check_output", return_value="000000\\n"):
                with self.assertRaises(kit.KitError):
                    kit.verify_source_checkout(root, manifest)

    def test_kit_contains_only_whitelisted_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / ".git").mkdir()
            (root / "LICENSE.txt").write_text("GPL sample fixture")
            fake = b"// SPDX-License-Identifier: GPL-3.0-or-later\\n"
            src = root / "src" / "test.cpp"
            src.parent.mkdir(parents=True)
            src.write_bytes(fake)
            (root / "game.nsp").write_bytes(b"DO NOT PACKAGE")
            manifest = {
                "schema": 1, "source_commit": "test",
                "files": [{"path": "src/test.cpp", "sha": kit.git_blob_sha1(fake),
                           "role": "test fixture"}],
            }
            out = root / "kit.zip"
            with mock.patch.object(kit, "load_manifest", return_value=manifest):
                with mock.patch.object(kit, "verify_source_checkout"):
                    kit.make_kit(root, out)
            with zipfile.ZipFile(out) as archive:
                names = archive.namelist()
                self.assertIn("eden/src/test.cpp", names)
                self.assertIn("LICENSE_Eden_GPLv3.txt", names)
                self.assertNotIn("game.nsp", names)
                self.assertNotIn("private", " ".join(names))
                self.assertFalse(any(name.endswith(".apk") for name in names))


if __name__ == "__main__":
    unittest.main()
