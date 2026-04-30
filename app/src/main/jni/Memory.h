//
// Created by Ascarre on 12-04-2026.
//

#pragma once

#include "Offsets.h"
#include "RemoteMemory.h"

// ========== Module Base / End ==========

uintptr_t GetModuleBase(const char* LibraryName = Memory.TargetProcess) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.getModuleBase(LibraryName);
    }

    // Original injected mode: read /proc/self/maps
    char buffer[1024];
    FILE *fp = fopen("/proc/self/maps", "rt");
    if (fp == NULL) return 0;

    uintptr_t address = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, LibraryName)) {
            address = (uintptr_t)strtoul(buffer, NULL, 16);
            break;
        }
    }
    fclose(fp);
    return address;
}

uintptr_t GetModuleEnd(const char* LibraryName = Memory.TargetProcess) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.getModuleEnd(LibraryName);
    }

    // Original injected mode: read /proc/self/maps
    char buffer[1024];
    FILE *fp = fopen("/proc/self/maps", "rt");
    if (fp == NULL) return 0;

    uintptr_t endAddress = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, LibraryName)) {
            uintptr_t start, end;
            if (sscanf(buffer, "%lx-%lx", &start, &end) == 2) {
                if (end > endAddress) {
                    endAddress = end;
                }
            }
        }
    }
    fclose(fp);
    return endAddress;
}

bool isLibraryLoaded(const char* LibraryName = Memory.TargetProcess) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.isModuleLoaded(LibraryName);
    }

    // Original injected mode
    char line[512] = {0};
    FILE *fp = fopen("/proc/self/maps", "rt");
    bool isLoaded = false;

    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, LibraryName)) {
                isLoaded = true;
                break;
            }
        }
        fclose(fp);
    }
    return isLoaded;
}

// ========== Memory Read Functions ==========

template <typename T>
T Read(uintptr_t address) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.read<T>(address);
    }
    // Injected mode: safe pointer dereference (catches SIGSEGV from AC traps)
    T value{};
    SafeRead::safeCopy(&value, reinterpret_cast<void*>(address), sizeof(T));
    return value;
}

template<typename T>
std::vector<T> ReadArray(uintptr_t address, size_t size) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.readArray<T>(address, size);
    }
    // Injected mode: safe memory copy
    std::vector<T> data(size);
    SafeRead::safeCopy(data.data(), reinterpret_cast<void*>(address), size * sizeof(T));
    return data;
}

std::string ReadString(uintptr_t address, size_t size = 100) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.readString(address, size);
    }
    // Injected mode: safe string read
    std::vector<char> buf(size + 1, '\0');
    SafeRead::safeCopy(buf.data(), reinterpret_cast<void*>(address), size);
    return std::string(buf.data());
}

std::string ReadStringNew(uintptr_t address, size_t size = 100) {
    if (gDumperMode == MODE_STANDALONE) {
        return gRemoteProcess.readStringNew(address, size);
    }
    // Injected mode: safe memory copy
    std::string name(size, '\0');
    SafeRead::safeCopy(&name[0], reinterpret_cast<void*>(address), size);
    return name;
}

std::string DecryptXorCypher(std::string Input, int size = 0) {
    if (size == 0) size = Input.size();
    std::string Output = Input;
    
    uint8_t key = (Offsets::XorKey != 0) ? Offsets::XorKey : (uint8_t)size;
    
    for (int i = 0; i < size; i++)
        Output[i] = Input[i] ^ key;

    return Output;
}