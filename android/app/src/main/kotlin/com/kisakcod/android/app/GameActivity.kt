package com.kisakcod.android.app

import android.app.Activity
import android.os.Build
import android.os.Bundle
import android.view.Choreographer
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.Window
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.FrameLayout

/**
 * Hosts the game: native rendering surface (GameSurfaceView) with the touch
 * overlay (TouchOverlay) on top, driven by a Choreographer frame loop so frame
 * pacing follows the display refresh rate (with an optional FPS cap for weak
 * devices). Bluetooth/USB gamepads are mapped here straight to engine keys.
 */
class GameActivity : Activity(), Choreographer.FrameCallback {

    private lateinit var prefs: UiPrefs
    private lateinit var overlay: TouchOverlay
    private var lastFrameNanos = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = UiPrefs(this)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
        hideSystemUi()

        // ---- native boot ----
        JniBridge.nativeCreate(GamePaths.gameRoot(this).absolutePath)
        pushSettings()

        // ---- views ----
        val root = FrameLayout(this)
        val surface = GameSurfaceView(this)
        root.addView(surface, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        overlay = TouchOverlay(this, prefs)
        root.addView(overlay, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        setContentView(root)
    }

    /** Push launcher settings (sensitivity, resolution scale, fps cap) to native. */
    private fun pushSettings() {
        JniBridge.nativeSetFloat("sens_h", prefs.sensHor)
        JniBridge.nativeSetFloat("sens_v", prefs.sensVert)
        JniBridge.nativeSetBool("invert_y", prefs.invertY)
        JniBridge.nativeSetFloat("look_curve", prefs.lookCurve.toFloat())
        JniBridge.nativeSetFloat("render_scale", prefs.renderScale)
        JniBridge.nativeSetBool("touch_enabled", prefs.touchEnabled)
    }

    private fun hideSystemUi() {
        if (Build.VERSION.SDK_INT >= 30) {
            window.insetsController?.let { c ->
                c.hide(WindowInsets.Type.systemBars())
                c.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility =
                (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
        }
    }

    override fun onStart() {
        super.onStart()
        JniBridge.nativeResume()
        lastFrameNanos = 0
        Choreographer.getInstance().postFrameCallback(this)
    }

    override fun onStop() {
        Choreographer.getInstance().removeFrameCallback(this)
        JniBridge.nativePause()
        super.onStop()
    }

    override fun onDestroy() {
        JniBridge.nativeDestroy()
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemUi()
    }

    override fun doFrame(frameTimeNanos: Long) {
        val thisFrame = if (lastFrameNanos == 0L) frameTimeNanos else lastFrameNanos
        val dtMs = (frameTimeNanos - thisFrame) / 1_000_000L
        lastFrameNanos = frameTimeNanos

        val capMs = if (prefs.fpsCap > 0) 1000L / prefs.fpsCap else 0L
        if (capMs > 0 && dtMs < capMs) {
            // skip this vsync (cap reached) — dt accumulates on the next frame
            Choreographer.getInstance().postFrameCallback(this)
            return
        }

        val keepRunning = JniBridge.nativeStep(dtMs)
        if (!keepRunning) {
            finish()
            return
        }
        Choreographer.getInstance().postFrameCallback(this)
    }

    // ============================================================== gamepads

    /** Android gamepad keycode -> engine keynum (see src/ui/keycodes.h). */
    private fun gamepadEngineKey(keyCode: Int): Int? = when (keyCode) {
        KeyEvent.KEYCODE_BUTTON_A -> 32            // K_SPACE (jump)
        KeyEvent.KEYCODE_BUTTON_B -> 'c'.code       // crouch
        KeyEvent.KEYCODE_BUTTON_X -> 'r'.code       // reload
        KeyEvent.KEYCODE_BUTTON_Y -> 'v'.code       // melee
        KeyEvent.KEYCODE_BUTTON_L1 -> 160           // K_SHIFT (sprint)
        KeyEvent.KEYCODE_BUTTON_R1 -> 'g'.code      // grenade
        KeyEvent.KEYCODE_BUTTON_L2 -> 201           // K_MOUSE2 (ADS)
        KeyEvent.KEYCODE_BUTTON_R2 -> 200           // K_MOUSE1 (fire)
        KeyEvent.KEYCODE_BUTTON_START -> 27         // K_ESCAPE (pause)
        KeyEvent.KEYCODE_BUTTON_SELECT -> 9         // K_TAB (scoreboard)
        KeyEvent.KEYCODE_BUTTON_THUMBL -> 159       // K_CTRL (crouch altern.)
        KeyEvent.KEYCODE_DPAD_UP -> 'w'.code
        KeyEvent.KEYCODE_DPAD_DOWN -> 's'.code
        KeyEvent.KEYCODE_DPAD_LEFT -> 'a'.code
        KeyEvent.KEYCODE_DPAD_RIGHT -> 'd'.code
        else -> null
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val key = gamepadEngineKey(keyCode)
        if (key != null) {
            if (event.repeatCount > 0) return true // ignore key-repeat
            JniBridge.nativeGamepadKey(key, true)
            return true
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        val key = gamepadEngineKey(keyCode)
        if (key != null) {
            JniBridge.nativeGamepadKey(key, false)
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.actionMasked != MotionEvent.ACTION_MOVE &&
            event.actionMasked != MotionEvent.ACTION_DOWN &&
            event.actionMasked != MotionEvent.ACTION_UP) {
            return super.onGenericMotionEvent(event)
        }
        val x = event.getAxisValue(MotionEvent.AXIS_X)
        val y = event.getAxisValue(MotionEvent.AXIS_Y)
        val rx = event.getAxisValue(MotionEvent.AXIS_Z)
        val ry = event.getAxisValue(MotionEvent.AXIS_RZ)
        JniBridge.nativeGamepadStick(0, x.coerceIn(-1f, 1f), y.coerceIn(-1f, 1f))
        JniBridge.nativeGamepadStick(1, rx.coerceIn(-1f, 1f), ry.coerceIn(-1f, 1f))
        return true
    }
}