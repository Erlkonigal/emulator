#ifndef TEST_TEST_FRAMEWORK_H
#define TEST_TEST_FRAMEWORK_H

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Keep all macro continuations on a single logical line to avoid
// breaking backslash-newline pairs.

namespace testfw {

using TestFn = std::function<void()>;

struct SkipException {
    std::string Why;
};

void RegisterTest(const char* name, TestFn fn);
void SetCurrentTestName(const char* name);
const char* GetCurrentTestName();

void AddFailure(const std::string& message);

[[noreturn]] void Skip(const std::string& why);

std::string FormatEqFailure(const char* file, int line, const char* aExpr, const char* bExpr,
    const std::string& a, const std::string& b);

template <typename T>
std::string ToString(const T& v) {
    if constexpr (std::is_same_v<T, std::string>) {
        return v;
    } else if constexpr (std::is_same_v<T, const char*>) {
        return v ? std::string(v) : std::string("<null>");
    } else if constexpr (std::is_integral_v<T>) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        return std::string(buf);
    } else {
        return "<value>";
    }
}

int RunAllTests();
void PrintReport();

} // namespace testfw

#define TEST(name) \
    static void name(); \
    namespace { \
    struct reg_##name { \
        reg_##name() { ::testfw::RegisterTest(#name, name); } \
    } reginst_##name; \
    } \
    static void name()

#define EXPECT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            ::testfw::AddFailure(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": EXPECT_TRUE failed: " #expr); \
        } \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            ::testfw::AddFailure(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": ASSERT_TRUE failed: " #expr); \
            return; \
        } \
    } while (0)

#define EXPECT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (!((_a) == (_b))) { \
            ::testfw::AddFailure(::testfw::FormatEqFailure(__FILE__, __LINE__, #a, #b, \
                ::testfw::ToString(_a), ::testfw::ToString(_b))); \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (!((_a) == (_b))) { \
            ::testfw::AddFailure(::testfw::FormatEqFailure(__FILE__, __LINE__, #a, #b, \
                ::testfw::ToString(_a), ::testfw::ToString(_b))); \
            return; \
        } \
    } while (0)

#define SKIP(reason) \
    do { \
        ::testfw::Skip(reason); \
    } while (0)

#endif
