#pragma once

#include <string>
#include <functional>
#include <vector>
#include <iostream>

// 简易测试框架
struct TestCase {
    std::string name;
    std::function<bool()> func;
};

std::vector<TestCase>& GetTests();

#define TEST_CASE(name) \
    static bool test_##name(); \
    static int reg_##name = []() { \
        GetTests().push_back({#name, test_##name}); \
        return 0; \
    }(); \
    static bool test_##name()

#define REQUIRE(expr) \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << #expr << std::endl; \
        return false; \
    }
