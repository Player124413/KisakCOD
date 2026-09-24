package com.kisak.cod.input

/**
 * On-screen control actions.
 *
 * IMPORTANT: every command string below was verified against the engine's
 * registered console commands (Cmd_AddCommandInternal(...) in src/client/cl_input.cpp).
 * Do not invent commands here — if a command is not registered, the button
 * silently does nothing and the failure is very hard to trace.
 *
 * Verified command set available in the KisakCOD SP client:
 *   +forward/+back/+moveleft/+moveright        movement
 *   +speed, +breath_sprint, +sprint            sprint variants
 *   +stance, gocrouch, togglecrouch            crouch
 *   +prone, goprone, toggleprone, +gostand     prone / stand
 *   +attack                                    fire
 *   toggleads / leaveads                       aim-down-sight (toggleads is a
 *                                              PLAIN command, it has no +/- form)
 *   +reload, +usereload                        reload
 *   +melee, +melee_breath                      melee
 *   +frag, +smoke, +throw                      grenades
 *   +holdbreath, +nightvision, +activate       misc
 *   +leanleft, +leanright                      leaning
 *
 * Note: Call of Duty 4 has no jump command. That is correct, not an omission.
 */
enum class ActionMode { HOLD, TOGGLE }

enum class GameAction(
    val id: String,
    val label: String,
    /** Command issued on press. Null for pure-axis actions (stick, look). */
    val press: String?,
    /** Command issued on release. Null for TOGGLE actions and axis actions. */
    val release: String?,
    val defaultMode: ActionMode = ActionMode.HOLD,
) {
    MOVE_STICK("move", "Move", null, null),
    LOOK_PAD("look", "Look", null, null),

    FIRE("fire", "Fire", "+attack", "-attack"),
    ADS("ads", "Aim", "toggleads", "leaveads"),
    RELOAD("reload", "Reload", "+reload", "-reload"),
    MELEE("melee", "Melee", "+melee", "-melee"),

    FRAG("frag", "Frag", "+frag", "-frag"),
    SMOKE("smoke", "Smoke", "+smoke", "-smoke"),

    SPRINT("sprint", "Sprint", "+speed", "-speed"),
    CROUCH("crouch", "Crouch", "togglecrouch", null, ActionMode.TOGGLE),
    PRONE("prone", "Prone", "toggleprone", null, ActionMode.TOGGLE),

    USE("use", "Use", "+activate", "-activate"),
    HOLD_BREATH("holdbreath", "Hold breath", "+holdbreath", "-holdbreath"),
    NIGHTVISION("nightvision", "NVG", "+nightvision", "-nightvision"),
    LEAN_LEFT("leanleft", "Lean L", "+leanleft", "-leanleft"),
    LEAN_RIGHT("leanright", "Lean R", "+leanright", "-leanright"),

    /** Free-form escape hatch: run any console command on tap. */
    CUSTOM("custom", "Custom", null, null),
    ;

    val isAxis: Boolean get() = press == null

    companion object {
        fun byId(id: String): GameAction? = entries.firstOrNull { it.id == id }
    }
}
