#include "MaterialsDialog.h"

#include "ui_MaterialsDialog.h"

#include <QComboBox>
#include <QPushButton>

using namespace resonant;

namespace {
enum Column { Property = 0, Value, Unit, ProvenanceColumn, ValidFrom, ValidTo, Source };

const Provenance kProvenances[] = {Provenance::Measured, Provenance::ManufacturerSupplied,
                                   Provenance::LiteratureDerived, Provenance::Theoretical, Provenance::Illustrative};

QString provenanceLabel(Provenance provenance)
{
    switch (provenance) {
    case Provenance::Measured: return QStringLiteral("Measured");
    case Provenance::ManufacturerSupplied: return QStringLiteral("Manufacturer-supplied");
    case Provenance::LiteratureDerived: return QStringLiteral("Literature-derived");
    case Provenance::Theoretical: return QStringLiteral("Theoretical");
    case Provenance::Illustrative: return QStringLiteral("Illustrative (not authoritative)");
    }
    return {};
}

QString numberText(double value)
{
    return QString::number(value, 'g', 12);
}

// Empty text is "no value"; otherwise the text must be a number.
bool parseOptional(const QString& text, std::optional<double>& out)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        out.reset();
        return true;
    }
    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    if (ok)
        out = value;
    return ok;
}
}  // namespace

MaterialsDialog::MaterialsDialog(const MaterialLibrary& library, QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::MaterialsDialog>()), library_(library)
{
    ui_->setupUi(this);
    ui_->splitter->setStretchFactor(0, 1);
    ui_->splitter->setStretchFactor(1, 4);
    ui_->propertyTable->setColumnWidth(Property, 190);
    ui_->propertyTable->setColumnWidth(ProvenanceColumn, 210);
    ui_->errorLabel->setStyleSheet(QStringLiteral("color: #c0392b;"));

    connect(ui_->materialList, &QListWidget::currentRowChanged, this, &MaterialsDialog::showCurrent);
    connect(ui_->newButton, &QPushButton::clicked, this, &MaterialsDialog::addMaterial);
    connect(ui_->duplicateButton, &QPushButton::clicked, this, &MaterialsDialog::duplicateCurrent);
    connect(ui_->deleteButton, &QPushButton::clicked, this, &MaterialsDialog::deleteCurrent);
    connect(ui_->domainCombo, &QComboBox::currentIndexChanged, this, &MaterialsDialog::fillPropertyTable);
    connect(ui_->propertyTable, &QTableWidget::cellChanged, this, &MaterialsDialog::onCellEdited);
    connect(ui_->nameEdit, &QLineEdit::editingFinished, this, [this] {
        Material* material = current();
        if (updating_ || !material)
            return;
        const QString name = ui_->nameEdit->text().trimmed();
        if (name.isEmpty()) {
            showError(QStringLiteral("A material needs a name."));
            ui_->nameEdit->setText(QString::fromStdString(material->name()));
            return;
        }
        material->setName(name.toStdString());
        ui_->materialList->currentItem()->setText(name);
        showError({});
    });
    connect(ui_->descriptionEdit, &QLineEdit::editingFinished, this, [this] {
        if (Material* material = current(); material && !updating_)
            material->setDescription(ui_->descriptionEdit->text().toStdString());
    });
    connect(ui_->domainApplies, &QCheckBox::toggled, this, [this](bool applies) {
        Material* material = current();
        if (updating_ || !material)
            return;
        if (applies)
            material->addDomain(domain());
        else
            material->removeDomain(domain());
        fillPropertyTable();
    });
    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, [this] {
        // Commit a name / description still being typed.
        ui_->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
        accept();
    });

    refreshList();
    if (ui_->materialList->count() > 0)
        ui_->materialList->setCurrentRow(0);
    else
        showCurrent();
}

MaterialsDialog::~MaterialsDialog() = default;

Material* MaterialsDialog::current()
{
    const QListWidgetItem* item = ui_->materialList->currentItem();
    if (!item)
        return nullptr;
    return library_.findMutable(Id(item->data(Qt::UserRole).toString().toStdString()));
}

PhysicsDomain MaterialsDialog::domain() const
{
    switch (ui_->domainCombo->currentIndex()) {
    case 1: return PhysicsDomain::Mechanical;
    case 2: return PhysicsDomain::Electromagnetic;
    default: return PhysicsDomain::Acoustic;
    }
}

void MaterialsDialog::setCurrentDomain(PhysicsDomain newDomain)
{
    ui_->domainCombo->setCurrentIndex(newDomain == PhysicsDomain::Mechanical ? 1
                                      : newDomain == PhysicsDomain::Electromagnetic ? 2 : 0);
}

Id MaterialsDialog::currentMaterialId() const
{
    const QListWidgetItem* item = ui_->materialList->currentItem();
    return item ? Id(item->data(Qt::UserRole).toString().toStdString()) : Id();
}

QString MaterialsDialog::errorText() const
{
    return ui_->errorLabel->text();
}

void MaterialsDialog::showError(const QString& message)
{
    ui_->errorLabel->setText(message);
}

void MaterialsDialog::refreshList()
{
    const Id selected = currentMaterialId();
    updating_ = true;
    ui_->materialList->clear();
    for (const Material& material : library_.materials()) {
        auto* item = new QListWidgetItem(QString::fromStdString(material.name()), ui_->materialList);
        item->setData(Qt::UserRole, QString::fromStdString(material.id().str()));
    }
    updating_ = false;
    selectMaterial(selected);
}

void MaterialsDialog::selectMaterial(const Id& id)
{
    for (int row = 0; row < ui_->materialList->count(); ++row) {
        if (ui_->materialList->item(row)->data(Qt::UserRole).toString().toStdString() == id.str()) {
            ui_->materialList->setCurrentRow(row);
            return;
        }
    }
}

Id MaterialsDialog::addMaterial()
{
    QString name = QStringLiteral("New material");
    for (int suffix = 2; library_.findByName(name.toStdString()); ++suffix)
        name = QStringLiteral("New material %1").arg(suffix);
    Material material(Id::generate(), name.toStdString());
    const Id id = material.id();
    if (Status status = library_.add(std::move(material)); !status) {
        showError(QString::fromStdString(status.message()));
        return Id();
    }
    refreshList();
    selectMaterial(id);
    return id;
}

Id MaterialsDialog::duplicateCurrent()
{
    const Material* source = current();
    if (!source)
        return Id();
    Material renamed(Id::generate(), source->name() + " copy");
    renamed.setDescription(source->description());
    for (const PhysicsDomain d : source->domains()) {
        renamed.addDomain(d);
        for (const auto& [key, value] : *source->properties(d))
            [[maybe_unused]] const Status status = renamed.setProperty(d, key, value);
    }
    const Id id = renamed.id();
    if (Status status = library_.add(std::move(renamed)); !status) {
        showError(QString::fromStdString(status.message()));
        return Id();
    }
    refreshList();
    selectMaterial(id);
    return id;
}

void MaterialsDialog::deleteCurrent()
{
    const Id id = currentMaterialId();
    if (id.isNull())
        return;
    const int row = ui_->materialList->currentRow();
    library_.remove(id);
    refreshList();
    if (ui_->materialList->count() > 0)
        ui_->materialList->setCurrentRow(std::min(row, ui_->materialList->count() - 1));
    else
        showCurrent();
}

void MaterialsDialog::showCurrent()
{
    if (updating_)
        return;
    const Material* material = current();
    updating_ = true;
    const bool has = material != nullptr;
    ui_->nameEdit->setEnabled(has);
    ui_->descriptionEdit->setEnabled(has);
    ui_->domainCombo->setEnabled(has);
    ui_->duplicateButton->setEnabled(has);
    ui_->deleteButton->setEnabled(has);
    ui_->nameEdit->setText(has ? QString::fromStdString(material->name()) : QString());
    ui_->descriptionEdit->setText(has ? QString::fromStdString(material->description()) : QString());
    ui_->idValue->setText(has ? QString::fromStdString(material->id().str()) : QStringLiteral("-"));
    updating_ = false;
    showError({});
    fillPropertyTable();
}

void MaterialsDialog::fillPropertyTable()
{
    if (updating_)
        return;
    const Material* material = current();
    updating_ = true;
    ui_->propertyTable->setRowCount(0);
    const bool applies = material && material->appliesTo(domain());
    ui_->domainApplies->setEnabled(material != nullptr);
    ui_->domainApplies->setChecked(applies);
    ui_->propertyTable->setEnabled(applies);
    if (applies) {
        const auto& defs = propertyDefinitions(domain());
        ui_->propertyTable->setRowCount(static_cast<int>(defs.size()));
        for (int row = 0; row < static_cast<int>(defs.size()); ++row) {
            const PropertyDef& def = defs[static_cast<size_t>(row)];
            const PropertyValue* value = material->property(domain(), def.key);

            const auto fixed = [](const QString& text) {
                auto* item = new QTableWidgetItem(text);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                return item;
            };
            QTableWidgetItem* label = fixed(QString::fromStdString(def.label));
            label->setData(Qt::UserRole, QString::fromStdString(def.key));
            ui_->propertyTable->setItem(row, Property, label);
            ui_->propertyTable->setItem(row, Value, new QTableWidgetItem(value ? numberText(value->value) : QString()));
            ui_->propertyTable->setItem(row, Unit, fixed(QString::fromStdString(def.unit)));
            ui_->propertyTable->setItem(
                row, ValidFrom,
                new QTableWidgetItem(value && value->validity.minFrequencyHz ? numberText(*value->validity.minFrequencyHz) : QString()));
            ui_->propertyTable->setItem(
                row, ValidTo,
                new QTableWidgetItem(value && value->validity.maxFrequencyHz ? numberText(*value->validity.maxFrequencyHz) : QString()));
            auto* source = new QTableWidgetItem(value ? QString::fromStdString(value->source) : QString());
            source->setToolTip(source->text());
            ui_->propertyTable->setItem(row, Source, source);

            auto* combo = new QComboBox(ui_->propertyTable);
            for (const Provenance provenance : kProvenances)
                combo->addItem(provenanceLabel(provenance), static_cast<int>(provenance));
            combo->setCurrentIndex(combo->findData(static_cast<int>(value ? value->provenance : Provenance::Measured)));
            connect(combo, &QComboBox::activated, this, [this, row] { onCellEdited(row, ProvenanceColumn); });
            ui_->propertyTable->setCellWidget(row, ProvenanceColumn, combo);
        }
    }
    updating_ = false;
}

void MaterialsDialog::onCellEdited(int row, int column)
{
    Material* material = current();
    if (updating_ || !material || column == Property || column == Unit)
        return;
    const std::string key = ui_->propertyTable->item(row, Property)->data(Qt::UserRole).toString().toStdString();
    const QString valueText = ui_->propertyTable->item(row, Value)->text().trimmed();

    if (valueText.isEmpty()) {
        material->removeProperty(domain(), key);
        showError({});
        fillPropertyTable();
        return;
    }
    PropertyValue value;
    if (const PropertyValue* existing = material->property(domain(), key))
        value = *existing;
    bool ok = false;
    value.value = valueText.toDouble(&ok);
    const auto* combo = qobject_cast<QComboBox*>(ui_->propertyTable->cellWidget(row, ProvenanceColumn));
    value.provenance = static_cast<Provenance>(combo->currentData().toInt());
    value.source = ui_->propertyTable->item(row, Source)->text().toStdString();
    ok = ok && parseOptional(ui_->propertyTable->item(row, ValidFrom)->text(), value.validity.minFrequencyHz)
         && parseOptional(ui_->propertyTable->item(row, ValidTo)->text(), value.validity.maxFrequencyHz);
    if (!ok) {
        showError(QStringLiteral("Enter a number (for example 1500 or 2.1e9), or leave the cell empty."));
        fillPropertyTable();
        return;
    }
    if (Status status = material->setProperty(domain(), key, value); !status)
        showError(QString::fromStdString(status.message()));
    else
        showError({});
    fillPropertyTable();
}

bool MaterialsDialog::setPropertyText(const QString& key, const QString& valueText)
{
    for (int row = 0; row < ui_->propertyTable->rowCount(); ++row) {
        if (ui_->propertyTable->item(row, Property)->data(Qt::UserRole).toString() == key) {
            ui_->propertyTable->item(row, Value)->setText(valueText);  // triggers onCellEdited
            return errorText().isEmpty();
        }
    }
    showError(QStringLiteral("This domain has no property '%1'.").arg(key));
    return false;
}
