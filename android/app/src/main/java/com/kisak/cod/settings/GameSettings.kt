package com.kisak.cod.settings

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import com.kisak.cod.input.TouchLayout
import org.json.JSONObject

/**
 * Persisted launcher + game settings.
 */
class GameSettings(context: Context) {

    private val prefs: SharedPreferences =
        context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    /** Directory (absolute path) holding the COD4 game files. Empty = not set. */
    var gameDir: String
        get() = prefs.getString(KEY_GAME_DIR, "").orEmpty()
        set(value) = prefs.edit { putString(KEY_GAME_DIR, value) }

    /** Extra console commands appended to the command line each launch. */
    var extraArgs: String
        get() = prefs.getString(KEY_EXTRA_ARGS, "").orEmpty()
        set(value) = prefs.edit { putString(KEY_EXTRA_ARGS, value) }

    /** Render resolution as a fraction of the surface size, 0.5 .. 1.0. */
    var resolutionScale: Float
        get() = prefs.getFloat(KEY_RES_SCALE, 1.0f)
        set(value) = prefs.edit { putFloat(KEY_RES_SCALE, value.coerceIn(0.5f, 1.0f)) }

    /** Cap the frame rate. 0 = uncapped. */
    var frameLimit: Int
        get() = prefs.getInt(KEY_FRAME_LIMIT, 0)
        set(value) = prefs.edit { putInt(KEY_FRAME_LIMIT, value.coerceIn(0, 240)) }

    /** Use the OpenAL sound backend (the Miles backend cannot be built for ARM). */
    var useOpenAl: Boolean
        get() = prefs.getBoolean(KEY_OPENAL, true)
        set(value) = prefs.edit { putBoolean(KEY_OPENAL, value) }

    /** Skip Bink cinematics. binkw32 is a closed x86 binary; there is no ARM build. */
    var skipCinematics: Boolean
        get() = prefs.getBoolean(KEY_SKIP_CINEMATICS, true)
        set(value) = prefs.edit { putBoolean(KEY_SKIP_CINEMATICS, value) }

    /** Show the on-screen performance overlay. */
    var showPerfHud: Boolean
        get() = prefs.getBoolean(KEY_PERF_HUD, false)
        set(value) = prefs.edit { putBoolean(KEY_PERF_HUD, value) }

    var touchLayoutJson: String
        get() = prefs.getString(KEY_LAYOUT, null)
            ?: TouchLayout.defaultLayout().toJson().toString()
        set(value) = prefs.edit { putString(KEY_LAYOUT, value) }

    var touchLayout: TouchLayout
        get() = runCatching { TouchLayout.fromJson(JSONObject(touchLayoutJson)) }
            .getOrDefault(TouchLayout.defaultLayout())
        set(value) { touchLayoutJson = value.toJson().toString() }

    fun resetLayout() {
        touchLayout = TouchLayout.defaultLayout()
    }

    companion object {
        private const val PREFS_NAME = "kisakcod_settings"
        private const val KEY_GAME_DIR = "game_dir"
        private const val KEY_EXTRA_ARGS = "extra_args"
        private const val KEY_RES_SCALE = "res_scale"
        private const val KEY_FRAME_LIMIT = "frame_limit"
        private const val KEY_OPENAL = "use_openal"
        private const val KEY_SKIP_CINEMATICS = "skip_cinematics"
        private const val KEY_PERF_HUD = "perf_hud"
        private const val KEY_LAYOUT = "touch_layout"
    }
}
