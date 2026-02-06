#include "test_framework.h"

#include <exception>

namespace testfw {
namespace {

struct TestCase {
    std::string Name;
    TestFn Fn;
};

struct TestFailure {
    std::string Name;
    std::string Message;
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

std::vector<TestFailure>& Failures() {
    static std::vector<TestFailure> failures;
    return failures;
}

std::vector<std::string>& Skips() {
    static std::vector<std::string> skips;
    return skips;
}

const char*& CurrentNameRef() {
    static const char* name = nullptr;
    return name;
}

} // namespace

void RegisterTest(const char* name, TestFn fn) {
    Registry().push_back({name ? name : "<null>", std::move(fn)});
}

void SetCurrentTestName(const char* name) {
    CurrentNameRef() = name;
}

const char* GetCurrentTestName() {
    return CurrentNameRef();
}

void AddFailure(const std::string& message) {
    const char* name = GetCurrentTestName();
    Failures().push_back({name ? name : "<unknown>", message});
}

[[noreturn]] void Skip(const std::string& why) {
    throw SkipException{why};
}

std::string FormatEqFailure(const char* file, int line, const char* aExpr, const char* bExpr,
    const std::string& a, const std::string& b) {
    char buf[1024];
    std::snprintf(buf, sizeof(buf), "%s:%d: EXPECT_EQ failed: %s != %s (lhs=%s rhs=%s)",
        file ? file : "<file>", line, aExpr ? aExpr : "<a>", bExpr ? bExpr : "<b>", a.c_str(),
        b.c_str());
    return std::string(buf);
}

int RunAllTests() {
    size_t passed = 0;
    for (const auto& tc : Registry()) {
        SetCurrentTestName(tc.Name.c_str());
        try {
            tc.Fn();
            ++passed;
        } catch (const SkipException& e) {
            Skips().push_back(tc.Name + ": " + e.Why);
        } catch (const std::exception& e) {
            AddFailure(std::string("Unhandled exception: ") + e.what());
        } catch (...) {
            AddFailure("Unhandled non-std exception");
        }
    }

    PrintReport();
    if (!Failures().empty()) {
        return 1;
    }
    std::fprintf(stdout, "PASS %zu\n", passed);
    if (passed == 0 && !Registry().empty()) {
        std::fprintf(stdout, "(all tests skipped)\n");
    }
    return 0;
}

void PrintReport() {
    if (!Skips().empty()) {
        std::fprintf(stderr, "SKIP %zu\n", Skips().size());
        for (const auto& s : Skips()) {
            std::fprintf(stderr, "  %s\n", s.c_str());
        }
    }
    if (!Failures().empty()) {
        std::fprintf(stderr, "FAIL %zu\n", Failures().size());
        for (const auto& f : Failures()) {
            std::fprintf(stderr, "  %s: %s\n", f.Name.c_str(), f.Message.c_str());
        }
    }
}

} // namespace testfw
