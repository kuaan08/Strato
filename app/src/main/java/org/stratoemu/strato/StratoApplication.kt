/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato

import android.app.Application
import android.content.Context
import com.google.android.material.color.DynamicColors
import com.google.android.material.color.DynamicColorsOptions
import dagger.hilt.android.HiltAndroidApp
import org.stratoemu.strato.di.getSettings
import org.stratoemu.strato.settings.EmulationSettings
import org.stratoemu.strato.utils.GpuDriverHelper
import org.stratoemu.strato.utils.GpuDriverInstallResult
import java.io.File

/**
 * @return The optimal directory for putting public files inside, this may return a private directory if a public directory cannot be retrieved
 */
fun Context.getPublicFilesDir() : File = getExternalFilesDir(null) ?: filesDir

@HiltAndroidApp
class StratoApplication : Application() {
    init {
        instance = this
    }

    companion object {
        lateinit var instance : StratoApplication
            private set

        val context : Context get() = instance.applicationContext
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        System.loadLibrary("skyline")

        // Bootstrap the bundled G615 PanVK package once on the target Mali
        // family.  A user's later manual driver selection is preserved.
        val bootstrapPrefs = getSharedPreferences("strato_mali_bootstrap", MODE_PRIVATE)
        if (!bootstrapPrefs.getBoolean("panvk_g615_v1_configured", false) &&
            GpuDriverHelper.isMaliDevice() &&
            GpuDriverHelper.getSystemGpuName()?.contains("Mali-G615", ignoreCase = true) == true) {
            when (GpuDriverHelper.installBundledPanVk(this)) {
                GpuDriverInstallResult.Success, GpuDriverInstallResult.AlreadyInstalled -> {
                    EmulationSettings.global.gpuDriver = GpuDriverHelper.BUNDLED_PANVK_LABEL
                    bootstrapPrefs.edit().putBoolean("panvk_g615_v1_configured", true).apply()
                }
                else -> Unit
            }
        }

        val dynamicColorsOptions = DynamicColorsOptions.Builder().setPrecondition { _, _ -> getSettings().useMaterialYou }.build()
        DynamicColors.applyToActivitiesIfAvailable(this, dynamicColorsOptions)
    }
}
