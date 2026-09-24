package com.kisak.cod

import android.content.Intent
import android.graphics.Typeface
import android.os.Bundle
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.kisak.cod.input.FeelConfig
import com.kisak.cod.settings.GameSettings
import com.kisak.cod.util.DeviceTier
import java.io.File

/**
 * Launcher / settings screen.
 *
 * Two jobs:
 *   1. Refuse to launch on hardware that cannot run DXVK, and say why.
 *   2. Expose every setting that decides whether the first run is playable.
 *
 * Storage note: the engine opens game data with plain C file I/O, so it needs a
 * real filesystem path. Scoped-storage content URIs are useless to it. The
 * default location is therefore getExternalFilesDir(), which is a real path the
 * app can access with no permission prompt.
 */
class LauncherActivity : AppCompatActivity() {

    private lateinit var settings: GameSettings
    private lateinit var container: LinearLayout
    private lateinit var launchButton: Button
    private lateinit var report: DeviceTier.Report

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_launcher)

        settings = GameSettings(this)
        container = findViewById(R.id.settings_container)
        launchButton = findViewById(R.id.btn_launch)

        report = DeviceTier.probe(this)

        buildDeviceSection()
        buildGameFilesSection()
        buildGraphicsSection()
        buildAudioSection()
        buildControlsSection()
        buildAdvancedSection()

        launchButton.setOnClickListener { launch() }
        refreshLaunchState()
    }

    // ---------------------------------------------------------------- sections

    private fun buildDeviceSection() {
        section("Device") {
            row("GPU", report.gpuRenderer.ifBlank { "unknown" })
            row("Vulkan", report.vulkanVersionString)
            row("Memory", "${report.deviceMemoryMb} MB")
            row("CPU cores", "${report.cpuCores}")
            row("Tier", report.level.label)

            if (!report.isUsable) {
                warning(report.blockingReason ?: "Unsupported device.")
            }
            report.warnings.forEach { warning(it) }
        }
    }

    private fun buildGameFilesSection() {
        section("Game files") {
            val defaultDir = defaultGameDir()
            val pathEdit = EditText(this@LauncherActivity).apply {
                setText(settings.gameDir.ifBlank { defaultDir })
                inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI
                setSingleLine(true)
            }

            val statusView = TextView(this@LauncherActivity).apply { textSize = 13f }

            fun refreshStatus(): Boolean {
                val ok = validateGameDir(pathEdit.text.toString().trim())
                statusView.text = if (ok) getString(R.string.game_files_ok)
                else getString(R.string.game_files_missing)
                statusView.setTextColor(
                    getColor(if (ok) android.R.color.holo_green_dark else android.R.color.holo_red_dark)
                )
                return ok
            }

            pathEdit.addTextChangedListener(object : TextWatcher {
                override fun afterTextChanged(s: Editable?) {
                    settings.gameDir = s?.toString()?.trim().orEmpty()
                    refreshStatus()
                    refreshLaunchState()
                }
                override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            })

            addView(TextView(this@LauncherActivity).apply {
                text = getString(R.string.game_dir_hint)
                textSize = 13f
            })
            addView(pathEdit)
            addView(statusView)

            action("Use default folder") {
                pathEdit.setText(defaultDir)
                settings.gameDir = defaultDir
                refreshStatus()
                refreshLaunchState()
            }

            refreshStatus()
        }
    }

    private fun buildGraphicsSection() {
        section("Graphics") {
            slider(
                "Resolution scale",
                settings.resolutionScale, 0.5f, 1.0f,
                format = { "${(it * 100).toInt()}%" }
            ) { settings.resolutionScale = it }

            slider(
                "Frame limit",
                settings.frameLimit.toFloat(), 0f, 120f,
                step = 10f,
                format = { if (it < 10f) "uncapped" else "${it.toInt()} fps" }
            ) { settings.frameLimit = it.toInt() }

            toggle("Performance overlay", settings.showPerfHud) {
                settings.showPerfHud = it
            }

            note("Recommended for this device: " +
                "${(report.recommendedResScale * 100).toInt()}% @ " +
                "${if (report.recommendedFrameLimit == 0) "uncapped" else "${report.recommendedFrameLimit} fps"}")
        }
    }

    private fun buildAudioSection() {
        section("Audio") {
            toggle("OpenAL backend", settings.useOpenAl) { settings.useOpenAl = it }
            note("The original Miles Sound System is a closed 32-bit x86 binary and " +
                "cannot be built for Android. OpenAL Soft is the supported backend.")
            toggle("Skip cinematics", settings.skipCinematics) { settings.skipCinematics = it }
            note("Bink video (binkw32.dll) is also a closed x86 binary. " +
                "Skipping cinematics is required until an ARM decoder is wired up.")
        }
    }

    private fun buildControlsSection() {
        section("Controls") {
            // Always read the CURRENT layout inside each callback. Capturing it
            // once here means slider N writes back a snapshot taken before
            // slider 1 changed anything, silently discarding the other settings.
            fun updateFeel(mutate: (FeelConfig) -> FeelConfig) {
                val current = settings.touchLayout
                settings.touchLayout = current.copy(feel = mutate(current.feel))
            }

            val feel = settings.touchLayout.feel

            slider("Look sensitivity", feel.lookSensitivity, 0.1f, 3.0f,
                format = { "%.2f".format(it) }) { v ->
                updateFeel { it.copy(lookSensitivity = v) }
            }
            slider("ADS sensitivity scale", feel.adsSensitivityScale, 0.1f, 1.5f,
                format = { "%.2f".format(it) }) { v ->
                updateFeel { it.copy(adsSensitivityScale = v) }
            }
            slider("Stick dead zone", feel.stickDeadZone, 0f, 0.4f,
                format = { "${(it * 100).toInt()}%" }) { v ->
                updateFeel { it.copy(stickDeadZone = v) }
            }
            slider("Move threshold", feel.moveThreshold, 0.1f, 0.8f,
                format = { "${(it * 100).toInt()}%" }) { v ->
                updateFeel { it.copy(moveThreshold = v) }
            }
            toggle("Invert Y", feel.invertY) { v ->
                updateFeel { it.copy(invertY = v) }
            }

            action("Edit layout") {
                Toast.makeText(this@LauncherActivity, R.string.layout_editor_soon, Toast.LENGTH_SHORT).show()
            }
            action("Reset layout") {
                settings.resetLayout()
                Toast.makeText(this@LauncherActivity, R.string.layout_reset, Toast.LENGTH_SHORT).show()
            }

            note("Physical gamepads are forwarded automatically; the on-screen " +
                "controls disappear once one is connected.")
        }
    }

    private fun buildAdvancedSection() {
        section("Advanced") {
            val edit = EditText(this@LauncherActivity).apply {
                setText(settings.extraArgs)
                setSingleLine(true)
                hint = "+set r_picmip 2 ..."
            }
            edit.addTextChangedListener(object : TextWatcher {
                override fun afterTextChanged(s: Editable?) {
                    settings.extraArgs = s?.toString().orEmpty()
                }
                override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            })
            addView(TextView(this@LauncherActivity).apply {
                text = "Extra launch arguments"
                textSize = 14f
            })
            addView(edit)
        }
    }

    // ---------------------------------------------------------------- launch

    private fun refreshLaunchState() {
        val dirOk = validateGameDir(settings.gameDir.ifBlank { defaultGameDir() })
        launchButton.isEnabled = report.isUsable && dirOk
        launchButton.text = when {
            !report.isUsable -> getString(R.string.launch_blocked)
            !dirOk -> getString(R.string.launch_no_files)
            else -> getString(R.string.launch)
        }
    }

    private fun launch() {
        if (settings.gameDir.isBlank()) settings.gameDir = defaultGameDir()

        if (!report.isUsable) {
            AlertDialog.Builder(this)
                .setTitle(R.string.unsupported_title)
                .setMessage(report.blockingReason)
                .setPositiveButton(android.R.string.ok, null)
                .show()
            return
        }
        if (report.warnings.isNotEmpty()) {
            AlertDialog.Builder(this)
                .setTitle(R.string.warnings_title)
                .setMessage(report.warnings.joinToString("\n\n"))
                .setPositiveButton(R.string.launch) { _, _ -> GameActivity.launch(this) }
                .setNegativeButton(R.string.cancel, null)
                .show()
            return
        }
        GameActivity.launch(this)
    }

    // ---------------------------------------------------------------- helpers

    private fun defaultGameDir(): String {
        val dir = getExternalFilesDir(null) ?: filesDir
        val game = File(dir, "cod4")
        if (!game.exists()) game.mkdirs()
        return game.absolutePath
    }

    /**
     * Cheap sanity check: the engine needs main/ (IWD archives) and zone/
     * (fastfiles). Without them it fails much later with a confusing error.
     */
    private fun validateGameDir(path: String): Boolean {
        if (path.isBlank()) return false
        val root = File(path)
        if (!root.isDirectory) return false
        val main = File(root, "main")
        val zone = File(root, "zone")
        val hasMain = main.isDirectory && (main.list()?.any { it.endsWith(".iwd", true) } == true)
        val hasZone = zone.isDirectory && (zone.list()?.isNotEmpty() == true)
        return hasMain || hasZone
    }

    // ---------------------------------------------------------------- mini DSL

    private fun section(title: String, block: LinearLayout.() -> Unit) {
        val titleView = TextView(this).apply {
            text = title
            textSize = 16f
            setTypeface(typeface, Typeface.BOLD)
            setPadding(0, 24, 0, 8)
        }
        container.addView(titleView)

        val body = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }
        body.block()
        container.addView(body)
    }

    private fun LinearLayout.row(label: String, value: String) {
        addView(LinearLayout(this@LauncherActivity).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(TextView(this@LauncherActivity).apply {
                text = label; textSize = 14f
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            })
            addView(TextView(this@LauncherActivity).apply {
                text = value; textSize = 14f; gravity = Gravity.END
            })
        })
    }

    private fun LinearLayout.note(text: String) {
        addView(TextView(this@LauncherActivity).apply {
            this.text = text
            textSize = 12f
            alpha = 0.7f
            setPadding(0, 8, 0, 8)
        })
    }

    private fun LinearLayout.warning(text: String) {
        addView(TextView(this@LauncherActivity).apply {
            this.text = text
            textSize = 13f
            setTextColor(getColor(android.R.color.holo_orange_dark))
            setPadding(0, 8, 0, 8)
        })
    }

    private fun LinearLayout.action(label: String, onClick: () -> Unit) {
        addView(Button(this@LauncherActivity).apply {
            text = label
            setOnClickListener { onClick() }
        })
    }

    private fun LinearLayout.toggle(label: String, initial: Boolean, onChange: (Boolean) -> Unit) {
        addView(LinearLayout(this@LauncherActivity).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            addView(TextView(this@LauncherActivity).apply {
                text = label; textSize = 14f
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            })
            addView(Switch(this@LauncherActivity).apply {
                isChecked = initial
                setOnCheckedChangeListener { _, checked -> onChange(checked) }
            })
        })
    }

    private fun LinearLayout.slider(
        label: String,
        initial: Float,
        min: Float,
        max: Float,
        step: Float = 0.05f,
        format: (Float) -> String,
        onChange: (Float) -> Unit,
    ) {
        val valueView = TextView(this@LauncherActivity).apply {
            text = format(initial.coerceIn(min, max))
            textSize = 13f
            gravity = Gravity.END
        }
        addView(LinearLayout(this@LauncherActivity).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(TextView(this@LauncherActivity).apply {
                text = label; textSize = 14f
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            })
            addView(valueView)
        })

        val steps = ((max - min) / step).toInt().coerceAtLeast(1)
        addView(SeekBar(this@LauncherActivity).apply {
            max = steps
            progress = ((initial.coerceIn(min, max) - min) / step).toInt().coerceIn(0, steps)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    val v = min + p * step
                    valueView.text = format(v)
                    if (fromUser) onChange(v)
                }
                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) {}
            })
        })
    }
}
