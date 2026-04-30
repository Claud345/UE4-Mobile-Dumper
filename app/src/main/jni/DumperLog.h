//
// DumperLog.h — Thread-safe in-app logging system
// Replaces the need for Android Studio's Logcat
// Logs to: ring buffer (in-memory), file on disk, and Android logcat
// Created by Ascarre on 30-04-2026.
//

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <chrono>
#include <atomic>
#include <android/log.h>

// ═══════════════════ Log Levels ═══════════════════

enum LogLevel {
    LOG_LEVEL_VERBOSE = 0,
    LOG_LEVEL_DEBUG   = 1,
    LOG_LEVEL_INFO    = 2,
    LOG_LEVEL_WARN    = 3,
    LOG_LEVEL_ERROR   = 4,
    LOG_LEVEL_FATAL   = 5
};

// ═══════════════════ Log Entry ═══════════════════

struct LogEntry {
    LogLevel level;
    std::string tag;
    std::string message;
    std::string timestamp;
    uint64_t epochMs;
};

// ═══════════════════ DumperLog Singleton ═══════════════════

class DumperLog {
public:
    static DumperLog& instance() {
        static DumperLog inst;
        return inst;
    }

    // Initialize with a file path for persistent logging
    void init(const std::string& logDir) {
        std::lock_guard<std::mutex> lock(mtx_);
        logDir_ = logDir;

        // Create log file with timestamp in name
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        char fname[128];
        snprintf(fname, sizeof(fname), "/dumper_log_%04d%02d%02d_%02d%02d%02d.txt",
                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

        logFilePath_ = logDir + fname;
        logFile_.open(logFilePath_, std::ios::out | std::ios::app);
        initialized_ = true;

        // Write header
        if (logFile_.is_open()) {
            logFile_ << "════════════════════════════════════════\n";
            logFile_ << "  UE Dumper Log — Session Started\n";
            logFile_ << "  " << formatTime(now) << "\n";
            logFile_ << "════════════════════════════════════════\n\n";
            logFile_.flush();
        }
    }

    // Core log function
    void log(LogLevel level, const char* tag, const char* fmt, ...) {
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        auto now = std::chrono::system_clock::now();
        std::string timestamp = formatTime(now);
        uint64_t epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        LogEntry entry;
        entry.level = level;
        entry.tag = tag ? tag : "Dumper";
        entry.message = buf;
        entry.timestamp = timestamp;
        entry.epochMs = epochMs;

        // Android logcat (always available even without init)
        int androidLevel;
        switch (level) {
            case LOG_LEVEL_VERBOSE: androidLevel = ANDROID_LOG_VERBOSE; break;
            case LOG_LEVEL_DEBUG:   androidLevel = ANDROID_LOG_DEBUG;   break;
            case LOG_LEVEL_INFO:    androidLevel = ANDROID_LOG_INFO;    break;
            case LOG_LEVEL_WARN:    androidLevel = ANDROID_LOG_WARN;    break;
            case LOG_LEVEL_ERROR:   androidLevel = ANDROID_LOG_ERROR;   break;
            case LOG_LEVEL_FATAL:   androidLevel = ANDROID_LOG_FATAL;   break;
            default: androidLevel = ANDROID_LOG_INFO; break;
        }
        __android_log_print(androidLevel, entry.tag.c_str(), "%s", buf);

        // Thread-safe insert into ring buffer + file
        {
            std::lock_guard<std::mutex> lock(mtx_);

            // Ring buffer
            if (entries_.size() >= maxEntries_) {
                entries_.erase(entries_.begin());
            }
            entries_.push_back(entry);
            totalCount_++;

            // File output
            if (logFile_.is_open()) {
                logFile_ << "[" << timestamp << "] "
                         << levelStr(level) << "/" << entry.tag
                         << ": " << buf << "\n";
                // Flush periodically (every 10 entries) to avoid data loss on crash
                if (totalCount_ % 10 == 0) {
                    logFile_.flush();
                }
            }
        }
    }

    // Get all entries (for JNI bridge)
    std::vector<LogEntry> getEntries() {
        std::lock_guard<std::mutex> lock(mtx_);
        return entries_;
    }

    // Get entries since a specific index (for incremental updates)
    std::vector<LogEntry> getEntriesSince(uint64_t sinceEpochMs) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<LogEntry> result;
        for (const auto& e : entries_) {
            if (e.epochMs > sinceEpochMs) {
                result.push_back(e);
            }
        }
        return result;
    }

    // Get formatted entries as a single string (for JNI — fewer crossings)
    // Format: "LEVEL|TAG|TIMESTAMP|MESSAGE\n" per line
    std::string getFormattedLog(int minLevel = LOG_LEVEL_VERBOSE) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string out;
        out.reserve(entries_.size() * 100);
        for (const auto& e : entries_) {
            if (e.level >= minLevel) {
                out += levelStr(e.level);
                out += "|";
                out += e.tag;
                out += "|";
                out += e.timestamp;
                out += "|";
                out += e.message;
                out += "\n";
            }
        }
        return out;
    }

    // Get only new entries as formatted string (since last poll)
    std::string pollNewEntries(int minLevel = LOG_LEVEL_VERBOSE) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string out;
        size_t start = lastPollIndex_;
        if (start >= entries_.size()) return "";
        for (size_t i = start; i < entries_.size(); i++) {
            const auto& e = entries_[i];
            if (e.level >= minLevel) {
                out += levelStr(e.level);
                out += "|";
                out += e.tag;
                out += "|";
                out += e.timestamp;
                out += "|";
                out += e.message;
                out += "\n";
            }
        }
        lastPollIndex_ = entries_.size();
        return out;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        entries_.clear();
        lastPollIndex_ = 0;
    }

    size_t count() {
        std::lock_guard<std::mutex> lock(mtx_);
        return entries_.size();
    }

    std::string getLogFilePath() {
        std::lock_guard<std::mutex> lock(mtx_);
        return logFilePath_;
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (logFile_.is_open()) logFile_.flush();
    }

private:
    DumperLog() = default;
    ~DumperLog() {
        if (logFile_.is_open()) {
            logFile_ << "\n[Session ended]\n";
            logFile_.close();
        }
    }
    DumperLog(const DumperLog&) = delete;
    DumperLog& operator=(const DumperLog&) = delete;

    static const char* levelStr(LogLevel l) {
        switch (l) {
            case LOG_LEVEL_VERBOSE: return "V";
            case LOG_LEVEL_DEBUG:   return "D";
            case LOG_LEVEL_INFO:    return "I";
            case LOG_LEVEL_WARN:    return "W";
            case LOG_LEVEL_ERROR:   return "E";
            case LOG_LEVEL_FATAL:   return "F";
            default: return "?";
        }
    }

    static std::string formatTime(std::chrono::system_clock::time_point tp) {
        auto t = std::chrono::system_clock::to_time_t(tp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()).count() % 1000;
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        char buf[64];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms);
        return std::string(buf);
    }

    std::mutex mtx_;
    std::vector<LogEntry> entries_;
    size_t maxEntries_ = 5000; // Ring buffer cap
    uint64_t totalCount_ = 0;
    size_t lastPollIndex_ = 0;
    bool initialized_ = false;
    std::string logDir_;
    std::string logFilePath_;
    std::ofstream logFile_;
};

// ═══════════════════ Convenience Macros ═══════════════════
// Usage: LOG_I("Scanner", "Found GNames at 0x%lx", offset);

#define LOG_V(tag, fmt, ...) DumperLog::instance().log(LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) DumperLog::instance().log(LOG_LEVEL_DEBUG,   tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) DumperLog::instance().log(LOG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) DumperLog::instance().log(LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) DumperLog::instance().log(LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOG_F(tag, fmt, ...) DumperLog::instance().log(LOG_LEVEL_FATAL,   tag, fmt, ##__VA_ARGS__)
