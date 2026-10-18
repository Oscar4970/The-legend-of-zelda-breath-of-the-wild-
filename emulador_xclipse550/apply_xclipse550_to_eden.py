#!/usr/bin/env python3
"""Apply an opt-in build-time Samsung A57 profile to the Eden Android source tree.

Run against independently obtained, GPL-compliant Eden source. This script
does not bundle emulator binaries, ROM files or firmware.
"""
import argparse
from pathlib import Path
import subprocess
import sys

# Pinned Eden mirror commit used to check available Enum names and call order.
EDEN_COMMIT = "de7e6e01d43e8cf2ec193d74fa053b0d7949955f"
HERE = Path(__file__).resolve().parent
APP = Path("src/android/app/src/main/java/org/yuzu/yuzu_emu")
IMPORTED = "import org.yuzu.yuzu_emu.utils.Xclipse550Profile\n"
ANCHOR_IMPORT = "import org.yuzu.yuzu_emu.utils.GpuDriverHelper\n"
ANCHOR_INIT = "        DirectoryInitialization.start()\n"
PATCHED_INIT = """        // Do not overwrite settings on an existing installation.
        val a57FirstRun = Xclipse550Profile.isTargetDevice() &&
            (getExternalFilesDir(null)?.resolve("config/config.ini")?.exists() == false)
        DirectoryInitialization.start()
        Xclipse550Profile.applyToFirstInstall(a57FirstRun)
"""


def apply(root: Path, *, require_pin: bool = True) -> str:
    root = root.resolve()
    if require_pin:
        rev = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
        if rev != EDEN_COMMIT:
            raise RuntimeError(f"Eden source SHA differs from audited pin: {rev}")
    app_file = root / APP / "YuzuApplication.kt"
    new_file = root / APP / "utils" / "Xclipse550Profile.kt"
    if not app_file.is_file():
        raise FileNotFoundError(f"Cannot locate Eden Android YuzuApplication.kt: {app_file}")
    text = app_file.read_text(encoding="utf-8")
    if (IMPORTED in text) and ("Xclipse550Profile.applyToFirstInstall" in text):
        if new_file.read_text(encoding="utf-8") != (HERE / "Xclipse550Profile.kt").read_text(encoding="utf-8"):
            raise ValueError("Existing Xclipse profile differs; manual review needed")
        return "already applied"

    if text.count(ANCHOR_IMPORT) != 1 or text.count(ANCHOR_INIT) != 1:
        raise ValueError("Eden source layout changed: abort instead of patching the wrong location")
    # Ensure settings names are present in the same upstream source.
    model = root / APP / "features/settings/model"
    for filename, keys in (
        ("IntSetting.kt", ("CPU_BACKEND(", "RENDERER_RESOLUTION(", "RENDERER_VRAM_USAGE_MODE(", "RENDERER_ASTC_DECODE_METHOD(")),
        ("BooleanSetting.kt", ("FASTMEM(", "USE_DOCKED_MODE(", "RENDERER_FORCE_MAX_CLOCK(", "RENDERER_FRAME_GEN(", "RENDERER_USE_DISK_SHADER_CACHE(", "RENDERER_ASYNCHRONOUS_GPU_EMULATION(", "RENDERER_ASYNCHRONOUS_SHADERS(")),
    ):
        content = (model / filename).read_text(encoding="utf-8")
        for key in keys:
            if key not in content:
                raise ValueError(f"Upstream is missing required setting {key}")
    text = text.replace(ANCHOR_IMPORT, ANCHOR_IMPORT + IMPORTED)
    text = text.replace(ANCHOR_INIT, PATCHED_INIT)
    # Write after ALL checks, so failures leave upstream source untouched.
    new_file.parent.mkdir(parents=True, exist_ok=True)
    app_file.write_text(text, encoding="utf-8")
    new_file.write_text((HERE / "Xclipse550Profile.kt").read_text(encoding="utf-8"), encoding="utf-8")
    return "applied"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("eden_checkout", type=Path)
    parser.add_argument("--allow-unpinned", action="store_true",
                        help="For local development only; still validates the expected source anchors")
    args = parser.parse_args()
    try:
        result = apply(args.eden_checkout, require_pin=not args.allow_unpinned)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as err:
        print(f"NOT APPLIED: {err}", file=sys.stderr)
        return 2
    print(f"Eden SM-A576B profile: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
