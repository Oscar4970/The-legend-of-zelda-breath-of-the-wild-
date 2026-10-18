#!/usr/bin/env python3
"""Offline patch-level tests. Does not assert a real Eden Android build or FPS."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATCH_SCRIPT = ROOT / "eden_a57" / "apply_to_eden.py"
VULKAN = "src/video_core/vulkan_common/vulkan_device.cpp"
APP = "src/android/app/src/main/java/org/yuzu/yuzu_emu/YuzuApplication.kt"
DEST = "src/android/app/src/main/java/org/yuzu/yuzu_emu/utils/GalaxyA57Preset.kt"
PIPELINE = "src/video_core/renderer_vulkan/vk_pipeline_cache.cpp"


def make_stub(root: Path, *, intact=True) -> None:
    app = root / APP
    app.parent.mkdir(parents=True, exist_ok=True)
    app.write_text(
        "import org.yuzu.yuzu_emu.utils.GpuDriverHelper\n"
        "class YuzuApplication {\n"
        "        GpuDriverHelper.initializeDriverParameters()\n"
        "}\n" if intact else "package unexpected\n", encoding="utf-8")
    vk = root / VULKAN
    vk.parent.mkdir(parents=True, exist_ok=True)
    vk.write_text(
        "#include <optional>\n"
        "namespace Vulkan {\n"
        "    CollectPhysicalMemoryInfo();\n"
        "}\n", encoding="utf-8")
    p = root / PIPELINE
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(
        "#include <span>\n"
        "      workers(device.HasBrokenParallelShaderCompiling() ? 1ULL : GetTotalPipelineWorkers(),\n",
        encoding="utf-8")


def run_patch(root: Path):
    return subprocess.run([sys.executable, str(PATCH_SCRIPT), str(root)],
                          text=True, capture_output=True)


class GalaxyA57PatchTests(unittest.TestCase):
    def test_apply_changes_only_owned_eden_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_stub(root)
            result = run_patch(root)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            app = (root / APP).read_text()
            vk = (root / VULKAN).read_text()
            profile = (root / DEST).read_text()
            self.assertIn("GalaxyA57Preset.applyIfNeeded(this)", app)
            self.assertIn("GalaxyA57Preset", profile)
            self.assertIn('deviceName}.find("Xclipse 550")', vk)
            self.assertIn("#include <string_view>", vk)
            pipe = (root / PIPELINE).read_text()
            self.assertIn('device.GetModelName().find("Xclipse 550")', pipe)
            self.assertIn("#include <string_view>", pipe)
            self.assertIn("Restore", (ROOT / "eden_a57" / "README.md").read_text() + "Restore")

    def test_is_idempotent(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_stub(root)
            self.assertEqual(run_patch(root).returncode, 0)
            initial = [(root / p).read_bytes() for p in [APP, VULKAN, PIPELINE, DEST]]
            second = run_patch(root)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(initial, [(root / p).read_bytes() for p in [APP, VULKAN, PIPELINE, DEST]])

    def test_fail_closed_on_unrecognized_eden_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_stub(root, intact=False)
            before = (root / APP).read_bytes()
            result = run_patch(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(before, (root / APP).read_bytes())
            self.assertFalse((root / DEST).exists())

    def test_reject_profile_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_stub(root)
            p = root / DEST
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("somebody else's profile")
            result = run_patch(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(p.read_text(), "somebody else's profile")

    def test_profile_is_samsung_a57_only_and_saves(self):
        p = (ROOT / "eden_a57" / "GalaxyA57Preset.kt").read_text()
        for phrase in ["SM-A576", 'equals("samsung"', "if (!isTargetDevice()) return",
                       "if (prefs.getBoolean(APPLIED_KEY, false)) {", "NativeConfig.saveGlobalConfig()",
                       "restorePrevious", "RENDERER_ASYNCHRONOUS_SHADERS",
                       "RENDERER_USE_DISK_SHADER_CACHE", "CPU_BACKEND.setInt(1)",
                       "ANDROID_PIPELINE_WORKERS.setInt(2)", "MEMORY_LAYOUT.setInt(0)",
                       "MEMORY_V2_KEY", "savePreV2Settings",
                       "PERFORMANCE_V4_KEY", "applySafeStartupV4",
                       "FAST_CPU_TIME.setInt(0)", "FAST_GPU_TIME.setInt(0)",
                       "RENDERER_FORCE_MAX_CLOCK.setBoolean(false)",
                       "RENDERER_ACCURACY.setInt(0)", "RENDERER_FRAME_GEN.setBoolean(false)",
                       "minOf(IntSetting.RENDERER_RESOLUTION.getInt(), 1)"]:
            self.assertIn(phrase, p)
        for danger in ["sudo", "setprop", "su -c", "GPU_DRIVER_OVERRIDE", "TurnipDriver"]:
            self.assertNotIn(danger, p)

if __name__ == "__main__":
    unittest.main(verbosity=2)
