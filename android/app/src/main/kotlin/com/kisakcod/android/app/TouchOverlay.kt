package com.kisakcod.android.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sqrt

/**
 * On-screen touch controls with a full-fidelity EDIT mode:
 *  - drag to MOVE a control
 *  - bottom-right handle to RESIZE it
 *  - eye button to toggle VISIBILITY
 *  - action button to REMAP what a control does
 *  - master switch to turn the whole touch input ON/OFF (e.g. play with a gamepad)
 *  - snap-to-grid, per-control opacity, reset to defaults, profiles
 *
 * All game-facing input is delivered to native as semantic actions
 * (JniBridge.nativeButton / nativeStick); the mapping to engine keys lives
 * natively, so remapping a control here needs no engine knowledge.
 */
class TouchOverlay(context: Context, private val prefs: UiPrefs) : View(context) {

    private val layout = prefs.layout()
    private val handler = Handler(Looper.getMainLooper())

    private var editMode = false
    private var selected: CtrlEntry? = null
    private var toolbar: Toolbar? = null

    private val grabs = HashMap<Int, Grab>()

    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pStroke = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pSelect = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pToolbarFill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pDim = Paint(Paint.ANTI_ALIAS_FLAG)

    private var u = 1f // px per layout unit

    /** hold (ms) after which the crouch button promotes to prone (mirrors cl_stanceHoldTime) */
    private val crouchHoldToProneMs = 350L

    init {
        pStroke.style = Paint.Style.STROKE
        pText.textAlign = Paint.Align.CENTER
        pText.textSize = 30f
    }

    // ---------------------------------------------------------------- public API

    fun reloadLayout() {
        layout.copyFrom(prefs.layout())
        selected = null
        invalidate()
    }

    fun saveLayout() {
        prefs.saveLayout(layout)
    }

    fun isTouchEnabled(): Boolean = layout.touchEnabled

    fun isEditing(): Boolean = editMode

    // ---------------------------------------------------------------- drawing

    private fun prepare() {
        u = min(width.toFloat(), height.toFloat()) / 1000f
        toolbar = if (editMode) Toolbar(width.toFloat(), height.toFloat()) else null
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        prepare()

        if (editMode) {
            pDim.color = Color.argb(90, 0, 0, 0)
            canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), pDim)
        }

        for (e in layout.entries) {
            if (!e.visible && !editMode) continue
            if (e.action.kind == CtrlKind.STICK) drawStick(canvas, e) else drawButton(canvas, e)
        }

        if (!editMode) drawEditChip(canvas)

        if (editMode) toolbar?.draw(canvas, this)

        if (editMode && selected != null) drawSelection(canvas, selected!!)

        if (!layout.touchEnabled && !editMode) {
            pText.textSize = 34f
            pText.color = Color.argb(230, 255, 180, 80)
            pText.textAlign = Paint.Align.CENTER
            canvas.drawText("СЕНСОРНОЕ УПРАВЛЕНИЕ ВЫКЛЮЧЕНО — используйте геймпад",
                width / 2f, height * 0.28f, pText)
            pText.textAlign = Paint.Align.CENTER
        }
    }

    private fun controlColor(e: CtrlEntry): Int {
        val a = ((e.opacity * 0.85f).coerceIn(0f, 100f) * 255 / 100).toInt()
        return when (e.action) {
            TouchAction.FIRE -> Color.argb(a, 200, 90, 60)
            TouchAction.ADS -> Color.argb(a, 120, 160, 200)
            else -> Color.argb(a, 70, 110, 90)
        }
    }

    private fun drawButton(canvas: Canvas, e: CtrlEntry) {
        val cx = e.cx * u
        val cy = e.cy * u
        val r = e.radius(u)
        pFill.color = controlColor(e)
        pStroke.color = Color.argb(180, 235, 235, 210)
        pStroke.strokeWidth = if (editMode && selected === e) 5f else 3f

        val rect = RectF(cx - r, cy - r, cx + r, cy + r)
        canvas.drawRoundRect(rect, r * 0.28f, r * 0.28f, pFill)
        canvas.drawRoundRect(rect, r * 0.28f, r * 0.28f, pStroke)

        pText.color = Color.argb(235, 255, 255, 255)
        val fs = (r * 0.62f).coerceIn(20f, 46f)
        pText.textSize = fs
        canvas.drawText(e.action.ui, cx, cy + fs * 0.34f, pText)
    }

    private fun drawStick(canvas: Canvas, e: CtrlEntry) {
        val cx = e.cx * u
        val cy = e.cy * u
        val r = e.radius(u)
        val grab = grabs.values.firstOrNull { g -> g.type == GrabType.STICK && g.entry === e }
        val knobX = grab?.knobX ?: cx
        val knobY = grab?.knobY ?: cy

        val c = controlColor(e)
        pFill.color = Color.argb(60, Color.red(c), Color.green(c), Color.blue(c))
        canvas.drawCircle(cx, cy, r, pFill)
        pStroke.color = Color.argb(160, 235, 235, 210)
        pStroke.strokeWidth = 3f
        canvas.drawCircle(cx, cy, r, pStroke)

        pFill.color = Color.argb(150, 255, 255, 255)
        canvas.drawCircle(knobX, knobY, r * 0.42f, pFill)
    }

    private fun drawSelection(canvas: Canvas, e: CtrlEntry) {
        val cx = e.cx * u
        val cy = e.cy * u
        val r = e.radius(u)
        pSelect.color = Color.argb(255, 255, 176, 60)
        pSelect.style = Paint.Style.STROKE
        pSelect.strokeWidth = 6f
        canvas.drawRect(cx - r - 8f, cy - r - 8f, cx + r + 8f, cy + r + 8f, pSelect)
        pSelect.style = Paint.Style.FILL
        canvas.drawCircle(cx + r + 6f, cy + r + 6f, 22f, pSelect)
        pText.color = Color.argb(255, 255, 230, 150)
        pText.textAlign = Paint.Align.LEFT
        pText.textSize = 24f
        canvas.drawText(e.action.label, cx + r + 16f, cy - r - 14f, pText)
        pText.textAlign = Paint.Align.CENTER
        pSelect.style = Paint.Style.STROKE
    }

    private fun drawEditChip(canvas: Canvas) {
        val cx = width * 0.5f
        val cy = min(width, height) * 0.16f
        val r = min(width, height) * 0.028f
        pFill.color = Color.argb(150, 40, 40, 40)
        canvas.drawCircle(cx, cy, r, pFill)
        pStroke.color = Color.argb(220, 255, 240, 200)
        pStroke.strokeWidth = 3f
        canvas.drawCircle(cx, cy, r, pStroke)
        pText.color = Color.WHITE
        pText.textSize = r * 0.8f
        canvas.drawText("EDIT", cx, cy + pText.textSize * 0.36f, pText)
    }

    private inner class Toolbar(w: Float, h: Float) {
        val barW: Float = w
        val hPx = min(barW, h) * 0.09f
        val items = mutableListOf<Pair<String, () -> Unit>>()
        val rects = mutableListOf<RectF>()

        init {
            val labels = arrayOf(
                "Готово",
                "Скрыть/Показать",
                "Действие",
                "Размер −",
                "Размер +",
                "Сетка",
                "Сенсор: " + (if (layout.touchEnabled) "ВКЛ" else "ВЫКЛ"),
                "Сброс"
            )
            val x0 = barW * 0.015f
            val bw = (barW - x0 * 2f) / labels.size
            val y0 = hPx * 0.12f
            for (i in labels.indices) {
                val action: () -> Unit = when (i) {
                    0 -> { exitEdit() }
                    1 -> { toggleVisibility() }
                    2 -> { remapAction() }
                    3 -> { nudgeSize(-1) }
                    4 -> { nudgeSize(1) }
                    5 -> { layout.snapToGrid = !layout.snapToGrid; invalidate() }
                    6 -> { toggleTouchEnabled() }
                    else -> { resetLayout() }
                }
                items.add(Pair(labels[i], action))
                rects.add(RectF(x0 + i * bw, y0, x0 + (i + 1) * bw, y0 + hPx))
            }
        }

        fun hit(px: Float, py: Float): Boolean {
            for (i in items.indices) {
                if (rects[i].contains(px, py)) {
                    items[i].second.invoke()
                    return true
                }
            }
            return false
        }

        fun draw(canvas: Canvas, overlay: View) {
            var i = 0
            for ((label, _) in items) {
                val r = rects[i]
                pToolbarFill.color = Color.argb(210, 28, 34, 30)
                canvas.drawRoundRect(r, 10f, 10f, pToolbarFill)
                if (i == 6) {
                    pStroke.color = if (layout.touchEnabled) Color.argb(255, 120, 200, 140)
                    else Color.argb(255, 220, 100, 80)
                    pStroke.strokeWidth = 3f
                    canvas.drawRoundRect(r, 10f, 10f, pStroke)
                }
                pText.color = Color.argb(235, 250, 250, 240)
                pText.textSize = (min(barW, overlay.height.toFloat()) * 0.026f).coerceIn(16f, 30f)
                canvas.drawText(label, r.centerX(), r.centerY() + pText.textSize * 0.34f, pText)
                i++
            }
        }
    }

    // ---------------------------------------------------------------- editor ops

    private fun exitEdit() {
        editMode = false
        selected = null
        toolbar = null
        saveLayout()
        JniBridge.nativeSetBool("touch_enabled", layout.touchEnabled)
        invalidate()
    }

    private fun toggleVisibility() {
        selected?.let { e ->
            e.visible = !e.visible
            invalidate()
        }
    }

    private fun remapAction() {
        selected?.let { e ->
            val all = TouchAction.values()
            val idx = all.indexOfFirst { it.id == e.action.id }
            val kind = e.action.kind
            var i = (idx + 1) % all.size
            while (i != idx && all[i].kind != kind) i = (i + 1) % all.size
            if (i != idx) {
                e.action = all[i]
                invalidate()
            }
        }
    }

    private fun nudgeSize(dir: Int) {
        selected?.let { e ->
            e.size = LayoutMath.clamp(e.size + dir * 20, 40, 500)
            invalidate()
        }
    }

    private fun toggleTouchEnabled() {
        layout.touchEnabled = !layout.touchEnabled
        toolbar = Toolbar(width.toFloat(), height.toFloat())
        JniBridge.nativeSetBool("touch_enabled", layout.touchEnabled)
        invalidate()
    }

    private fun resetLayout() {
        layout.copyFrom(LayoutConfig.defaults())
        selected = null
        invalidate()
    }

    // ---------------------------------------------------------------- touch

    private enum class GrabType { STICK, BUTTON, EDIT, EDITOR_MOVE, EDITOR_RESIZE }

    private class Grab(
        var type: GrabType,
        var entry: CtrlEntry?,
        var pointerId: Int,
        var startX: Float = 0f,
        var startY: Float = 0f,
        var grabDX: Float = 0f,   // px offset from control center at grab
        var grabDY: Float = 0f,
        var knobX: Float = 0f,    // stick knob px (absolute)
        var knobY: Float = 0f,
        var downT: Long = 0,
        var crouchPromoted: Boolean = false
    )

    private fun normX(px: Float) = LayoutMath.clamp((px / u).toInt(), 0, 1000)
    private fun normY(py: Float) = LayoutMath.clamp((py / u).toInt(), 0, 1000)

    override fun onTouchEvent(ev: MotionEvent): Boolean {
        // Haptic feedback on touch-down (view method, works on most devices)
        if (ev.actionMasked == MotionEvent.ACTION_DOWN ||
            ev.actionMasked == MotionEvent.ACTION_POINTER_DOWN) {
            try { performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY) } catch (_: Exception) {}
        }
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> onPointerDown(ev)
            MotionEvent.ACTION_MOVE -> onPointerMove(ev)
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> onPointerUp(ev)
            MotionEvent.ACTION_CANCEL -> {
                for ((_, g) in grabs) releaseGrab(g)
                grabs.clear()
                invalidate()
            }
            else -> {}
        }
        return true
    }

    private fun toggleEditMode() {
        editMode = !editMode
        if (editMode) {
            reloadLayout()
            selected = null
        } else {
            exitEdit()
        }
        invalidate()
    }

    private fun onPointerDown(ev: MotionEvent): Boolean {
        val idx = ev.actionIndex
        val id = ev.getPointerId(idx)
        val px = ev.getX(idx)
        val py = ev.getY(idx)

        if (!editMode) {
            val chipR = min(width.toFloat(), height.toFloat()) * 0.032f
            val chipCx = width * 0.5f
            val chipCy = min(width.toFloat(), height.toFloat()) * 0.16f
            if (abs(px - chipCx) <= chipR * 2.0f && abs(py - chipCy) <= chipR * 2.0f) {
                toggleEditMode()
                return true
            }
        }

        if (editMode) {
            if (py < toolbarH()) {
                toolbar?.hit(px, py)
                invalidate()
                return true
            }
            val top = layout.entries.lastOrNull { e -> LayoutMath.hitsResize(e, normX(px), normY(py), u) }
            if (top != null) {
                selected = top
                grabs[id] = Grab(GrabType.EDITOR_RESIZE, top, id, px, py)
            } else {
                val sel = layout.entries.lastOrNull { e -> LayoutMath.hits(e, normX(px), normY(py), u) }
                if (sel != null) {
                    selected = sel
                    grabs[id] = Grab(GrabType.EDITOR_MOVE, sel, id, px, py,
                        grabDX = px - sel.cx * u, grabDY = py - sel.cy * u)
                } else {
                    selected = null
                }
            }
            invalidate()
            return true
        }

        if (!layout.touchEnabled) return true

        var captured = false
        for (e in layout.entries.reversed()) {
            if (e.action.kind != CtrlKind.BUTTON || !e.visible) continue
            if (LayoutMath.hits(e, normX(px), normY(py), u)) {
                val g = Grab(GrabType.BUTTON, e, id, px, py)
                g.downT = SystemClock.uptimeMillis()
                grabs[id] = g
                onButtonDown(e, g)
                captured = true
                break
            }
        }
        if (captured) { invalidate(); return true }

        val side = if (px < width * 0.45f) 0 else 1
        val stickAction = if (side == 0) TouchAction.STICK_LEFT else TouchAction.STICK_RIGHT
        val entry = layout.entry(stickAction) ?: return true
        grabs[id] = Grab(GrabType.STICK, entry, id, px, py, knobX = px, knobY = py)
        invalidate()
        return true
    }

    private fun toolbarH(): Float =
        (min(width.toFloat(), height.toFloat()) * 0.09f * 1.35f)

    private fun onPointerMove(ev: MotionEvent): Boolean {
        val now = SystemClock.uptimeMillis()
        for (i in 0 until ev.pointerCount) {
            val id = ev.getPointerId(i)
            val g = grabs[id] ?: continue
            val px = ev.getX(i)
            val py = ev.getY(i)
            val dt = max(1L, now - g.downT)
            when (g.type) {
                GrabType.STICK -> {
                    val e = g.entry ?: continue
                    val r = e.radius(u)
                    var ox = px - g.startX
                    var oy = py - g.startY
                    val maxD = r * 0.9f
                    val len = sqrt(ox * ox + oy * oy)
                    if (len > maxD) {
                        ox = ox / len * maxD
                        oy = oy / len * maxD
                    }
                    g.knobX = g.startX + ox
                    g.knobY = g.startY + oy
                    val nx = if (maxD > 0) ox / maxD else 0f
                    val ny = if (maxD > 0) oy / maxD else 0f
                    val side = if (e.action == TouchAction.STICK_LEFT) 0 else 1
                    JniBridge.nativeStick(side, nx, ny, dt)
                }
                GrabType.BUTTON -> {
                    val e = g.entry ?: continue
                    if (e.action == TouchAction.CROUCH && !g.crouchPromoted &&
                        now - g.downT >= crouchHoldToProneMs) {
                        g.crouchPromoted = true
                        JniBridge.nativeButton(TouchAction.CROUCH.id, false)
                        JniBridge.nativeButton(TouchAction.PRONE.id, true)
                        invalidate()
                    }
                }
                GrabType.EDITOR_MOVE -> {
                    val e = g.entry ?: continue
                    var cx = normX(px - g.grabDX)
                    var cy = normY(py - g.grabDY)
                    if (layout.snapToGrid) {
                        cx = LayoutMath.snap(cx)
                        cy = LayoutMath.snap(cy)
                    }
                    e.cx = cx
                    e.cy = cy
                    invalidate()
                }
                GrabType.EDITOR_RESIZE -> {
                    val e = g.entry ?: continue
                    val cxPx = e.cx * u
                    val cyPx = e.cy * u
                    val dist = sqrt((px - cxPx) * (px - cxPx) + (py - cyPx) * (py - cyPx))
                    var s = ((dist * 2f) / u).toInt()
                    if (layout.snapToGrid) s = LayoutMath.snap(s)
                    e.size = LayoutMath.clamp(s, 40, 500)
                    invalidate()
                }
                else -> {}
            }
        }
        return true
    }

    private fun onPointerUp(ev: MotionEvent): Boolean {
        val idx = ev.actionIndex
        val id = ev.getPointerId(idx)
        val g = grabs.remove(id) ?: return true
        releaseGrab(g)
        invalidate()
        return true
    }

    private fun releaseGrab(g: Grab) {
        when (g.type) {
            GrabType.BUTTON -> {
                val e = g.entry ?: return
                if (e.action == TouchAction.CROUCH) {
                    JniBridge.nativeButton(TouchAction.CROUCH.id, false)
                    if (g.crouchPromoted) {
                        JniBridge.nativeButton(TouchAction.PRONE.id, false)
                    }
                } else if (e.action == TouchAction.ADS && prefs.adsToggle) {
                    // toggle mode: state persists until the next tap
                } else {
                    JniBridge.nativeButton(e.action.id, false)
                }
            }
            GrabType.STICK -> {
                val side = if (g.entry?.action == TouchAction.STICK_LEFT) 0 else 1
                JniBridge.nativeStick(side, 0f, 0f, 16L)
            }
            else -> {}
        }
    }

    private fun onButtonDown(e: CtrlEntry, g: Grab) {
        if (e.action == TouchAction.ADS && prefs.adsToggle) {
            // tap-to-toggle: first tap presses ADS, second tap releases it
            val want = !(adsLatches[e.action.id] ?: false)
            adsLatches[e.action.id] = want
            JniBridge.nativeButton(e.action.id, want)
            return
        }
        JniBridge.nativeButton(e.action.id, true)
        if (e.action.momentary) {
            val actionId = e.action.id
            handler.postDelayed({ JniBridge.nativeButton(actionId, false) }, 40L)
        }
    }

    private val adsLatches = HashMap<String, Boolean>()
}