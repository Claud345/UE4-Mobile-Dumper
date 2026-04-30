//
// Created by Ascarre on 12-04-2026.
//

#pragma once

#include "Memory.h"

struct FString {
    uintptr_t Data;
    int32_t Count;

    // Use toString().c_str() instead of this directly if possible, or return a static buffer
    const char* c_str() {
        static std::string staticStr;
        staticStr = toString();
        return staticStr.c_str();
    }

    std::string toString() const {
        if (Count <= 0 || Count > 2048) return "";
        std::string str;
        str.reserve(Count);
        std::vector<uint16_t> buf = ReadArray<uint16_t>(Data, Count);
        for (int i = 0; i < Count; i++) {
            char data = (char)(buf[i] & 0xFF);
            str += isascii(data) ? data : '?';
        }
        return str;
    }
};

struct WideStr {
    static bool is_surrogate(uint16_t uc) {
        return (uc - 0xd800u) < 2048u;
    }

    static bool is_high_surrogate(uint16_t uc) {
        return (uc & 0xfffffc00) == 0xd800;
    }

    static bool is_low_surrogate(uint16_t uc) {
        return (uc & 0xfffffc00) == 0xdc00;
    }

    static wchar_t surrogate_to_utf32(uint16_t high, uint16_t low) {
        return (high << 10) + low - 0x35fdc00;
    }

    static std::wstring w_str(uintptr_t str, size_t len) {
        std::vector<uint16_t> source = ReadArray<uint16_t>(str, len);
        std::wstring output(len, L'\0');

        for (size_t i = 0; i < len; i++) {
            const uint16_t uc = source[i];
            if (!is_surrogate(uc)) {
                output[i] = uc;
            } else {
                if (is_high_surrogate(uc) && is_low_surrogate(source[i]))
                    output[i] = surrogate_to_utf32(uc, source[i]);
                else
                    output[i] = L'?';
            }
        }

        return output;
    }

    static std::string getString(uintptr_t StrPtr, int StrLength) {
        std::wstring str = w_str(StrPtr, StrLength);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(str);
    }
};

std::string GetNameFromFName(int index) {
    // UE4.23+ FNamePool path (only supported mode)
    uint32_t Block = index >> 16;
    uint16_t Offset = index & 65535;

    uintptr_t FNamePool = (Memory.ModuleBase + Offsets::GNames) + Offsets::GNamesToFNamePool;

    uintptr_t NamePoolChunk = Read<uintptr_t>(FNamePool + Offsets::FNamePoolToBlocks + (Block * Offsets::PointerSize));
    if (NamePoolChunk == 0) return "None";

    uintptr_t FNameEntry = NamePoolChunk + (Offsets::FNameStride * Offset);

    int16_t FNameEntryHeader = Read<int16_t>(FNameEntry + Offsets::FNameEntryHeader);
    uintptr_t StrPtr = FNameEntry + Offsets::FNameEntryToString;
    int StrLength = FNameEntryHeader >> Offsets::FNameEntryToLenBit;

    bool wide = FNameEntryHeader & 1;

    if (StrLength > 0 && StrLength < 250) {
        if (Offsets::isXorDecrypt) {
            return wide ? DecryptXorCypher(WideStr::getString(StrPtr, StrLength)) : DecryptXorCypher(ReadStringNew(StrPtr, StrLength));
        } else {
            return wide ? WideStr::getString(StrPtr, StrLength) : ReadStringNew(StrPtr, StrLength);
        }
    } else {
        return "None";
    }
}