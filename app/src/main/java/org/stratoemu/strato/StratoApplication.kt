/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato

import android.app.Application
import android.app.Activity
import android.content.Context
import android.os.Bundle
import androidx.appcompat.app.AppCompatDelegate
import com.google.android.material.color.DynamicColors
import com.google.android.material.color.DynamicColorsOptions
import dagger.hilt.android.HiltAndroidApp
import org.stratoemu.strato.di.getSettings
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
        AppCompatDelegate.setDefaultNightMode(when (getSettings().appTheme) {
            0 -> AppCompatDelegate.MODE_NIGHT_NO
            1 -> AppCompatDelegate.MODE_NIGHT_YES
            2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
        })
        val dynamicColorsOptions = DynamicColorsOptions.Builder().setPrecondition { _, _ -> getSettings().useMaterialYou }.build()
        DynamicColors.applyToActivitiesIfAvailable(this, dynamicColorsOptions)
        registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityPreCreated(activity: Activity, savedInstanceState: Bundle?) {
                val settings = getSettings()
                if (settings.useMaterialYou) return

                val paletteOverlay = when (settings.appColorPalette) {
                    1 -> R.style.ThemeOverlay_Strato_Palette_Emerald
                    2 -> R.style.ThemeOverlay_Strato_Palette_Violet
                    3 -> R.style.ThemeOverlay_Strato_Palette_Amoled
                    else -> 0 // The default Strato blue palette is already defined by AppTheme.
                }
                if (paletteOverlay != 0) activity.theme.applyStyle(paletteOverlay, true)
            }

            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) = Unit
            override fun onActivityStarted(activity: Activity) = Unit
            override fun onActivityResumed(activity: Activity) = Unit
            override fun onActivityPaused(activity: Activity) = Unit
            override fun onActivityStopped(activity: Activity) = Unit
            override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) = Unit
            override fun onActivityDestroyed(activity: Activity) = Unit
        })
    }
}
