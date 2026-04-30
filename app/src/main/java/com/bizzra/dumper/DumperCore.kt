package com.bizzra.dumper

import android.content.Context

object DumperCore {
    const val MODE_INJECTED = 0
    const val MODE_STANDALONE = 1

    init {
        System.loadLibrary("Dumper")
    }

    @JvmStatic
    external fun CheckOverlayPermission(context: Context)

    // Standalone mode functions
    @JvmStatic
    external fun SetMode(mode: Int)

    @JvmStatic
    external fun GetMode(): Int

    @JvmStatic
    external fun FindProcess(packageName: String): Int

    @JvmStatic
    external fun AttachToProcess(pid: Int)

    @JvmStatic
    external fun IsProcessAttached(): Boolean

    @JvmStatic
    external fun GetRunningProcesses(): Array<String>

    @JvmStatic
    external fun HasRootAccess(): Boolean

    @JvmStatic
    external fun AutoFindOffsets(): String

    @JvmStatic
    fun Start(context: Context) {
        CheckOverlayPermission(context)
    }

    // ═══ Logging System JNI ═══

    @JvmStatic
    external fun GetFormattedLog(minLevel: Int): String

    @JvmStatic
    external fun PollNewLogs(minLevel: Int): String

    @JvmStatic
    external fun ClearLogs()

    @JvmStatic
    external fun GetLogFilePath(): String

    @JvmStatic
    external fun GetLogCount(): Int
}