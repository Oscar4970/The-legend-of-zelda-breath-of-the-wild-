#!/usr/bin/env python3
"""Build a private BOTW Switch native Android port attempt with the MIT engine.

Game data is NOT included. Input must be a legally obtained, privately
extracted and decrypted ExeFS containing NSO0 modules main and sdk.
This AOT builder does not guarantee compatible Horizon/NVN/Maxwell shaders.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "private" / "botw"
PIN = "7c2531d9bdce94506bad9bf53d8a9848b240f111"
UPSTREAM = "https://github.com/BelmanteGu/SuperMarioWonderRecomp.git"

class PortBuildError(Exception):
    pass

def checked_module(path: Path) -> None:
    try:
        with path.open("rb") as f:
            header = f.read(0x100)
    except OSError as exc:
        raise PortBuildError("Missing/illegible module: " + str(path)) from exc
    if len(header) < 0x100 or header[:4] != b"NSO0":
        raise PortBuildError("Require decrypted extracted NSO0, not NSP/XCI: " + str(path))

def validate_inputs(exefs: Path, romfs: Path | None = None) -> tuple[Path,Path]:
    if not exefs.is_dir():
        raise PortBuildError("No such ExeFS directory: " + str(exefs))
    main,sdk=exefs/"main",exefs/"sdk"
    checked_module(main)
    checked_module(sdk)
    if romfs is not None and not romfs.is_dir():
        raise PortBuildError("No such RomFS directory: " + str(romfs))
    return main,sdk

def run(*args: object, cwd: Path | None = None) -> None:
    command = [str(x) for x in args]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)

def pinned_engine(engine: Path) -> None:
    if not (engine/".git").exists():
        if engine.exists():
            raise PortBuildError("An unrelated file occupies the engine path")
        engine.parent.mkdir(parents=True,exist_ok=True)
        run("git","init","-q",engine)
        run("git","-C",engine,"remote","add","origin",UPSTREAM)
    try:
        current=subprocess.check_output(["git","-C",str(engine),"rev-parse","HEAD"],
                        text=True,stderr=subprocess.DEVNULL).strip()
    except subprocess.CalledProcessError:
        current=""
    if current!=PIN:
        run("git","-C",engine,"fetch","--depth","1","origin",PIN)
        run("git","-C",engine,"-c","advice.detachedHead=false",
            "checkout","--detach","FETCH_HEAD")
    actual=subprocess.check_output(["git","-C",str(engine),"rev-parse","HEAD"],text=True).strip()
    if actual!=PIN:
        raise PortBuildError("Refusing a mismatched engine commit")

def configure_game(engine: Path, romfs: Path | None) -> Path:
    game=engine/"games"/"botw"
    game.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/"port_botw"/"game.cpp",game/"game.cpp")
    private=engine/"private"/"botw"
    for item in ("gen","analysis","extract"):
        (private/item).mkdir(parents=True,exist_ok=True)
    if romfs:
        target=private/"extract"/"romfs"
        if target.is_symlink():
            target.unlink()
        elif target.exists():
            raise PortBuildError("Do not overwrite an existing RomFS directory: "+str(target))
        if os.name=="nt":
            run("cmd","/c","mklink","/J",target,romfs.resolve())
        else:
            target.symlink_to(romfs.resolve(),target_is_directory=True)
    return private

def aot_game(engine: Path, private: Path, main: Path, sdk: Path) -> None:
    for name,source,base in (
            ("main",main,"0x4000000000"),
            ("sdk",sdk,"0x4010000000")):
        output=private/"gen"/name
        print("Full ARM64 -> C++ of actual local NSO",name,flush=True)
        run(sys.executable,"-m","recomp",source,output,
            "--name",name,"--base",base,cwd=engine)
        report=output/"report.txt"
        if not report.is_file():
            raise PortBuildError("No AOT coverage report for "+name)
        print(report.read_text(encoding="utf-8",errors="replace")[:500])
        run(sys.executable,engine/"tools"/"nso2bin.py",
            source,private/"analysis"/(name+".bin"))
    print("Code generated. This does NOT demonstrate BOTW boots.")

def build_native(engine: Path, private: Path, sdk: Path, jobs: int) -> None:
    ndk=sdk/"ndk"/"28.2.13676358"
    toolchain=ndk/"build"/"cmake"/"android.toolchain.cmake"
    if not toolchain.is_file():
        raise PortBuildError("Install Android NDK 28.2.13676358 first: "+str(ndk))
    build=private/"android-build"
    run("cmake","-S",engine,"-B",build,"-G","Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE="+str(toolchain),
        "-DANDROID_ABI=arm64-v8a","-DANDROID_PLATFORM=android-29",
        "-DANDROID_STL=c++_static","-DNX_GAME=botw",
        "-DNX_GEN_DIR="+str(private/"gen"))
    run("cmake","--build",build,"--target","botw","--parallel",jobs)
    lib=build/"libbotw.so"
    if not lib.is_file() or lib.stat().st_size<4096:
        raise PortBuildError("Native libbotw.so not generated")
    private_dest=ROOT/"app"/"src"/"main"/"jniLibs"/"arm64-v8a"
    private_dest.mkdir(parents=True,exist_ok=True)
    shutil.copy2(lib,private_dest/"libbotw.so")
    print("Staged PRIVATE native library:",private_dest/"libbotw.so")
    print("IMPORTANT: BOTW NVN shader translation and runtime integration remain unverified.")
    print("Run Gradle locally only; never upload an APK with Nintendo code to public CI.")

def main(argv: list[str] | None = None) -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage",choices=("check","prepare","android"))
    p.add_argument("--exefs",type=Path,required=True)
    p.add_argument("--romfs",type=Path)
    p.add_argument("--android-sdk",type=Path)
    p.add_argument("--jobs",type=int,default=1)
    args=p.parse_args(argv)
    if not 1<=args.jobs<=32:
        p.error("--jobs must be in 1..32")
    try:
        main_nso,sdk_nso=validate_inputs(
            args.exefs.resolve(),args.romfs.resolve() if args.romfs else None)
        print("Two local NSO0 headers validated; game identity NOT yet proven.")
        if args.stage=="check":
            return 0
        try:
            import capstone  # noqa: F401
            import lz4.block  # noqa: F401
        except ImportError as exc:
            raise PortBuildError(
                "Install with python -m pip install capstone==5.0.6 lz4==4.4.4") from exc
        engine=PRIVATE/"engine"
        pinned_engine(engine)
        private=configure_game(engine,args.romfs)
        aot_game(engine,private,main_nso,sdk_nso)
        if args.stage=="android":
            sdk=args.android_sdk or os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
            if not sdk:
                raise PortBuildError("Set ANDROID_HOME or --android-sdk")
            build_native(engine,private,Path(sdk).resolve(),args.jobs)
        return 0
    except (OSError,PortBuildError,subprocess.CalledProcessError) as exc:
        print("BOTW PORT BLOCKED:",exc,file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
