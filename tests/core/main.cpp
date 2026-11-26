// Test runner. When PROJECT_TEST_JUNIT names a file, a JUnit-style XML
// report is written there (the interface of scripts/run_tests.py).
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    if (const char* report = std::getenv("PROJECT_TEST_JUNIT"); report && *report)
        ::testing::GTEST_FLAG(output) = std::string("xml:") + report;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
