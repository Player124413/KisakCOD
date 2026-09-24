package com.kisak.cod

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.kisak.cod.input.InputBridge
import com.kisak.cod.input.TouchControlsView
import com.kisak.cod.input.TouchLayout
import com.kisak.cod.settings.GameSettings

/**
 * Hosts the running game.
 *
 * Structure:
 *   SurfaceView        -> the ANativeWindow DXVK builds its Vulkan swapchain on
 *   TouchControlsView  -> transparent overlay, owns all input
 *
 * The render loop lives on the engine's own thread (see android_bridge.cpp);
 * this Activity only owns lifecycle and the input overlay.
 */
class GameActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var controls: TouchControlsView
    private lateinit var settings: GameSettings

    private val mainHandler = Handler(Looper.getMainLooper())
    private var engineStarted = false
    private var surfaceReady = false

    // ---------------------------------------------------------------- lifecycle

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_game)

        settings = GameSettings(this)
        surfaceView = findViewById(R.id.game_surface)
        controls = findViewById(R.id.touch_controls)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // The engine must not be handed the GPU before a real surface exists.
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                surfaceReady = true
                InputBridge.nativeSurfaceCreated(holder.surface)
                maybeStartEngine()
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {
                InputBridge.nativeSurfaceChanged(holder.surface, w, h)
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                surfaceReady = false
                InputBridge.nativeSurfaceDestroyed()
            }
        })

        setupControls()
        applyLayout(settings.touchLayout)
        enterImmersiveMode()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enterImmersiveMode()
        if (!hasFocus) {
            // Losing focus with a button held must release it, otherwise the
            // game keeps firing after the user alt-tabs away.
            controls.onHostPause()
        }
    }

    override fun onResume() {
        super.onResume()
        InputBridge.nativeSetPaused(false)
    }

    override fun onPause() {
        // Order matters: stop input first, then pause the engine. Otherwise a
        // queued command can land after the engine has stopped draining.
        controls.releaseAll()
        InputBridge.nativeSetPaused(true)
        super.onPause()
    }

    override fun onDestroy() {
        controls.releaseAll()
        InputBridge.nativeShutdown()
        engineStarted = false
        super.onDestroy()
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        // A game must never exit silently on a stray back press.
        AlertDialog.Builder(this)
            .setTitle(R.string.quit_title)
            .setMessage(R.string.quit_message)
            .setPositiveButton(R.string.quit) { _, _ ->
                controls.releaseAll()
                InputBridge.nativeShutdown()
                finish()
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    // ---------------------------------------------------------------- engine

    private fun maybeStartEngine() {
        if (engineStarted || !surfaceReady) return
        val dir = settings.gameDir
        if (dir.isBlank()) {
            Toast.makeText(this, R.string.no_game_dir, Toast.LENGTH_LONG).show()
            finish()
            return
        }

        if (!InputBridge.load()) {
            Toast.makeText(this, R.string.native_load_failed, Toast.LENGTH_LONG).show()
            finish()
            return
        }

        val args = buildString {
            append("+set fs_game \"\" ")
            if (settings.useOpenAl) append("+set snd_useOpenAL 1 ")
            if (settings.skipCinematics) append("+set r_skipCinematics 1 ")
            if (settings.extraArgs.isNotBlank()) append(settings.extraArgs).append(' ')
        }.trim()

        engineStarted = InputBridge.nativeInit(dir, args)
        if (!engineStarted) {
            val err = InputBridge.nativeLastError()
            Toast.makeText(
                this, getString(R.string.engine_start_failed, err ?: "unknown"), Toast.LENGTH_LONG
            ).show()
        } else {
            // Surface the first error the engine reports, if any.
            mainHandler.postDelayed(checkErrorRunnable, 1500)
        }
    }

    private val checkErrorRunnable = object : Runnable {
        override fun run() {
            val err = InputBridge.nativeLastError()
            if (!err.isNullOrBlank()) {
                Toast.makeText(this@GameActivity, err, Toast.LENGTH_LONG).show()
            } else if (engineStarted) {
                mainHandler.postDelayed(this, 5000)
            }
        }
    }

    // ---------------------------------------------------------------- input

    private fun setupControls() {
        controls.listener = object : TouchControlsView.Listener {
            override fun onCommand(command: String) {
                InputBridge.nativeQueueCommand(command)
            }

            override fun onLookDelta(dx: Float, dy: Float) {
                InputBridge.nativeAddLook(dx, dy)
            }

            override fun onMoveAxis(x: Float, y: Float) {
                InputBridge.nativeSetMoveAxis(x, y)
            }
        }
    }

    private fun applyLayout(layout: TouchLayout) {
        controls.layout = layout
    }

    // ---------------------------------------------------------------- chrome

    @SuppressLint("InlinedApi")
    private fun enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.hide(
                WindowInsets.Type.systemBars() or WindowInsets.Type.navigationBars()
            )
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                )
        }
    }

    companion object {
        fun launch(context: Context) {
            context.startActivity(Intent(context, GameActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
            })
        }
    }
}
