/* SPDX-License-Identifier: MPL-2.0 */
package org.stratoemu.strato.setup

import android.content.Context
import android.net.Uri
import org.stratoemu.strato.getPublicFilesDir
import org.stratoemu.strato.utils.ZipUtils
import java.io.File
import java.io.IOException
import java.util.UUID

/** Uses the existing NCA/version/font readers; validates everything before replacing installed data. */
object FirmwareInstaller {
    fun install(context: Context, uri: Uri): String {
        val root = context.getPublicFilesDir()
        val firmware = File(root, "switch/nand/system/Contents/registered")
        val fonts = File(root, "fonts")
        val id = UUID.randomUUID().toString()
        val stagedFirmware = File(firmware.parentFile, ".firmware-$id")
        val stagedFonts = File(root, ".fonts-$id")
        val keys = File(context.filesDir, "keys").path
        try {
            if (!stagedFirmware.mkdirs() || !stagedFonts.mkdirs())
                throw IOException("Unable to create installation directories")
            val stream = context.contentResolver.openInputStream(uri)
                ?: throw IOException("Unable to read the selected archive")
            stream.use { ZipUtils.unzip(it, stagedFirmware) }
            val files = stagedFirmware.listFiles()
            if (files.isNullOrEmpty() || files.any { !it.isFile || !it.name.endsWith(".nca", true) || it.length() == 0L })
                throw IOException("Select a ZIP containing firmware NCA files")
            val version = fetchFirmwareVersion(stagedFirmware.path, keys)
            if (version.isBlank()) throw IOException("Firmware could not be verified with the installed prod.keys")
            extractFonts(stagedFirmware.path, keys, stagedFonts.path)
            replaceDirectories(listOf(stagedFirmware to firmware, stagedFonts to fonts), id)
            return version
        } finally {
            stagedFirmware.deleteRecursively()
            stagedFonts.deleteRecursively()
        }
    }

    private fun replaceDirectories(entries: List<Pair<File, File>>, id: String) {
        val backups = mutableListOf<Pair<File, File>>()
        val installed = mutableListOf<File>()
        try {
            for ((source, target) in entries) {
                if (target.exists()) {
                    val backup = File(target.parentFile, ".${target.name}-backup-$id")
                    if (!target.renameTo(backup)) throw IOException("Unable to preserve installed ${target.name}")
                    backups += backup to target
                }
                if (!source.renameTo(target)) throw IOException("Unable to install ${target.name}")
                installed += target
            }
        } catch (error: Exception) {
            installed.asReversed().forEach { it.deleteRecursively() }
            backups.asReversed().forEach { (backup, target) ->
                if (!backup.renameTo(target)) error.addSuppressed(IOException("Previous files preserved at ${backup.path}"))
            }
            throw error
        }
        backups.forEach { (backup, _) -> backup.deleteRecursively() }
    }

    private external fun fetchFirmwareVersion(systemArchivesPath: String, keysPath: String): String
    private external fun extractFonts(systemArchivesPath: String, keysPath: String, fontsPath: String)
}
