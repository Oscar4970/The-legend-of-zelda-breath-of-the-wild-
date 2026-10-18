"""Offline validation of game-specific private build tool, no Nintendo data."""
from pathlib import Path
import importlib.util
import tempfile
import unittest
script=Path(__file__).resolve().parents[1]/"port_botw"/"build_port.py"
spec=importlib.util.spec_from_file_location("botw_private_builder",script)
port=importlib.util.module_from_spec(spec)
spec.loader.exec_module(port)
class PortBuilderTests(unittest.TestCase):
    def test_plain_nso_inputs(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name in ("main","sdk"):
                (root/name).write_bytes(b"NSO0"+bytes(252))
            self.assertEqual(port.validate_inputs(root),(root/"main",root/"sdk"))
            self.assertEqual(port.main(["check","--exefs",str(root)]),0)
    def test_reject_nsp(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/"main").write_bytes(b"PFS0"+bytes(252))
            (root/"sdk").write_bytes(b"NSO0"+bytes(252))
            with self.assertRaises(port.PortBuildError):port.validate_inputs(root)
    def test_missing_sdk(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/"main").write_bytes(b"NSO0"+bytes(252))
            with self.assertRaises(port.PortBuildError):port.validate_inputs(root)
    def test_private_output(self):
        self.assertEqual(port.PRIVATE,port.ROOT/"private"/"botw")
        self.assertIn("private/",(port.ROOT/".gitignore").read_text())
if __name__=="__main__":
    unittest.main(verbosity=2)
