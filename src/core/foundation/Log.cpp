#include "foundation/Log.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace resonant {

std::string_view toString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warning: return "warning";
    case LogLevel::Error: return "error";
    }
    return "info";
}

std::string formatLogEntry(const LogEntry& entry)
{
    using namespace std::chrono;
    const auto seconds = time_point_cast<std::chrono::seconds>(entry.time);
    const auto millis = duration_cast<milliseconds>(entry.time - seconds).count();
    const std::time_t time = system_clock::to_time_t(seconds);
    std::tm local{};
    localtime_r(&time, &local);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "%s.%03d", stamp, static_cast<int>(millis));

    std::string text = prefix;
    text += " [";
    text += toString(entry.level);
    text += "] ";
    text += entry.category;
    text += ": ";
    text += entry.message;
    return text;
}

Logger& Logger::global()
{
    static Logger logger;
    return logger;
}

int Logger::addSink(Sink sink)
{
    std::lock_guard lock(mutex_);
    const int handle = nextHandle_++;
    sinks_.emplace_back(handle, std::move(sink));
    return handle;
}

void Logger::removeSink(int handle)
{
    std::lock_guard lock(mutex_);
    std::erase_if(sinks_, [handle](const auto& entry) { return entry.first == handle; });
}

void Logger::log(LogLevel level, std::string_view category, std::string_view message)
{
    LogEntry entry{std::chrono::system_clock::now(), level, std::string(category), std::string(message)};
    std::vector<Sink> sinks;
    {
        std::lock_guard lock(mutex_);
        if (history_.size() >= kMaxHistory)
            history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(kMaxHistory / 10));
        history_.push_back(entry);
        sinks.reserve(sinks_.size());
        for (const auto& [handle, sink] : sinks_)
            sinks.push_back(sink);
    }
    // Sinks run outside the lock so a sink may log.
    for (const auto& sink : sinks)
        sink(entry);
}

std::vector<LogEntry> Logger::history() const
{
    std::lock_guard lock(mutex_);
    return history_;
}

void Logger::clearHistory()
{
    std::lock_guard lock(mutex_);
    history_.clear();
}

void logDebug(std::string_view category, std::string_view message) { Logger::global().log(LogLevel::Debug, category, message); }
void logInfo(std::string_view category, std::string_view message) { Logger::global().log(LogLevel::Info, category, message); }
void logWarning(std::string_view category, std::string_view message) { Logger::global().log(LogLevel::Warning, category, message); }
void logError(std::string_view category, std::string_view message) { Logger::global().log(LogLevel::Error, category, message); }

}  // namespace resonant
