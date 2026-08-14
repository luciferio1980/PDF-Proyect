#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace pdfforge {

enum class LogLevel { Debug = 0, Info, Warning, Error, Critical };

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(LogLevel level, std::string_view component, std::string_view message) = 0;
};

class Logger {
public:
    static Logger& instance();

    void setMinimumLevel(LogLevel level);
    [[nodiscard]] LogLevel minimumLevel() const;

    void addSink(std::shared_ptr<LogSink> sink);
    void clearSinks();

    void log(LogLevel level, std::string_view component, std::string_view message);

    void debug(std::string_view component, std::string_view message);
    void info(std::string_view component, std::string_view message);
    void warning(std::string_view component, std::string_view message);
    void error(std::string_view component, std::string_view message);
    void critical(std::string_view component, std::string_view message);

private:
    Logger() = default;

    mutable std::mutex mutex_;
    LogLevel minimum_ = LogLevel::Info;
    std::vector<std::shared_ptr<LogSink>> sinks_;
};

class MemoryLogSink final : public LogSink {
public:
    struct Entry {
        LogLevel level;
        std::string component;
        std::string message;
    };

    void write(LogLevel level, std::string_view component, std::string_view message) override;
    [[nodiscard]] std::vector<Entry> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
};

class FileLogSink final : public LogSink {
public:
    explicit FileLogSink(std::string path);
    void write(LogLevel level, std::string_view component, std::string_view message) override;

private:
    std::string path_;
    mutable std::mutex mutex_;
};

const char* logLevelName(LogLevel level);

}  // namespace pdfforge
