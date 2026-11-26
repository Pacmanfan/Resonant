#include "AnalysisPanel.h"

#include "DocumentController.h"
#include "foundation/Units.h"
#include "foundation/Version.h"
#include "project/ProjectSerializer.h"
#include "ui_AnalysisPanel.h"

#include <QSysInfo>
#include <QThread>

using namespace resonant;

namespace {
constexpr int kJobIdRole = Qt::UserRole;

QString fromView(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
}  // namespace

AnalysisPanel::AnalysisPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::AnalysisPanel>())
{
    ui_->setupUi(this);
    ui_->levelCombo->setCurrentIndex(1);
    ui_->jobTable->setColumnWidth(0, 300);
    ui_->diagnosticsTree->setColumnWidth(0, 260);

    connect(ui_->clearButton, &QPushButton::clicked, this, &AnalysisPanel::clearConsole);
    connect(ui_->levelCombo, &QComboBox::currentIndexChanged, this, &AnalysisPanel::reloadConsole);
    connect(ui_->refreshButton, &QPushButton::clicked, this, &AnalysisPanel::refreshDiagnostics);
    connect(ui_->jobTable, &QTableWidget::itemSelectionChanged, this, &AnalysisPanel::updateJobSummary);
    connect(ui_->cancelJobButton, &QPushButton::clicked, this, [this] {
        const int row = ui_->jobTable->currentRow();
        if (row >= 0 && controller_)
            controller_->cancelJob(Id(ui_->jobTable->item(row, 0)->data(kJobIdRole).toString().toStdString()));
    });

    // Log entries can come from any thread; deliver them to this one.
    logSink_ = Logger::global().addSink([this](const LogEntry& entry) {
        QMetaObject::invokeMethod(this, [this, entry] { appendEntry(entry); }, Qt::QueuedConnection);
    });
    reloadConsole();
}

AnalysisPanel::~AnalysisPanel()
{
    Logger::global().removeSink(logSink_);
}

void AnalysisPanel::setController(DocumentController* controller)
{
    controller_ = controller;
    connect(controller_, &DocumentController::jobChanged, this, &AnalysisPanel::onJobChanged);
    connect(controller_, &DocumentController::projectReplaced, this, &AnalysisPanel::refreshDiagnostics);
    connect(controller_, &DocumentController::documentStateChanged, this, &AnalysisPanel::refreshDiagnostics);
    refreshDiagnostics();
}

void AnalysisPanel::setGraphicsDescription(const QString& description)
{
    graphics_ = description;
    refreshDiagnostics();
}

int AnalysisPanel::addTab(QWidget* widget, const QString& title)
{
    return ui_->tabs->addTab(widget, title);
}

void AnalysisPanel::showTab(QWidget* widget)
{
    ui_->tabs->setCurrentWidget(widget);
}

void AnalysisPanel::showSimulationStatus()
{
    ui_->tabs->setCurrentWidget(ui_->simulationTab);
}

QString AnalysisPanel::consoleText() const
{
    return ui_->consoleText->toPlainText();
}

int AnalysisPanel::jobRowCount() const
{
    return ui_->jobTable->rowCount();
}

QString AnalysisPanel::jobState(int row) const
{
    return ui_->jobTable->item(row, 1) ? ui_->jobTable->item(row, 1)->text() : QString();
}

QStringList AnalysisPanel::diagnosticsItems() const
{
    QStringList items;
    for (int i = 0; i < ui_->diagnosticsTree->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* group = ui_->diagnosticsTree->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
            items.append(group->text(0) + QLatin1Char('/') + group->child(j)->text(0) + QLatin1Char('=')
                         + group->child(j)->text(1));
    }
    return items;
}

void AnalysisPanel::clearConsole()
{
    Logger::global().clearHistory();
    ui_->consoleText->clear();
}

void AnalysisPanel::appendEntry(const LogEntry& entry)
{
    if (static_cast<int>(entry.level) < ui_->levelCombo->currentIndex())
        return;
    ui_->consoleText->appendPlainText(QString::fromStdString(formatLogEntry(entry)));
}

void AnalysisPanel::reloadConsole()
{
    ui_->consoleText->clear();
    for (const LogEntry& entry : Logger::global().history())
        appendEntry(entry);
}

void AnalysisPanel::refreshDiagnostics()
{
    ui_->diagnosticsTree->clear();
    const auto group = [this](const QString& title) {
        auto* item = new QTreeWidgetItem(ui_->diagnosticsTree, {title});
        item->setExpanded(true);
        return item;
    };
    const auto row = [](QTreeWidgetItem* parent, const QString& name, const QString& value) {
        new QTreeWidgetItem(parent, {name, value});
    };

    QTreeWidgetItem* application = group(QStringLiteral("Application"));
    row(application, QStringLiteral("Version"), QStringLiteral("%1 %2").arg(kApplicationName, kApplicationVersion));
    row(application, QStringLiteral("Project file format"), QString::number(kProjectSchemaVersion));
    row(application, QStringLiteral("Qt"), QString::fromLatin1(qVersion()));
    row(application, QStringLiteral("Operating system"), QSysInfo::prettyProductName());
    row(application, QStringLiteral("CPU architecture"), QSysInfo::currentCpuArchitecture());
    row(application, QStringLiteral("Logical processors"), QString::number(QThread::idealThreadCount()));
#ifdef NDEBUG
    row(application, QStringLiteral("Build"), QStringLiteral("Release"));
#else
    row(application, QStringLiteral("Build"), QStringLiteral("Debug"));
#endif

    QTreeWidgetItem* graphics = group(QStringLiteral("Graphics"));
    row(graphics, QStringLiteral("Context"), graphics_);
    row(graphics, QStringLiteral("Required"), QStringLiteral("OpenGL 4.5 Core"));

    QTreeWidgetItem* conventions = group(QStringLiteral("Conventions"));
    row(conventions, QStringLiteral("Internal units"), QStringLiteral("SI: m, s, Hz, kg, rad"));
    row(conventions, QStringLiteral("Axes"), fromView(units::Conventions::axes));
    row(conventions, QStringLiteral("Rotation"), fromView(units::Conventions::rotation));

    if (controller_) {
        const Project& project = controller_->project();
        QTreeWidgetItem* projectGroup = group(QStringLiteral("Project"));
        row(projectGroup, QStringLiteral("Name"), QString::fromStdString(project.name));
        row(projectGroup, QStringLiteral("File"),
            controller_->filePath().isEmpty() ? QStringLiteral("(not saved)") : controller_->filePath());
        row(projectGroup, QStringLiteral("Id"), QString::fromStdString(project.id.str()));
        row(projectGroup, QStringLiteral("Unsaved changes"), controller_->isModified() ? QStringLiteral("yes") : QStringLiteral("no"));
        row(projectGroup, QStringLiteral("Objects"), QString::number(project.scene.objectCount()));
        row(projectGroup, QStringLiteral("Materials"), QString::number(project.materials.materials().size()));
        row(projectGroup, QStringLiteral("Undo steps"), QString::number(controller_->commands().size()));
    }
}

void AnalysisPanel::onJobChanged(const JobInfo& info)
{
    const QString id = QString::fromStdString(info.id.str());
    int row = jobRows_.value(id, -1);
    if (row < 0) {
        row = ui_->jobTable->rowCount();
        ui_->jobTable->insertRow(row);
        for (int column = 0; column < 5; ++column)
            ui_->jobTable->setItem(row, column, new QTableWidgetItem());
        ui_->jobTable->item(row, 0)->setData(kJobIdRole, id);
        jobRows_.insert(id, row);
        ui_->jobTable->scrollToBottom();
    }
    ui_->jobTable->item(row, 0)->setText(QString::fromStdString(info.name));
    ui_->jobTable->item(row, 1)->setText(fromView(toString(info.state)));
    ui_->jobTable->item(row, 2)->setText(QStringLiteral("%1 %").arg(info.progress * 100.0, 0, 'f', 0));
    ui_->jobTable->item(row, 3)->setText(QStringLiteral("%1 s").arg(info.elapsedSeconds, 0, 'f', 2));
    ui_->jobTable->item(row, 4)->setText(QString::fromStdString(info.message));
    updateJobSummary();
}

void AnalysisPanel::updateJobSummary()
{
    int unfinished = 0;
    for (int row = 0; row < ui_->jobTable->rowCount(); ++row) {
        const QString state = ui_->jobTable->item(row, 1)->text();
        if (state == QLatin1String("queued") || state == QLatin1String("running"))
            ++unfinished;
    }
    ui_->jobSummaryLabel->setText(ui_->jobTable->rowCount() == 0
                                      ? QStringLiteral("No jobs have been run.")
                                      : QStringLiteral("%1 job(s), %2 unfinished").arg(ui_->jobTable->rowCount()).arg(unfinished));
    const int row = ui_->jobTable->currentRow();
    const QString state = row >= 0 ? ui_->jobTable->item(row, 1)->text() : QString();
    ui_->cancelJobButton->setEnabled(state == QLatin1String("queued") || state == QLatin1String("running"));
}
