package com.kisak.cod.input

/**
 * JNI boundary to the native engine.
 *
 * Threading contract (this matters — getting it wrong is the difference between
 * "works" and "randomly corrupts the command buffer"):
 *
 *   Every method here may be called from the Android UI thread. The engine runs
 *   on its own game thread. NOTHING on this side ever calls into the engine
 *   directly; the native layer only writes into a lock-free staging area, and
 *   the game thread drains it once per frame inside Android_PumpInput().
 *
 * That is why there is no "run this now" API here: console commands and look
 * deltas are queued, not executed, so Cbuf_AddText() and CL_MouseEvent() are
 * always invoked on the thread that owns them.
 */
object InputBridge {

    private var loaded = false

    @Synchronized
    fun load(): Boolean {
        if (loaded) return true
        return try {
            System.loadLibrary("kisakcod")
            loaded = true
            true
        } catch (t: Throwable) {
            loaded = false
            false
        }
    }

    val isLoaded: Boolean get() = loaded

    // ------------------------------------------------------------ lifecycle

    external fun nativeInit(gameDir: String, args: String): Boolean

    external fun nativeShutdown()

    /** Driven from the render loop on the game thread; drains queued input. */
    external fun nativePumpInput()

    external fun nativeSetPaused(paused: Boolean)

    external fun nativeSurfaceCreated(surface: android.view.Surface): Boolean

    external fun nativeSurfaceChanged(
        surface: android.view.Surface, width: Int, height: Int
    )

    external fun nativeSurfaceDestroyed()

    // ------------------------------------------------------------ input staging

    /** Queue a console command, e.g. "+attack". Drained by nativePumpInput(). */
    external fun nativeQueueCommand(command: String)

    /** Accumulate look delta in engine mouse units. */
    external fun nativeAddLook(dx: Float, dy: Float)

    /** Analogue stick deflection in [-1,1]. Staged for CL_GamepadMove. */
    external fun nativeSetMoveAxis(x: Float, y: Float)

    // ------------------------------------------------------------ queries

    /** True once the engine has reached its main loop and is rendering. */
    external fun nativeIsRunning(): Boolean

    /** Last fatal error string, or null. */
    external fun nativeLastError(): String?
}
