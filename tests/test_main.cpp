#include "test_framework.h"
#include <iostream>

std::vector<TestCase>& GetTests() {
    static std::vector<TestCase> tests;
    return tests;
}

int main() {
    int passed = 0;
    int failed = 0;
    
    std::cout << "Running " << GetTests().size() << " tests..." << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    for (const auto& test : GetTests()) {
        std::cout << "[ RUN ] " << test.name << std::endl;
        try {
            if (test.func()) {
                std::cout << "[  OK  ] " << test.name << std::endl;
                passed++;
            } else {
                std::cout << "[ FAIL ] " << test.name << std::endl;
                failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "[ FAIL ] " << test.name << " - exception: " << e.what() << std::endl;
            failed++;
        }
    }
    
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}
