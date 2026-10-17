#!/usr/bin/env python3
"""Make a tiny self-authored, legal Nintendo Switch NSO containing ARM64 code.

Generated function:
  mov w0, #42
  ret
This is NOT BOTW and contains no Nintendo-owned bytes, ROM, assets, or keys.
The purpose is to feed a real upstream AArch64 -> C++ recompiler, not emulate ARM.
"""
from pathlib import Path
import argparse
import struct


def build():
    # .text virtual 0..0x80, tiny complete MOD0 at +0x20, our function at +0x40.
    code=bytearray(0x80)
    struct.pack_into("<I",code,4,0x20)
    code[0x20:0x24]=b"MOD0"
    struct.pack_into("<iiiii",code,0x24,0x100-0x20,0x220-0x20,
                     0x240-0x20,-0x20,-0x20)  # no .eh_frame_hdr
    struct.pack_into("<II",code,0x40,0x52800540,0xD65F03C0)
    # Padding is NOP (unreachable after ret), to keep a well-formed text section.
    for offset in range(0x48,0x80,4):
        struct.pack_into("<I",code,offset,0xD503201F)

    # rodata virtual 0x100..0x1c0. Dynamic: SYMTAB and STRTAB with one symbol.
    ro=bytearray(0xC0)
    dynamic=[(5,0x190),(6,0x160),(11,24),(10,9),(0,0)]
    for n,(tag,value) in enumerate(dynamic):
        struct.pack_into("<qQ",ro,n*16,tag,value)
    # Two ELF64 symbols (undef index 0; function at virtual 0x40 in .text).
    struct.pack_into("<IBBHQQ",ro,0x60+24,1,0x12,0,1,0x40,8)
    ro[0x90:0x99]=b"\0test_fn\0"
    data=bytes(0x20)

    nso=bytearray(0x260)
    nso[:4]=b"NSO0"
    # All three segments are stored as plain, uncompressed bytes.
    struct.pack_into("<III",nso,0x10,0x100,0,0x80)
    struct.pack_into("<III",nso,0x20,0x180,0x100,0xC0)
    struct.pack_into("<III",nso,0x30,0x240,0x200,0x20)
    struct.pack_into("<I",nso,0x3C,0x20)  # uninitialized BSS
    struct.pack_into("<III",nso,0x60,0x80,0xC0,0x20)
    nso[0x100:0x180]=code
    nso[0x180:0x240]=ro
    nso[0x240:0x260]=data
    return bytes(nso)


def main():
    p=argparse.ArgumentParser()
    p.add_argument("output",type=Path)
    a=p.parse_args()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    blob=build()
    a.output.write_bytes(blob)
    assert blob[0x140:0x148]==struct.pack("<II",0x52800540,0xD65F03C0)
    print("Created own AArch64 Switch NSO:",a.output,len(blob),"bytes")


if __name__=="__main__":
    main()
