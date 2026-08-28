package com.kisakcod.android.app

import android.content.Context

/**
 * All user settings live in SharedPreferences (survives restarts, no files to break).
 * The touch layout itself is stored as serialized text (see TouchLayout) so it can be
 * backed up / restored / shared as a plain string.
 */
class UiPrefs(context: Context) {

    private val sp = context.getSharedPreferences("kisak_port", Context.MODE_PRIVATE)

    // ---- touch master switch ----
    var touchEnabled: Boolean
        get() = sp.getBoolean("touch_enabled", true)
        set(v) = sp.edit().putBoolean("touch_enabled", v).apply()

    // ---- look sensitivity ----
    var sensHor: Float        // normalized 0.1..3.0, 1.0 = default
        get() = sp.getFloat("sens_h", 1.0f)
        set(v) = sp.edit().putFloat("sens_h", v.coerceIn(0.1f, 3.0f)).apply()

    var sensVert: Float
        get() = sp.getFloat("sens_v", 1.0f)
        set(v) = sp.edit().putFloat("sens_v", v.coerceIn(0.1f, 3.0f)).apply()

    var invertY: Boolean
        get() = sp.getBoolean("invert_y", false)
        set(v) = sp.edit().putBoolean("invert_y", v).apply()

    /** look stick response: 0=linear, 1=slightly boosted at high deflection */
    var lookCurve: Int
        get() = sp.getInt("look_curve", 1)
        set(v) = sp.edit().putInt("look_curve", v.coerceIn(0, 1)).apply()

    var adsToggle: Boolean   // ADS button behaves as tap-to-toggle instead of hold
        get() = sp.getBoolean("ads_toggle", false)
        set(v) = sp.edit().putBoolean("ads_toggle", v).apply()

    // ---- rendering perf (game host reads these and passes them to native) ----
    /** render resolution scale 0.5..1.0 of the surface */
    var renderScale: Float
        get() = sp.getFloat("render_scale", 1.0f)
        set(v) = sp.edit().putFloat("render_scale", v.coerceIn(0.4f, 1.0f)).apply()

    /** frame cap: 0 = match display refresh, else target fps */
    var fpsCap: Int
        get() = sp.getInt("fps_cap", 0)
        set(v) = sp.edit().putInt("fps_cap", v.coerceIn(0, 120)).apply()

    // ---- layout ----
    var layoutText: String
        get() = sp.getString("layout_text", "") ?: ""
        set(v) = sp.edit().putString("layout_text", v).apply()

    var layoutProfileName: String
        get() = sp.getString("layout_profile", "default") ?: "default"
        set(v) = sp.edit().putString("layout_profile", v).apply()

    // ---- launcher ----
    var perfProfileName: String
        get() = sp.getString("perf_profile", "auto") ?: "auto"
        set(v) = sp.edit().putString("perf_profile", v).apply()

    /** persisted SAF uri of the last selected source (folder or zip) */
    var lastImportUri: String
        get() = sp.getString("last_import_uri", "") ?: ""
        set(v) = sp.edit().putString("last_import_uri", v).apply()

    var lastImportIsZip: Boolean
        get() = sp.getBoolean("last_import_is_zip", false)
        set(v) = sp.edit().putBoolean("last_import_is_zip", v).apply()

    var importOverwrite: Boolean
        get() = sp.getBoolean("import_overwrite", true)
        set(v) = sp.edit().putBoolean("import_overwrite", v).apply()

    fun layout(): LayoutConfig {
        val t = layoutText
        return if (t.isBlank()) LayoutConfig.defaults() else LayoutConfig.parse(t)
    }

    fun saveLayout(cfg: LayoutConfig) {
        layoutText = cfg.serialize()
        layoutProfileName = cfg.layoutName
    }
}