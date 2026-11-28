#include "MaterialScalesPanel.h"

#include "MultiscaleController.h"

#include "ui_MaterialScalesPanel.h"

#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidgetItem>

using nlohmann::json;

namespace {

QString text(const json& v, const char* key)
{
    return QString::fromStdString(v.is_object() ? v.value(key, std::string()) : std::string());
}

QString number(const json& v)
{
    return v.is_number() ? QString::number(v.get<double>(), 'g', 7) : QStringLiteral("—");
}

const char* kScales[] = {"continuum", "effective", "microstructure", "molecular", "atomistic", "electronic"};

}  // namespace

MaterialScalesPanel::MaterialScalesPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::MaterialScalesPanel>())
{
    ui_->setupUi(this);
    ui_->representationTree->header()->setStretchLastSection(true);
    ui_->lineageTable->horizontalHeader()->setStretchLastSection(true);
    ui_->lineageTable->verticalHeader()->setVisible(false);
    setEnabled(false);
}

MaterialScalesPanel::~MaterialScalesPanel() = default;

void MaterialScalesPanel::setController(MultiscaleController* controller)
{
    controller_ = controller;
    setEnabled(controller_ != nullptr);
    connect(controller_, &MultiscaleController::modelChanged, this, &MaterialScalesPanel::refresh);
    connect(controller_, &MultiscaleController::resultReady, this, [this](const QString& id) {
        resultId_ = id;
        const json r = controller_->result(id);
        QStringList lines;
        for (const json& p : r.value("properties", json::array()))
            lines << QStringLiteral("%1.%2 (%3) = %4 %5").arg(text(p, "domain"), text(p, "key"), text(p, "component"), number(p["value"]), text(p, "unit"));
        const bool reliable = r.value("reliable", false);
        QString status = tr("Derived by %1: %2").arg(text(r, "method"), lines.isEmpty() ? tr("observables only (no library property)") : lines.join(QStringLiteral("; ")));
        if (!reliable)
            status += tr(" — not marked reliable: ") + QString::fromStdString(r.value("reliability", json::array()).dump());
        ui_->statusLabel->setText(status);
        ui_->assignButton->setEnabled(!lines.isEmpty() && !r.value("stale", false));
        ui_->deriveButton->setEnabled(true);
    });
    connect(controller_, &MultiscaleController::derivationFailed, this, [this](const QString& message) {
        ui_->statusLabel->setText(tr("The derivation failed: %1").arg(message));
        ui_->deriveButton->setEnabled(true);
    });
    connect(ui_->representationTree, &QTreeWidget::currentItemChanged, this, [this] {
        if (!filling_)
            showRepresentation();
    });
    connect(ui_->deriveButton, &QPushButton::clicked, this, [this] {
        auto job = controller_->derive(currentRepresentation(), ui_->methodCombo->currentData().toString());
        if (!job) {
            ui_->statusLabel->setText(QString::fromStdString(job.message()));
            return;
        }
        resultId_.clear();
        ui_->assignButton->setEnabled(false);
        ui_->deriveButton->setEnabled(false);
        ui_->statusLabel->setText(tr("Deriving…"));
    });
    connect(ui_->assignButton, &QPushButton::clicked, this, [this] {
        const QString target = ui_->targetCombo->currentData().toString();
        const auto status = controller_->assign(resultId_, target);
        if (!status) {
            ui_->statusLabel->setText(tr("Not assigned: %1").arg(QString::fromStdString(status.message())));
            return;
        }
        ui_->statusLabel->setText(tr("Assigned to '%1' with its derivation. Undo removes it.").arg(ui_->targetCombo->currentText()));
        showLineage(target);
    });
    connect(ui_->lineageCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_)
            fillLineage();
    });
    refresh();
}

void MaterialScalesPanel::refresh()
{
    if (!controller_)
        return;
    filling_ = true;
    const QString current = currentRepresentation();
    const json reps = controller_->representations();
    ui_->representationTree->clear();
    QTreeWidgetItem* select = nullptr;
    for (const char* scale : kScales) {
        QTreeWidgetItem* group = nullptr;
        for (const json& r : reps) {
            if (r.value("scale", "") != scale)
                continue;
            if (!group) {
                group = new QTreeWidgetItem(ui_->representationTree, {QString::fromLatin1(scale)});
                group->setFlags(Qt::ItemIsEnabled);
            }
            auto* item = new QTreeWidgetItem(group, {text(r, "name"), text(r, "kind"), text(r, "provenance"), text(r, "validation")});
            item->setData(0, Qt::UserRole, QString::fromStdString(r.value("id", "")));
            if (QString::fromStdString(r.value("id", "")) == current || !select)
                select = item;
        }
    }
    ui_->representationTree->expandAll();
    ui_->summaryLabel->setText(reps.empty() ? tr("No material representations in this project. Example projects: examples/multiscale.")
                                            : tr("%1 representation(s). Derived values carry their lineage; select a material below to trace it.").arg(reps.size()));
    {
        const QSignalBlocker a(ui_->targetCombo), b(ui_->lineageCombo);
        const QString target = ui_->targetCombo->currentData().toString(), traced = ui_->lineageCombo->currentData().toString();
        ui_->targetCombo->clear();
        ui_->lineageCombo->clear();
        for (const json& m : controller_->materials()) {
            ui_->targetCombo->addItem(text(m, "name"), text(m, "id"));
            ui_->lineageCombo->addItem(text(m, "name"), text(m, "id"));
        }
        if (const int i = ui_->targetCombo->findData(target); i >= 0)
            ui_->targetCombo->setCurrentIndex(i);
        if (const int i = ui_->lineageCombo->findData(traced); i >= 0)
            ui_->lineageCombo->setCurrentIndex(i);
    }
    filling_ = false;
    if (select)
        ui_->representationTree->setCurrentItem(select);
    showRepresentation();
    fillLineage();
    if (!resultId_.isEmpty())
        ui_->assignButton->setEnabled(!controller_->result(resultId_).value("stale", true));
}

void MaterialScalesPanel::showRepresentation()
{
    const QString id = currentRepresentation();
    const QSignalBlocker block(ui_->methodCombo);
    ui_->methodCombo->clear();
    if (id.isEmpty()) {
        ui_->detailsText->clear();
        ui_->deriveButton->setEnabled(false);
        return;
    }
    const json r = controller_->representation(id);
    QStringList d;
    d << tr("%1 — %2 scale, %3").arg(text(r, "name"), text(r, "scale"), text(r, "kind"));
    if (!text(r, "description").isEmpty())
        d << text(r, "description");
    d << tr("Provenance: %1 (%2)").arg(text(r, "provenance"), text(r, "source"));
    d << tr("Validation: %1").arg(text(r, "validation"));
    d << tr("Revision: %1").arg(text(r, "revision"));
    const json card = r.value("modelCardDocument", json::object());
    if (!card.empty()) {
        d << tr("Model card: %1").arg(text(card, "title"));
        for (const json& a : card.value("assumptions", json::array()))
            d << QStringLiteral("  • ") + QString::fromStdString(a.get<std::string>());
        d << tr("  valid: %1").arg(text(card, "validity"));
        d << tr("  read results as: %1").arg(text(card, "interpretation"));
    }
    for (const json& rel : r.value("relationships", json::array()))
        d << tr("Relationship: %1 —%2→ %3").arg(text(rel, "from"), text(rel, "kind"), text(rel, "to"));
    for (const json& m : r.value("methods", json::array())) {
        if (m.value("applicable", false))
            ui_->methodCombo->addItem(text(m, "id"), text(m, "id"));
        else
            d << tr("Not applicable: %1 (%2)").arg(text(m, "id"), QString::fromStdString(m.value("issues", json::array()).empty() ? std::string()
                                                                                                                         : m["issues"][0].get<std::string>()));
    }
    ui_->detailsText->setPlainText(d.join(QLatin1Char('\n')));
    ui_->deriveButton->setEnabled(ui_->methodCombo->count() > 0 && !controller_->busy());
}

void MaterialScalesPanel::fillLineage()
{
    ui_->lineageTable->setRowCount(0);
    const QString id = ui_->lineageCombo->currentData().toString();
    if (id.isEmpty())
        return;
    const json l = controller_->lineage(id);
    for (const json& p : l.value("properties", json::array())) {
        const int row = ui_->lineageTable->rowCount();
        ui_->lineageTable->insertRow(row);
        ui_->lineageTable->setItem(row, 0, new QTableWidgetItem(text(p, "domain") + QLatin1Char('.') + text(p, "key")));
        QString value = number(p["value"]);
        if (p.contains("uncertainty"))
            value += QStringLiteral(" ± ") + number(p["uncertainty"]);
        ui_->lineageTable->setItem(row, 1, new QTableWidgetItem(value));
        ui_->lineageTable->setItem(row, 2, new QTableWidgetItem(text(p, "provenance")));
        const json rep = p.value("representation", json::object());
        auto* source = new QTableWidgetItem(rep.empty() ? text(p, "source") : tr("%1 (%2)").arg(text(rep, "name"), text(rep, "scale")));
        source->setToolTip(text(p, "source"));
        ui_->lineageTable->setItem(row, 3, source);
        QString status = p.value("derivation", "").empty() ? tr("—") : tr("current");
        if (p.value("stale", false)) {
            QStringList reasons;
            for (const json& s : p.value("staleReasons", json::array()))
                reasons << QString::fromStdString(s.get<std::string>());
            status = tr("stale: %1").arg(reasons.join(QStringLiteral("; ")));
        }
        ui_->lineageTable->setItem(row, 4, new QTableWidgetItem(status));
    }
}

int MaterialScalesPanel::representationCount() const
{
    int n = 0;
    for (int i = 0; i < ui_->representationTree->topLevelItemCount(); ++i)
        n += ui_->representationTree->topLevelItem(i)->childCount();
    return n;
}

void MaterialScalesPanel::selectRepresentation(const QString& id)
{
    for (int i = 0; i < ui_->representationTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* g = ui_->representationTree->topLevelItem(i);
        for (int k = 0; k < g->childCount(); ++k)
            if (g->child(k)->data(0, Qt::UserRole).toString() == id)
                ui_->representationTree->setCurrentItem(g->child(k));
    }
}

QString MaterialScalesPanel::currentRepresentation() const
{
    const QTreeWidgetItem* item = ui_->representationTree->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

QStringList MaterialScalesPanel::methodChoices() const
{
    QStringList out;
    for (int i = 0; i < ui_->methodCombo->count(); ++i)
        out << ui_->methodCombo->itemData(i).toString();
    return out;
}

void MaterialScalesPanel::chooseMethod(const QString& id)
{
    ui_->methodCombo->setCurrentIndex(ui_->methodCombo->findData(id));
}

void MaterialScalesPanel::chooseTarget(const QString& materialId)
{
    ui_->targetCombo->setCurrentIndex(ui_->targetCombo->findData(materialId));
}

void MaterialScalesPanel::showLineage(const QString& materialId)
{
    ui_->lineageCombo->setCurrentIndex(ui_->lineageCombo->findData(materialId));
    fillLineage();
}

QPushButton* MaterialScalesPanel::deriveButton() const
{
    return ui_->deriveButton;
}

QPushButton* MaterialScalesPanel::assignButton() const
{
    return ui_->assignButton;
}

QString MaterialScalesPanel::statusText() const
{
    return ui_->statusLabel->text();
}

QString MaterialScalesPanel::detailsText() const
{
    return ui_->detailsText->toPlainText();
}

int MaterialScalesPanel::lineageRowCount() const
{
    return ui_->lineageTable->rowCount();
}

QString MaterialScalesPanel::lineageCell(int row, int column) const
{
    const QTableWidgetItem* item = ui_->lineageTable->item(row, column);
    return item ? item->text() : QString();
}
