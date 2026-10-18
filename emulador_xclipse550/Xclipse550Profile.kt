// SPDX-License-Identifier: GPL-3.0-or-later
// Integration add-on for Eden: A57 profile, no alternative GPU driver or firmware.
package org.yuzu.yuzu_emu.utils

import android.os.Build
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting

/**
 * Conservative first-install settings for the confirmed Samsung SM-A576B.
 *
 * These settings are applied ONLY when no global config.ini exists.
 * Afterwards they are normal, user-editable Eden settings.
 * An existing Eden configuration is never replaced.
 *
 * Enum positions are pinned to Eden src/common/settings_enums.h at the
 * commit noted in scripts/apply_xclipse550_to_eden.py.
 */
object Xclipse550Profile {
    fun isTargetDevice(): Boolean =
        Build.MANUFACTURER.equals("samsung", ignoreCase = true) &&
        Build.MODEL.equals("SM-A576B", ignoreCase = true)

    fun applyToFirstInstall(isFreshInstall: Boolean) {
        if (!isFreshInstall || !isTargetDevice()) return
        try {
            // 0=Dynarmic; 1=NCE. Eden will fall back when native execution is unsafe.
            IntSetting.CPU_BACKEND.setInt(1)
            BooleanSetting.FASTMEM.setBoolean(true)

            // 0=1/4, 1=1/2, 2=3/4, 3=1x. 3/4 of handheld 720p ~= 540p.
            IntSetting.RENDERER_RESOLUTION.setInt(2)
            BooleanSetting.USE_DOCKED_MODE.setBoolean(false)

            // VRAM conservative (0); ASTC decode by GPU (1).
            IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(0)
            IntSetting.RENDERER_ASTC_DECODE_METHOD.setInt(1)

            // No forced CPU/GPU clock, no synthetic frame generation.
            BooleanSetting.RENDERER_FORCE_MAX_CLOCK.setBoolean(false)
            BooleanSetting.RENDERER_FRAME_GEN.setBoolean(false)

            // Persist the compiled shader cache and keep asynchronous GPU enabled.
            BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
            BooleanSetting.RENDERER_ASYNCHRONOUS_GPU_EMULATION.setBoolean(true)

            // Avoid asynchronously skipping draw calls on first compatibility trials.
            BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(false)

            NativeConfig.saveGlobalConfig()
            Log.info("[Xclipse550Profile] conservative SM-A576B first-install profile applied")
        } catch (e: Exception) {
            // Never stop the app from opening if a setting changed upstream.
            Log.error("[Xclipse550Profile] could not apply optional profile: ${e.message}")
        }
    }
}
