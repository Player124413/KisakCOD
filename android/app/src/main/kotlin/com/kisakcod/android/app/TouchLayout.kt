package com.kisakcod.android.app

/**
 * Data model for the on-screen touch layout.
 *
 * Everything is stored in a normalized coordinate space (0..1000) so a layout
 * looks identical on any resolution and orientation. One unit == screenMin/1000
 * where screenMin is the shorter side of the game surface.
 *
 * The file format is a trivial "key=value" text (no third-party JSON), so it is
 * easy to hand-edit and stable across versions.
 */

enum class CtrlKind { BUTTON, STICK }

enum class TouchAction(
    val id: String,
    val label: String,
    val kind: CtrlKind,
    val momentary: Boolean, // momentary = press-and-release in a few ms, like a key tap
    val ui: String          // short icon/label drawn on the control
) {
    STICK_LEFT("stick_left", "Джойстик: ходьба", CtrlKind.STICK, false, "◉"),
    STICK_RIGHT("stick_right", "Джойстик: камера", CtrlKind.STICK, false, "◎"),
    FIRE("fire", "Огонь", CtrlKind.BUTTON, false, "FIRE"),
    ADS("ads", "Прицел (ADS)", CtrlKind.BUTTON, false, "ADS"),
    JUMP("jump", "Прыжок", CtrlKind.BUTTON, true, "JUMP"),
    SPRINT("sprint", "Спринт", CtrlKind.BUTTON, false, "SPRINT"),
    CROUCH("crouch", "Присесть / лечь", CtrlKind.BUTTON, false, "CROUCH"),
    PRONE("prone", "Лечь", CtrlKind.BUTTON, false, "PRONE"),
    RELOAD("reload", "Перезарядка", CtrlKind.BUTTON, true, "R"),
    MELEE("melee", "Удар", CtrlKind.BUTTON, true, "MELEE"),
    USE("use", "Использовать", CtrlKind.BUTTON, true, "USE"),
    NADE("nade", "Граната", CtrlKind.BUTTON, true, "NADE"),
    SCOREBOARD("scoreboard", "Табло", CtrlKind.BUTTON, false, "SCORE"),
    SWITCH_1("weapon1", "Оружие 1", CtrlKind.BUTTON, true, "1"),
    SWITCH_2("weapon2", "Оружие 2", CtrlKind.BUTTON, true, "2"),
    SWITCH_3("weapon3", "Оружие 3", CtrlKind.BUTTON, true, "3"),
    PAUSE("pause", "Пауза / меню", CtrlKind.BUTTON, true, "MENU");

    companion object {
        fun byId(id: String): TouchAction? = values().firstOrNull { it.id == id }
    }
}

data class CtrlEntry(
    var action: TouchAction,
    var cx: Int = 500,      // center, normalized 0..1000 (x)
    var cy: Int = 500,      // center, normalized 0..1000 (y)
    var size: Int = 140,    // diameter, normalized 0..1000
    var visible: Boolean = true,
    var opacity: Int = 90   // percent
) {
    fun radius(u: Float) = (size / 1000f) * u / 2f
}

class LayoutConfig {
    val entries: MutableList<CtrlEntry> = mutableListOf()
    var touchEnabled: Boolean = true        // master switch: touch controls on/off
    var snapToGrid: Boolean = true          // editor snaps to 1% grid
    var layoutName: String = "default"

    fun entry(action: TouchAction): CtrlEntry? = entries.firstOrNull { it.action == action }

    fun copyFrom(other: LayoutConfig) {
        entries.clear()
        other.entries.forEach {
            entries.add(CtrlEntry(it.action, it.cx, it.cy, it.size, it.visible, it.opacity))
        }
        touchEnabled = other.touchEnabled
        snapToGrid = other.snapToGrid
        layoutName = other.layoutName
    }

    /** Serialize to the stable text format. */
    fun serialize(): String {
        val sb = StringBuilder()
        sb.append("v=1\n")
        sb.append("layout=").append(layoutName).append('\n')
        sb.append("touch.enabled=").append(if (touchEnabled) 1 else 0).append('\n')
        sb.append("snap=").append(if (snapToGrid) 1 else 0).append('\n')
        for (e in entries) {
            sb.append(e.action.id)
                .append('=')
                .append(e.cx).append(',')
                .append(e.cy).append(',')
                .append(e.size).append(',')
                .append(if (e.visible) 1 else 0).append(',')
                .append(e.opacity).append('\n')
        }
        return sb.toString()
    }

    companion object {
        fun defaults(): LayoutConfig {
            val cfg = LayoutConfig()
            // Defaults tuned for a landscape phone screen.
            val d = arrayOf(
                arrayOf("stick_left", 95, 740, 280, 1, 85),
                arrayOf("stick_right", 905, 740, 280, 1, 85),
                arrayOf("fire", 830, 620, 180, 1, 90),
                arrayOf("ads", 700, 640, 150, 1, 90),
                arrayOf("jump", 705, 470, 120, 1, 85),
                arrayOf("crouch", 585, 690, 120, 1, 85),
                arrayOf("sprint", 240, 640, 120, 1, 80),
                arrayOf("reload", 545, 470, 110, 1, 80),
                arrayOf("melee", 470, 640, 110, 1, 80),
                arrayOf("use", 630, 430, 100, 1, 75),
                arrayOf("nade", 370, 690, 110, 1, 80),
                arrayOf("scoreboard", 235, 95, 105, 1, 75),
                arrayOf("weapon1", 640, 95, 95, 1, 80),
                arrayOf("weapon2", 735, 95, 95, 1, 80),
                arrayOf("weapon3", 830, 95, 95, 1, 80),
                arrayOf("pause", 60, 70, 90, 1, 80)
            )
            for (row in d) {
                val a = TouchAction.byId(row[0] as String) ?: continue
                cfg.entries.add(
                    CtrlEntry(a, row[1] as Int, row[2] as Int, row[3] as Int, row[4] == 1, row[5] as Int)
                )
            }
            return cfg
        }

        /** Parse the stable text format; falls back to defaults on malformed lines. */
        fun parse(text: String): LayoutConfig {
            val cfg = defaults()
            val seen = mutableSetOf<String>()
            var haveTouch = false
            var haveSnap = false
            var haveLayout = false
            for (raw in text.lineSequence()) {
                val line = raw.trim()
                if (line.isEmpty() || line.startsWith("#")) continue
                val eq = line.indexOf('=')
                if (eq <= 0) continue
                val key = line.substring(0, eq).trim()
                val value = line.substring(eq + 1).trim()
                when {
                    key == "touch.enabled" && !haveTouch -> {
                        cfg.touchEnabled = value.toIntOrNull()?.let { it != 0 } ?: cfg.touchEnabled
                        haveTouch = true
                    }
                    key == "snap" && !haveSnap -> {
                        cfg.snapToGrid = value.toIntOrNull()?.let { it != 0 } ?: cfg.snapToGrid
                        haveSnap = true
                    }
                    key == "layout" && !haveLayout -> {
                        cfg.layoutName = value
                        haveLayout = true
                    }
                    else -> {
                        val action = TouchAction.byId(key) ?: continue
                        if (action in seen) continue
                        seen.add(action)
                        val parts = value.split(',')
                        if (parts.size != 5) continue
                        val x = parts[0].trim().toIntOrNull()
                        val y = parts[1].trim().toIntOrNull()
                        val s = parts[2].trim().toIntOrNull()
                        val v = parts[3].trim().toIntOrNull()
                        val o = parts[4].trim().toIntOrNull()
                        if (x == null || y == null || s == null || v == null || o == null) continue
                        val e = cfg.entry(action)
                        if (e != null) {
                            e.cx = x.coerceIn(0, 1000)
                            e.cy = y.coerceIn(0, 1000)
                            e.size = s.coerceIn(40, 500)
                            e.visible = v != 0
                            e.opacity = o.coerceIn(20, 100)
                        }
                    }
                }
            }
            return cfg
        }
    }
}

/** Pure helpers used by the editor — kept here so they are trivially unit-testable. */
object LayoutMath {

    fun clamp(v: Int, lo: Int, hi: Int): Int = if (v < lo) lo else if (v > hi) hi else v

    /** Snap a coordinate to the layout grid (1% steps). */
    fun snap(v: Int): Int = ((v + 5) / 10) * 10

    /** Hit-test: is point (px,py) inside the control (in normalized space)? */
    fun hits(e: CtrlEntry, px: Int, py: Int, u: Float): Boolean {
        if (!e.visible) return false
        val r = e.radius(u)
        val dx = px - e.cx
        val dy = py - e.cy
        val ext = r + 18f // generous touch target
        return kotlin.math.abs(dx) <= ext && kotlin.math.abs(dy) <= ext
    }

    /** Is (px,py) on the resize handle of the control (its bottom-right corner)? */
    fun hitsResize(e: CtrlEntry, px: Int, py: Int, u: Float): Boolean {
        if (!e.visible) return false
        val r = e.radius(u)
        val hx = e.cx + r
        val hy = e.cy + r
        val tol = 34f
        return kotlin.math.abs(px - hx) <= tol && kotlin.math.abs(py - hy) <= tol
    }
}