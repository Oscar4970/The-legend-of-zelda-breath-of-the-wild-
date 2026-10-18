#!/usr/bin/env python3
"""Apply the GPLv3 Galaxy A57 Eden in-process compatibility shim offline.

Usage: python3 eden_a57/apply_to_eden.py /path/to/eden
Refuse mismatched sources instead of guessing what to patch. Never touches
Android system drivers or loads external GPU blobs.
"""
from pathlib import Path
import argparse
import shutil
import sys


REPO = Path(__file__).resolve().parent
EDEN_KT = "src/android/app/src/main/java/org/yuzu/yuzu_emu"
APP = EDEN_KT + "/YuzuApplication.kt"
DEST = EDEN_KT + "/utils/GalaxyA57Preset.kt"
VULKAN = "src/video_core/vulkan_common/vulkan_device.cpp"

IMPORT_MARKER = "import org.yuzu.yuzu_emu.utils.GpuDriverHelper\n"
IMPORT_NEW = "import org.yuzu.yuzu_emu.utils.GalaxyA57Preset\n"
START_MARKER = "        GpuDriverHelper.initializeDriverParameters()\n"
START_NEW = "        GalaxyA57Preset.applyIfNeeded(this)\n"

# Report actual Vulkan driver capabilities on the selected Xclipse 550. Eden
# already implements format fallbacks; diagnostics help find *real* driver bugs
# before adding risky workarounds. Reporting alone does not improve FPS.
NATIVE_ANCHOR = "    CollectPhysicalMemoryInfo();\n"
NATIVE_SNIPPET = r'''    // Galaxy A57: instrumentation of the real Samsung Vulkan ICD.
    // No feature spoofing or kernel/driver changes, and other GPUs unaffected.
    if (std::string_view{properties.properties.deviceName}.find("Xclipse 550") !=
        std::string_view::npos) {
        LOG_INFO(Render_Vulkan,
                 "Galaxy A57/Xclipse 550: vk={}.{} driverID={} ASTC={} D24S8={} "
                 "D32S8-blit={}; using advertised Vulkan capabilities",
                 VK_API_VERSION_MAJOR(instance_version), VK_API_VERSION_MINOR(instance_version),
                 static_cast<int>(properties.driver.driverID),
                 static_cast<bool>(features.features.textureCompressionASTC_LDR),
                 supports_d24_depth, is_blit_depth32_stencil8_supported);
    }
'''


def patch_file(path: Path, anchor: str, addition: str, *, before: bool = False):
    original = path.read_text(encoding="utf-8")
    if addition in original:
        print("Already applied:", path)
        return
    if original.count(anchor) != 1:
        raise ValueError(f"Expected a single Eden anchor in {path}: {anchor!r}")
    new = original.replace(anchor, (addition + anchor) if before else (anchor + addition))
    path.write_text(new, encoding="utf-8")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("eden_root", type=Path, help="Local git checkout of Eden")
    a = p.parse_args()
    root = a.eden_root.resolve()
    app = root / APP
    vulkan = root / VULKAN
    dest = root / DEST
    for path in (app, vulkan):
        if not path.is_file():
            raise SystemExit(f"Not an expected Eden Android checkout: missing {path}")
    # Make compatibility errors fail BEFORE modifying source files.
    app_data = app.read_text(encoding="utf-8")
    vk_data = vulkan.read_text(encoding="utf-8")
    for source, needle in ((app_data, IMPORT_MARKER), (app_data, START_MARKER),
                           (vk_data, NATIVE_ANCHOR)):
        if source.count(needle) != 1:
            raise SystemExit("Upstream Eden changed: refusing unsafe automatic patch")
    if dest.exists() and dest.read_text(encoding="utf-8") != (REPO / "GalaxyA57Preset.kt").read_text(encoding="utf-8"):
        raise SystemExit("Device profile differs in target tree; not overwriting.")
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(REPO / "GalaxyA57Preset.kt", dest)
    patch_file(app, IMPORT_MARKER, IMPORT_NEW)
    patch_file(app, START_MARKER, START_NEW)
    patch_file(vulkan, NATIVE_ANCHOR, NATIVE_SNIPPET, before=True)
    if "#include <string_view>" not in vulkan.read_text(encoding="utf-8"):
        patch_file(vulkan, "#include <optional>\n", "#include <string_view>\n")
    print("Source patch staged for Eden. NO APK BUILT. Review 'git diff', build with")
    print("Eden's Android Gradle/NDK pipeline, then validate on the Galaxy A57.")
    print("Rollback: git restore --", APP, VULKAN)
    print("and remove", DEST)


if __name__ == "__main__":
    main()
