package com.kisak.cod.input

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.os.SystemClock
import android.util.AttributeSet
import android.util.SparseArray
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot
import kotlin.math.min

/**
 * Virtual gamepad overlay for the KisakCOD Android port.
 *
 * Design notes — these are the things that actually bite in touch input:
 *
 * 1. Pointer bookkeeping is done by pointerId, NEVER by pointer index.
 *    Indices are reassigned when a finger lifts, so index-keyed state is the
 *    classic cause of buttons sticking down or sticks freezing.
 *
 * 2. Every exit path (ACTION_CANCEL, ACTION_UP, detach, visibility loss)
 *    funnels through releaseAll(). A stuck "+attack" is the single most
 *    damaging bug a touch layer can have, so release is idempotent and
 *    unconditional.
 *
 * 3. Digital movement (which is all the engine accepts today, since
 *    CL_GamepadMove is commented out in src/client/cl_input.cpp) is derived
 *    from the analogue stick with a dead zone and hysteresis, so the emitted
 *    commands cannot chatter at the threshold.
 *
 * 4. Hit testing is priority ordered: buttons, then sticks, then the look pad.
 *    Without this, a button drawn over the look zone would never fire.
 */
class TouchControlsView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : View(context, attrs, defStyleAttr) {

    interface Listener {
        /** Run an engine console command, e.g. "+attack". */
        fun onCommand(command: String)
        /** Look delta in engine mouse units; applied once per frame. */
        fun onLookDelta(dx: Float, dy: Float)
        /** Analogue stick deflection in [-1,1]; reserved for CL_GamepadMove. */
        fun onMoveAxis(x: Float, y: Float)
    }

    var listener: Listener? = null
    var layout: TouchLayout = TouchLayout.defaultLayout()
        set(value) {
            field = value
            rebuildRuntime()
        }

    /** When true, controls can be dragged to reposition; input is suppressed. */
    var editMode: Boolean = false
        set(value) {
            if (field == value) return
            field = value
            if (value) releaseAll()
            invalidate()
        }

    /** Element currently selected in edit mode, for property editing. */
    var selectedId: String? = null
        set(value) { field = value; invalidate() }

    var onElementMoved: ((ControlElement) -> Unit)? = null

    // ------------------------------------------------------------ runtime state

    private class StickRuntime {
        var pointerId: Int = -1
        var originX = 0f
        var originY = 0f
        var knobX = 0f
        var knobY = 0f
        var axisX = 0f
        var axisY = 0f
    }

    private class PointerBinding(
        val pointerId: Int,
        val element: ControlElement,
        val startX: Float,
        val startY: Float,
        val downTimeMs: Long,
        var lastX: Float,
        var lastY: Float,
    )

    private val sticks = HashMap<String, StickRuntime>()
    private val pointers = SparseArray<PointerBinding>()

    /** Currently-emitted movement directions: forward, back, left, right. */
    private val moveDown = BooleanArray(4)
    private const val IDX_FORWARD = 0
    private const val IDX_BACK = 1
    private const val IDX_LEFT = 2
    private const val IDX_RIGHT = 3

    /** Bitmask of GameAction ordinals currently held, used for the ADS scale. */
    private val heldActions = HashSet<GameAction>()

    private val zoneRect = RectF()

    init { rebuildRuntime() }

    private fun rebuildRuntime() {
        sticks.clear()
        layout.elements.filter { it.kind == ElementKind.STICK }.forEach { e ->
            sticks[e.id] = StickRuntime()
        }
        invalidate()
    }

    // ------------------------------------------------------------ geometry

    private fun unit(): Float = min(width, height).toFloat().coerceAtLeast(1f)

    private fun radiusPx(e: ControlElement): Float = e.radius * unit()

    private fun centerX(e: ControlElement): Float = e.cx * width
    private fun centerY(e: ControlElement): Float = e.cy * height

    private fun zonePx(e: ControlElement): RectF = zoneRect.apply {
        set(e.zone.left * width, e.zone.top * height, e.zone.right * width, e.zone.bottom * height)
    }

    // ------------------------------------------------------------ touch

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                val i = event.actionIndex
                claim(event.getPointerId(i), event.getX(i), event.getY(i))
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                if (editMode) {
                    if (event.pointerCount >= 1) {
                        val i = 0
                        updateEditDrag(event.getX(i), event.getY(i))
                    }
                    return true
                }
                // Iterate by index (that is the only way to read coordinates)
                // but look state up by id.
                for (i in 0 until event.pointerCount) {
                    val b = pointers.get(event.getPointerId(i)) ?: continue
                    updatePointer(b, event.getX(i), event.getY(i))
                }
                return true
            }

            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                if (editMode) { commitEditDrag(); return true }
                val i = event.actionIndex
                release(event.getPointerId(i))
                return true
            }

            MotionEvent.ACTION_CANCEL -> {
                releaseAll()
                return true
            }
        }
        return false
    }

    private fun claim(pointerId: Int, x: Float, y: Float) {
        if (width == 0 || height == 0) return
        // A pointer that is already bound cannot claim again (guards duplicate
        // DOWN events some devices emit after a configuration change).
        if (pointers.get(pointerId) != null) return

        val element = hitTest(x, y) ?: return
        val binding = PointerBinding(
            pointerId = pointerId,
            element = element,
            startX = x, startY = y,
            downTimeMs = SystemClock.uptimeMillis(),
            lastX = x, lastY = y,
        )
        pointers.put(pointerId, binding)

        when (element.kind) {
            ElementKind.STICK -> {
                val st = sticks[element.id] ?: return
                st.pointerId = pointerId
                st.originX = if (element.floating) x else centerX(element)
                st.originY = if (element.floating) y else centerY(element)
                st.knobX = st.originX
                st.knobY = st.originY
                st.axisX = 0f
                st.axisY = 0f
            }
            ElementKind.BUTTON -> press(element)
            ElementKind.LOOK -> Unit // no press edge
        }
        invalidate()
    }

    private fun updatePointer(b: PointerBinding, x: Float, y: Float) {
        when (b.element.kind) {
            ElementKind.STICK -> updateStick(b.element, x, y)
            ElementKind.LOOK -> {
                val rawDx = x - b.lastX
                val rawDy = y - b.lastY
                b.lastX = x
                b.lastY = y
                emitLook(rawDx, rawDy)
            }
            ElementKind.BUTTON -> Unit
        }
    }

    private fun release(pointerId: Int) {
        val b = pointers.get(pointerId) ?: return
        pointers.remove(pointerId)
        when (b.element.kind) {
            ElementKind.STICK -> resetStick(b.element)
            ElementKind.BUTTON -> releaseButton(b.element)
            ElementKind.LOOK -> Unit
        }
        invalidate()
    }

    /**
     * Idempotent full reset. Called on cancel, detach and visibility loss so a
     * held action can never survive a context switch.
     */
    fun releaseAll() {
        if (pointers.size() == 0 && !moveDown.any { it } && heldActions.isEmpty()) return
        // Snapshot the keys first. SparseArray.remove() only marks a slot
        // DELETED, and get() returns null for deleted slots, so releasing while
        // iterating by index would silently skip every element's cleanup and
        // leave buttons held down.
        val ids = (0 until pointers.size()).map { pointers.keyAt(it) }
        ids.forEach { release(it) }
        pointers.clear()
        sticks.values.forEach { it.pointerId = -1; it.axisX = 0f; it.axisY = 0f }
        heldActions.forEach { act -> act.release?.let { listener?.onCommand(it) } }
        heldActions.clear()
        setMove(IDX_FORWARD, false)
        setMove(IDX_BACK, false)
        setMove(IDX_LEFT, false)
        setMove(IDX_RIGHT, false)
        listener?.onMoveAxis(0f, 0f)
        invalidate()
    }

    override fun onDetachedFromWindow() {
        releaseAll()
        super.onDetachedFromWindow()
    }

    override fun onWindowVisibilityChanged(visibility: Int) {
        super.onWindowVisibilityChanged(visibility)
        if (visibility != VISIBLE) releaseAll()
    }

    // ------------------------------------------------------------ hit testing

    private fun hitTest(x: Float, y: Float): ControlElement? {
        val nx = if (width > 0) x / width else 0f
        val ny = if (height > 0) y / height else 0f

        // 1. Buttons first: they are the smallest targets and must win.
        for (e in layout.elements) {
            if (e.kind != ElementKind.BUTTON || !e.visible) continue
            // Slight forgiveness: thumbs are not precise instruments.
            val r = radiusPx(e) * 1.15f
            if (hypot(x - centerX(e), y - centerY(e)) <= r) return e
        }
        // 2. Sticks: floating sticks claim their whole zone.
        for (e in layout.elements) {
            if (e.kind != ElementKind.STICK || !e.visible) continue
            if (e.floating) {
                if (zonePx(e).contains(x, y)) return e
            } else {
                if (hypot(x - centerX(e), y - centerY(e)) <= radiusPx(e) * 1.5f) return e
            }
        }
        // 3. Look pad: whatever is left inside its zone.
        for (e in layout.elements) {
            if (e.kind != ElementKind.LOOK || !e.visible) continue
            if (e.zone.contains(nx, ny)) return e
        }
        return null
    }

    // ------------------------------------------------------------ stick

    private fun updateStick(e: ControlElement, x: Float, y: Float) {
        val st = sticks[e.id] ?: return
        val maxR = radiusPx(e)
        var dx = x - st.originX
        var dy = y - st.originY
        val dist = hypot(dx, dy)
        if (dist > maxR) {
            // Clamp the knob to the ring.
            val k = maxR / dist
            dx *= k
            dy *= k
            // A floating stick also slides its origin so the thumb never runs
            // out of ring — this is what makes long pushes comfortable.
            if (e.floating) {
                st.originX = x - dx
                st.originY = y - dy
            }
        }
        st.knobX = st.originX + dx
        st.knobY = st.originY + dy

        val feel = layout.feel
        var ax = if (maxR > 0f) dx / maxR else 0f
        var ay = if (maxR > 0f) dy / maxR else 0f

        // Radial dead zone, then rescale so control stays continuous across it.
        val mag = hypot(ax, ay)
        if (mag <= feel.stickDeadZone) {
            ax = 0f
            ay = 0f
        } else {
            val sat = feel.stickSaturation.coerceIn(0.05f, 1f)
            val scaled = ((mag - feel.stickDeadZone) / (1f - feel.stickDeadZone)).coerceIn(0f, 1f)
            val out = (scaled / sat).coerceAtMost(1f)
            if (mag > 0f) {
                ax = ax / mag * out
                ay = ay / mag * out
            }
        }
        st.axisX = ax
        st.axisY = ay

        listener?.onMoveAxis(ax, ay)
        updateMovement(ay, ax)
        invalidate()
    }

    private fun resetStick(e: ControlElement) {
        val st = sticks[e.id] ?: return
        st.pointerId = -1
        st.axisX = 0f
        st.axisY = 0f
        st.knobX = 0f
        st.knobY = 0f
        listener?.onMoveAxis(0f, 0f)
        updateMovement(0f, 0f)
    }

    /**
     * Convert analogue stick deflection into the four digital movement commands.
     * Hysteresis stops the commands from chattering when the thumb sits exactly
     * on the threshold.
     */
    private fun updateMovement(axisForward: Float, axisRight: Float) {
        val feel = layout.feel
        val t = feel.moveThreshold.coerceIn(0f, 1f)
        val h = feel.moveHysteresis.coerceIn(0f, t)

        val (fwd, back) = axisEdges(axisForward, t, h, moveDown[IDX_FORWARD], moveDown[IDX_BACK])
        val (left, right) = axisEdges(axisRight, t, h, moveDown[IDX_LEFT], moveDown[IDX_RIGHT])

        setMove(IDX_FORWARD, fwd)
        setMove(IDX_BACK, back)
        setMove(IDX_LEFT, left)
        setMove(IDX_RIGHT, right)
    }

    private fun axisEdges(
        v: Float, threshold: Float, hyst: Float, negActive: Boolean, posActive: Boolean
    ): Pair<Boolean, Boolean> {
        val negThr = if (negActive) (threshold - hyst) else threshold
        val posThr = if (posActive) (threshold - hyst) else threshold
        return (v <= -negThr) to (v >= posThr)
    }

    private fun setMove(idx: Int, down: Boolean) {
        if (moveDown[idx] == down) return
        moveDown[idx] = down
        val cmd = when (idx) {
            IDX_FORWARD -> if (down) "+forward" else "-forward"
            IDX_BACK -> if (down) "+back" else "-back"
            IDX_LEFT -> if (down) "+moveleft" else "-moveleft"
            else -> if (down) "+moveright" else "-moveright"
        }
        listener?.onCommand(cmd)
    }

    // ------------------------------------------------------------ buttons

    private fun press(e: ControlElement) {
        val act = e.action
        val pressCmd = if (act == GameAction.CUSTOM) e.customPress else act.press
        if (pressCmd.isNullOrBlank()) return

        if (e.mode == ActionMode.TOGGLE) {
            listener?.onCommand(pressCmd)
            return
        }
        listener?.onCommand(pressCmd)
        heldActions.add(act)
        invalidate()
    }

    private fun releaseButton(e: ControlElement) {
        val act = e.action
        if (e.mode == ActionMode.TOGGLE) return // toggles manage their own state
        val releaseCmd = if (act == GameAction.CUSTOM) e.customRelease else act.release
        releaseCmd?.let { listener?.onCommand(it) }
        heldActions.remove(act)
        invalidate()
    }

    // ------------------------------------------------------------ look

    private fun emitLook(rawDx: Float, rawDy: Float) {
        if (rawDx == 0f && rawDy == 0f) return
        val feel = layout.feel
        // Normalise by view size so sensitivity is resolution independent,
        // then convert to engine mouse units.
        val base = 1000f
        var sx = feel.lookSensitivity * base / width.toFloat().coerceAtLeast(1f)
        var sy = feel.lookSensitivity * base / height.toFloat().coerceAtLeast(1f)
        if (heldActions.contains(GameAction.ADS)) {
            sx *= feel.adsSensitivityScale
            sy *= feel.adsSensitivityScale
        }
        val dyOut = if (feel.invertY) rawDy else -rawDy
        listener?.onLookDelta(rawDx * sx, dyOut * sy)
    }

    // ------------------------------------------------------------ edit mode

    private var dragElement: ControlElement? = null

    private fun updateEditDrag(x: Float, y: Float) {
        val e = dragElement ?: run {
            val found = layout.elements.firstOrNull { el ->
                el.kind != ElementKind.LOOK &&
                    hypot(x - centerX(el), y - centerY(el)) <= radiusPx(el) * 1.3f
            }
            dragElement = found
            selectedId = found?.id
            found
        } ?: return
        val mutable = e
        // ControlElement is immutable, so replace it in the list.
        val idx = layout.elements.indexOfFirst { it.id == e.id }
        if (idx >= 0) {
            val nx = (x / width).coerceIn(0f, 1f)
            val ny = (y / height).coerceIn(0f, 1f)
            val updated = mutable.copy(cx = nx, cy = ny)
            layout = layout.copy(elements = layout.elements.toMutableList().apply { this[idx] = updated })
            dragElement = updated
            onElementMoved?.invoke(updated)
        }
    }

    private fun commitEditDrag() {
        dragElement = null
    }

    // ------------------------------------------------------------ drawing

    private val paintBase = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        setARGB(90, 255, 255, 255)
    }
    private val paintKnob = Paint(Paint.ANTI_ALIAS_FLAG).apply { setARGB(150, 255, 255, 255) }
    private val paintKnobActive = Paint(Paint.ANTI_ALIAS_FLAG).apply { setARGB(210, 120, 200, 255) }
    private val paintButton = Paint(Paint.ANTI_ALIAS_FLAG).apply { setARGB(70, 255, 255, 255) }
    private val paintButtonHeld = Paint(Paint.ANTI_ALIAS_FLAG).apply { setARGB(160, 255, 160, 60) }
    private val paintText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        setARGB(220, 255, 255, 255)
        textAlign = Paint.Align.CENTER
        textSize = 26f
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (width == 0 || height == 0) return

        paintText.textSize = unit() * 0.022f

        for (e in layout.elements) {
            if (!e.visible && !editMode) continue
            when (e.kind) {
                ElementKind.STICK -> drawStick(canvas, e)
                ElementKind.BUTTON -> drawButton(canvas, e)
                ElementKind.LOOK -> if (editMode) drawZone(canvas, e)
            }
        }
    }

    private fun drawStick(canvas: Canvas, e: ControlElement) {
        val st = sticks[e.id] ?: return
        val r = radiusPx(e)
        val active = st.pointerId != -1
        val cx = if (active || !e.floating) st.originX else centerX(e)
        val cy = if (active || !e.floating) st.originY else centerY(e)
        val alpha = if (active) 200 else 90
        paintBase.alpha = alpha
        canvas.drawCircle(cx, cy, r, paintBase)
        val kx = if (active) st.knobX else cx
        val ky = if (active) st.knobY else cy
        canvas.drawCircle(kx, ky, r * 0.42f, if (active) paintKnobActive else paintKnob)
    }

    private fun drawButton(canvas: Canvas, e: ControlElement) {
        val r = radiusPx(e)
        val held = heldActions.contains(e.action) && e.mode == ActionMode.HOLD
        canvas.drawCircle(centerX(e), centerY(e), r, if (held) paintButtonHeld else paintButton)
        val text = e.label ?: e.action.label
        val fm = paintText.fontMetrics
        val baseline = centerY(e) - (fm.ascent + fm.descent) / 2f
        canvas.drawText(text, centerX(e), baseline, paintText)
    }

    private fun drawZone(canvas: Canvas, e: ControlElement) {
        val z = zonePx(e)
        canvas.drawRect(z, paintBase)
    }

    // ------------------------------------------------------------ misc

    @SuppressLint("ClickableViewAccessibility")
    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    /** Called by the host Activity when the game loses focus (onPause). */
    fun onHostPause() = releaseAll()
}
