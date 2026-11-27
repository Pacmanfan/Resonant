// The Python console and the script editor in the real main window
// (REQ-CHAT-20261126-010, RES-PY-007..010): execution in the worker process,
// multi-line input, errors, inspection, history, completion, undoable
// app.apply, interruption without blocking the interface, crash isolation,
// and the editor's run / run-selection / error lines / templates / examples /
// project scripts. Skipped, with the reason, when Python or the module is missing.
#include "DocumentController.h"
#include "MainWindow.h"
#include "PythonConsole.h"
#include "PythonSession.h"
#include "ScriptEditor.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()>& condition, int ms = 30000)
{
    QElapsedTimer t;
    t.start();
    while (!condition()) {
        if (t.elapsed() > ms)
            return false;
        QTest::qWait(10);
    }
    return true;
}

class PythonUiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        if (!window_->startPython())
            GTEST_SKIP() << "Python is not available: " << window_->pythonSession().startupError().toStdString();
        ASSERT_TRUE(waitUntil([&] { return window_->pythonSession().isReady(); }));
        if (window_->pythonSession().readyInfo().value(QStringLiteral("api")).isNull())
            GTEST_SKIP() << "the resonant package cannot be imported (module not built)";
    }
    void TearDown() override
    {
        if (window_) {
            window_->pythonSession().stop();
            window_->close();
        }
        window_.reset();
    }
    PythonConsole& console() { return window_->pythonConsole(); }
    void run(const QString& code)
    {
        console().submit(code);
        ASSERT_TRUE(waitUntil([&] { return !console().isBusy(); })) << code.toStdString();
    }
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(PythonUiTest, TheConsoleRunsCodeWithMultiLineInputErrorsAndInspection)
{
    run(QStringLiteral("x = 6 * 7"));
    run(QStringLiteral("print(x)"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("42")));
    run(QStringLiteral("for i in range(2):"));  // incomplete: kept for more input
    EXPECT_TRUE(console().inputText().startsWith(QStringLiteral("for i in range(2):\n    ")));
    run(QStringLiteral("for i in range(2):\n    print('row', i)\n"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("row 1")));
    run(QStringLiteral("1 / 0"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("ZeroDivisionError")));
    run(QStringLiteral("resonant.scene.add_array?"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("Add")) || console().outputText().contains(QStringLiteral("array")));
    run(QStringLiteral("print('still', 'alive')"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("still alive")));
    EXPECT_TRUE(console().history().contains(QStringLiteral("x = 6 * 7")));
    // Completion comes from the worker's names.
    QSignalSpy received(&window_->pythonSession(), &PythonSession::received);
    const int id = window_->pythonSession().request({{QStringLiteral("op"), QStringLiteral("complete")}, {QStringLiteral("text"), QStringLiteral("reso")}});
    ASSERT_TRUE(waitUntil([&] {
        for (const auto& args : received)
            if (args.at(0).toInt() == id && args.at(1).toJsonObject().value(QStringLiteral("type")).toString() == QLatin1String("completions"))
                return args.at(1).toJsonObject().value(QStringLiteral("items")).toArray().contains(QStringLiteral("resonant"));
        return false;
    }));
}

TEST_F(PythonUiTest, AppApplyIsOneUndoableChangeOfTheOpenProject)
{
    const size_t before = window_->controller().scene().objectCount();
    console().sendProject();
    run(QStringLiteral("resonant.scene.add_geometry(project, 'Scripted box', object_id='scripted-box')\napp.apply(project, 'Add a scripted box')\n"));
    ASSERT_TRUE(waitUntil([&] { return window_->controller().scene().objectCount() == before + 1; }));
    EXPECT_NE(window_->controller().scene().find(resonant::Id("scripted-box")), nullptr);
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("Edit > Undo reverts it")));
    window_->controller().undo();
    EXPECT_EQ(window_->controller().scene().objectCount(), before);
    // The console's copy is a copy: changing it without apply changes nothing.
    run(QStringLiteral("project.remove(project.objects()[0].id) if project.objects() else None"));
    EXPECT_EQ(window_->controller().scene().objectCount(), before);
}

TEST_F(PythonUiTest, ALongScriptIsInterruptedAndNeverBlocksTheInterface)
{
    console().submit(QStringLiteral("import time\nwhile True:\n    time.sleep(0.01)\n"));
    ASSERT_TRUE(waitUntil([&] { return console().isBusy(); }));
    int ticks = 0;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] { ++ticks; });
    timer.start(10);
    QTest::qWait(400);
    EXPECT_GT(ticks, 10) << "the event loop must keep running while Python works";
    EXPECT_TRUE(console().isBusy());
    window_->pythonSession().interrupt();
    ASSERT_TRUE(waitUntil([&] { return !console().isBusy(); }));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("KeyboardInterrupt")));
    run(QStringLiteral("print('after the interrupt')"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("after the interrupt")));
}

TEST_F(PythonUiTest, AWorkerThatDiesLeavesTheApplicationAndProjectIntact)
{
    const size_t before = window_->controller().scene().objectCount();
    QSignalSpy crashed(&window_->pythonSession(), &PythonSession::crashed);
    console().submit(QStringLiteral("import os\nos._exit(3)\n"));
    ASSERT_TRUE(waitUntil([&] { return crashed.count() == 1; }));
    EXPECT_FALSE(window_->pythonSession().isReady());
    EXPECT_EQ(window_->controller().scene().objectCount(), before);
    window_->pythonSession().restart();
    ASSERT_TRUE(waitUntil([&] { return window_->pythonSession().isReady(); }));
    run(QStringLiteral("print('restarted')"));
    EXPECT_TRUE(console().outputText().contains(QStringLiteral("restarted")));
}

TEST_F(PythonUiTest, TheScriptEditorRunsScriptsAndSelectionsAndPointsAtErrors)
{
    ScriptEditor& editor = window_->scriptEditor();
    editor.setText(QStringLiteral("a = 1\nprint('a is', a)\nb = undefined_name\n"));
    ASSERT_TRUE(editor.run());
    ASSERT_TRUE(waitUntil([&] { return !editor.isRunning(); }));
    EXPECT_TRUE(editor.outputText().contains(QStringLiteral("a is 1")));
    ASSERT_EQ(editor.problems().size(), 1);
    EXPECT_TRUE(editor.problems().front().startsWith(QStringLiteral("Line 3: NameError")));
    EXPECT_EQ(editor.errorLine(), 3);
    // Templates and examples are found; an example runs to the end in the editor.
    EXPECT_GE(editor.templateFiles().size(), 3);
    ASSERT_GE(editor.exampleFiles().size(), 7);
    ASSERT_TRUE(editor.openFile(editor.exampleFiles().filter(QStringLiteral("03_beam_modes")).front()));
    ASSERT_TRUE(editor.run());
    ASSERT_TRUE(waitUntil([&] { return !editor.isRunning(); }, 120000));
    EXPECT_TRUE(editor.outputText().contains(QStringLiteral("16.33"))) << editor.outputText().toStdString();
    EXPECT_TRUE(editor.problems().isEmpty());
    // Scripts stored in the project, undoably.
    ASSERT_TRUE(editor.attachToProject(QStringLiteral("beam study")));
    EXPECT_TRUE(editor.projectScripts().contains(QStringLiteral("beam study")));
    EXPECT_TRUE(editor.openProjectScript(QStringLiteral("beam study")));
    EXPECT_TRUE(editor.text().contains(QStringLiteral("cantilever")));
    window_->controller().undo();
    EXPECT_TRUE(editor.projectScripts().isEmpty());
    EXPECT_FALSE(ScriptEditor::userScriptDirectory().contains(QStringLiteral(RESONANT_SOURCE_DIR)));
}
