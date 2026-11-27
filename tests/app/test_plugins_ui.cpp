// Plugins and workflows in the real main window (REQ-CHAT-20261126-010,
// RES-PLUG-*, RES-AUTO-*): the Plugins dialog lists every plugin with its
// state; enabling persists and loads on Reload; a workflow pack's templates
// run from the Workflows dialog as one undoable change recorded in the
// project; a failing plugin is reported and leaves the project untouched;
// a Python plugin's extension appears in Tools > Extensions.
#include "DocumentController.h"
#include "MainWindow.h"
#include "PluginsDialog.h"
#include "PythonSession.h"
#include "WorkflowsDialog.h"
#include "plugins/PluginManager.h"
#include "project/ProjectSerializer.h"

#include <QAction>
#include <QDir>
#include <QElapsedTimer>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

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

class PluginsUiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // A private plugin configuration and the test plugins on the search path.
        ASSERT_TRUE(config_.isValid());
        savedConfig_ = qgetenv("XDG_CONFIG_HOME");
        savedPath_ = qgetenv("RESONANT_PLUGIN_PATH");
        qputenv("XDG_CONFIG_HOME", config_.path().toLocal8Bit());
        qputenv("RESONANT_PLUGIN_PATH", QByteArray(RESONANT_SOURCE_DIR) + "/tests/data/plugins");
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
    }
    void TearDown() override
    {
        window_->pythonSession().stop();
        window_->close();
        window_.reset();
        qputenv("XDG_CONFIG_HOME", savedConfig_);
        qputenv("RESONANT_PLUGIN_PATH", savedPath_);
    }
    const resonant::plugins::PluginRecord* plugin(const char* id) { return window_->plugins().find(id); }
    QTemporaryDir config_;
    QByteArray savedConfig_, savedPath_;
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(PluginsUiTest, TheDialogShowsEveryPluginAndLoadsOnlyWhatIsEnabled)
{
    PluginsDialog* dialog = window_->showPluginsDialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_GE(dialog->rowCount(), 10);
    const int pack = dialog->rowOf(QStringLiteral("examples.workflow-pack"));
    ASSERT_GE(pack, 0);
    EXPECT_EQ(dialog->cell(pack, 4), QStringLiteral("disabled"));
    EXPECT_EQ(dialog->cell(dialog->rowOf(QStringLiteral("test.wrong-api")), 4), QStringLiteral("incompatible"));
    EXPECT_EQ(dialog->cell(dialog->rowOf(QStringLiteral("test.needs-missing")), 4), QStringLiteral("missing dependency"));
    EXPECT_FALSE(dialog->cell(dialog->rowOf(QStringLiteral("bad-manifest")), 5).contains(QDir::homePath()));
    // Enable: persisted, effective after Reload.
    ASSERT_TRUE(dialog->setEnabled(QStringLiteral("examples.workflow-pack"), true));
    ASSERT_TRUE(dialog->setEnabled(QStringLiteral("test.bad-native"), true));
    const nlohmann::json before = resonant::serializeProject(window_->controller().project());
    window_->reloadPlugins();
    dialog->refresh();
    EXPECT_EQ(plugin("examples.workflow-pack")->state, resonant::plugins::PluginState::Loaded);
    EXPECT_EQ(plugin("test.bad-native")->state, resonant::plugins::PluginState::Failed);
    EXPECT_EQ(dialog->cell(dialog->rowOf(QStringLiteral("test.bad-native")), 4), QStringLiteral("failed"));
    // A failing plugin changes nothing in the project.
    nlohmann::json after = resonant::serializeProject(window_->controller().project());
    EXPECT_EQ(after["scene"], before["scene"]);
    EXPECT_EQ(after["extensions"], before["extensions"]);
    dialog->close();
}

TEST_F(PluginsUiTest, AWorkflowFromAPluginRunsAsOneUndoableRecordedChange)
{
    ASSERT_TRUE(window_->plugins().setEnabled("examples.workflow-pack", true).ok());
    window_->reloadPlugins();
    WorkflowsDialog* dialog = window_->showWorkflowsDialog();
    ASSERT_TRUE(dialog->workflowIds().contains(QStringLiteral("examples.acoustic-spacing-sweep")));
    ASSERT_TRUE(dialog->selectWorkflow(QStringLiteral("examples.acoustic-spacing-sweep")));
    QTemporaryDir out;
    dialog->setOutputDirectory(out.path());
    ASSERT_TRUE(dialog->setParameter(QStringLiteral("spacing"), QStringLiteral("[0.0043, 0.006]")));
    QSignalSpy finished(dialog, &WorkflowsDialog::runFinished);
    const size_t objects = window_->controller().scene().objectCount();
    ASSERT_TRUE(dialog->run());
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 1; }, 120000));
    EXPECT_TRUE(finished.front().at(0).toBool()) << dialog->resultText().toStdString();
    EXPECT_TRUE(dialog->resultText().contains(QStringLiteral("completed")));
    EXPECT_TRUE(QFile::exists(out.filePath(QStringLiteral("spacing-sweep.json"))));
    EXPECT_EQ(window_->controller().scene().objectCount(), objects + 2);  // the array and the plane
    EXPECT_EQ(dialog->historyCount(), 1);
    window_->controller().undo();  // one step reverts the workflow's changes and its record
    EXPECT_EQ(window_->controller().scene().objectCount(), objects);
    EXPECT_EQ(dialog->historyCount(), 0) << "the run's record is part of the undone step";
    dialog->close();
}

TEST_F(PluginsUiTest, APythonPluginsExtensionAppearsInTheExtensionsMenu)
{
    ASSERT_TRUE(window_->plugins().setEnabled("examples.array-report", true).ok());
    ASSERT_TRUE(window_->plugins().setEnabled("test.broken-python", true).ok());
    window_->reloadPlugins();
    if (plugin("examples.array-report")->state == resonant::plugins::PluginState::Incompatible)
        GTEST_SKIP() << "Python is not available: " << plugin("examples.array-report")->message;
    ASSERT_TRUE(waitUntil([&] { return plugin("examples.array-report")->state != resonant::plugins::PluginState::Ready; }, 60000));
    EXPECT_EQ(plugin("examples.array-report")->state, resonant::plugins::PluginState::Loaded) << plugin("examples.array-report")->message;
    EXPECT_EQ(plugin("test.broken-python")->state, resonant::plugins::PluginState::Failed);
    EXPECT_NE(plugin("test.broken-python")->message.find("fails on purpose"), std::string::npos);
    QMenu* menu = window_->findChild<QMenu*>(QStringLiteral("menuExtensions"));
    ASSERT_NE(menu, nullptr);
    QStringList titles;
    for (QAction* a : menu->actions())
        titles.append(a->text());
    EXPECT_TRUE(titles.contains(QStringLiteral("Analyse: Array report"))) << titles.join(QStringLiteral(", ")).toStdString();
}
