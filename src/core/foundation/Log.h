// Structured application logging. An entry has a time, a level, a category
// (the subsystem that wrote it) and a message. Sinks receive entries on the
// thread that logged them; a sink that touches a user interface must hand
// the entry over to its own thread.
#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {

enum class LogLevel { Debug, Info, Warning, Error };

std::string_view toString(LogLevel level);

struct LogEntry {
    std::chrono::system_clock::time_point time;
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
};

// "2026-11-26 15:04:05.123 [warning] project: message"
std::string formatLogEntry(const LogEntry& entry);

class Logger {
public:
    using Sink = std::function<void(const LogEntry&)>;

    // The process-wide logger used by the free functions below.
    static Logger& global();

    int addSink(Sink sink);
    void removeSink(int handle);

    void log(LogLevel level, std::string_view category, std::string_view message);

    // The most recent entries (bounded), oldest first.
    std::vector<LogEntry> history() const;
    void clearHistory();

private:
    static constexpr size_t kMaxHistory = 5000;

    mutable std::mutex mutex_;
    std::vector<std::pair<int, Sink>> sinks_;
    std::vector<LogEntry> history_;
    int nextHandle_ = 1;
};

void logDebug(std::string_view category, std::string_view message);
void logInfo(std::string_view category, std::string_view message);
void logWarning(std::string_view category, std::string_view message);
void logError(std::string_view category, std::string_view message);

}  // namespace resonant
