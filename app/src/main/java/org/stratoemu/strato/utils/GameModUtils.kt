/* SPDX-License-Identifier: MPL-2.0 */
package org.stratoemu.strato.utils

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.stratoemu.strato.getPublicFilesDir
import java.io.File
import java.io.FileOutputStream
import java.util.UUID
import java.util.zip.ZipInputStream

/**
 * Per-game mod manager. Keeps import files outside the game-content NSP directory.
 * Layout: mods/<TITLE_ID>/<NAME>/{romfs,exefs,cheats}/...
 */
object GameModUtils {
    private val titlePattern = Regex("[0-9a-fA-F]{16}")
    private val unsafePart = Regex("""[\\/:*?"<>|\u0000-\u001f]""")

    private fun root(context: Context, titleId: String): File {
        require(titlePattern.matches(titleId)) { "Invalid game title ID" }
        return File(context.getPublicFilesDir(), "mods/${titleId.uppercase()}")
    }

    fun list(context: Context, titleId: String): List<File> =
        root(context, titleId).listFiles()?.filter {
            it.isDirectory && !it.name.startsWith(".") && !java.nio.file.Files.isSymbolicLink(it.toPath())
        }?.sortedBy { it.name.lowercase() } ?: emptyList()

    fun isEnabled(dir: File) = !File(dir, ".disabled").exists()

    fun setEnabled(dir: File, enabled: Boolean) {
        check(dir.isDirectory && !java.nio.file.Files.isSymbolicLink(dir.toPath())) { "Invalid mod directory" }
        val marker = File(dir, ".disabled")
        if (enabled) {
            check(!marker.exists() || marker.delete()) { "Could not enable mod" }
        } else {
            check(marker.exists() || marker.createNewFile()) { "Could not disable mod" }
        }
    }

    fun remove(context: Context, titleId: String, dir: File) {
        val base = root(context, titleId).canonicalFile
        require(dir.parentFile?.canonicalFile == base && dir.isDirectory &&
            !java.nio.file.Files.isSymbolicLink(dir.toPath())) { "Invalid mod location" }
        check(dir.deleteRecursively()) { "Could not delete mod" }
    }

    suspend fun install(context: Context, titleId: String, uri: Uri): String = withContext(Dispatchers.IO) {
        val base = root(context, titleId)
        check(base.mkdirs() || base.isDirectory) { "Could not create mod directory" }
        var filename = "Imported Mod"
        context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0 && !cursor.isNull(index)) filename = cursor.getString(index)
            }
        }
        require(filename.endsWith(".zip", ignoreCase = true)) { "Select a ZIP containing a mod's romfs, exefs or cheats directory" }
        val safeName = filename.dropLast(4).replace(unsafePart, "_").trim().trim('.').take(96)
        require(safeName.isNotEmpty()) { "Invalid mod name" }
        val target = File(base, safeName)
        check(!target.exists()) { "Mod already installed: $safeName" }
        val temp = File(base, ".install-${UUID.randomUUID()}")
        check(temp.mkdir()) { "Could not prepare mod installation" }
        var installed = 0
        var totalBytes = 0L
        try {
            context.contentResolver.openInputStream(uri)?.use { input ->
                ZipInputStream(input.buffered()).use { zip ->
                    while (true) {
                        val entry = zip.nextEntry ?: break
                        val raw = entry.name.replace('\\', '/')
                        val parts = raw.split('/').filter { it.isNotEmpty() }
                        require(parts.none { it == "." || it == ".." } && !raw.startsWith("/") &&
                            !raw.contains(':') && !raw.contains('\u0000')) { "Unsafe ZIP path" }
                        if (entry.isDirectory) { zip.closeEntry(); continue }
                        val prefix = parts.indexOfFirst { it.equals("romfs", true) || it.equals("exefs", true) || it.equals("cheats", true) }
                        if (prefix < 0 || prefix == parts.lastIndex) { zip.closeEntry(); continue }
                        val type = parts[prefix].lowercase()
                        val rest = parts.drop(prefix + 1)
                        require(rest.size <= 24 && rest.all { it.length in 1..128 }) { "Unsupported mod file path" }
                        val dest = File(temp, (listOf(type) + rest).joinToString("/"))
                        require(dest.canonicalPath.startsWith(temp.canonicalPath + File.separator)) { "Unsafe mod path" }
                        check(dest.parentFile?.mkdirs() == true || dest.parentFile?.isDirectory == true) { "Could not create mod folder" }
                        check(!dest.exists()) { "Duplicate file in mod ZIP: ${dest.name}" }
                        installed++
                        require(installed <= 8000) { "Mod contains too many files" }
                        FileOutputStream(dest).use { out ->
                            val data = ByteArray(65536)
                            while (true) {
                                val n = zip.read(data)
                                if (n < 0) break
                                totalBytes += n
                                require(totalBytes <= 512L * 1024 * 1024) { "Mod ZIP exceeds 512 MB uncompressed" }
                                out.write(data, 0, n)
                            }
                            out.fd.sync()
                        }
                        zip.closeEntry()
                    }
                }
            } ?: error("Could not open the ZIP")
            check(installed > 0) { "ZIP does not contain a romfs, exefs or cheats folder" }
            check(temp.renameTo(target)) { "Could not complete mod installation" }
            safeName
        } finally {
            if (temp.exists()) temp.deleteRecursively()
        }
    }
}
