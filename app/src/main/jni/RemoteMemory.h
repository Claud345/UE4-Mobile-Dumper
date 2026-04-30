//
// Created by Ascarre on 29-04-2026.
// Remote (cross-process) memory reading for standalone mode
// v2.0 — Hardened against S-tier kernel-level anti-cheat (ACE, GameGuard, etc.)
//

#pragma once

#include <sys/uio.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <mutex>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <linux/unistd.h>

// ════════════════════════════════════════════════════════════════════
// Anti-Cheat Evasion: Raw Syscall Wrappers
// Bypasses libc function hooking by invoking syscalls directly via SVC.
// Kernel-level ACs hook libc's process_vm_readv / open / read.
// Direct SVC instructions skip those hooks entirely.
// ════════════════════════════════════════════════════════════════════

namespace RawSyscall {

    // Direct syscall for process_vm_readv (syscall 270 on ARM64)
    // Bypasses any libc-level hooking of process_vm_readv
    static inline ssize_t vm_readv(pid_t pid, const struct iovec* local_iov, unsigned long liovcnt,
                                   const struct iovec* remote_iov, unsigned long riovcnt, unsigned long flags) {
        return syscall(__NR_process_vm_readv, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);
    }

    // Direct syscall for openat (used to open /proc/pid/maps without libc)
    static inline int openat_raw(int dirfd, const char* pathname, int flags) {
        return syscall(__NR_openat, dirfd, pathname, flags, 0);
    }

    // Direct syscall for read
    static inline ssize_t read_raw(int fd, void* buf, size_t count) {
        return syscall(__NR_read, fd, buf, count);
    }

    // Direct syscall for close
    static inline int close_raw(int fd) {
        return syscall(__NR_close, fd);
    }

    // Direct syscall for pread64 (used for /proc/pid/mem fallback)
    static inline ssize_t pread64_raw(int fd, void* buf, size_t count, off_t offset) {
        return syscall(__NR_pread64, fd, buf, count, offset);
    }

    // Direct syscall for nanosleep (jitter timing)
    static inline int nanosleep_raw(const struct timespec* req, struct timespec* rem) {
        return syscall(__NR_nanosleep, req, rem);
    }
}

// ════════════════════════════════════════════════════════════════════
// Read Strategy
// ════════════════════════════════════════════════════════════════════

enum ReadStrategy {
    STRAT_VM_READV = 0,  // process_vm_readv via raw syscall
    STRAT_PROC_MEM = 1,  // /proc/pid/mem via pread64 raw syscall
};

// ════════════════════════════════════════════════════════════════════
// Page Cache: Reduces syscall frequency by caching 4KB memory pages.
// S-tier ACs detect rapid sequential cross-process reads.
// ════════════════════════════════════════════════════════════════════

class PageCache {
private:
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t MAX_CACHE_PAGES = 4096; // 16MB max cache

    struct CachedPage {
        uint8_t data[PAGE_SIZE];
        uint64_t accessCount;
    };

    std::unordered_map<uintptr_t, CachedPage> cache;
    std::mutex cacheMutex;
    uint64_t globalAccessCounter = 0;

public:
    void clear() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cache.clear();
        globalAccessCounter = 0;
    }

    bool lookup(uintptr_t pageAlignedAddr, uint8_t* outData) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(pageAlignedAddr);
        if (it != cache.end()) {
            memcpy(outData, it->second.data, PAGE_SIZE);
            it->second.accessCount = ++globalAccessCounter;
            return true;
        }
        return false;
    }

    void store(uintptr_t pageAlignedAddr, const uint8_t* data) {
        std::lock_guard<std::mutex> lock(cacheMutex);

        // Evict oldest page if cache is full
        if (cache.size() >= MAX_CACHE_PAGES) {
            uintptr_t oldestAddr = 0;
            uint64_t oldestAccess = UINT64_MAX;
            for (auto& kv : cache) {
                if (kv.second.accessCount < oldestAccess) {
                    oldestAccess = kv.second.accessCount;
                    oldestAddr = kv.first;
                }
            }
            cache.erase(oldestAddr);
        }

        CachedPage& page = cache[pageAlignedAddr];
        memcpy(page.data, data, PAGE_SIZE);
        page.accessCount = ++globalAccessCounter;
    }

    void invalidate(uintptr_t addr, size_t size) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        uintptr_t pageStart = addr & ~(PAGE_SIZE - 1);
        uintptr_t pageEnd = (addr + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        for (uintptr_t p = pageStart; p < pageEnd; p += PAGE_SIZE) {
            cache.erase(p);
        }
    }
};

// ════════════════════════════════════════════════════════════════════
// SIGSEGV / SIGBUS Safe-Read Guard
// Catches invalid pointer dereferences in injected mode
// ════════════════════════════════════════════════════════════════════

namespace SafeRead {
    static thread_local sigjmp_buf jumpBuf;
    static thread_local bool guardActive = false;

    static void signalHandler(int sig) {
        if (guardActive) {
            siglongjmp(jumpBuf, 1);
        }
    }

    static void install() {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_NODEFER;
        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
    }

    // Safe memcpy that catches SIGSEGV/SIGBUS.
    // Returns true on success, false if the read faulted.
    static bool safeCopy(void* dst, const void* src, size_t size) {
        guardActive = true;
        if (sigsetjmp(jumpBuf, 1) == 0) {
            memcpy(dst, src, size);
            guardActive = false;
            return true;
        } else {
            // Faulted: zero out destination for safety
            memset(dst, 0, size);
            guardActive = false;
            return false;
        }
    }
}

// ════════════════════════════════════════════════════════════════════
// Module Segment Cache
// Caches parsed /proc/pid/maps to avoid repeated reads that AC monitors.
// ════════════════════════════════════════════════════════════════════

struct CachedModuleInfo {
    uintptr_t base = 0;
    uintptr_t end = 0;
    bool valid = false;
};

// ════════════════════════════════════════════════════════════════════
// RemoteProcess — Hardened Cross-Process Memory Reader
// ════════════════════════════════════════════════════════════════════

class RemoteProcess {
private:
    pid_t targetPid = -1;
    ReadStrategy strategy = STRAT_VM_READV;
    int memFd = -1;  // fd for /proc/pid/mem (fallback strategy)
    PageCache pageCache;
    uint64_t readCount = 0;

    // Module info cache to avoid re-reading maps
    std::unordered_map<std::string, CachedModuleInfo> moduleCache;
    bool moduleCacheValid = false;

    // Anti-cheat: Jitter timing between read bursts
    void jitterDelay() {
        // Every 64 reads, insert a random small delay to avoid timing anomaly detection
        if ((++readCount & 0x3F) == 0) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (rand() % 500) * 1000; // 0-500 microseconds
            RawSyscall::nanosleep_raw(&ts, nullptr);
        }
    }

    // Read memory using process_vm_readv (raw syscall)
    size_t readVmReadv(uintptr_t address, void* buffer, size_t size) {
        struct iovec local_iov;
        struct iovec remote_iov;

        local_iov.iov_base = buffer;
        local_iov.iov_len = size;
        remote_iov.iov_base = (void*)address;
        remote_iov.iov_len = size;

        ssize_t nread = RawSyscall::vm_readv(targetPid, &local_iov, 1, &remote_iov, 1, 0);
        return nread > 0 ? (size_t)nread : 0;
    }

    // Read memory using /proc/pid/mem + pread64 (raw syscall)
    // This is often less monitored than process_vm_readv by kernel-level AC
    size_t readProcMem(uintptr_t address, void* buffer, size_t size) {
        if (memFd < 0) {
            char path[64];
            snprintf(path, sizeof(path), "/proc/%d/mem", targetPid);
            memFd = RawSyscall::openat_raw(AT_FDCWD, path, O_RDONLY);
            if (memFd < 0) return 0;
        }

        ssize_t nread = RawSyscall::pread64_raw(memFd, buffer, size, (off_t)address);
        return nread > 0 ? (size_t)nread : 0;
    }

    // Parse /proc/pid/maps using raw syscalls only (no fopen/fgets)
    // This avoids libc hooks that AC uses to monitor who reads maps
    std::string readMapsRaw() {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/maps", targetPid);

        int fd = RawSyscall::openat_raw(AT_FDCWD, path, O_RDONLY);
        if (fd < 0) return "";

        std::string content;
        char buf[4096];
        ssize_t n;
        while ((n = RawSyscall::read_raw(fd, buf, sizeof(buf))) > 0) {
            content.append(buf, n);
        }
        RawSyscall::close_raw(fd);
        return content;
    }

    // Parse module info from maps string and populate cache
    void parseAndCacheModules() {
        std::string maps = readMapsRaw();
        if (maps.empty()) return;

        moduleCache.clear();

        std::istringstream iss(maps);
        std::string line;
        while (std::getline(iss, line)) {
            uintptr_t start = 0, end = 0;
            char perms[5] = {0};

            if (sscanf(line.c_str(), "%lx-%lx %4s", &start, &end, perms) != 3) continue;

            // Extract module name (last field after spaces)
            size_t lastSlash = line.rfind('/');
            if (lastSlash == std::string::npos) continue;
            std::string moduleName = line.substr(lastSlash + 1);

            // Trim trailing whitespace/newline
            while (!moduleName.empty() && (moduleName.back() == '\n' || moduleName.back() == '\r' || moduleName.back() == ' ')) {
                moduleName.pop_back();
            }
            if (moduleName.empty()) continue;

            auto& info = moduleCache[moduleName];
            if (!info.valid || start < info.base) info.base = start;
            if (end > info.end) info.end = end;
            info.valid = true;
        }
        moduleCacheValid = true;
    }

public:
    void setPid(pid_t pid) {
        targetPid = pid;
        moduleCacheValid = false;
        pageCache.clear();
        if (memFd >= 0) {
            RawSyscall::close_raw(memFd);
            memFd = -1;
        }
    }

    pid_t getPid() const { return targetPid; }
    bool isAttached() const { return targetPid > 0; }

    void setStrategy(ReadStrategy strat) {
        strategy = strat;
        if (strat != STRAT_PROC_MEM && memFd >= 0) {
            RawSyscall::close_raw(memFd);
            memFd = -1;
        }
    }

    ReadStrategy getStrategy() const { return strategy; }

    void invalidateCache() {
        pageCache.clear();
        moduleCacheValid = false;
        moduleCache.clear();
    }

    // ════════════════════ Core Memory Read ════════════════════

    // Read memory from another process, with caching + AC evasion
    size_t readMemory(uintptr_t address, void* buffer, size_t size) {
        if (targetPid <= 0 || size == 0) return 0;

        // For small reads (≤4KB), try page cache first
        static constexpr size_t PAGE_SIZE = 4096;
        if (size <= PAGE_SIZE) {
            uintptr_t pageAddr = address & ~(PAGE_SIZE - 1);
            uint32_t pageOffset = address & (PAGE_SIZE - 1);

            // Check if entire read fits within one page
            if (pageOffset + size <= PAGE_SIZE) {
                uint8_t pageData[PAGE_SIZE];
                if (pageCache.lookup(pageAddr, pageData)) {
                    memcpy(buffer, pageData + pageOffset, size);
                    return size;
                }

                // Cache miss: read full page and cache it
                jitterDelay();
                size_t nread;
                if (strategy == STRAT_PROC_MEM) {
                    nread = readProcMem(pageAddr, pageData, PAGE_SIZE);
                } else {
                    nread = readVmReadv(pageAddr, pageData, PAGE_SIZE);
                }

                if (nread == PAGE_SIZE) {
                    pageCache.store(pageAddr, pageData);
                    memcpy(buffer, pageData + pageOffset, size);
                    return size;
                } else if (nread > pageOffset) {
                    // Partial page read (edge of mapped region)
                    size_t available = nread - pageOffset;
                    size_t toCopy = (available < size) ? available : size;
                    memcpy(buffer, pageData + pageOffset, toCopy);
                    return toCopy;
                }
            }
        }

        // Large reads: direct read with jitter
        jitterDelay();

        if (strategy == STRAT_PROC_MEM) {
            return readProcMem(address, buffer, size);
        } else {
            return readVmReadv(address, buffer, size);
        }
    }

    // Scatter-gather read: batch multiple addresses into one syscall
    // Reduces total syscall count which is key for evading AC anomaly detection
    bool readScatterGather(const std::vector<std::pair<uintptr_t, size_t>>& requests,
                           std::vector<std::vector<uint8_t>>& results) {
        if (targetPid <= 0) return false;

        results.resize(requests.size());

        // Build iovec arrays for batch read
        std::vector<struct iovec> localIovs(requests.size());
        std::vector<struct iovec> remoteIovs(requests.size());

        for (size_t i = 0; i < requests.size(); i++) {
            results[i].resize(requests[i].second);
            localIovs[i].iov_base = results[i].data();
            localIovs[i].iov_len = requests[i].second;
            remoteIovs[i].iov_base = (void*)requests[i].first;
            remoteIovs[i].iov_len = requests[i].second;
        }

        jitterDelay();

        ssize_t nread = RawSyscall::vm_readv(targetPid, localIovs.data(), localIovs.size(),
                                             remoteIovs.data(), remoteIovs.size(), 0);
        return nread > 0;
    }

    // ════════════════════ Convenience Templates ════════════════════

    template <typename T>
    T read(uintptr_t address) {
        T value{};
        readMemory(address, &value, sizeof(T));
        return value;
    }

    template<typename T>
    std::vector<T> readArray(uintptr_t address, size_t count) {
        std::vector<T> data(count);
        readMemory(address, data.data(), count * sizeof(T));
        return data;
    }

    std::string readString(uintptr_t address, size_t maxLen = 100) {
        std::vector<char> buf(maxLen, '\0');
        size_t nread = readMemory(address, buf.data(), maxLen);
        if (nread == 0) return "";

        buf[nread - 1] = '\0';
        return std::string(buf.data());
    }

    std::string readStringNew(uintptr_t address, size_t size = 100) {
        std::string name(size, '\0');
        readMemory(address, &name[0], size);
        name.shrink_to_fit();
        return name;
    }

    // ════════════════════ Module Info (Cached) ════════════════════

    uintptr_t getModuleBase(const char* moduleName) {
        if (!moduleCacheValid) parseAndCacheModules();

        auto it = moduleCache.find(moduleName);
        if (it != moduleCache.end() && it->second.valid) {
            return it->second.base;
        }

        // Fallback: substring search across all cached modules
        for (auto& kv : moduleCache) {
            if (kv.first.find(moduleName) != std::string::npos && kv.second.valid) {
                return kv.second.base;
            }
        }
        return 0;
    }

    uintptr_t getModuleEnd(const char* moduleName) {
        if (!moduleCacheValid) parseAndCacheModules();

        auto it = moduleCache.find(moduleName);
        if (it != moduleCache.end() && it->second.valid) {
            return it->second.end;
        }

        for (auto& kv : moduleCache) {
            if (kv.first.find(moduleName) != std::string::npos && kv.second.valid) {
                return kv.second.end;
            }
        }
        return 0;
    }

    bool isModuleLoaded(const char* moduleName) {
        // Force refresh when checking if loaded
        moduleCacheValid = false;
        return getModuleBase(moduleName) != 0;
    }

    // Read remote memory into buffer (for lib dump)
    bool readRemoteBuffer(uintptr_t address, uint8_t* buffer, size_t size) {
        return readMemory(address, buffer, size) > 0;
    }

    // ════════════════════ Process Discovery ════════════════════

    static pid_t findPidByName(const char* packageName) {
        DIR* dir = opendir("/proc");
        if (!dir) return -1;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;

            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0' || pid <= 0) continue;

            char cmdlinePath[64];
            snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%ld/cmdline", pid);

            // Use raw syscall to read cmdline
            int fd = RawSyscall::openat_raw(AT_FDCWD, cmdlinePath, O_RDONLY);
            if (fd < 0) continue;

            char cmdline[256] = {0};
            RawSyscall::read_raw(fd, cmdline, sizeof(cmdline) - 1);
            RawSyscall::close_raw(fd);

            if (strcmp(cmdline, packageName) == 0) {
                closedir(dir);
                return (pid_t)pid;
            }
        }
        closedir(dir);
        return -1;
    }

    static std::vector<std::pair<pid_t, std::string>> getRunningProcesses() {
        std::vector<std::pair<pid_t, std::string>> processes;
        DIR* dir = opendir("/proc");
        if (!dir) return processes;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;

            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0' || pid <= 0) continue;

            char cmdlinePath[64];
            snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%ld/cmdline", pid);

            int fd = RawSyscall::openat_raw(AT_FDCWD, cmdlinePath, O_RDONLY);
            if (fd < 0) continue;

            char cmdline[512] = {0};
            RawSyscall::read_raw(fd, cmdline, sizeof(cmdline) - 1);
            RawSyscall::close_raw(fd);

            std::string name(cmdline);
            if (!name.empty() && name.find('.') != std::string::npos) {
                processes.push_back({(pid_t)pid, name});
            }
        }
        closedir(dir);
        return processes;
    }

    ~RemoteProcess() {
        if (memFd >= 0) {
            RawSyscall::close_raw(memFd);
        }
    }
};

// Global remote process instance
static RemoteProcess gRemoteProcess;
