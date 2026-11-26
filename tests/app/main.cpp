// Runner of the application tests: one QApplication for all of them. The
// tests need a display with OpenGL 4.5 (run them under xvfb-run when there
// is none). Settings go to a private scratch location so a test run never
// reads or changes the user's workspace.
#include "Application.h"

#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    if (const char* report = std::getenv("PROJECT_TEST_JUNIT"); report && *report)
        ::testing::GTEST_FLAG(output) = std::string("xml:") + report;
    ::testing::InitGoogleTest(&argc, argv);

    QStandardPaths::setTestModeEnabled(true);
    resonant_app::configureBeforeApplication();
    QApplication application(argc, argv);
    QSettings().clear();
    return RUN_ALL_TESTS();
}
