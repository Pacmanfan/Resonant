// The in-app help in the real main window (REQ-CHAT-20261126-010,
// RES-HELP-001..010): it starts offline from the built help, navigates
// (contents, breadcrumbs, back / forward, reading order, sections), searches
// with highlighted matches, loads images and opens them full size, shows
// readable mathematics, handles a missing topic, opens the right topic for the
// panel the user works in (F1), and works from an installed layout.
#include "AcousticsPanel.h"
#include "DocumentController.h"
#include "EmPanel.h"
#include "HelpTopics.h"
#include "HelpWindow.h"
#include "MainWindow.h"
#include "MechanicsPanel.h"
#include "OptimizationPanel.h"
#include "PropertiesPanel.h"
#include "ResourcePaths.h"
#include "ViewportWidget.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>

namespace {

class HelpUiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        ASSERT_TRUE(window_->helpWindow().isLoaded()) << window_->helpWindow().loadError().toStdString();
    }
    void TearDown() override
    {
        window_->close();
        window_.reset();
    }
    HelpWindow& help() { return window_->helpWindow(); }
    // F1 with the keyboard focus in `widget`.
    QString contextTopicFor(QWidget* widget)
    {
        widget->setFocus(Qt::OtherFocusReason);
        QTest::qWait(5);
        return HelpTopics::topicFor(widget);
    }
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(HelpUiTest, NavigationBreadcrumbsHistoryAndReadingOrder)
{
    ASSERT_TRUE(help().showTopic(QStringLiteral("acoustics.focusing")));
    EXPECT_TRUE(help().articleText().contains(QStringLiteral("Focusing")));
    EXPECT_EQ(help().breadcrumbText(), QStringLiteral("Help › Acoustic Engineering › Focusing"));
    EXPECT_FALSE(help().sections().isEmpty());
    EXPECT_TRUE(help().showSection(1));
    const QString first = help().currentTopic();
    help().next();
    const QString second = help().currentTopic();
    EXPECT_NE(second, first);
    help().previous();
    EXPECT_EQ(help().currentTopic(), first);
    help().back();
    EXPECT_EQ(help().currentTopic(), second);
    help().forward();
    EXPECT_EQ(help().currentTopic(), first);
    help().home();
    EXPECT_EQ(help().currentTopic(), QStringLiteral("home"));
    EXPECT_TRUE(help().articleText().contains(QStringLiteral("Common workflows")));
}

TEST_F(HelpUiTest, SearchRanksAndHighlightsOffline)
{
    const QStringList hits = help().search(QStringLiteral("modal analysis"));
    ASSERT_FALSE(hits.isEmpty());
    EXPECT_EQ(hits.front(), QStringLiteral("mechanics.modal"));
    ASSERT_TRUE(help().openResult(0));
    EXPECT_EQ(help().currentTopic(), QStringLiteral("mechanics.modal"));
    EXPECT_GT(help().highlightCount(), 0);
    EXPECT_TRUE(help().search(QStringLiteral("xyzzy-not-a-word")).isEmpty());
    EXPECT_FALSE(help().search(QStringLiteral("refl")).isEmpty());  // a prefix while typing
}

TEST_F(HelpUiTest, ImagesMathematicsAndAMissingTopic)
{
    ASSERT_TRUE(help().showTopic(QStringLiteral("ui.main-window")));
    EXPECT_GE(help().loadedImageCount(), 1) << "the screenshot must load from the help's own files";
    QDialog* viewer = help().showImage(QStringLiteral("images/screenshots/main-window-overview.png"));
    ASSERT_NE(viewer, nullptr);
    viewer->close();
    EXPECT_EQ(help().showImage(QStringLiteral("images/screenshots/no-such.png")), nullptr);
    ASSERT_TRUE(help().showTopic(QStringLiteral("acoustics.focusing")));
    EXPECT_TRUE(help().articleText().contains(QStringLiteral("φ_i = k · r_i"))) << "equations are readable text";
    EXPECT_FALSE(help().showTopic(QStringLiteral("no.such.topic")));
    EXPECT_TRUE(help().articleText().contains(QStringLiteral("Topic not found")));
}

TEST_F(HelpUiTest, ContextHelpOpensTheTopicOfThePanelInUse)
{
    // Every topic of the central registry exists in the built help.
    for (auto it = HelpTopics::registry().constBegin(); it != HelpTopics::registry().constEnd(); ++it)
        EXPECT_NE(help().index()->find(it.value().toStdString()), nullptr) << it.key().toStdString() << " -> " << it.value().toStdString();
    // Panels and their tabs.
    window_->findChild<QDockWidget*>(QStringLiteral("emDock"))->show();
    window_->emPanel().showTab(QStringLiteral("patternTab"));
    auto* patternTab = window_->emPanel().findChild<QWidget*>(QStringLiteral("patternTab"));
    ASSERT_NE(patternTab, nullptr);
    EXPECT_EQ(contextTopicFor(patternTab), QStringLiteral("em.patterns"));
    window_->mechanicsPanel().showTab(QStringLiteral("harmonicTab"));
    EXPECT_EQ(contextTopicFor(window_->mechanicsPanel().findChild<QWidget*>(QStringLiteral("harmonicTab"))), QStringLiteral("mechanics.forced-response"));
    EXPECT_EQ(contextTopicFor(&window_->optimizationPanel()), QStringLiteral("opt.studio"));
    EXPECT_EQ(contextTopicFor(&window_->viewport()), QStringLiteral("ui.viewport"));
    // A property editor: the antenna-array page for an array, not the home page.
    ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(
        (std::filesystem::path(RESONANT_SOURCE_DIR) / "examples/acoustics/b-linear-array.resonant").string())).ok());
    window_->controller().select(resonant::Id("ex-b-array"));
    QTest::qWait(20);
    QWidget* editor = nullptr;
    for (QWidget* w : window_->properties().findChildren<QWidget*>())
        if (QString::fromLatin1(w->metaObject()->className()) == QLatin1String("ArrayEditor") && w->isVisible())
            editor = w;
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(contextTopicFor(editor), QStringLiteral("acoustics.arrays"));
    // F1 itself opens the help window on that topic.
    editor->setFocus();
    window_->action(QStringLiteral("actionContextHelp"))->trigger();
    EXPECT_TRUE(help().isVisible());
    EXPECT_EQ(help().currentTopic(), QStringLiteral("acoustics.arrays"));
    // Failure messages lead to troubleshooting.
    EXPECT_EQ(HelpTopics::topicForMessage(QStringLiteral("Traceback: NameError in the script")), QStringLiteral("trouble.python"));
    EXPECT_EQ(HelpTopics::topicForMessage(QStringLiteral("the full-wave solver refuses: no active emitter")), QStringLiteral("trouble.solver-failures"));
}

TEST_F(HelpUiTest, TheHelpWorksFromAnInstalledLayout)
{
    // An installation: <prefix>/bin and <prefix>/share/resonant/help, no source or build tree.
    QTemporaryDir prefix;
    ASSERT_TRUE(prefix.isValid());
    const std::filesystem::path built = help().index()->directory();
    const std::filesystem::path installed = std::filesystem::path(prefix.path().toStdString()) / "share" / "resonant" / "help";
    std::filesystem::create_directories(installed);
    for (const char* item : {"html", "index.json", "search.json"})
        std::filesystem::copy(built / item, installed / item, std::filesystem::copy_options::recursive);
    QDir(prefix.path()).mkpath(QStringLiteral("bin"));
    const ResourcePaths paths = ResourcePaths::locateFrom(prefix.path() + QStringLiteral("/bin"), false);
    EXPECT_EQ(QDir(paths.help).canonicalPath(), QDir(QString::fromStdString(installed.string())).canonicalPath());
    HelpWindow installedHelp;
    ASSERT_TRUE(installedHelp.load(paths.help)) << installedHelp.loadError().toStdString();
    EXPECT_TRUE(installedHelp.showTopic(QStringLiteral("ui.main-window")));
    EXPECT_GE(installedHelp.loadedImageCount(), 1);
    EXPECT_FALSE(installedHelp.search(QStringLiteral("dipole")).isEmpty());
    HelpWindow missing;
    EXPECT_FALSE(missing.load(QString()));
    EXPECT_TRUE(missing.articleText().contains(QStringLiteral("Help unavailable")));
}
