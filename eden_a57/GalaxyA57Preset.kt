// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 BOTW Android laboratory contributors
//
// Conservative in-process compatibility defaults for Eden on a Galaxy A57.
// This is *not* a Samsung Vulkan kernel/userspace driver replacement.
package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.app.ActivityManager
import android.util.Log
import android.os.Build
import java.util.Locale
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting

/**
 * Isolated opt-in-by-device default profile. Does not modify system settings.
 *
 * Verified enum ordinals against Eden src/common/settings_enums.h
 * @ de7e6e01d43e8cf2ec193d74fa053b0d7949955f:
 *   CpuBackend: Dynarmic=0, Nce=1
 *   RendererBackend: OpenGL_GLSL=0, Vulkan=1
 *   ResolutionSetup: Res1_4X=0, Res1_2X=1, Res3_4X=2
 *   ScalingFilter: NearestNeighbor=0, Bilinear=1
 *   AntiAliasing: None=0
 *   VramUsageMode: Conservative=0
 *
 * The Xclipse 550 is NOT automatically treated as a proprietary Turnip/Adreno
 * GPU. Never claim unsupported Vulkan features or install custom GPU drivers.
 */
object GalaxyA57Preset {
    private const val PREF_NAME = "galaxy_a57_eden_experiment"
    private const val APPLIED_KEY = "profile_v1_applied"
    private const val MEMORY_V2_KEY = "profile_v2_low_memory_applied"
    private const val PERFORMANCE_V4_KEY = "profile_v4_safe_startup_applied"

    /**
     * BOTW field report: game reaches Link at roughly 10 FPS but exits to Eden.
     * Prioritize reduced pixel work and conservative emulation timing over
     * interpolation. Save previous choices; apply once, never per launch.
     *
     * FSR1 is already present in Eden (ScalingFilter.Fsr = 6); users can select
     * it manually after game stability has been measured. FSR1 != FSR3 frame gen.
     */
    private fun applySafeStartupV4(context: Context, prefs: android.content.SharedPreferences) {
        if (prefs.getBoolean(PERFORMANCE_V4_KEY, false)) return
        prefs.edit()
            .putInt("old_v4_cpu_accuracy", IntSetting.CPU_ACCURACY.getInt())
            .putInt("old_v4_gpu_accuracy", IntSetting.RENDERER_ACCURACY.getInt())
            .putInt("old_v4_cpu_clock", IntSetting.FAST_CPU_TIME.getInt())
            .putInt("old_v4_gpu_clock", IntSetting.FAST_GPU_TIME.getInt())
            .putBoolean("old_v4_force_clock", BooleanSetting.RENDERER_FORCE_MAX_CLOCK.getBoolean())
            .apply()

        // Do not raise resolution if the user already chose 0.25x or 0.5x.
        IntSetting.RENDERER_RESOLUTION.setInt(
            minOf(IntSetting.RENDERER_RESOLUTION.getInt(), 1)
        ) // <= 0.5x handheld
        IntSetting.CPU_BACKEND.setInt(1)              // NCE
        IntSetting.CPU_ACCURACY.setInt(0)             // Auto, not unsafe
        IntSetting.RENDERER_ACCURACY.setInt(0)        // Low GPU accuracy; check visual artifacts
        IntSetting.FAST_CPU_TIME.setInt(0)            // Normal emulated CPU clock
        IntSetting.FAST_GPU_TIME.setInt(0)            // Normal emulated GPU clock
        IntSetting.RENDERER_BACKEND.setInt(1)         // Vulkan
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(0) // Conservative
        IntSetting.MEMORY_LAYOUT.setInt(0)            // Switch 4 GiB
        IntSetting.RENDERER_SCALING_FILTER.setInt(1)  // Bilinear: cheaper than FSR1
        IntSetting.RENDERER_ANTI_ALIASING.setInt(0)
        BooleanSetting.USE_DOCKED_MODE.setBoolean(false)
        BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(false)
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN.setBoolean(false)

        NativeConfig.saveGlobalConfig()
        prefs.edit().putBoolean(PERFORMANCE_V4_KEY, true).apply()

        // Read-only Android memory snapshot for later diagnostic comparison.
        val memoryManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        memoryManager?.let {
            val info = ActivityManager.MemoryInfo()
            it.getMemoryInfo(info)
            Log.i("EdenA57Profile", "v4 freeRAMMiB=${info.availMem / 1048576L} " +
                "lowMemory=${info.lowMemory} thresholdMiB=${info.threshold / 1048576L}")
        }
    }

    // Use Eden's existing configuration to limit GPU shader compilation pressure.
    private fun applyLowMemorySettings() {
        IntSetting.ANDROID_PIPELINE_WORKERS.setInt(2)
        IntSetting.MEMORY_LAYOUT.setInt(0) // 4 GiB emulated Switch memory
    }

    private fun savePreV2Settings(prefs: android.content.SharedPreferences) {
        if (!prefs.contains("old_workers")) {
            prefs.edit()
                .putInt("old_workers", IntSetting.ANDROID_PIPELINE_WORKERS.getInt())
                .putInt("old_memory", IntSetting.MEMORY_LAYOUT.getInt())
                .apply()
        }
    }

    fun isTargetDevice(): Boolean {
        val model = Build.MODEL.uppercase(Locale.ROOT)
        return Build.MANUFACTURER.equals("samsung", ignoreCase = true) &&
            model.startsWith("SM-A576")
    }

    fun applyIfNeeded(context: Context) {
        if (!isTargetDevice()) return

        val prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
        // Do not touch a per-game configuration.
        if (NativeConfig.isPerGameConfigLoaded()) return
        if (prefs.getBoolean(PERFORMANCE_V4_KEY, false)) return
        if (prefs.getBoolean(MEMORY_V2_KEY, false)) {
            applySafeStartupV4(context, prefs)
            return
        }

        // Previous v1 installations retain their other customized preferences.
        if (prefs.getBoolean(APPLIED_KEY, false)) {
            savePreV2Settings(prefs)
            applyLowMemorySettings()
            NativeConfig.saveGlobalConfig()
            prefs.edit().putBoolean(MEMORY_V2_KEY, true).apply()
            applySafeStartupV4(context, prefs)
            return
        }
        savePreV2Settings(prefs)

        // Keep original values for targeted rollback without clearing game data.
        prefs.edit()
            .putInt("old_cpu", IntSetting.CPU_BACKEND.getInt())
            .putInt("old_backend", IntSetting.RENDERER_BACKEND.getInt())
            .putInt("old_resolution", IntSetting.RENDERER_RESOLUTION.getInt())
            .putInt("old_scaling", IntSetting.RENDERER_SCALING_FILTER.getInt())
            .putInt("old_aa", IntSetting.RENDERER_ANTI_ALIASING.getInt())
            .putInt("old_vram", IntSetting.RENDERER_VRAM_USAGE_MODE.getInt())
            .putBoolean("old_docked", BooleanSetting.USE_DOCKED_MODE.getBoolean())
            .putBoolean("old_cache", BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.getBoolean())
            .putBoolean("old_async_shader", BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.getBoolean())
            .putBoolean("old_async_gpu", BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.getBoolean())
            .putBoolean("old_framegen", BooleanSetting.RENDERER_FRAME_GEN.getBoolean())
            .apply()

        IntSetting.CPU_BACKEND.setInt(1)                   // NCE on ARM64
        IntSetting.RENDERER_BACKEND.setInt(1)              // Vulkan
        IntSetting.RENDERER_RESOLUTION.setInt(2)           // 0.75x (540p handheld)
        IntSetting.RENDERER_SCALING_FILTER.setInt(1)       // bilinear
        IntSetting.RENDERER_ANTI_ALIASING.setInt(0)        // none, first benchmark
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(0)      // conservative memory usage

        BooleanSetting.USE_DOCKED_MODE.setBoolean(false)
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)
        BooleanSetting.RENDERER_FRAME_GEN.setBoolean(false)

        // Do NOT force ASTC decode paths, BC1, depth/stencil, EDS3 or custom
        // drivers: the Vulkan device must advertise actual support itself.
        applyLowMemorySettings()
        NativeConfig.saveGlobalConfig()
        prefs.edit()
            .putBoolean(APPLIED_KEY, true)
            .putBoolean(MEMORY_V2_KEY, true)
            .apply()
        applySafeStartupV4(context, prefs)
    }

    /**
     * Recovery path for developers. Use from a controlled UI/debug action.
     * This does not touch firmware or the installed system graphics driver.
     */
    fun restorePrevious(context: Context): Boolean {
        if (!isTargetDevice()) return false
        val prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
        if (!prefs.getBoolean(APPLIED_KEY, false) ||
            !prefs.contains("old_cpu") || NativeConfig.isPerGameConfigLoaded()) {
            return false
        }
        IntSetting.CPU_BACKEND.setInt(prefs.getInt("old_cpu", 1))
        IntSetting.RENDERER_BACKEND.setInt(prefs.getInt("old_backend", 1))
        IntSetting.RENDERER_RESOLUTION.setInt(prefs.getInt("old_resolution", 3))
        IntSetting.RENDERER_SCALING_FILTER.setInt(prefs.getInt("old_scaling", 1))
        IntSetting.RENDERER_ANTI_ALIASING.setInt(prefs.getInt("old_aa", 0))
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(prefs.getInt("old_vram", 0))
        BooleanSetting.USE_DOCKED_MODE.setBoolean(prefs.getBoolean("old_docked", false))
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(prefs.getBoolean("old_cache", true))
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(
            prefs.getBoolean("old_async_shader", false))
        BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(
            prefs.getBoolean("old_async_gpu", true))
        BooleanSetting.RENDERER_FRAME_GEN.setBoolean(prefs.getBoolean("old_framegen", false))
        if (prefs.contains("old_workers")) {
            IntSetting.ANDROID_PIPELINE_WORKERS.setInt(prefs.getInt("old_workers", 2))
        }
        if (prefs.contains("old_memory")) {
            IntSetting.MEMORY_LAYOUT.setInt(prefs.getInt("old_memory", 0))
        }
        if (prefs.contains("old_v4_cpu_accuracy")) {
            IntSetting.CPU_ACCURACY.setInt(prefs.getInt("old_v4_cpu_accuracy", 0))
            IntSetting.RENDERER_ACCURACY.setInt(prefs.getInt("old_v4_gpu_accuracy", 0))
            IntSetting.FAST_CPU_TIME.setInt(prefs.getInt("old_v4_cpu_clock", 0))
            IntSetting.FAST_GPU_TIME.setInt(prefs.getInt("old_v4_gpu_clock", 0))
            BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(
                prefs.getBoolean("old_v4_force_clock", false))
        }
        NativeConfig.saveGlobalConfig()
        prefs.edit()
            .remove(APPLIED_KEY)
            .remove(MEMORY_V2_KEY)
            .remove(PERFORMANCE_V4_KEY)
            .apply()
        return true
    }
}
