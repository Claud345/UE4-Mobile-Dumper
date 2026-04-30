//
// Created by Ascarre on 12-04-2026.
//

#pragma once

#include "JavaNative.h"

void DumperThread() {
    // Install crash-safe signal handlers (catches SIGSEGV/SIGBUS from AC memory traps)
    SafeRead::install();
    LOG_I("Thread", "Dumper thread started — installing signal handlers");

    // In standalone mode, we need to wait for the user to select a process first.
    // gRemoteProcess won't have a valid PID until AttachToProcess is called from Java.
    if (gDumperMode == MODE_STANDALONE) {
        LOG_I("Thread", "Standalone mode — waiting for process attachment...");
        // Wait until a remote process has been attached
        while (!gRemoteProcess.isAttached()) {
            if (!Menu.isRunning) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        LOG_I("Thread", "Process attached (PID: %d)", gRemoteProcess.getPid());
    }

    // Now wait for the target library to be loaded (in either mode)
    LOG_I("Thread", "Waiting for library: %s", Memory.TargetProcess);
    while (!isLibraryLoaded()) {
        if (!Menu.isRunning) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Memory.ModuleBase = GetModuleBase();
    Memory.ModuleEnd = GetModuleEnd();
    LOG_I("Thread", "Library loaded — Base: 0x%lx | End: 0x%lx | Size: %zu KB",
          Memory.ModuleBase, Memory.ModuleEnd,
          (Memory.ModuleEnd - Memory.ModuleBase) / 1024);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Menu.isRunning) {
        if (isAutoFindDump) {
            isAutoFindDump = false; // Reset immediately
            LOG_I("Scanner", "Auto-find offsets triggered...");
            auto result = AutoOffsets::FindOffsets();
            AutoOffsets::ApplyOffsets(result);
            Memory.ModuleBase = GetModuleBase();
            Memory.ModuleEnd = GetModuleEnd();
            LOG_I("Scanner", "Result: %s", result.status.c_str());
            if (result.GNames != 0)
                LOG_I("Scanner", "GNames: 0x%lx (score: %d%%)", result.GNames, result.gnamesScore);
            if (result.GUObjectArray != 0)
                LOG_I("Scanner", "GUObjectArray: 0x%lx (score: %d%%)", result.GUObjectArray, result.guobjectScore);
            if (result.GWorld != 0)
                LOG_I("Scanner", "GWorld: 0x%lx (score: %d%%)", result.GWorld, result.gworldScore);
        }

        if (isDumpLib && !isDumpLibDone) {
            LOG_I("Dump", "Dumping library to: %s", Menu.DumpLocation.c_str());
            DumpLib(Menu.DumpLocation);
            isDumpLibDone = true;
            LOG_I("Dump", "Library dump complete");
        }

        if (isStringsDump && !isStringDumped) {
            LOG_I("Dump", "Dumping FName strings...");
            DumpStrings(Menu.DumpLocation);
            isStringDumped = true;
            LOG_I("Dump", "FName strings dump complete");
        }

        if (isObjectsDump && !isObjectsDumped) {
            LOG_I("Dump", "Dumping UObjects...");
            DumpObjects(Menu.DumpLocation);
            isObjectsDumped = true;
            LOG_I("Dump", "UObjects dump complete");
        }

        if (isSDKUDump && !isSDKUDumped) {
            LOG_I("Dump", "Dumping SDK (UObjects)...");
            DumpSDK(Menu.DumpLocation);
            isSDKUDumped = true;
            LOG_I("Dump", "SDK (UObjects) dump complete");
        }

        if (isSDKWDump && !isSDKWDumped) {
            LOG_I("Dump", "Dumping SDK (GWorld)...");
            DumpSDKW(Menu.DumpLocation);
            isSDKWDumped = true;
            LOG_I("Dump", "SDK (GWorld) dump complete");
        }

        if (isActorsDump && !isActorsDumped) {
            LOG_I("Dump", "Dumping actors...");
            DumpActors(Menu.DumpLocation);
            isActorsDumped = true;
            LOG_I("Dump", "Actors dump complete");
        }

        if (isBoneDump && !isBoneDumped) {
            LOG_I("Dump", "Dumping bones...");
            DumpBones(Menu.DumpLocation);
            isBoneDumped = true;
            LOG_I("Dump", "Bones dump complete");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    LOG_I("Thread", "Dumper thread exiting");
}