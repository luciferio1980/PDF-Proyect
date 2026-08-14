#include "tests/TestHarness.h"

#include <exception>
#include <iostream>
#include <vector>

namespace pdfforge::test {
namespace {

struct Registry {
    std::vector<std::pair<std::string, TestFn>> tests;
    int failures = 0;
};

Registry& registry() {
    static Registry r;
    return r;
}

}  // namespace

bool registerTest(const char* name, TestFn fn) {
    registry().tests.emplace_back(name, fn);
    return true;
}

void check(bool condition, const char* expr, const char* file, int line) {
    if (condition) {
        return;
    }
    ++registry().failures;
    std::cerr << "  FAIL " << file << ':' << line << "  " << expr << '\n';
}

int failures() {
    return registry().failures;
}

int runAll() {
    int failedTests = 0;
    std::cout << "PDFForge tests: " << registry().tests.size() << " cases\n";
    for (const auto& [name, fn] : registry().tests) {
        const int before = registry().failures;
        std::cout << "[ RUN ] " << name << '\n';
        try {
            fn();
        } catch (const std::exception& ex) {
            ++registry().failures;
            std::cerr << "  EXC  " << ex.what() << '\n';
        }
        if (registry().failures > before) {
            ++failedTests;
            std::cout << "[ FAIL] " << name << '\n';
        } else {
            std::cout << "[  OK ] " << name << '\n';
        }
    }
    std::cout << "Done. failed_tests=" << failedTests << " assertions=" << registry().failures
              << '\n';
    return registry().failures == 0 ? 0 : 1;
}

}  // namespace pdfforge::test

int main() {
    return pdfforge::test::runAll();
}
