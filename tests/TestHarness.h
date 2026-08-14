#pragma once

#include <functional>
#include <string>
#include <vector>

namespace pdfforge::test {

using TestFn = void (*)();

bool registerTest(const char* name, TestFn fn);
void check(bool condition, const char* expr, const char* file, int line);
int runAll();
int failures();

}  // namespace pdfforge::test

#define TEST(name)                                                                 \
    static void test_##name();                                                     \
    static const bool registered_##name =                                          \
        ::pdfforge::test::registerTest(#name, test_##name);                        \
    static void test_##name()

#define CHECK(cond) ::pdfforge::test::check(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b)                                                                             \
    ::pdfforge::test::check((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_TRUE(cond) CHECK(cond)
#define CHECK_FALSE(cond) CHECK(!(cond))
