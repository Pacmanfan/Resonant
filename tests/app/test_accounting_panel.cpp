// The Accounting panel in the real main window (RES-ACC-016): a study run as a job; flows
// whose band widths follow the calculated amounts; the conservation checks with honest
// statuses; forces; histories; a custom boundary reclassifying the same exchanges without
// running again; a record kept in the project and an export.
#include "AccountingController.h"
#include "AccountingPanel.h"
#include "DocumentController.h"
#include "EnergyFlowView.h"
#include "MainWindow.h"

#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;
using nlohmann::json;

namespace {

class AccountingPanelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
    }
    void TearDown() override
    {
        window_->close();
        window_.reset();
    }
    void open(const char* file)
    {
        const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "accounting" / file;
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(path.string())).ok());
        QTest::qWait(20);
    }
    QString run(const char* study)
    {
        QSignalSpy ready(&window_->accounting(), &AccountingController::resultReady);
        panel().selectStudy(QString::fromLatin1(study));
        panel().runButton()->click();
        EXPECT_TRUE(ready.wait(60000) || ready.count() > 0);
        return ready.isEmpty() ? QString() : ready.front().front().toString();
    }
    AccountingPanel& panel() { return window_->accountingPanel(); }
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(AccountingPanelTest, RunsAStudyAndShowsFlowsChecksForcesAndHistory)
{
    open("d-coupled-resonators.resonant");
    ASSERT_EQ(panel().studyCount(), 1);
    const QString id = run("st-beats");
    ASSERT_FALSE(id.isEmpty());
    EXPECT_EQ(panel().checkStatus("energy.total"), "verified");
    EXPECT_EQ(panel().checkStatus("momentum.angular"), "not applicable");
    EXPECT_EQ(panel().systemChoices(), (QStringList{"whole", "r1", "r2", "custom"}));
    // The bands follow the amounts: the widest is the largest, and the ratio of two is the ratio of their amounts.
    const json report = window_->accounting().result(id)["reports"][0];
    json first, second;
    for (const json& e : report["flows"]["edges"])
        if (e["family"] == "energy" && e["magnitude"].get<double>() > 0.0) {
            if (first.is_null())
                first = e;
            else if (second.is_null() && std::abs(e["magnitude"].get<double>() - first["magnitude"].get<double>()) > 1e-6 * first["magnitude"].get<double>())
                second = e;
        }
    ASSERT_FALSE(first.is_null());
    ASSERT_FALSE(second.is_null());
    const auto width = [&](const json& e) {
        return panel().flowView().bandWidth(QString::fromStdString(e["from"].get<std::string>()), QString::fromStdString(e["to"].get<std::string>()));
    };
    const double w1 = width(first), w2 = width(second);
    ASSERT_GT(w1, 0.0);
    ASSERT_GT(w2, 0.0);
    if (w1 > 1.0 && w2 > 1.0) {  // above the one-pixel floor the widths are proportional
        EXPECT_NEAR(w1 / w2, first["magnitude"].get<double>() / second["magnitude"].get<double>(), 1e-9);
    }
    EXPECT_TRUE(panel().flowView().edgeLabel(QString::fromStdString(first["from"].get<std::string>()), QString::fromStdString(first["to"].get<std::string>()))
                    .contains("J"));
    EXPECT_GT(panel().forceRows(), 0);
    ASSERT_TRUE(panel().familyChoices().contains("linear-momentum"));
    panel().selectFamily("linear-momentum");
    EXPECT_GT(panel().flowView().edgeCount(), 0);
    ASSERT_FALSE(panel().seriesChoices().isEmpty());
    panel().showSeries(0);
    EXPECT_GT(panel().plottedPoints(), 10);
}

TEST_F(AccountingPanelTest, ACustomBoundaryReclassifiesWithoutRunningAgain)
{
    open("d-coupled-resonators.resonant");
    ASSERT_FALSE(run("st-beats").isEmpty());
    QSignalSpy runs(&window_->accounting(), &AccountingController::resultReady);
    panel().selectSystem("custom");
    ASSERT_TRUE(panel().applyButton()->isEnabled());
    panel().tickMembers({"body:r1"});
    panel().applyButton()->click();
    QTest::qWait(20);
    EXPECT_EQ(runs.count(), 0);  // no new run
    EXPECT_TRUE(panel().statusText().contains("Custom system")) << panel().statusText().toStdString();
    EXPECT_EQ(panel().checkStatus("energy.total"), "verified");
    EXPECT_EQ(panel().checkStatus("momentum.linear"), "verified");
}

TEST_F(AccountingPanelTest, MissingMomentumIsSaidAndRecordsAndExportsWork)
{
    open("e-microwave-absorption.resonant");
    ASSERT_FALSE(run("st-steady").isEmpty());
    EXPECT_EQ(panel().checkStatus("energy.total"), "verified");
    EXPECT_EQ(panel().checkStatus("momentum.linear"), "insufficient data");
    ASSERT_TRUE(panel().saveButton()->isEnabled());
    panel().saveButton()->click();
    QTest::qWait(20);
    EXPECT_EQ(window_->accounting().records().size(), 1u);
    window_->controller().undo();
    QTest::qWait(20);
    EXPECT_TRUE(window_->accounting().records().empty());
    QTemporaryDir dir;
    const QString path = dir.filePath("accounting.md");
    ASSERT_TRUE(panel().exportTo(path));
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_TRUE(f.readAll().contains("| Check | Status |"));
}
