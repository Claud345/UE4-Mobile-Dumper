//
// AutoOffsets.h — v2.0 Hardened offset finder for custom UE 4.23+ engines
// Scoring system, adaptive validation, merged brute-force, no pre-4.23 code
//

#pragma once
#include "PatternScanner.h"

namespace AutoOffsets {

    struct FindResult {
        uintptr_t GNames = 0;
        uintptr_t GUObjectArray = 0;
        uintptr_t GWorld = 0;
        bool isUE5 = false;
        int gnamesScore = 0;
        int guobjectScore = 0;
        int gworldScore = 0;
        std::string status;
        int foundCount = 0;
    };

    struct MemSegment {
        uintptr_t start, end;
        bool isExecutable, isReadable, isWritable;
    };

    // ═══════════════════ Segment Parsing ═══════════════════

    std::vector<MemSegment> GetModuleSegments(const char* moduleName) {
        std::vector<MemSegment> segs;
        char path[64];
        if (gDumperMode == MODE_STANDALONE)
            snprintf(path, sizeof(path), "/proc/%d/maps", gRemoteProcess.getPid());
        else
            snprintf(path, sizeof(path), "/proc/self/maps");

        // Use raw syscall in standalone to avoid AC detection
        std::string content;
        if (gDumperMode == MODE_STANDALONE) {
            int fd = RawSyscall::openat_raw(AT_FDCWD, path, O_RDONLY);
            if (fd < 0) return segs;
            char buf[4096];
            ssize_t n;
            while ((n = RawSyscall::read_raw(fd, buf, sizeof(buf))) > 0)
                content.append(buf, n);
            RawSyscall::close_raw(fd);
        } else {
            FILE* fp = fopen(path, "r");
            if (!fp) return segs;
            char buf[1024];
            while (fgets(buf, sizeof(buf), fp)) content += buf;
            fclose(fp);
        }

        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find(moduleName) == std::string::npos) continue;
            uintptr_t s, e; char perms[5] = {0};
            if (sscanf(line.c_str(), "%lx-%lx %4s", &s, &e, perms) == 3) {
                segs.push_back({s, e, perms[2]=='x', perms[0]=='r', perms[1]=='w'});
            }
        }
        return segs;
    }

    // ═══════════════════ Adaptive GNames Validator ═══════════════════
    // Returns a confidence score 0-100. Tries multiple stride/header combos
    // for custom engines that modify FNamePool layout.

    struct FNameConfig {
        uintptr_t poolOffset;    // GNamesToFNamePool
        uintptr_t blocksOffset;  // FNamePoolToBlocks
        uintptr_t blockField;    // FNamePoolToCurrentBlock
        uintptr_t cursorField;   // FNamePoolToCurrentByteCursor
        uintptr_t stride;
    };

    static const FNameConfig kFNameConfigs[] = {
        {0x0, 0x10, 0x8, 0xC, 0x2},  // Standard UE4.23-4.27
        {0x0, 0x10, 0x8, 0xC, 0x4},  // UE5.0+
        {0x0, 0x10, 0x8, 0xC, 0x1},  // Some custom engines use stride 1
        {0x0, 0x20, 0x8, 0xC, 0x2},  // Custom: blocks at 0x20
        {0x0, 0x18, 0x8, 0xC, 0x2},  // Custom: blocks at 0x18
        {0x0, 0x10, 0x10, 0x14, 0x2}, // Custom: shifted block/cursor fields
    };

    int ValidateGNames(uintptr_t base, uintptr_t offset, FNameConfig& outConfig) {
        if (offset == 0) return 0;
        uintptr_t addr = base + offset;
        int bestScore = 0;

        for (const auto& cfg : kFNameConfigs) {
            int score = 0;
            uintptr_t pool = addr + cfg.poolOffset;

            uint32_t curBlock = Read<uint32_t>(pool + cfg.blockField);
            uint32_t curCursor = Read<uint32_t>(pool + cfg.cursorField);

            if (curBlock == 0 || curBlock >= 8192) continue;
            if (curCursor == 0 || curCursor >= 0x40000) continue;
            score += 25; // Block/cursor values are sane

            uintptr_t firstBlock = Read<uintptr_t>(pool + cfg.blocksOffset);
            if (firstBlock < 0x10000000) continue;
            score += 25; // First block is a valid pointer

            // Deep validation: try to read a FName entry from block 0
            uintptr_t entry = firstBlock;
            int16_t header = Read<int16_t>(entry);
            int strLen = header >> 6;
            if (strLen > 0 && strLen < 250) {
                score += 20; // First entry has valid header

                // Try reading the string — check for printable ASCII
                std::string name = ReadStringNew(entry + 2, std::min(strLen, 32));
                int printable = 0;
                for (char c : name) {
                    if (c >= 0x20 && c <= 0x7E) printable++;
                }
                if (printable > 0 && printable >= (int)name.size() / 2) {
                    score += 15; // String looks like valid ASCII
                }

                // Check block 0, entry 1 (should be "None" at index 0 in most UE builds)
                if (name.find("None") != std::string::npos || strLen <= 8) {
                    score += 15; // Likely the "None" entry
                }
            }

            if (score > bestScore) {
                bestScore = score;
                outConfig = cfg;
            }
        }
        return bestScore;
    }

    // ═══════════════════ GUObjectArray Validator ═══════════════════
    // Returns confidence score 0-100

    int ValidateGUObjectArray(uintptr_t base, uintptr_t offset) {
        if (offset == 0) return 0;
        uintptr_t addr = base + offset;
        int score = 0;

        // Try standard layout: FUObjectArray → TUObjectArray at +0x10
        int32_t num1 = Read<int32_t>(addr + 0x10 + 0x14);
        if (num1 > 100 && num1 < 2000000) {
            score += 40;
            // Deeper: read the Objects pointer from TUObjectArray
            uintptr_t objPtr = Read<uintptr_t>(addr + 0x10);
            if (objPtr > 0x10000000) {
                score += 20;
                // Try reading first chunk
                uintptr_t chunk0 = Read<uintptr_t>(objPtr);
                if (chunk0 > 0x10000000) score += 20;
                // Try reading first UObject pointer
                uintptr_t obj0 = Read<uintptr_t>(chunk0);
                if (obj0 > 0x1000) score += 20;
            }
            return score;
        }

        // Try without 0x10 offset (some custom engines)
        int32_t num2 = Read<int32_t>(addr + 0x14);
        if (num2 > 100 && num2 < 2000000) {
            score += 30;
            uintptr_t objPtr = Read<uintptr_t>(addr);
            if (objPtr > 0x10000000) score += 20;
        }

        // Try alternative: TUObjectArray at +0x18 (seen in some custom builds)
        int32_t num3 = Read<int32_t>(addr + 0x18 + 0x14);
        if (num3 > 100 && num3 < 2000000) {
            score += 25;
        }

        return score;
    }

    // ═══════════════════ GWorld Validator ═══════════════════
    // Returns confidence score 0-100

    int ValidateGWorld(uintptr_t base, uintptr_t offset) {
        if (offset == 0) return 0;
        uintptr_t addr = base + offset;
        int score = 0;

        uintptr_t worldPtr = Read<uintptr_t>(addr);
        if (worldPtr < 0x1000) return 0;
        score += 20;

        // ClassPrivate should be valid
        uintptr_t classPtr = Read<uintptr_t>(worldPtr + 0x10);
        if (classPtr > 0x1000) score += 20;

        // InternalIndex should be small positive
        int32_t intIdx = Read<int32_t>(worldPtr + 0xC);
        if (intIdx > 0 && intIdx < 500000) score += 15;

        // FNameIndex should be small-ish
        uint32_t nameIdx = Read<uint32_t>(worldPtr + 0x18);
        if (nameIdx > 0 && nameIdx < 500000) score += 15;

        // OuterPrivate for World is usually the package (valid pointer)
        uintptr_t outer = Read<uintptr_t>(worldPtr + 0x20);
        if (outer > 0x1000) score += 15;

        // PersistentLevel should be a valid pointer
        uintptr_t level = Read<uintptr_t>(worldPtr + Offsets::UWorldToPersistentLevel);
        if (level > 0x1000) score += 15;

        return score;
    }

    // ═══════════════════ Pattern-Based Search ═══════════════════

    template<typename Validator>
    uintptr_t FindViaPatterns(uintptr_t codeBase, size_t codeSize,
                              const std::vector<PatternScanner::Pattern>& patterns,
                              uintptr_t modBase, uintptr_t modEnd, Validator validator,
                              int& bestScore) {
        uintptr_t bestOffset = 0;
        bestScore = 0;

        for (const auto& pat : patterns) {
            auto matches = PatternScanner::FindAllPatterns(codeBase, codeSize, pat);
            for (uintptr_t matchOff : matches) {
                uintptr_t matchAddr = codeBase + matchOff;
                // Search within function boundary
                uintptr_t funcStart = PatternScanner::FindFunctionStart(matchAddr, codeBase);
                uintptr_t searchStart = funcStart;
                uintptr_t searchEnd = matchAddr + 200;

                for (uintptr_t pc = searchStart; pc < searchEnd; pc += 4) {
                    uintptr_t candidate = PatternScanner::ResolveAdrpPair(pc);
                    if (candidate <= modBase || candidate >= modEnd) {
                        // Also try standalone ADR
                        candidate = PatternScanner::ResolveADR(pc);
                        if (candidate <= modBase || candidate >= modEnd) continue;
                    }
                    uintptr_t rel = candidate - modBase;
                    int score = validator(rel);
                    if (score > bestScore) {
                        bestScore = score;
                        bestOffset = rel;
                    }
                }
            }
            if (bestScore >= 60) break; // High confidence, stop searching
        }
        return bestOffset;
    }

    // ═══════════════════ String Markers for Custom Engines ═══════════════════

    static const char* kGNamesStrings[] = {
        "FNamePool", "NamePoolData", "FName", "NamePool",
        "FNameEntry", "GFNameTableForDebuggerVisualizers_MT", nullptr
    };

    static const char* kGUObjectStrings[] = {
        "ObjObjects", "GUObjectArray", "UObjectArray",
        "FUObjectArray", "ObjHash", nullptr
    };

    static const char* kGWorldStrings[] = {
        "GWorld", "WorldContext", "UWorld", nullptr
    };

    // ═══════════════════ Main Entry Point ═══════════════════

    FindResult FindOffsets() {
        FindResult result;
        result.status = "Starting scan...";

        uintptr_t base = Memory.ModuleBase;
        uintptr_t modEnd = Memory.ModuleEnd;
        size_t modSize = modEnd - base;
        if (base == 0 || modSize == 0) { result.status = "Error: Module not loaded"; return result; }

        auto segs = GetModuleSegments(Memory.TargetProcess);
        if (segs.empty()) { result.status = "Error: Could not read maps"; return result; }

        uintptr_t codeBase = 0, codeEnd = 0, dataBase = 0, dataEnd = 0;
        for (auto& s : segs) {
            if (s.isExecutable && s.isReadable) {
                if (codeBase == 0 || s.start < codeBase) codeBase = s.start;
                if (s.end > codeEnd) codeEnd = s.end;
            }
            if (s.isReadable && !s.isExecutable) {
                if (dataBase == 0 || s.start < dataBase) dataBase = s.start;
                if (s.end > dataEnd) dataEnd = s.end;
            }
        }
        if (codeBase == 0) { result.status = "Error: No code segment"; return result; }
        size_t codeSize = codeEnd - codeBase;
        size_t dataSize = (dataEnd > dataBase) ? (dataEnd - dataBase) : 0;

        FNameConfig bestFNameCfg = kFNameConfigs[0];

        // ─── Strategy 1: String XREF Search ───
        auto stringXrefSearch = [&](const char** strings, uintptr_t searchBase, size_t searchSize,
                                     auto validator, uintptr_t& outOffset, int& outScore) {
            for (int si = 0; strings[si] != nullptr; si++) {
                uintptr_t strAddr = PatternScanner::FindStringInMemory(searchBase, searchSize, strings[si]);
                if (strAddr == 0 && searchBase != base)
                    strAddr = PatternScanner::FindStringInMemory(base, modSize, strings[si]);
                if (strAddr == 0) continue;

                auto xrefs = PatternScanner::FindAllXrefs(codeBase, codeSize, strAddr);
                for (uintptr_t xref : xrefs) {
                    uintptr_t funcStart = PatternScanner::FindFunctionStart(xref, codeBase);
                    for (uintptr_t pc = funcStart; pc < xref + 120; pc += 4) {
                        uintptr_t candidate = PatternScanner::ResolveAdrpPair(pc);
                        if (candidate <= base || candidate >= modEnd) continue;
                        uintptr_t rel = candidate - base;
                        int score = validator(rel);
                        if (score > outScore) { outScore = score; outOffset = rel; }
                    }
                }
                if (outScore >= 60) break;
            }
        };

        // ─── 1. Find GNames ───
        if (dataSize > 0) {
            stringXrefSearch(kGNamesStrings, dataBase, dataSize,
                [&](uintptr_t rel) { return ValidateGNames(base, rel, bestFNameCfg); },
                result.GNames, result.gnamesScore);
        }

        // Pattern fallback for GNames
        if (result.gnamesScore < 50) {
            std::vector<PatternScanner::Pattern> pats = {
                PatternScanner::Pattern("E0 03 00 AA ?? ?? ?? ?? ?? ?? ?? 91 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 94"),
                PatternScanner::Pattern("?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 08 1A 40 F9 00 01 3F D6"),
                PatternScanner::Pattern("08 5A 40 F9 ?? ?? ?? ?? 08 01 40 F9 ?? ?? ?? ?? 00 00 00 90 00 00 00 91"),
            };
            int patScore = 0;
            uintptr_t patOffset = FindViaPatterns(codeBase, codeSize, pats, base, modEnd,
                [&](uintptr_t rel) { return ValidateGNames(base, rel, bestFNameCfg); }, patScore);
            if (patScore > result.gnamesScore) {
                result.GNames = patOffset;
                result.gnamesScore = patScore;
            }
        }

        // ─── 2. Find GUObjectArray ───
        stringXrefSearch(kGUObjectStrings, base, modSize,
            [&](uintptr_t rel) { return ValidateGUObjectArray(base, rel); },
            result.GUObjectArray, result.guobjectScore);

        if (result.guobjectScore < 50) {
            std::vector<PatternScanner::Pattern> pats = {
                PatternScanner::Pattern("?? ?? ?? ?? 08 0A 40 F9 ?? ?? ?? ?? 08 01 40 F9 08 01 40 F9 00 01 3F D6"),
                PatternScanner::Pattern("?? ?? ?? ?? ?? ?? ?? ?? 08 0A 40 F9 08 01 40 F9 00 01 3F D6"),
                PatternScanner::Pattern("?? ?? ?? ?? 08 0A 40 F9 ?? ?? ?? ?? 08 01 40 F9"),
            };
            int patScore = 0;
            uintptr_t patOffset = FindViaPatterns(codeBase, codeSize, pats, base, modEnd,
                [&](uintptr_t rel) { return ValidateGUObjectArray(base, rel); }, patScore);
            if (patScore > result.guobjectScore) {
                result.GUObjectArray = patOffset;
                result.guobjectScore = patScore;
            }
        }

        // ─── 3. Find GWorld ───
        stringXrefSearch(kGWorldStrings, base, modSize,
            [&](uintptr_t rel) { return ValidateGWorld(base, rel); },
            result.GWorld, result.gworldScore);

        if (result.gworldScore < 50) {
            std::vector<PatternScanner::Pattern> pats = {
                PatternScanner::Pattern("?? ?? ?? ?? 08 0A 40 F9 ?? ?? ?? ?? 08 01 40 F9 00 01 3F D6"),
                PatternScanner::Pattern("08 5A 40 F9 ?? ?? ?? ?? 08 0A 40 F9 ?? ?? ?? ?? 08 01 40 F9 00 01 3F D6"),
            };
            int patScore = 0;
            uintptr_t patOffset = FindViaPatterns(codeBase, codeSize, pats, base, modEnd,
                [&](uintptr_t rel) { return ValidateGWorld(base, rel); }, patScore);
            if (patScore > result.gworldScore) {
                result.GWorld = patOffset;
                result.gworldScore = patScore;
            }
        }

        // ─── 4. Merged Brute-Force (single pass for all 3) ───
        bool needBrute = (result.gnamesScore < 40 || result.guobjectScore < 40 || result.gworldScore < 40);
        if (needBrute && dataSize > 0) {
            const size_t ptrSize = sizeof(void*);
            const size_t chunkPtrs = 256 * 1024 / ptrSize;
            size_t totalPtrs = dataSize / ptrSize;

            for (size_t i = 0; i < totalPtrs; i += chunkPtrs) {
                size_t count = std::min(chunkPtrs, totalPtrs - i);
                size_t bytes = count * ptrSize;
                uintptr_t chunkAddr = dataBase + (i * ptrSize);

                std::vector<uint8_t> buf;
                PatternScanner::ReadRegion(chunkAddr, bytes, buf);

                for (size_t j = 0; j < count; j++) {
                    uintptr_t relOff = (chunkAddr + (j * ptrSize)) - base;

                    if (result.gnamesScore < 40) {
                        FNameConfig cfg;
                        int s = ValidateGNames(base, relOff, cfg);
                        if (s > result.gnamesScore) {
                            result.GNames = relOff;
                            result.gnamesScore = s;
                            bestFNameCfg = cfg;
                        }
                    }
                    if (result.guobjectScore < 40) {
                        int s = ValidateGUObjectArray(base, relOff);
                        if (s > result.guobjectScore) {
                            result.GUObjectArray = relOff;
                            result.guobjectScore = s;
                        }
                    }
                    if (result.gworldScore < 40) {
                        int s = ValidateGWorld(base, relOff);
                        if (s > result.gworldScore) {
                            result.GWorld = relOff;
                            result.gworldScore = s;
                        }
                    }
                }

                if (result.gnamesScore >= 40 && result.guobjectScore >= 40 && result.gworldScore >= 40) break;
            }
        }

        // ─── Determine UE5 from stride ───
        if (bestFNameCfg.stride == 0x4) result.isUE5 = true;

        // ─── Count results ───
        if (result.GNames != 0) result.foundCount++;
        if (result.GUObjectArray != 0) result.foundCount++;
        if (result.GWorld != 0) result.foundCount++;

        // ─── Status ───
        std::ostringstream ss;
        ss << result.foundCount << "/3 | GNames:0x" << std::hex << result.GNames << "(" << std::dec << result.gnamesScore << "%)"
           << " GUObj:0x" << std::hex << result.GUObjectArray << "(" << std::dec << result.guobjectScore << "%)"
           << " GWorld:0x" << std::hex << result.GWorld << "(" << std::dec << result.gworldScore << "%)"
           << (result.isUE5 ? " UE5" : " UE4.23+");
        result.status = ss.str();
        return result;
    }

    // ═══════════════════ Apply Offsets ═══════════════════

    void ApplyOffsets(const FindResult& result) {
        if (result.GNames != 0)        Offsets::GNames = result.GNames;
        if (result.GUObjectArray != 0)  Offsets::GUObjectArray = result.GUObjectArray;
        if (result.GWorld != 0)         Offsets::GWorld = result.GWorld;

        Offsets::isUE423 = true; // Always 4.23+ now
        Offsets::isUE5 = result.isUE5;
        Offsets::FNameStride = result.isUE5 ? 0x4 : 0x2;
    }
}
