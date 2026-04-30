//
// Created by Ascarre on 12-04-2026.
//

#include "Main.h"

extern "C" {

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_DumperCore_CheckOverlayPermission(JNIEnv *env, jclass clazz,jobject context) {
    CheckPermissionStartOverlay(env, context);
}

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_FloatingService_Title(JNIEnv *env, jobject thiz, jobject text_view) {
    setText(env, text_view, Menu.MenuName);
}

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_FloatingService_SubTitle(JNIEnv *env, jobject thiz, jobject text_view) {
    setText(env, text_view, Menu.Credits);
}

JNIEXPORT jstring JNICALL
Java_com_bizzra_dumper_FloatingService_Icon(JNIEnv *env, jobject thiz) {
    //using Base64 data
    //return env->NewStringUTF("data:image/png;base64, ...");

    // From online (specify .gif/.png/.jpeg and use i.imgur.com always)
    return env->NewStringUTF("https://i.imgur.com/JywgRaP.png");

    // From assets folder
    // return env->NewStringUTF("file:///android_asset/example.gif");
}

JNIEXPORT jobjectArray JNICALL
Java_com_bizzra_dumper_FloatingService_GetFeatureList(JNIEnv *env, jobject thiz) {
    jobjectArray ret;

    const char *features[] = {
            "Category_Offset Scanner",
            "Button_Auto Find Offsets",
            "Toggle_UE5 Mode",
            "Category_Anti-Cheat Evasion",
            "Toggle_Stealth Read (proc/mem)",
            "Button_Flush Read Cache",
            "Category_Library Dump Options",
            "Toggle_Fast Lib",
            "Toggle_Rebuilt Lib",
            "Button_Dump Lib",
            "Category_Dump Options",
            "Toggle_GNames Dump (String Dump)",
            "Toggle_GUObject Dump (Objects Dump)",
            "Toggle_SDKU Dump (SDK Dump using GUObjects)",
            "Toggle_SDKW Dump (SDK Dump using GWorld)",
            "Toggle_Actors Dump (Dump all actors in world)",
            "Toggle_Bones Dump (Dump Bones)",
            "Input_Set XOR Key (Hex)",
            "Category_Diagnostics",
            "Button_View Logs"
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray) env->NewObjectArray(Total_Feature, env->FindClass("java/lang/String"), env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_MenuChanges_Changes(JNIEnv *env, jobject thiz, jobject context, jint f_num, jstring f_name, jint value, jboolean boolean, jstring str) {
    switch (f_num) {
        case 0: {
            // Trigger Auto Find Offsets in background thread
            isAutoFindDump = true;
            break;
        }
        case 1: {
            Offsets::isUE5 = boolean;
            if (boolean) {
                Offsets::isUE423 = true;
                Offsets::FNameStride = 0x4;
            } else {
                Offsets::FNameStride = 0x2;
            }
            break;
        }
        case 2: {
            // Stealth Read toggle — switches to /proc/pid/mem + pread64
            Offsets::useStealthRead = boolean;
            if (gDumperMode == MODE_STANDALONE) {
                gRemoteProcess.setStrategy(boolean ? STRAT_PROC_MEM : STRAT_VM_READV);
            }
            break;
        }
        case 3: {
            // Flush read cache — forces fresh memory reads
            if (gDumperMode == MODE_STANDALONE) {
                gRemoteProcess.invalidateCache();
            }
            break;
        }
        case 4: isFastDump = boolean; break;
        case 5: isRebuiltLibDump = boolean; break;
        case 6: isDumpLib = true; isDumpLibDone = false; break;
        case 7: isStringsDump = boolean; break;
        case 8: isObjectsDump = boolean; break;
        case 9: isSDKUDump = boolean; break;
        case 10: isSDKWDump = boolean; break;
        case 11: isActorsDump = boolean; break;
        case 12: isBoneDump = boolean; break;
        case 13: {
            if (str != nullptr) {
                const char* keyStr = env->GetStringUTFChars(str, nullptr);
                Offsets::XorKey = (uint8_t)strtol(keyStr, nullptr, 16);
                env->ReleaseStringUTFChars(str, keyStr);
            }
            break;
        }
    }
}

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_FloatingService_CloseThreads(JNIEnv *env, jobject clazz) {
    Menu.isRunning = false;
}

// ============ Standalone Mode JNI Functions ============

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_DumperCore_SetMode(JNIEnv *env, jclass clazz, jint mode) {
    gDumperMode = (DumperMode)mode;
}

JNIEXPORT jint JNICALL
Java_com_bizzra_dumper_DumperCore_GetMode(JNIEnv *env, jclass clazz) {
    return (jint)gDumperMode;
}

JNIEXPORT jint JNICALL
Java_com_bizzra_dumper_DumperCore_FindProcess(JNIEnv *env, jclass clazz, jstring packageName) {
    const char* pkg = env->GetStringUTFChars(packageName, nullptr);
    pid_t pid = RemoteProcess::findPidByName(pkg);
    env->ReleaseStringUTFChars(packageName, pkg);
    return (jint)pid;
}

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_DumperCore_AttachToProcess(JNIEnv *env, jclass clazz, jint pid) {
    gRemoteProcess.setPid((pid_t)pid);
}

JNIEXPORT jboolean JNICALL
Java_com_bizzra_dumper_DumperCore_IsProcessAttached(JNIEnv *env, jclass clazz) {
    return (jboolean)gRemoteProcess.isAttached();
}

JNIEXPORT jobjectArray JNICALL
Java_com_bizzra_dumper_DumperCore_GetRunningProcesses(JNIEnv *env, jclass clazz) {
    auto processes = RemoteProcess::getRunningProcesses();
    int count = processes.size();

    // Return array of strings in format "pid:packageName"
    jobjectArray result = env->NewObjectArray(count, env->FindClass("java/lang/String"), env->NewStringUTF(""));

    for (int i = 0; i < count; i++) {
        std::string entry = std::to_string(processes[i].first) + ":" + processes[i].second;
        env->SetObjectArrayElement(result, i, env->NewStringUTF(entry.c_str()));
    }

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_bizzra_dumper_DumperCore_HasRootAccess(JNIEnv *env, jclass clazz) {
    // Check if su binary exists
    FILE* fp = popen("which su", "r");
    if (fp == nullptr) return JNI_FALSE;

    char path[128] = {0};
    fgets(path, sizeof(path), fp);
    int status = pclose(fp);

    return (strlen(path) > 0 && status == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_bizzra_dumper_DumperCore_AutoFindOffsets(JNIEnv *env, jclass clazz) {
    auto result = AutoOffsets::FindOffsets();
    AutoOffsets::ApplyOffsets(result);

    // Re-init module base/end
    Memory.ModuleBase = GetModuleBase();
    Memory.ModuleEnd = GetModuleEnd();

    return env->NewStringUTF(result.status.c_str());
}


// ═══════════ Logging System JNI Functions ═══════════

JNIEXPORT jstring JNICALL
Java_com_bizzra_dumper_DumperCore_GetFormattedLog(JNIEnv *env, jclass clazz, jint minLevel) {
    std::string log = DumperLog::instance().getFormattedLog(minLevel);
    return env->NewStringUTF(log.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_bizzra_dumper_DumperCore_PollNewLogs(JNIEnv *env, jclass clazz, jint minLevel) {
    std::string log = DumperLog::instance().pollNewEntries(minLevel);
    return env->NewStringUTF(log.c_str());
}

JNIEXPORT void JNICALL
Java_com_bizzra_dumper_DumperCore_ClearLogs(JNIEnv *env, jclass clazz) {
    DumperLog::instance().clear();
}

JNIEXPORT jstring JNICALL
Java_com_bizzra_dumper_DumperCore_GetLogFilePath(JNIEnv *env, jclass clazz) {
    std::string path = DumperLog::instance().getLogFilePath();
    return env->NewStringUTF(path.c_str());
}

JNIEXPORT jint JNICALL
Java_com_bizzra_dumper_DumperCore_GetLogCount(JNIEnv *env, jclass clazz) {
    return (jint)DumperLog::instance().count();
}

}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    Menu.MenuName = "Android UE Dumper";
    Menu.Credits = "Made by Ascarre";
    Menu.isRunning = true;

    Offsets::ExampleGame();

    // Initialize the logging system — logs go to /data/local/tmp/ until SetDumpLocation is called
    DumperLog::instance().init("/data/local/tmp");
    LOG_I("Core", "UE Dumper initialized — Mode: %s",
          gDumperMode == MODE_STANDALONE ? "Standalone" : "Injected");
    LOG_I("Core", "Target: %s | Game: %s", Memory.TargetProcess, Memory.GameName);

    std::thread(DumperThread).detach();

    return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    Menu.isRunning = false;
}