// Resonant - Wave Field Engineering Studio. Program entry point.
#include "Application.h"
#include "AcousticController.h"
#include "AcousticsPanel.h"
#include "FdtdController.h"
#include "FdtdPanel.h"
#include "MainWindow.h"
#include "OptimizationController.h"
#include "OptimizationPanel.h"
#include "MechanicsController.h"
#include "EmController.h"
#include "EmPanel.h"
#include "ExperimentsController.h"
#include "ExperimentsPanel.h"
#include "MechanicsPanel.h"
#include "ViewportWidget.h"
#include "foundation/Log.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDockWidget>
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
    const QCommandLineOption runEmOption(
        QStringLiteral("run-em"),
        QStringLiteral("After opening the project: 'field' evaluates the analytical electromagnetic field on its first region, 'pattern' "
                       "shows the 3D pattern of its first antenna or array, 'full-wave' runs the Yee solver on its first region."),
        QStringLiteral("what"));
    parser.addOption(runEmOption);
    const QCommandLineOption runExperimentOption(
        QStringLiteral("run-experiment"),
        QStringLiteral("After opening the (saved) project: run its experiment with this id (or its first experiment for 'first'). "
                       "An experiment with physical outputs runs only with --authorize-output."),
        QStringLiteral("id"));
    parser.addOption(runExperimentOption);
    const QCommandLineOption authorizeOutputOption(
        QStringLiteral("authorize-output"),
        QStringLiteral("Authorize the physical outputs of the --run-experiment run, as this operator (an explicit act: outputs are never "
                       "started without it)."),
        QStringLiteral("operator"));
    parser.addOption(authorizeOutputOption);
    const QCommandLineOption runOptimizationOption(
        QStringLiteral("run-optimization"),
        QStringLiteral("After opening the project: run its optimization problem with this id (or its first problem for 'first') in the "
                       "Optimization Studio and log the outcome."),
        QStringLiteral("id"));
    parser.addOption(runOptimizationOption);
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
        bool waitingForEm = false;
        if (parser.isSet(runEmOption)) {
            window.viewport().frameAll();
            EmController& em = window.electromagnetics();
            EmPanel& panel = window.emPanel();
            if (auto* dock = qobject_cast<QDockWidget*>(panel.parentWidget())) {
                dock->show();
                dock->raise();
            }
            const QString what = parser.value(runEmOption);
            const auto regions = em.regions();
            const auto objects = em.objects();
            bool done = false;
            if (what == QLatin1String("pattern") && !objects.empty()) {
                panel.selectObject(objects.front().first);
                panel.showTab(QStringLiteral("patternTab"));
                panel.calculatePattern();
                done = panel.showPattern3D();
            } else if ((what == QLatin1String("field") || what == QLatin1String("full-wave")) && !regions.empty()) {
                if (!objects.empty())
                    panel.selectObject(objects.front().first);
                panel.selectRegion(regions.front().first);
                panel.showTab(QStringLiteral("fieldsTab"));
                if (what == QLatin1String("field"))
                    done = panel.evaluateRegion();
                else
                    done = waitingForEm = panel.runFullWave();
            }
            if (!done)
                resonant::logError("application", "--run-em " + what.toStdString() + ": nothing suitable in the project, or it was refused ("
                                                      + panel.planText().toStdString() + ")");
        }
        bool waitingForExperiment = false;
        if (parser.isSet(runExperimentOption)) {
            ExperimentsPanel& panel = window.experimentsPanel();
            if (auto* dock = qobject_cast<QDockWidget*>(panel.parentWidget())) {
                dock->show();
                dock->raise();
            }
            const auto experiments = window.experiments().workspace().experiments;
            std::string id = parser.value(runExperimentOption).toStdString();
            if (id == "first" && !experiments.empty())
                id = experiments.front().id;
            panel.selectExperiment(id);
            bool ready = panel.selectedExperiment() == id;
            if (!ready)
                resonant::logError("application", "--run-experiment: the project has no experiment '" + id + "'");
            if (ready && parser.isSet(authorizeOutputOption))
                ready = panel.authorizeSelected(parser.value(authorizeOutputOption));
            if (ready)
                waitingForExperiment = panel.runSelected();
            if (!waitingForExperiment)
                resonant::logError("application", "--run-experiment: " + panel.statusText().toStdString());
            else
                QObject::connect(&window.experiments(), &ExperimentsController::runFinished, &window, [](const QString& message) {
                    resonant::logInfo("application", "--run-experiment: " + message.toStdString());
                });
        }
        bool waitingForOptimization = false;
        if (parser.isSet(runOptimizationOption)) {
            OptimizationPanel& panel = window.optimizationPanel();
            if (auto* dock = qobject_cast<QDockWidget*>(panel.parentWidget())) {
                dock->show();
                dock->raise();
            }
            const auto problems = window.optimization().workspace().problems;
            std::string id = parser.value(runOptimizationOption).toStdString();
            if (id == "first" && !problems.empty())
                id = problems.front().id;
            panel.selectProblem(id);
            if (panel.selectedProblem() != id)
                resonant::logError("application", "--run-optimization: the project has no optimization problem '" + id + "'");
            else {
                panel.showTab(QStringLiteral("runTab"));
                waitingForOptimization = panel.start();
                if (!waitingForOptimization)
                    resonant::logError("application", "--run-optimization: " + panel.statusText().toStdString());
                else
                    QObject::connect(&window.optimization(), &OptimizationController::runFinished, &window, [&panel](const QString& message) {
                        panel.showTab(QStringLiteral("candidatesTab"));
                        resonant::logInfo("application", "--run-optimization: " + message.toStdString());
                    });
            }
        }
        if (parser.isSet(screenshotOption)) {
            if (waitingForOptimization)
                QObject::connect(&window.optimization(), &OptimizationController::runFinished, &window, capture);
            else if (waitingForExperiment)
                QObject::connect(&window.experiments(), &ExperimentsController::runFinished, &window, capture);
            else if (waitingForEm)
                QObject::connect(&window.electromagnetics(), &EmController::resultsChanged, &window, capture);
            else if (waitingForMechanics)
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
