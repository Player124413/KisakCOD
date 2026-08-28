package com.kisakcod.android.app

import android.content.Context
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.provider.DocumentsContract
import java.io.File
import java.io.InputStream
import java.util.zip.ZipInputStream

/**
 * Copies Call of Duty 4 game files from anywhere the user picks (a folder tree
 * via SAF, a zip file, or "open with") into the correct location for the engine
 * (see GamePaths). The engine never guesses where its data lives: the launcher
 * puts it in exactly the right place, automatically.
 *
 * The importer scans the source (up to a bounded depth) for the well-known
 * directories ("main", "zone", "raw", "players", "mods") wherever they sit, then
 * remaps each file to <gameRoot>/<dir>/<relative-path>.
 */
class GameFileImporter(private val context: Context) {

    interface Callback {
        fun onProgress(message: String, fraction: Float)
        fun onDone(copiedFiles: Int, copiedBytes: Long, message: String)
        fun onError(message: String)
    }

    companion object {
        private const val MAX_DEPTH = 10
        private const val MAX_COPY_FILES = 60000

        @JvmStatic
        fun isZip(uri: Uri, context: Context): Boolean {
            val type = context.contentResolver.getType(uri) ?: ""
            return type.contains("zip") || uri.toString().lowercase().endsWith(".zip")
        }
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val prefs by lazy { UiPrefs(context) }

    // ================================================================ folder tree

    fun importFromTree(treeUri: Uri, cb: Callback) {
        Thread {
            try {
                val treeDocId = DocumentsContract.getTreeDocumentId(treeUri)
                val plan = ArrayList<CopyPlan>()
                walkTree(treeUri, treeDocId, 0, "main", "", plan)
                if (plan.isEmpty()) {
                    fail(cb, "В выбранном месте не найдены файлы игры (main/, zone/, ...). " +
                            "Выберите папку Call of Duty 4 или папку, которая содержит main/.")
                    return@Thread
                }
                val total = plan.size.toFloat().coerceAtLeast(1f)
                var copied = 0
                var bytes = 0L
                plan.forEachIndexed { i, p ->
                    try {
                        context.contentResolver.openInputStream(p.srcUri)?.use { ins ->
                            bytes += copyStream(p, ins)
                        }
                        copied++
                    } catch (_: Exception) {
                        // a single bad file must not abort the whole import
                    }
                    if (i % 25 == 0 || i == plan.size - 1) {
                        cb.onProgress("Копирование: $i из ${plan.size}", i / total)
                    }
                }
                done(cb, copied, bytes)
            } catch (e: Exception) {
                fail(cb, "Ошибка импорта: ${e.message ?: e.javaClass.simpleName}")
            }
        }.start()
    }

    private fun walkTree(
        treeUri: Uri, docId: String, depth: Int,
        destBase: String, relPrefix: String, plan: MutableList<CopyPlan>
    ) {
        if (depth > MAX_DEPTH || plan.size > MAX_COPY_FILES) return
        val docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId)
        val meta = queryDoc(docUri) ?: return
        if (meta.isDir) {
            // Any recognized game folder anywhere in the tree becomes the new base.
            val matched = GamePaths.GAME_DIRS.firstOrNull { it.equals(meta.name, ignoreCase = true) }
            val base = matched ?: destBase
            val rel = if (matched != null) "" else relPrefix
            val children = queryChildren(treeUri, docId)
            for (child in children) {
                if (child.isDir) {
                    val childRel = if (rel.isEmpty()) child.name else "$rel/${child.name}"
                    walkTree(treeUri, child.docId, depth + 1, base, childRel, plan)
                } else {
                    val path = if (rel.isEmpty()) child.name else "$rel/${child.name}"
                    plan.add(CopyPlan(base, child.uri, path, child.size))
                }
            }
        } else {
            plan.add(CopyPlan(destBase, docUri, meta.name, meta.size))
        }
    }

    // ================================================================ zip

    fun importFromZip(zipUri: Uri, cb: Callback) {
        Thread {
            try {
                val entries = ArrayList<String>()
                var totalEntries = 0
                context.contentResolver.openInputStream(zipUri)?.use { ins ->
                    val zis = ZipInputStream(ins)
                    var e = zis.nextEntry
                    while (e != null) {
                        if (!e.isDirectory) {
                            totalEntries++
                            entries.add(e.name)
                        }
                        e = zis.nextEntry
                    }
                }
                if (entries.isEmpty()) {
                    fail(cb, "В ZIP-архиве нет файлов игры.")
                    return@Thread
                }
                var copied = 0
                var bytes = 0L
                context.contentResolver.openInputStream(zipUri)?.use { ins ->
                    val zis = ZipInputStream(ins)
                    var e = zis.nextEntry
                    while (e != null) {
                        if (!e.isDirectory) {
                            val dest = destFromPath(e.name)
                            val f = File(GamePaths.gameRoot(context), dest)
                            if (f.exists() && !prefs.importOverwrite) {
                                // skip
                            } else {
                                f.parentFile?.mkdirs()
                                f.outputStream().use { outs ->
                                    bytes += pump(zis, outs)
                                }
                                copied++
                            }
                        }
                        e = zis.nextEntry
                        if (e != null && !e.isDirectory && (copied % 50 == 0)) {
                            cb.onProgress("Распаковка: $copied файлов", copied.toFloat() / totalEntries)
                        }
                    }
                }
                done(cb, copied, bytes)
            } catch (e: Exception) {
                fail(cb, "Ошибка импорта ZIP: ${e.message ?: e.javaClass.simpleName}")
            }
        }.start()
    }

    /** Maps an archive path like "steamapps/common/Call of Duty 4/main/iw_01.iwd"
     *  to "main/iw_01.iwd". */
    private fun destFromPath(path: String): String {
        val parts = path.replace('\\', '/').split('/').filter { it.isNotEmpty() }
        for (i in parts.indices) {
            val matched = GamePaths.GAME_DIRS.firstOrNull {
                it.equals(parts[i], ignoreCase = true)
            }
            if (matched != null) {
                val rel = parts.drop(i + 1).joinToString("/")
                return if (rel.isEmpty()) matched else matched + "/" + rel
            }
        }
        return "main/" + parts.lastOrNull().orEmpty()
    }

    // ================================================================ helpers

    private data class CopyPlan(val destDir: String, val srcUri: Uri, val name: String, val size: Long)

    private class DocMeta(val name: String, val size: Long, val isDir: Boolean)

    private fun queryDoc(uri: Uri): DocMeta? {
        return try {
            var name = ""
            var size = 0L
            var isDir = false
            context.contentResolver.query(uri, null, null, null, null)?.use { c ->
                if (c.moveToFirst()) {
                    name = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME)) ?: ""
                    size = try { c.getLong(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_SIZE)) } catch (_: Exception) { 0L }
                    isDir = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE)) == DocumentsContract.Document.MIME_TYPE_DIR
                }
            }
            DocMeta(name, size, isDir)
        } catch (_: Exception) {
            null
        }
    }

    private class ChildRef(val uri: Uri, val docId: String, val name: String, val size: Long, val isDir: Boolean)

    private fun queryChildren(treeUri: Uri, parentDocId: String): List<ChildRef> {
        val out = ArrayList<ChildRef>()
        try {
            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId)
            context.contentResolver.query(childrenUri, null, null, null, null)?.use { c ->
                while (c.moveToNext()) {
                    val docId = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID))
                    val childUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId)
                    val name = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME)) ?: ""
                    val size = try { c.getLong(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_SIZE)) } catch (_: Exception) { 0L }
                    val isDir = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE)) == DocumentsContract.Document.MIME_TYPE_DIR
                    out.add(ChildRef(childUri, docId, name, size, isDir))
                }
            }
        } catch (_: Exception) {
        }
        return out
    }

    private fun copyStream(p: CopyPlan, ins: InputStream): Long {
        val dir = File(GamePaths.gameRoot(context), p.destDir)
        if (!dir.exists()) dir.mkdirs()
        val safeRel = p.name.replace('\\', '/').split('/').joinToString("/") { s -> s.trim() }
        val outFile = File(dir, safeRel)
        if (outFile.parentFile != null && !outFile.parentFile.exists()) outFile.parentFile.mkdirs()
        if (outFile.exists() && !prefs.importOverwrite) return 0
        outFile.outputStream().use { outs -> return pump(ins, outs) }
    }

    private fun pump(ins: InputStream, outs: java.io.OutputStream): Long {
        val buf = ByteArray(128 * 1024)
        var total = 0L
        var r = ins.read(buf)
        while (r > 0) {
            outs.write(buf, 0, r)
            total += r
            r = ins.read(buf)
        }
        return total
    }

    private fun done(cb: Callback, files: Int, bytes: Long) {
        val mb = bytes / (1024 * 1024)
        mainHandler.post { cb.onDone(files, bytes, "Импортировано: $files файлов, $mb МБ") }
    }

    private fun fail(cb: Callback, msg: String) {
        mainHandler.post { cb.onError(msg) }
    }
}