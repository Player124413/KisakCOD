package com.kisakcod.android.app

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast

/**
 * Launcher: imports the COD4 game files (folder / ZIP / "open with") into the
 * right place automatically, sets up a performance profile for the device,
 * and starts the game.
 */
class LauncherActivity : Activity() {

    private lateinit var prefs: UiPrefs
    private lateinit var importer: GameFileImporter

    private lateinit var statusText: TextView
    private lateinit var progressBar: ProgressBar
    private lateinit var progressText: TextView
    private lateinit var repeatImportBtn: Button

    private var lastImportUri: Uri? = null
    private var lastImportZip = false

    companion object {
        private const val REQ_TREE = 1
        private const val REQ_ZIP = 2
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = UiPrefs(this)
        importer = GameFileImporter(this)
        buildUi()
        handleIncomingIntent(intent)
    }

    // ================================================================ UI

    private fun buildUi() {
        val scroll = ScrollView(this)
        val col = LinearLayout(this)
        col.orientation = LinearLayout.VERTICAL
        col.setPadding(dp(16), dp(16), dp(16), dp(24))

        fun text(size: Float, color: Int, bold: Boolean = false): TextView {
            val t = TextView(this)
            t.textSize = size
            t.setTextColor(color)
            if (bold) t.typeface = android.graphics.Typeface.DEFAULT_BOLD
            return t
        }

        val title = text(26f, 0xFFE9D8A6.toInt(), true)
        title.text = "KISAKCOD — ANDROID PORT"
        TitleCard(title)
        col.addView(title)

        val sub = text(14f, 0xFF9DB4A0.toInt())
        sub.text = "Лаунчер · файлы игры · сенсорное управление с редактором раскладки"
        col.addView(margin(sub, top = 2))

        // ---- status card ----
        statusText = text(16f, 0xFFEAF2E3.toInt())
        statusText.text = GamePaths.gameDataSummary(this)
        col.addView(margin(statusText, top = 18, bottom = 2))

        val refresh = smallButton("Обновить состояние")
        refresh.setOnClickListener {
            statusText.text = GamePaths.gameDataSummary(this)
        }
        col.addView(refresh)

        // ---- import buttons ----
        col.addView(space(14))

        val pickFolder = bigButton("📁 Импортировать папку с игрой")
        pickFolder.setOnClickListener { pickFolder() }
        col.addView(pickFolder)

        val pickZip = bigButton("🗜 Импортировать ZIP с игрой")
        pickZip.setOnClickListener { pickZip() }
        col.addView(margin(pickZip, top = 8))

        repeatImportBtn = smallButton("↻ Повторить последний импорт")
        repeatImportBtn.isEnabled = prefs.lastImportUri.isNotEmpty()
        repeatImportBtn.setOnClickListener {
            val u = prefs.lastImportUri
            if (u.isNotEmpty()) runImport(Uri.parse(u), prefs.lastImportIsZip)
        }
        col.addView(repeatImportBtn)

        // ---- progress ----
        progressBar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal)
        progressBar.max = 1000
        progressBar.isIndeterminate = false
        col.addView(margin(progressBar, top = 14))
        progressText = text(13f, 0xFFB0C4B1.toColor())
        progressText.text = " "
        col.addView(progressText)

        // ---- launch ----
        col.addView(space(10))
        val launch = bigButton("▶  ЗАПУСТИТЬ ИГРУ", big = true, accent = true)
        launch.setOnClickListener {
            if (GamePaths.hasGameData(this)) {
                startActivity(Intent(this, GameActivity::class.java))
            } else {
                AlertDialog.Builder(this)
                    .setTitle("Файлы игры не найдены")
                    .setMessage("Сначала импортируйте файлы Call of Duty 4 из папки или ZIP:")
                    .setPositiveButton("Импорт") { _, _ -> pickFolder() }
                    .setNegativeButton("Отмена", null)
                    .show()
            }
        }
        col.addView(launch)

        // ---- perf profile ----
        col.addView(sectionHeader("Производительность"))

        val probe = PerfProfile.detect(this)
        val deviceInfo = text(14f, 0xFFC8D6C2.toColor())
        deviceInfo.text = PerfProfile.describe(probe)
        deviceInfo.setTextIsSelectable(true)
        col.addView(deviceInfo)

        val spinnerTitles = arrayOf("Авто (по железу)", "Слабый — 30 FPS", "Средний — 60 FPS", "Мощный", "Флагман")
        val spinnerValues = arrayOf("auto", "low", "mid", "high", "flagship")
        val spinner = Spinner(this)
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, spinnerTitles)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinner.adapter = adapter
        val storedTier = prefs.perfProfileName
        val storedIdx = spinnerValues.indexOfFirst { it == storedTier }.coerceAtLeast(0)
        spinner.setSelection(storedIdx)
        spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        col.addView(spinner)

        val apply = smallButton("Применить профиль")
        apply.setOnClickListener {
            val idx = spinner.selectedItemPosition
            val tier = when (idx) {
                0 -> probe.tier
                1 -> PerfProfile.Tier.LOW
                2 -> PerfProfile.Tier.MID
                3 -> PerfProfile.Tier.HIGH
                else -> PerfProfile.Tier.FLAGSHIP
            }
            prefs.perfProfileName = spinnerValues[idx]
            PerfProfile.writeEngineCfg(this, tier)
            Toast.makeText(this, "Профиль записан (android_launcher.cfg)", Toast.LENGTH_SHORT).show()
        }
        col.addView(apply)

        // ---- touch settings ----
        col.addView(sectionHeader("Сенсорное управление"))

        col.addView(labelRow("Чувствительность камеры (гориз.):",
            "${prefs.sensHor}x"))
        val sensH = SeekBar(this)
        sensH.max = 100
        sensH.progress = ((prefs.sensHor - 0.1f) / 2.9f * 100).toInt()
        sensH.setOnSeekBarChangeListener(seek { p ->
            prefs.sensHor = 0.1f + p / 100f * 2.9f
        })
        col.addView(sensH)

        col.addView(labelRow("Чувствительность (верт.):", "${prefs.sensVert}x"))
        val sensV = SeekBar(this)
        sensV.max = 100
        sensV.progress = ((prefs.sensVert - 0.1f) / 2.9f * 100).toInt()
        sensV.setOnSeekBarChangeListener(seek { p ->
            prefs.sensVert = 0.1f + p / 100f * 2.9f
        })
        col.addView(sensV)

        val invertY = CheckBox(this)
        invertY.text = "Инвертировать вертикаль (Y)"
        invertY.isChecked = prefs.invertY
        invertY.setOnCheckedChangeListener { _, v -> prefs.invertY = v }
        col.addView(invertY)

        val adsToggle = CheckBox(this)
        adsToggle.text = "ADS переключением (не удержанием)"
        adsToggle.isChecked = prefs.adsToggle
        adsToggle.setOnCheckedChangeListener { _, v -> prefs.adsToggle = v }
        col.addView(adsToggle)

        col.addView(labelRow("Разрешение рендера: ${(prefs.renderScale * 100).toInt()}%"))
        val scale = SeekBar(this)
        scale.max = 60
        scale.progress = ((prefs.renderScale - 0.4f) / 0.6f * 60).toInt()
        scale.setOnSeekBarChangeListener(seek { p ->
            prefs.renderScale = 0.4f + p / 60f * 0.6f
        })
        col.addView(scale)

        col.addView(labelRow("Ограничение FPS:"))
        val fpsSpinner = Spinner(this)
        val fpsAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_item,
            arrayOf("Авто (по экрану)", "30 FPS", "45 FPS", "60 FPS"))
        fpsAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        fpsSpinner.adapter = fpsAdapter
        val fpsVals = intArrayOf(0, 30, 45, 60)
        fpsSpinner.setSelection(fpsVals.indexOfFirst { it == prefs.fpsCap }.coerceAtLeast(0))
        fpsSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                prefs.fpsCap = fpsVals[position]
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        col.addView(fpsSpinner)

        // ---- help ----
        col.addView(sectionHeader("Как перенести игру с ПК"))
        val help = text(13f, 0xFFA9B8A4.toColor())
        help.text = """
            1. У вас должен быть установлен Call of Duty 4 (Steam/диск). Нужны папки main/, zone/ и (желательно) players/.
            2. Нажмите «Импортировать папку с игрой» и укажите папку игры (Google Files / проводник).
               ИЛИ положите ZIP с игрой на телефон и выберите «Импортировать ZIP» — или откройте ZIP через это приложение.
            3. Приложение само разложит файлы в нужное место (main/, zone/, …) — ничего вручную переносить не нужно.
            4. Нажмите «Запустить игру».
            Подсказка: в игре нажмите кнопку EDIT — раскладку кнопок можно двигать, менять размер,
            скрывать кнопки и выключать сенсорное управление целиком.
        """.trimIndent()
        help.setLineSpacing(0f, 1.08f)
        col.addView(help)

        scroll.addView(col, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT))
        setContentView(scroll)
    }

    private fun TitleCard(v: TextView) {
        v.setPadding(dp(2), dp(6), dp(2), dp(6))
    }

    private fun sectionHeader(title: String): TextView {
        val t = TextView(this)
        t.text = title
        t.textSize = 18f
        t.typeface = android.graphics.Typeface.DEFAULT_BOLD
        t.setTextColor(0xFFE9D8A6.toInt())
        t.setPadding(0, dp(22), 0, dp(6))
        return t
    }

    private fun labelRow(label: String): TextView {
        val t = TextView(this)
        t.text = label
        t.textSize = 14f
        t.setTextColor(0xFFC8D6C2.toColor())
        t.setPadding(0, dp(10), 0, 0)
        return t
    }

    private fun bigButton(label: String, big: Boolean = false, accent: Boolean = false): Button {
        val b = Button(this)
        b.text = label
        b.textSize = if (big) 19f else 16f
        b.setPadding(dp(10), dp(14), dp(10), dp(14))
        b.gravity = Gravity.CENTER
        if (accent) b.setTextColor(0xFF0B0E10.toColor())
        return b
    }

    private fun smallButton(label: String): Button {
        val b = Button(this)
        b.text = label
        b.textSize = 13f
        return b
    }

    private fun margin(v: android.view.View, top: Int = 0, bottom: Int = 0): android.view.View {
        val lp = v.layoutParams as? LinearLayout.LayoutParams
            ?: LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT).also { v.layoutParams = it }
        if (top > 0) lp.topMargin = dp(top)
        if (bottom > 0) lp.bottomMargin = dp(bottom)
        return v
    }

    private fun space(h: Int): TextView {
        val t = TextView(this)
        t.text = " "
        t.textSize = h.toFloat() / 8f
        return t
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()

    private fun seek(onProgress: (Int) -> Unit) = object : SeekBar.OnSeekBarChangeListener {
        override fun onProgressChanged(sb: SeekBar, progress: Int, fromUser: Boolean) {
            if (fromUser) onProgress(progress)
        }
        override fun onStartTrackingTouch(sb: SeekBar) {}
        override fun onStopTrackingTouch(sb: SeekBar) {}
    }

    private fun Long.toColor(): Int = toInt()

    // ================================================================ import flows

    private fun pickFolder() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or
            Intent.FLAG_GRANT_PREFIX_URI_PERMISSION)
        try {
            startActivityForResult(intent, REQ_TREE)
        } catch (e: Exception) {
            Toast.makeText(this, "Не удалось открыть выбор папки", Toast.LENGTH_SHORT).show()
        }
    }

    private fun pickZip() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
        intent.type = "application/zip"
        intent.addCategory(Intent.CATEGORY_OPENABLE)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        try {
            startActivityForResult(intent, REQ_ZIP)
        } catch (e: Exception) {
            Toast.makeText(this, "Не удалось открыть выбор ZIP", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        val uri = data?.data ?: return
        if (resultCode != RESULT_OK) return
        try {
            contentResolver.takePersistableUriPermission(uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (_: Exception) {
        }
        when (requestCode) {
            REQ_TREE -> runImport(uri, isZip = false)
            REQ_ZIP -> runImport(uri, isZip = true)
        }
    }

    /** Handles ACTION_VIEW / ACTION_SEND (user opened a game zip with this app). */
    private fun handleIncomingIntent(intent: Intent?) {
        if (intent == null || intent.data == null) return
        val action = intent.action
        if (action == Intent.ACTION_VIEW || action == Intent.ACTION_SEND) {
            val uri = intent.data
            if (uri != null && intent.type != null && intent.type!!.contains("zip")) {
                prefs.lastImportUri = uri.toString()
                prefs.lastImportIsZip = true
                runImport(uri, isZip = true)
            }
        }
    }

    private fun runImport(uri: Uri, isZip: Boolean) {
        prefs.lastImportUri = uri.toString()
        prefs.lastImportIsZip = isZip
        repeatImportBtn.isEnabled = true
        progressBar.progress = 0
        progressText.text = if (isZip) "Распаковка ZIP…" else "Копирование папки…"
        val cb = object : GameFileImporter.Callback {
            override fun onProgress(message: String, fraction: Float) {
                progressText.text = message
                progressBar.progress = (fraction * 1000).toInt()
            }
            override fun onDone(copiedFiles: Int, copiedBytes: Long, message: String) {
                progressBar.progress = 1000
                progressText.text = message
                statusText.text = GamePaths.gameDataSummary(this@LauncherActivity)
                Toast.makeText(this@LauncherActivity, message, Toast.LENGTH_LONG).show()
            }
            override fun onError(message: String) {
                progressText.text = "Ошибка"
                AlertDialog.Builder(this@LauncherActivity)
                    .setTitle("Импорт не удался")
                    .setMessage(message)
                    .setPositiveButton("OK", null)
                    .show()
            }
        }
        if (isZip) importer.importFromZip(uri, cb) else importer.importFromTree(uri, cb)
    }
}