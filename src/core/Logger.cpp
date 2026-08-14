#include "core/Logger.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace pdfforge {
namespace {

std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

bool looksSensitive(std::string_view message) {
    // Defence-in-depth: never write secrets even if a caller slips.
    const auto has = [&](std::string_view needle) {
        return message.find(needle) != std::string_view::npos;
    };
    return has("password") || has("passwd") || has("private key") || has("-----BEGIN");
}

}  // namespace

const char* logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
    }
    return "UNKNOWN";
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setMinimumLevel(LogLevel level) {
    std::lock_guard lock(mutex_);
    minimum_ = level;
}

LogLevel Logger::minimumLevel() const {
    std::lock_guard lock(mutex_);
    return minimum_;
}

void Logger::addSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clearSinks() {
    std::lock_guard lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    if (looksSensitive(message)) {
        message = "[redacted sensitive field]";
    }
    std::vector<std::shared_ptr<LogSink>> copy;
    {
        std::lock_guard lock(mutex_);
        if (static_cast<int>(level) < static_cast<int>(minimum_)) {
            return;
        }
        copy = sinks_;
    }
    if (copy.empty()) {
        std::cerr << timestampNow() << ' ' << logLevelName(level) << " [" << component << "] "
                  << message << '\n';
        return;
    }
    for (const auto& sink : copy) {
        sink->write(level, component, message);
    }
}

void Logger::debug(std::string_view component, std::string_view message) {
    log(LogLevel::Debug, component, message);
}
void Logger::info(std::string_view component, std::string_view message) {
    log(LogLevel::Info, component, message);
}
void Logger::warning(std::string_view component, std::string_view message) {
    log(LogLevel::Warning, component, message);
}
void Logger::error(std::string_view component, std::string_view message) {
    log(LogLevel::Error, component, message);
}
void Logger::critical(std::string_view component, std::string_view message) {
    log(LogLevel::Critical, component, message);
}

void MemoryLogSink::write(LogLevel level, std::string_view component, std::string_view message) {
    std::lock_guard lock(mutex_);
    entries_.push_back(Entry{level, std::string(component), std::string(message)});
}

std::vector<MemoryLogSink::Entry> MemoryLogSink::snapshot() const {
    std::lock_guard lock(mutex_);
    return entries_;
}

FileLogSink::FileLogSink(std::string path) : path_(std::move(path)) {}

void FileLogSink::write(LogLevel level, std::string_view component, std::string_view message) {
    std::lock_guard lock(mutex_);
    std::ofstream out(path_, std::ios::app);
    if (!out) {
        return;
    }
    out << timestampNow() << ' ' << logLevelName(level) << " [" << component << "] " << message
        << '\n';
}

}  // namespace pdfforge
