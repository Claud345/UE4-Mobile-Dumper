//
// Created by Ascarre on 12-04-2026.
//

#pragma once

#include <jni.h>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <list>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <thread>
#include <codecvt>
#include <unordered_map>
#include <filesystem>

#include <unistd.h>
#include <dirent.h>
#include <sys/uio.h>

#ifndef __cpp_lib_filesystem
namespace std { namespace filesystem = __fs::filesystem; }
#endif

#include "ElfFixer/fix.h"
#include "DumperLog.h"

std::vector<uint32_t> structIDMap;

namespace fs = std::filesystem;

// Dumper operating mode
enum DumperMode {
    MODE_INJECTED = 0,   // Classic mode: injected inside game process
    MODE_STANDALONE = 1  // Standalone mode: reads memory cross-process (needs root)
};

// Global mode flag - defaults to injected for backward compatibility
static DumperMode gDumperMode = MODE_INJECTED;

struct MemoryRequirements {
    const char* GameName;
    const char* TargetProcess;
    uintptr_t ModuleBase;
    uintptr_t ModuleEnd;
} Memory;

struct MenuRequirements {
    bool isRunning = false;
    std::string DumpLocation;
    const char* MenuName;
    const char* Credits;
} Menu;
