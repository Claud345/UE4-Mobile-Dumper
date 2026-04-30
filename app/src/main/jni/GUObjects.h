//
// Created by Ascarre on 12-04-2026.
// Simplified for UE4.23+ only (pre-4.23 / isDereferencing paths removed)
//

#pragma once

#include "FNames.h"

int32_t GetObjectCount() {
    return Read<int32_t>((Memory.ModuleBase + Offsets::GUObjectArray) + Offsets::FUObjectArrayToTUObjectArray + Offsets::TUObjectArrayToNumElements);
}

uintptr_t GetUObjectFromID(uint32_t index) {
    uintptr_t TUObjectArray = Read<uintptr_t>(Memory.ModuleBase + Offsets::GUObjectArray + Offsets::FUObjectArrayToTUObjectArray);
    if (TUObjectArray == 0) return 0;
    uintptr_t Chunk = Read<uintptr_t>(TUObjectArray + ((index / 0x10000) * Offsets::PointerSize));
    if (Chunk == 0) return 0;
    return Read<uintptr_t>(Chunk + Offsets::FUObjectItemPadd + ((index % 0x10000) * Offsets::FUObjectItemSize));
}