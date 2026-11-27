// Resonant - Wave Field Engineering Studio. Program entry point.
#include "Application.h"
#include "AcousticController.h"
#include "AcousticsPanel.h"
#include "FdtdController.h"
#include "FdtdPanel.h"
#include "MainWindow.h"
#include "MechanicsController.h"
#include "MechanicsPanel.h"
#include "ViewportWidget.h"
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
    const QCommandLineOption runFieldOption(
        QStringLiteral("run-field"),
        QStringLiteral("Run the field simulation of the project's first field region after opening it."));
    parser.addOption(runFieldOption);
    const QCommandLineOption runFullWaveOption(
        QStringLiteral("run-full-wave"),
        QStringLiteral("Run the full-wave (FDTD) solver on the project's first field region after opening it."));
    parser.addOption(runFullWaveOption);
    const QCommandLineOption runMechanicsOption(
        QStringLiteral("run-mechanics"),
        QStringLiteral("Run a mechanical analysis (modal, harmonic, transient or static) of the project's first structure after opening it."),
        QStringLiteral("analysis"));
    parser.addOption(runMechanicsOption);
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
        const auto capture = [&window, &parser, &screenshotOption] {
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
        };
        bool waitingForField = false;
        if (parser.isSet(runFieldOption)) {
            window.viewport().frameAll();
            waitingForField = window.acousticsPanel().runSimulation();
            if (!waitingForField)
                resonant::logError("application", "--run-field: the project has no field region that can be simulated");
        }
        bool waitingForFullWave = false;
        if (parser.isSet(runFullWaveOption)) {
            window.viewport().frameAll();
            waitingForFullWave = window.fullWavePanel().run();
            if (!waitingForFullWave)
                resonant::logError("application", "--run-full-wave: " + window.fullWavePanel().statusText().toStdString());
        }
        bool waitingForMechanics = false;
        if (parser.isSet(runMechanicsOption)) {
            window.viewport().frameAll();
            const QString analysis = parser.value(runMechanicsOption);
            MechanicsPanel& panel = window.mechanicsPanel();
            waitingForMechanics = analysis == QLatin1String("modal")       ? panel.runModal()
                                  : analysis == QLatin1String("harmonic")  ? panel.runHarmonic()
                                  : analysis == QLatin1String("transient") ? panel.runTransient()
                                  : analysis == QLatin1String("static")    ? panel.runStatic()
                                                                           : false;
            if (!waitingForMechanics)
                resonant::logError("application", "--run-mechanics: " + panel.statusText().toStdString());
        }
        if (parser.isSet(screenshotOption)) {
            if (waitingForMechanics)
                QObject::connect(&window.mechanics(), &MechanicsController::resultsChanged, &window, capture);
            else if (waitingForFullWave)
                QObject::connect(&window.fullWave(), &FdtdController::resultsChanged, &window, capture);
            else if (waitingForField)
                QObject::connect(&window.acoustics(), &AcousticController::resultsChanged, &window, capture);
            else
                capture();
        }
        result = QApplication::exec();
    }
    resonant::logInfo("application", "shut down");
    return result;
}
