// Resonant - Wave Field Engineering Studio. Program entry point.
#include "Application.h"
#include "MainWindow.h"
#include "foundation/Log.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>

#include <cstdio>

int main(int argc, char** argv)
{
    resonant_app::configureBeforeApplication();
    QApplication application(argc, argv);

    // Everything logged also goes to standard error, so a failure before
    // the console panel exists is still visible.
    resonant::Logger::global().addSink([](const resonant::LogEntry& entry) {
        std::fprintf(stderr, "%s\n", resonant::formatLogEntry(entry).c_str());
    });
    resonant_app::logStartup(application);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Resonant - Wave Field Engineering Studio"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("project"), QStringLiteral("Project file to open."), QStringLiteral("[project]"));
    const QCommandLineOption screenshotOption(
        QStringLiteral("screenshot"),
        QStringLiteral("Save an image of the main window to <file> once it is shown, then exit."), QStringLiteral("file"));
    parser.addOption(screenshotOption);
    parser.process(application);

    int result = 0;
    {
        MainWindow window;
        window.restoreWorkspace();
        window.show();
        if (!parser.positionalArguments().isEmpty()) {
            const QString path = parser.positionalArguments().first();
            if (resonant::Status status = window.openProjectFile(path); !status)
                QMessageBox::critical(&window, QStringLiteral("Could not open the project"),
                                      QString::fromStdString(status.message()));
        }
        if (parser.isSet(screenshotOption)) {
            // Let the window lay out and the viewport draw before capturing.
            const QString file = parser.value(screenshotOption);
            QTimer::singleShot(1500, &window, [&window, file] {
                const bool saved = window.grab().save(file);
                if (!saved)
                    resonant::logError("application", "could not save the screenshot to " + file.toStdString());
                window.unsavedChangesPrompt = [] { return QMessageBox::Discard; };
                window.close();
                QApplication::exit(saved ? 0 : 1);
            });
        }
        result = QApplication::exec();
    }
    resonant::logInfo("application", "shut down");
    return result;
}
