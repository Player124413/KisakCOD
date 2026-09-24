package com.kisak.cod.input

import org.json.JSONArray
import org.json.JSONObject

/**
 * Geometry is stored in NORMALISED coordinates so a layout survives any screen
 * size, aspect ratio, cutout or rotation without drifting.
 *
 * - Positions (cx, cy) are fractions of the view width/height in [0,1].
 * - Sizes (radius, halfW, halfH) are fractions of the SMALLER view dimension,
 *   so a control is the same physical size on a tall phone and a wide tablet.
 */
enum class ElementKind { STICK, BUTTON, LOOK }

data class ControlElement(
    val id: String,
    val action: GameAction,
    val kind: ElementKind,
    val cx: Float = 0f,
    val cy: Float = 0f,
    /** For STICK/BUTTON: radius as a fraction of the smaller view dimension. */
    val radius: Float = 0.06f,
    /** For LOOK: the zone, in normalised view coordinates. */
    val zone: Rect4 = Rect4(0.5f, 0f, 1f, 1f),
    val mode: ActionMode = action.defaultMode,
    /** Only used when action == GameAction.CUSTOM. */
    val customPress: String? = null,
    val customRelease: String? = null,
    val label: String? = null,
    /**
     * A floating stick re-centres itself wherever the finger lands inside its
     * zone. This is what modern mobile shooters do and it removes the
     * "I can't find the stick without looking" problem entirely.
     */
    val floating: Boolean = true,
    var visible: Boolean = true,
)

data class Rect4(val left: Float, val top: Float, val right: Float, val bottom: Float) {
    fun contains(x: Float, y: Float): Boolean = x >= left && x <= right && y >= top && y <= bottom
}

/** Sensitivity / feel tuning. All persisted. */
data class FeelConfig(
    /** Look: view-widths per full stick-width sweep, at 1.0 deflection. */
    val lookSensitivity: Float = 1.0f,
    /** Look: extra multiplier applied while ADS is held (usually < 1). */
    val adsSensitivityScale: Float = 0.65f,
    /** Stick: radial fraction ignored around centre, kills drift. */
    val stickDeadZone: Float = 0.12f,
    /** Stick: deflection at which full speed is reached (allows fine control). */
    val stickSaturation: Float = 0.85f,
    /** Movement: fraction of stick radius that triggers a digital direction. */
    val moveThreshold: Float = 0.30f,
    /** Movement: hysteresis, prevents chatter at the threshold. */
    val moveHysteresis: Float = 0.06f,
    /** Tap shorter than this (ms) still registers as a press. */
    val minTapMs: Int = 0,
    val invertY: Boolean = false,
)

data class TouchLayout(
    val elements: List<ControlElement>,
    val feel: FeelConfig = FeelConfig(),
) {
    fun byId(id: String): ControlElement? = elements.firstOrNull { it.id == id }

    // ---------------------------------------------------------------- JSON

    fun toJson(): JSONObject = JSONObject().apply {
        put("version", LAYOUT_VERSION)
        put("feel", JSONObject().apply {
            put("lookSensitivity", feel.lookSensitivity.toDouble())
            put("adsSensitivityScale", feel.adsSensitivityScale.toDouble())
            put("stickDeadZone", feel.stickDeadZone.toDouble())
            put("stickSaturation", feel.stickSaturation.toDouble())
            put("moveThreshold", feel.moveThreshold.toDouble())
            put("moveHysteresis", feel.moveHysteresis.toDouble())
            put("minTapMs", feel.minTapMs)
            put("invertY", feel.invertY)
        })
        put("elements", JSONArray().apply {
            elements.forEach { e -> put(elementToJson(e)) }
        })
    }

    companion object {
        const val LAYOUT_VERSION = 1

        fun fromJson(obj: JSONObject): TouchLayout {
            val feelObj = obj.optJSONObject("feel")
            val feel = if (feelObj == null) FeelConfig() else FeelConfig(
                lookSensitivity = feelObj.optDouble("lookSensitivity", 1.0).toFloat(),
                adsSensitivityScale = feelObj.optDouble("adsSensitivityScale", 0.65).toFloat(),
                stickDeadZone = feelObj.optDouble("stickDeadZone", 0.12).toFloat(),
                stickSaturation = feelObj.optDouble("stickSaturation", 0.85).toFloat(),
                moveThreshold = feelObj.optDouble("moveThreshold", 0.30).toFloat(),
                moveHysteresis = feelObj.optDouble("moveHysteresis", 0.06).toFloat(),
                minTapMs = feelObj.optInt("minTapMs", 0),
                invertY = feelObj.optBoolean("invertY", false),
            )

            val arr = obj.optJSONArray("elements")
            val elements = mutableListOf<ControlElement>()
            if (arr != null) {
                for (i in 0 until arr.length()) {
                    val e = arr.optJSONObject(i) ?: continue
                    val action = GameAction.byId(e.optString("action")) ?: continue
                    elements += ControlElement(
                        id = e.optString("id", action.id),
                        action = action,
                        kind = runCatching { ElementKind.valueOf(e.optString("kind")) }
                            .getOrDefault(ElementKind.BUTTON),
                        cx = e.optDouble("cx", 0.0).toFloat(),
                        cy = e.optDouble("cy", 0.0).toFloat(),
                        radius = e.optDouble("radius", 0.06).toFloat(),
                        zone = (e.optJSONObject("zone"))?.let { z ->
                            Rect4(
                                z.optDouble("left", 0.0).toFloat(),
                                z.optDouble("top", 0.0).toFloat(),
                                z.optDouble("right", 1.0).toFloat(),
                                z.optDouble("bottom", 1.0).toFloat(),
                            )
                        } ?: Rect4(0.5f, 0f, 1f, 1f),
                        mode = runCatching { ActionMode.valueOf(e.optString("mode")) }
                            .getOrDefault(action.defaultMode),
                        customPress = e.optStringOrNull("customPress"),
                        customRelease = e.optStringOrNull("customRelease"),
                        label = e.optStringOrNull("label"),
                        floating = e.optBoolean("floating", true),
                        visible = e.optBoolean("visible", true),
                    )
                }
            }
            return TouchLayout(elements, feel)
        }

        private fun elementToJson(e: ControlElement) = JSONObject().apply {
            put("id", e.id)
            put("action", e.action.id)
            put("kind", e.kind.name)
            put("cx", e.cx.toDouble())
            put("cy", e.cy.toDouble())
            put("radius", e.radius.toDouble())
            put("zone", JSONObject().apply {
                put("left", e.zone.left.toDouble())
                put("top", e.zone.top.toDouble())
                put("right", e.zone.right.toDouble())
                put("bottom", e.zone.bottom.toDouble())
            })
            put("mode", e.mode.name)
            e.customPress?.let { put("customPress", it) }
            e.customRelease?.let { put("customRelease", it) }
            e.label?.let { put("label", it) }
            put("floating", e.floating)
            put("visible", e.visible)
        }

        private fun JSONObject.optStringOrNull(key: String): String? =
            if (has(key) && !isNull(key)) optString(key) else null

        // ------------------------------------------------------------ default

        /**
         * A sane COD4 SP layout.
         *
         * Left thumb moves (floating stick over the whole left half).
         * Right thumb looks (drag anywhere on the right half).
         * Everything else is a button sized for a thumb and placed on the right,
         * which is the side that is NOT doing continuous dragging.
         */
        fun defaultLayout(): TouchLayout = TouchLayout(
            elements = listOf(
                ControlElement(
                    id = "stick_move", action = GameAction.MOVE_STICK, kind = ElementKind.STICK,
                    cx = 0.18f, cy = 0.68f, radius = 0.085f,
                    zone = Rect4(0f, 0.15f, 0.45f, 1f), floating = true,
                ),
                ControlElement(
                    id = "pad_look", action = GameAction.LOOK_PAD, kind = ElementKind.LOOK,
                    zone = Rect4(0.45f, 0.05f, 1f, 1f),
                ),
                ControlElement(
                    id = "btn_fire", action = GameAction.FIRE, kind = ElementKind.BUTTON,
                    cx = 0.86f, cy = 0.70f, radius = 0.072f, label = "FIRE",
                ),
                ControlElement(
                    id = "btn_ads", action = GameAction.ADS, kind = ElementKind.BUTTON,
                    cx = 0.70f, cy = 0.55f, radius = 0.055f, label = "ADS",
                ),
                ControlElement(
                    id = "btn_reload", action = GameAction.RELOAD, kind = ElementKind.BUTTON,
                    cx = 0.90f, cy = 0.50f, radius = 0.050f, label = "RELOAD",
                ),
                ControlElement(
                    id = "btn_crouch", action = GameAction.CROUCH, kind = ElementKind.BUTTON,
                    cx = 0.62f, cy = 0.82f, radius = 0.048f, label = "CROUCH",
                ),
                ControlElement(
                    id = "btn_prone", action = GameAction.PRONE, kind = ElementKind.BUTTON,
                    cx = 0.62f, cy = 0.93f, radius = 0.048f, label = "PRONE",
                ),
                ControlElement(
                    id = "btn_melee", action = GameAction.MELEE, kind = ElementKind.BUTTON,
                    cx = 0.80f, cy = 0.88f, radius = 0.050f, label = "MELEE",
                ),
                ControlElement(
                    id = "btn_frag", action = GameAction.FRAG, kind = ElementKind.BUTTON,
                    cx = 0.93f, cy = 0.30f, radius = 0.048f, label = "FRAG",
                ),
                ControlElement(
                    id = "btn_use", action = GameAction.USE, kind = ElementKind.BUTTON,
                    cx = 0.50f, cy = 0.36f, radius = 0.050f, label = "USE",
                ),
            ),
            feel = FeelConfig(),
        )
    }
}
