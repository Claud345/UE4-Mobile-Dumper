//
// PatternScanner.h — ARM64 bytecode analysis & pattern matching engine
// Expanded decoder set for custom UE engines (ADR, MOVZ/MOVK, LDR32, STR, B/BL)
//

#pragma once
#include "Memory.h"

namespace PatternScanner {

    // ═══════════════════ Pattern Struct ═══════════════════
    struct Pattern {
        std::vector<uint8_t> bytes;
        std::vector<bool>    mask;
        Pattern() = default;
        Pattern(const char* s) {
            std::istringstream iss(s);
            std::string tok;
            while (iss >> tok) {
                if (tok == "??" || tok == "?") { bytes.push_back(0); mask.push_back(false); }
                else { bytes.push_back((uint8_t)strtol(tok.c_str(), nullptr, 16)); mask.push_back(true); }
            }
        }
        size_t size() const { return bytes.size(); }
    };

    // ═══════════════════ Buffer Reader ═══════════════════
    // Reads a memory region into a local buffer (works in both modes)
    static bool ReadRegion(uintptr_t base, size_t size, std::vector<uint8_t>& buf) {
        buf.resize(size);
        if (gDumperMode == MODE_STANDALONE) {
            return gRemoteProcess.readMemory(base, buf.data(), size) > 0;
        } else {
            return SafeRead::safeCopy(buf.data(), reinterpret_cast<void*>(base), size);
        }
    }

    static bool ReadRegion32(uintptr_t base, size_t count, std::vector<uint32_t>& buf) {
        buf.resize(count);
        if (gDumperMode == MODE_STANDALONE) {
            return gRemoteProcess.readMemory(base, buf.data(), count * 4) > 0;
        } else {
            return SafeRead::safeCopy(buf.data(), reinterpret_cast<void*>(base), count * 4);
        }
    }

    // ═══════════════════ Pattern Scan ═══════════════════
    // Returns UINTPTR_MAX on failure (fixes the offset-0 bug from original code)
    static constexpr uintptr_t NOT_FOUND = UINTPTR_MAX;

    uintptr_t FindPattern(uintptr_t regionBase, size_t regionSize, const Pattern& pat) {
        if (pat.size() == 0 || regionSize < pat.size()) return NOT_FOUND;
        std::vector<uint8_t> buf;
        if (!ReadRegion(regionBase, regionSize, buf)) return NOT_FOUND;

        size_t end = regionSize - pat.size();
        for (size_t i = 0; i <= end; i++) {
            bool ok = true;
            for (size_t j = 0; j < pat.size() && ok; j++)
                if (pat.mask[j] && buf[i+j] != pat.bytes[j]) ok = false;
            if (ok) return i;
        }
        return NOT_FOUND;
    }

    std::vector<uintptr_t> FindAllPatterns(uintptr_t regionBase, size_t regionSize, const Pattern& pat) {
        std::vector<uintptr_t> results;
        if (pat.size() == 0 || regionSize < pat.size()) return results;
        std::vector<uint8_t> buf;
        if (!ReadRegion(regionBase, regionSize, buf)) return results;

        size_t end = regionSize - pat.size();
        for (size_t i = 0; i <= end; i++) {
            bool ok = true;
            for (size_t j = 0; j < pat.size() && ok; j++)
                if (pat.mask[j] && buf[i+j] != pat.bytes[j]) ok = false;
            if (ok) results.push_back(i);
        }
        return results;
    }

    // ═══════════════════ ARM64 Decoders ═══════════════════

    // ADRP Xd, #imm → Xd = (PC & ~0xFFF) + (imm << 12)
    bool DecodeADRP(uint32_t insn, uintptr_t pc, uintptr_t& result) {
        if ((insn & 0x9F000000) != 0x90000000) return false;
        int64_t immHi = (int64_t)((insn >> 5) & 0x7FFFF);
        int64_t immLo = (int64_t)((insn >> 29) & 0x3);
        int64_t imm = (immHi << 2) | immLo;
        if (imm & (1LL << 20)) imm |= ~((1LL << 21) - 1);
        result = (pc & ~0xFFFULL) + (imm << 12);
        return true;
    }

    // ADR Xd, #imm → Xd = PC + imm (21-bit signed)
    bool DecodeADR(uint32_t insn, uintptr_t pc, uintptr_t& result) {
        if ((insn & 0x9F000000) != 0x10000000) return false;
        int64_t immHi = (int64_t)((insn >> 5) & 0x7FFFF);
        int64_t immLo = (int64_t)((insn >> 29) & 0x3);
        int64_t imm = (immHi << 2) | immLo;
        if (imm & (1LL << 20)) imm |= ~((1LL << 21) - 1);
        result = pc + imm;
        return true;
    }

    // ADD Xd, Xn, #imm (64-bit immediate)
    bool DecodeADDImm(uint32_t insn, uint32_t& imm) {
        if ((insn & 0xFF800000) != 0x91000000) return false;
        uint32_t shift = (insn >> 22) & 0x3;
        imm = (insn >> 10) & 0xFFF;
        if (shift == 1) imm <<= 12;
        return true;
    }

    // LDR Xt, [Xn, #imm] (64-bit unsigned offset)
    bool DecodeLDRImm64(uint32_t insn, uint32_t& imm) {
        if ((insn & 0xFFC00000) != 0xF9400000) return false;
        imm = ((insn >> 10) & 0xFFF) << 3;
        return true;
    }

    // LDR Wt, [Xn, #imm] (32-bit unsigned offset)
    bool DecodeLDRImm32(uint32_t insn, uint32_t& imm) {
        if ((insn & 0xFFC00000) != 0xB9400000) return false;
        imm = ((insn >> 10) & 0xFFF) << 2;
        return true;
    }

    // STR Xt, [Xn, #imm] (64-bit unsigned offset) — for finding stores to globals
    bool DecodeSTRImm64(uint32_t insn, uint32_t& imm) {
        if ((insn & 0xFFC00000) != 0xF9000000) return false;
        imm = ((insn >> 10) & 0xFFF) << 3;
        return true;
    }

    // B/BL #imm — decode branch target
    bool DecodeBranch(uint32_t insn, uintptr_t pc, uintptr_t& target) {
        bool isBL = (insn & 0xFC000000) == 0x94000000;
        bool isB  = (insn & 0xFC000000) == 0x14000000;
        if (!isBL && !isB) return false;
        int64_t imm = (int64_t)(insn & 0x03FFFFFF);
        if (imm & (1LL << 25)) imm |= ~((1LL << 26) - 1);
        target = pc + (imm << 2);
        return true;
    }

    // Resolve ADRP + {ADD|LDR64|LDR32|STR64} pair at instrAddr
    uintptr_t ResolveAdrpPair(uintptr_t instrAddr) {
        uint32_t insn1 = Read<uint32_t>(instrAddr);
        uint32_t insn2 = Read<uint32_t>(instrAddr + 4);

        uintptr_t pageAddr = 0;
        if (!DecodeADRP(insn1, instrAddr, pageAddr)) return 0;

        uint32_t off = 0;
        if (DecodeADDImm(insn2, off))   return pageAddr + off;
        if (DecodeLDRImm64(insn2, off)) return pageAddr + off;
        if (DecodeLDRImm32(insn2, off)) return pageAddr + off;
        if (DecodeSTRImm64(insn2, off)) return pageAddr + off;
        return 0;
    }

    // Resolve standalone ADR instruction
    uintptr_t ResolveADR(uintptr_t instrAddr) {
        uint32_t insn = Read<uint32_t>(instrAddr);
        uintptr_t result = 0;
        if (DecodeADR(insn, instrAddr, result)) return result;
        return 0;
    }

    // ═══════════════════ Function Boundary Detection ═══════════════════
    // Detects STP X29,X30,[SP,#-N]! prologues to bound searches within functions

    bool IsFunctionPrologue(uint32_t insn) {
        // STP X29, X30, [SP, #imm]! — common ARM64 function prologue
        // Encoding: x010 1001 1xxx xxxx x111 0111 1110 1001  (varies)
        // Simplified: look for STP with X29(FP) and X30(LR)
        if ((insn & 0xFFE003E0) == 0xA98003E0) return true;  // STP X29,X30 pre-index
        if ((insn & 0xFFE003E0) == 0xA90003E0) return true;  // STP X29,X30 signed offset
        return false;
    }

    // Find function start by scanning backwards for prologue
    uintptr_t FindFunctionStart(uintptr_t addr, uintptr_t codeBase, size_t maxBackScan = 256) {
        size_t maxInsns = maxBackScan / 4;
        for (size_t i = 1; i <= maxInsns; i++) {
            uintptr_t candidate = addr - (i * 4);
            if (candidate < codeBase) break;
            uint32_t insn = Read<uint32_t>(candidate);
            if (IsFunctionPrologue(insn)) return candidate;
        }
        return addr - maxBackScan; // fallback: use max range
    }

    // ═══════════════════ String Search ═══════════════════

    uintptr_t FindStringInMemory(uintptr_t base, size_t size, const char* str) {
        size_t strLen = strlen(str);
        if (strLen == 0 || size < strLen) return 0;
        std::vector<uint8_t> buf;
        if (!ReadRegion(base, size, buf)) return 0;
        for (size_t i = 0; i <= size - strLen; i++) {
            if (memcmp(buf.data() + i, str, strLen) == 0) return base + i;
        }
        return 0;
    }

    // ═══════════════════ XREF Finder ═══════════════════
    // Find ADRP+ADD/LDR or ADR in code that references targetAddr

    std::vector<uintptr_t> FindAllXrefs(uintptr_t codeBase, size_t codeSize, uintptr_t targetAddr) {
        std::vector<uintptr_t> xrefs;
        if (codeSize < 8) return xrefs;

        size_t numInsns = codeSize / 4;
        std::vector<uint32_t> insns;
        if (!ReadRegion32(codeBase, numInsns, insns)) return xrefs;

        uintptr_t targetPage = targetAddr & ~0xFFFULL;
        uint32_t targetPageOff = targetAddr & 0xFFF;

        for (size_t i = 0; i < numInsns; i++) {
            uintptr_t pc = codeBase + (i * 4);

            // Check ADR
            uintptr_t adrResult = 0;
            if (DecodeADR(insns[i], pc, adrResult) && adrResult == targetAddr) {
                xrefs.push_back(pc);
                continue;
            }

            // Check ADRP+next
            if (i + 1 < numInsns && (insns[i] & 0x9F000000) == 0x90000000) {
                uintptr_t pageAddr = 0;
                if (!DecodeADRP(insns[i], pc, pageAddr)) continue;
                if (pageAddr != targetPage) continue;

                uint32_t off = 0;
                if ((DecodeADDImm(insns[i+1], off) || DecodeLDRImm64(insns[i+1], off) ||
                     DecodeLDRImm32(insns[i+1], off) || DecodeSTRImm64(insns[i+1], off))
                    && off == targetPageOff) {
                    xrefs.push_back(pc);
                }
            }
        }
        return xrefs;
    }

    uintptr_t FindXrefToAddress(uintptr_t codeBase, size_t codeSize, uintptr_t targetAddr) {
        auto xrefs = FindAllXrefs(codeBase, codeSize, targetAddr);
        return xrefs.empty() ? 0 : xrefs[0];
    }
}
