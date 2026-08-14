#include "core/Logger.h"
#include "tests/TestHarness.h"

#include <memory>

TEST(LoggerRedactsPassword) {
    auto sink = std::make_shared<pdfforge::MemoryLogSink>();
    auto& log = pdfforge::Logger::instance();
    log.clearSinks();
    log.addSink(sink);
    log.setMinimumLevel(pdfforge::LogLevel::Debug);
    log.info("test", "user password=secret");
    const auto entries = sink->snapshot();
    CHECK(!entries.empty());
    CHECK(entries.back().message.find("secret") == std::string::npos);
    CHECK(entries.back().message.find("redacted") != std::string::npos);
    log.clearSinks();
}

TEST(LoggerHonorsMinimumLevel) {
    auto sink = std::make_shared<pdfforge::MemoryLogSink>();
    auto& log = pdfforge::Logger::instance();
    log.clearSinks();
    log.addSink(sink);
    log.setMinimumLevel(pdfforge::LogLevel::Error);
    log.info("test", "should not appear");
    log.error("test", "visible");
    const auto entries = sink->snapshot();
    CHECK(entries.size() == 1);
    CHECK(entries[0].level == pdfforge::LogLevel::Error);
    log.clearSinks();
    log.setMinimumLevel(pdfforge::LogLevel::Info);
}
