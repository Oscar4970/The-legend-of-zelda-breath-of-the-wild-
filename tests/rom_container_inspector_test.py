#!/usr/bin/env python3
"""No copyrighted material used: all container metadata is synthetic."""
import struct
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from inspeccionar_rom_switch import inspect_file, ContainerError


def pfs0(items: list[tuple[str, bytes]]) -> bytes:
    table=bytearray()
    names=bytearray()
    content=bytearray()
    for name, payload in items:
        encoded = name.encode("utf-8")
        table += struct.pack("<QQII", len(content), len(payload), len(names), 0)
        names += encoded + b"\0"
        content += payload
    return b"PFS0" + struct.pack("<III", len(items), len(names), 0) + table + names + content


class InspectionTests(unittest.TestCase):
    def read(self, content: bytes):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "fixture.bin"
            path.write_bytes(content)
            return inspect_file(path)

    def test_nsp_valid(self):
        data=pfs0([("sample.nca",b"own-data"),("metadata.cnmt",b"123")])
        info=self.read(data)
        self.assertEqual(info["format"],"NSP/PFS0")
        self.assertEqual(info["entries"],2)
        self.assertEqual(info["types"][".nca"],1)
        self.assertFalse(info["game_detected"])
        self.assertFalse(info["bootable"])

    def test_xci_valid(self):
        data=bytearray(0x200)
        data[0x100:0x104]=b"HEAD"
        info=self.read(data)
        self.assertEqual(info["format"],"XCI")
        self.assertFalse(info["bootable"])

    def test_wrong_magic(self):
        with self.assertRaises(ContainerError):
            self.read(b"BAD!" + b"\0"*0x200)

    def test_tiny(self):
        with self.assertRaises(ContainerError):
            self.read(b"PFS0")

    def test_header_cut(self):
        with self.assertRaises(ContainerError):
            self.read(b"PFS0"+struct.pack("<III",2,20,0))

    def test_oversized_entries(self):
        with self.assertRaises(ContainerError):
            self.read(b"PFS0"+struct.pack("<III",20001,0,0))

    def test_malformed_entry_size(self):
        data=bytearray(pfs0([("owned.nca",b"ABC")]))
        struct.pack_into("<Q",data,16+8,10000)
        with self.assertRaises(ContainerError):
            self.read(data)

    def test_out_of_bounds_string_offset(self):
        data=bytearray(pfs0([("owned.nca",b"ABC")]))
        struct.pack_into("<I",data,16+16,9999)
        with self.assertRaises(ContainerError):
            self.read(data)

    def test_reject_path_traversal(self):
        with self.assertRaises(ContainerError):
            self.read(pfs0([("../file.nca",b"ABC")]))

    def test_no_file_payload_leaks_in_report(self):
        info=self.read(pfs0([("owned.nca",b"PRIVATEBYTES")]))
        self.assertNotIn("PRIVATEBYTES",str(info))
        self.assertEqual(info["file_list"][0]["bytes"],12)


if __name__ == "__main__":
    unittest.main(verbosity=2)
