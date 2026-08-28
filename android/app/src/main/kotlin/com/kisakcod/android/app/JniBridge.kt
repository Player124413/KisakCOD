package com.kisakcod.android.app

import android.view.Surface

/**
 * Thin Kotlin <-> native bridge. The native side (jni/…) owns the engine host:
 * EGL/GLES context on the game surface, the COM_ init/frame loop of the engine
 * (when compiled with the engine, M2+) and the touch -> engine translation.
 *
 * Method names match the JNI exports in jni/host.cpp — keep them in sync.
 */
object JniBridge {

    init {
        System.loadLibrary("kisakcod_game")
    }

    /** True when this build was linked against the real KisakCOD engine. */
    external fun isEngineLinked(): Boolean

    /** Pass the game data root (fs_basepath) to native. Calls before surface. */
    external fun nativeCreate(gameRootDir: String)

    external fun nativeSurfaceCreated(surface: Surface): Boolean
    external fun nativeSurfaceChanged(width: Int, height: Int)
    external fun nativeSurfaceDestroyed()

    /** Step one engine frame; dtMs is elapsed wall time. Returns false if the game quit. */
    external fun nativeStep(dtMs: Long): Boolean

    // ----- touch (semantic actions; mapping to engine keys is native side) -----
    external fun nativeButton(actionId: String, down: Boolean)
    external fun nativeStick(side: Int, xNorm: Float, yNorm: Float, dtMs: Long)

    // ----- gamepad (HID) — never gated by the touch master switch -----
    external fun nativeGamepadKey(keyCode: Int, down: Boolean)
    external fun nativeGamepadStick(side: Int, x: Float, y: Float)

    // ----- lifecycle / settings -----
    external fun nativePause()
    external fun nativeResume()
    external fun nativeDestroy()
    external fun nativeSetFloat(name: String, value: Float)
    external fun nativeSetBool(name: String, value: Boolean)

    /**
     * Push an engine console command (e.g. "seta r_picmip 2") — used by the
     * launcher to apply the perf profile without restarting.
     */
    external fun nativeCmd(text: String)
}