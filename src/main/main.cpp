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
#include "DemoController.h"
#include "HelpWindow.h"
#include "ResourcePaths.h"
#include "demo/Demo.h"
#include "foundation/Log.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDockWidget>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>

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
    const QCommandLineOption runDemoOption(
        QStringLiteral("run-demo"),
        QStringLiteral("Run the Demo Studio demonstration with this profile id unattended (narration pauses pass), write its run manifest, "
                       "then exit (status 0 when it completed)."),
        QStringLiteral("profile"));
    parser.addOption(runDemoOption);
    const QCommandLineOption demoOutputOption(QStringLiteral("demo-output"),
                                              QStringLiteral("Where --run-demo writes its screenshots and run manifest."), QStringLiteral("directory"));
    parser.addOption(demoOutputOption);
    const QCommandLineOption presenterOption(QStringLiteral("presenter"), QStringLiteral("Start in presenter mode (with --run-demo: present it)."));
    parser.addOption(presenterOption);
    const QCommandLineOption windowSizeOption(QStringLiteral("window-size"),
                                              QStringLiteral("Window size WIDTHxHEIGHT, e.g. 1920x1080 for recording."), QStringLiteral("size"));
    parser.addOption(windowSizeOption);
    const QCommandLineOption captureOption(
        QStringLiteral("capture-screenshots"),
        QStringLiteral("Capture the help's screenshots from the running application as the manifest describes "
                       "(help/screenshots/manifest.json), then exit."),
        QStringLiteral("manifest"));
    parser.addOption(captureOption);
    const QCommandLineOption onlyOption(QStringLiteral("only"), QStringLiteral("With --capture-screenshots: only these names (comma separated)."),
                                        QStringLiteral("names"));
    parser.addOption(onlyOption);
    const QCommandLineOption runWorkflowOption(
        QStringLiteral("run-workflow"),
        QStringLiteral("After opening the project: run the workflow template with this id on it (changes applied as one undoable step), "
                       "then exit with its outcome."),
        QStringLiteral("id"));
    parser.addOption(runWorkflowOption);
    const QCommandLineOption checkHelpOption(
        QStringLiteral("check-help"),
        QStringLiteral("Load the installed help, open its home page, search it, and exit (status 0 when the help works)."));
    parser.addOption(checkHelpOption);
    parser.process(application);

    int result = 0;
    // --capture-screenshots: the manifest's environment (for example the plugin
    // search path that shows the test plugins) applies before the window exists.
    // Capture runs keep their settings apart: they never change the user's (last folder, layout).
    QTemporaryDir captureSettings;
    if (parser.isSet(captureOption) && captureSettings.isValid())
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, captureSettings.path());
    if (parser.isSet(captureOption)) {
        QFile manifestFile(parser.value(captureOption));
        if (manifestFile.open(QIODevice::ReadOnly)) {
            const QJsonObject env = QJsonDocument::fromJson(manifestFile.readAll()).object().value(QStringLiteral("environment")).toObject();
            const QDir root(QFileInfo(parser.value(captureOption)).absoluteDir().absoluteFilePath(QStringLiteral("../..")));
            for (auto it = env.begin(); it != env.end(); ++it)
                qputenv(it.key().toLocal8Bit(), root.absoluteFilePath(it.value().toString()).toLocal8Bit());
        }
    }
    {
        MainWindow window;
        const bool automated = parser.isSet(captureOption) || parser.isSet(runDemoOption);
        if (!automated)
            window.restoreWorkspace();  // automated runs start from the default layout, for repeatable results
        if (parser.isSet(windowSizeOption)) {
            const QStringList wh = parser.value(windowSizeOption).split(QLatin1Char('x'));
            if (wh.size() == 2)
                window.resize(wh[0].toInt(), wh[1].toInt());
        } else if (automated)
            window.resize(1600, 1000);
        window.show();
        if (parser.isSet(presenterOption))
            window.setPresenterMode(true);
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
        // --check-help: the help as installed (or built) works offline.
        if (parser.isSet(checkHelpOption)) {
            HelpWindow& help = window.helpWindow();
            bool ok = help.isLoaded() && help.showTopic(QStringLiteral("home"));
            const QStringList hits = ok ? help.search(QStringLiteral("array")) : QStringList();
            ok = ok && !hits.isEmpty() && help.openResult(0) && help.highlightCount() > 0;
            std::printf("help: %s; %s; search 'array': %d topics, first %s\n", ok ? "PASS" : "FAIL",
                        help.isLoaded() ? qPrintable(QStringLiteral("%1 topics from %2").arg(help.index()->topics().size()).arg(
                                              QString::fromStdString(help.index()->directory().string())))
                                        : qPrintable(help.loadError()),
                        static_cast<int>(hits.size()), hits.isEmpty() ? "-" : qPrintable(hits.front()));
            window.unsavedChangesPrompt = [] { return QMessageBox::Discard; };
            return ok ? 0 : 1;
        }
        // --run-demo / --run-workflow: unattended, ended by the run's own event.
        if (parser.isSet(runDemoOption) || parser.isSet(runWorkflowOption)) {
            DemoController& demos = window.demos();
            if (parser.isSet(demoOutputOption))
                demos.setOutputDirectory(QDir(parser.value(demoOutputOption)).absolutePath());
            QObject::connect(&demos, &DemoController::runFinished, &window, [&window, &demos](const QString& status, const QString& message) {
                std::printf("demo: %s%s; manifest %s\n", qPrintable(status), message.isEmpty() ? "" : qPrintable(QStringLiteral(" (") + message + QLatin1Char(')')),
                            qPrintable(demos.manifestPath()));
                // No window.close(): an automated run leaves the user's saved layout alone.
                QApplication::exit(status == QLatin1String("completed") ? 0 : 1);
            });
            bool started = false;
            if (parser.isSet(runDemoOption))
                started = demos.startProfile(parser.value(runDemoOption), false);
            else {
                resonant::demo::DemoSequence sequence;
                sequence.id = "workflow-" + parser.value(runWorkflowOption).toStdString();
                sequence.name = sequence.id;
                resonant::demo::DemoStep step;
                step.id = "workflow";
                step.action = "workflow";
                step.params = {{"id", parser.value(runWorkflowOption).toStdString()}};
                step.timeoutSeconds = 3600;
                sequence.steps.push_back(step);
                started = demos.start(sequence);
            }
            if (!started) {
                std::printf("demo: could not start (see the log)\n");
                return 1;
            }
            return QApplication::exec();
        }
        // --capture-screenshots: each entry of the manifest is a short sequence
        // of Demo Studio steps that ends with a capture into the help's images.
        if (parser.isSet(captureOption)) {
            QFile file(parser.value(captureOption));
            if (!file.open(QIODevice::ReadOnly)) {
                std::printf("screenshots: cannot read %s\n", qPrintable(parser.value(captureOption)));
                return 1;
            }
            const auto manifest = nlohmann::json::parse(file.readAll().toStdString(), nullptr, false);
            if (manifest.is_discarded() || !manifest.contains("screenshots")) {
                std::printf("screenshots: the manifest is not valid\n");
                return 1;
            }
            const QStringList only = parser.isSet(onlyOption) ? parser.value(onlyOption).split(QLatin1Char(',')) : QStringList();
            auto queue = std::make_shared<std::vector<resonant::demo::DemoSequence>>();
            for (const auto& shot : manifest["screenshots"]) {
                const std::string name = shot.value("name", std::string());
                if (!only.isEmpty() && !only.contains(QString::fromStdString(name)))
                    continue;
                nlohmann::json seq = {{"format", resonant::demo::kSequenceFormat}, {"version", 1}, {"id", "screenshot-" + name},
                                      {"name", shot.value("title", name)},
                                      {"steps", shot.value("steps", nlohmann::json::array())}};
                nlohmann::json captureStep = shot.value("capture", nlohmann::json::object());
                captureStep["name"] = name;
                captureStep["directory"] = manifest.value("outputDirectory", std::string("help/src/images/screenshots"));
                captureStep["page"] = shot.value("page", std::string());
                seq["steps"].push_back({{"id", "capture"}, {"action", "capture"}, {"description", shot.value("caption", std::string())},
                                        {"params", captureStep}, {"timeoutSeconds", 60}});
                auto parsed = resonant::demo::sequenceFromJson(seq);
                if (!parsed) {
                    std::printf("screenshots: %s: %s\n", name.c_str(), parsed.message().c_str());
                    return 1;
                }
                queue->push_back(parsed.take());
            }
            DemoController& demos = window.demos();
            demos.setOutputDirectory(QDir::temp().absoluteFilePath(QStringLiteral("resonant-screenshot-runs")));
            auto failures = std::make_shared<int>(0);
            auto position = std::make_shared<size_t>(0);
            auto runNext = std::make_shared<std::function<void()>>();
            *runNext = [&window, &demos, queue, position, failures, runNext] {
                if (*position >= queue->size()) {
                    std::printf("screenshots: %zu captured, %d failed\n", queue->size() - static_cast<size_t>(*failures), *failures);
                    QApplication::exit(*failures == 0 ? 0 : 1);
                    return;
                }
                window.setPresenterMode(false);
                demos.start((*queue)[(*position)++]);
            };
            QObject::connect(&demos, &DemoController::runFinished, &window, [failures, runNext, &demos](const QString& status, const QString& message) {
                std::printf("screenshot %s: %s %s\n", demos.lastRun().sequenceId.c_str(), qPrintable(status), qPrintable(message));
                if (status != QLatin1String("completed"))
                    ++*failures;
                QTimer::singleShot(0, [runNext] { (*runNext)(); });
            });
            QTimer::singleShot(0, [runNext] { (*runNext)(); });
            return QApplication::exec();
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
