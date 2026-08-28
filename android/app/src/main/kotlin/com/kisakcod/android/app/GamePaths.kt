package com.kisakcod.android.app

import android.content.Context
import java.io.File

/**
 * Where game data lives on this install.
 *
 * Layout on disk (fs_basepath for the engine):
 *   <filesDir>/
 *     main/            <- main game data (iw_*.iwd, localized_*.iwd, ...)
 *     zone/            <- compiled fastfiles
 *     raw/             <- (optional) dev assets
 *     players/         <- profiles/configs
 *     mods/            <- (optional) mods
 *     config_mp.cfg, android_launcher.cfg, ...
 */
object GamePaths {

    /** Root of the game data directory (internal to the app; no storage permissions needed). */
    fun gameRoot(context: Context): File =
        context.getExternalFilesDir(null) ?: context.filesDir

    /** Subdirs the game expects below the root. */
    val GAME_DIRS: Array<String> = arrayOf(
        "main", "zone", "raw", "raw_shared", "devraw", "players", "mods"
    )

    /** True when the essential data folder exists and looks like a COD4 install. */
    fun hasGameData(context: Context): Boolean {
        val main = File(gameRoot(context), "main")
        if (!main.isDirectory) return false
        val iwd = main.listFiles { f -> f.isFile && f.name.lowercase().endsWith(".iwd") }
        return !(iwd == null || iwd.isEmpty())
    }

    /** Human-readable summary used on the launcher home card. */
    fun gameDataSummary(context: Context): String {
        val root = gameRoot(context)
        val sizeB = root.walkTopDown().filter { it.isFile }.sumOf { it.length() }
        return if (hasGameData(context)) {
            val mb = sizeB / (1024 * 1024)
            "Файлы игры найдены ($mb МБ). Можно запускать."
        } else {
            "Файлы игры не найдены. Импортируйте их из папки или ZIP."
        }
    }
}