#!/usr/bin/env python3
"""Build-time integration with pinned MIT-licensed SwitchGamesRecomp AArch64->C++.

Only generates/executes OUR OWN tiny AArch64 NSO (MOV W0,#42; RET).
The output includes no code or assets from any commercial Switch game.
"""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parent.parent
UPSTREAM="https://github.com/BelmanteGu/SuperMarioWonderRecomp.git"
PINNED_SHA="7c2531d9bdce94506bad9bf53d8a9848b240f111"


def call(*args,**kw):
    print("+", " ".join(str(p) for p in args),flush=True)
    subprocess.run([str(p) for p in args],check=True,**kw)


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--skip-fetch",action="store_true")
    args=p.parse_args()
    vendor=ROOT/"recomp_aot"/"_build_upstream"
    if not args.skip_fetch or not (vendor/".git").exists():
        if vendor.exists():shutil.rmtree(vendor)
        call("git","init","-q",str(vendor))
        call("git","-C",str(vendor),"remote","add","origin",UPSTREAM)
        call("git","-C",str(vendor),"fetch","--depth","1","origin",PINNED_SHA)
        call("git","-C",str(vendor),"-c","advice.detachedHead=false",
             "checkout","--detach","FETCH_HEAD")
    commit=subprocess.check_output(["git","-C",str(vendor),"rev-parse","HEAD"],text=True).strip()
    if commit!=PINNED_SHA:
        raise RuntimeError("Upstream source commit did not match our reviewed pin")
    fixture=ROOT/"recomp_aot"/"_build_fixture"/"own_code.nso"
    call(sys.executable,ROOT/"recomp_aot"/"generate_sample_nso.py",fixture)
    output=ROOT/"app"/"src"/"main"/"cpp"/"aot_generated"
    (output/"output").mkdir(parents=True,exist_ok=True)
    (output/"include"/"nx").mkdir(parents=True,exist_ok=True)
    # Uses the exact upstream recompiler's AArch64 disassembler and C++ emitter.
    call(sys.executable,"-m","recomp",fixture,output/"output",
         "--name","test","--only","0x40",cwd=vendor)
    code=(output/"output"/"code_000.cpp").read_text(encoding="utf-8")
    match=re.search(r"void test_00000040\s*\(",code)
    if not match or "nx::unimpl(" in code:
        raise RuntimeError("Upstream did not recompile the fixture entry correctly")
    (output/"include"/"nx"/"cpu.h").write_bytes(
        (vendor/"runtime"/"include"/"nx"/"cpu.h").read_bytes())
    # Retain full upstream license in the APK and repository attribution.
    notice="SwitchGamesRecomp reference CPU code and AArch64 recompiler:\n"
    notice+=f"{UPSTREAM}\nPinned commit: {PINNED_SHA}\n\n"
    notice+=(vendor/"LICENSE").read_text(encoding="utf-8")
    asset=ROOT/"app"/"src"/"main"/"assets"
    asset.mkdir(exist_ok=True,parents=True)
    (asset/"THIRD_PARTY_NOTICES.txt").write_text(notice,encoding="utf-8")
    print("PASS: upstream AArch64 instructions statically recompiled to generated C++")
    print("Generated C++:",output/"output"/"code_000.cpp")


if __name__=="__main__":
    main()
