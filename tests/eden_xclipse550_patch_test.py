#!/usr/bin/env python3
"""Pure Python checks: no game, no emulator executable, no phone modification."""
import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "emulador_xclipse550" / "apply_xclipse550_to_eden.py"
spec = importlib.util.spec_from_file_location("xprofile", MODULE_PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class XclipseProfilePatchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.src = self.root / module.APP
        (self.src / "utils").mkdir(parents=True)
        (self.src / "features/settings/model").mkdir(parents=True)
        self.app = self.src / "YuzuApplication.kt"
        self.original = """package org.yuzu.yuzu_emu
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
class YuzuApplication {
    fun onCreate() {
        DirectoryInitialization.start()
    }
}
"""
        self.app.write_text(self.original)
        (self.src / "features/settings/model/IntSetting.kt").write_text(
            "CPU_BACKEND( RENDERER_RESOLUTION( RENDERER_VRAM_USAGE_MODE( RENDERER_ASTC_DECODE_METHOD("
        )
        (self.src / "features/settings/model/BooleanSetting.kt").write_text(
            "FASTMEM( USE_DOCKED_MODE( RENDERER_FORCE_MAX_CLOCK( RENDERER_FRAME_GEN( "
            "RENDERER_USE_DISK_SHADER_CACHE( RENDERER_ASYNCHRONOUS_GPU_EMULATION( "
            "RENDERER_ASYNCHRONOUS_SHADERS("
        )

    def test_patches_application_once(self):
        self.assertEqual(module.apply(self.root, require_pin=False), "applied")
        modified = self.app.read_text()
        self.assertIn("val a57FirstRun", modified)
        self.assertIn("Xclipse550Profile.applyToFirstInstall(a57FirstRun)", modified)
        self.assertEqual(modified.count("DirectoryInitialization.start()"), 1)
        self.assertTrue((self.src / "utils/Xclipse550Profile.kt").is_file())
        self.assertEqual(module.apply(self.root, require_pin=False), "already applied")
        self.assertEqual(self.app.read_text(), modified)

    def test_refuses_unknown_application_layout_without_modification(self):
        self.app.write_text("class YuzuApplication {}")
        with self.assertRaises(ValueError):
            module.apply(self.root, require_pin=False)
        self.assertEqual(self.app.read_text(), "class YuzuApplication {}")

    def test_refuses_unknown_setting_without_modification(self):
        (self.src / "features/settings/model/IntSetting.kt").write_text("CPU_BACKEND(")
        with self.assertRaises(ValueError):
            module.apply(self.root, require_pin=False)
        self.assertEqual(self.app.read_text(), self.original)

    def test_profile_is_model_specific_and_reversible(self):
        kotlin = (MODULE_PATH.parent / "Xclipse550Profile.kt").read_text()
        self.assertIn('Build.MODEL.equals("SM-A576B"', kotlin)
        self.assertIn('Build.MANUFACTURER.equals("samsung"', kotlin)
        self.assertIn("if (!isFreshInstall || !isTargetDevice()) return", kotlin)
        self.assertIn("NativeConfig.saveGlobalConfig()", kotlin)
        self.assertNotIn("installCustomDriver", kotlin)
        self.assertNotIn("installDefaultDriver", kotlin)
        self.assertNotIn("forceMaxClock(true)", kotlin)

    def test_pinned_commit_cannot_be_bypassed_by_default(self):
        with patch.object(module.subprocess, "check_output", return_value="other revision\n"):
            with self.assertRaises(RuntimeError):
                module.apply(self.root)


if __name__ == "__main__":
    unittest.main()
