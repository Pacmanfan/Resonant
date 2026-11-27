#include "EmEditor.h"

#include "DisplayUnits.h"
#include "EmController.h"
#include "electromagnetics/EmComponents.h"
#include "ui_EmEditor.h"

#include <QSignalBlocker>

#include <cmath>

using namespace resonant;
using namespace resonant::em;

namespace {

constexpr double kDeg = kPi / 180.0;
constexpr SourceKind kSources[] = {SourceKind::ElectricDipole, SourceKind::MagneticDipole, SourceKind::PlaneWave};
constexpr EmArrayLayout kLayouts[] = {EmArrayLayout::Linear, EmArrayLayout::Planar, EmArrayLayout::Circular, EmArrayLayout::Custom};
constexpr ElementPattern kPatterns[] = {ElementPattern::Isotropic, ElementPattern::ShortDipole, ElementPattern::HalfWaveDipole,
                                        ElementPattern::CosineHemisphere};

template <typename E, size_t N>
int indexOf(const E (&values)[N], E value)
{
    for (size_t i = 0; i < N; ++i)
        if (values[i] == value)
            return static_cast<int>(i);
    return 0;
}

}  // namespace

QString emTypeLabel(const SceneObject& object)
{
    const Component* c = object.state.component.get();
    if (dynamic_cast<const AntennaComponent*>(c))
        return QStringLiteral("Antenna (%1)").arg(object.state.geometry ? QString::fromStdString(object.state.geometry->kind().displayName) : QString());
    if (const auto* s = dynamic_cast<const SourceComponent*>(c))
        return QStringLiteral("EM source (%1)").arg(QString::fromUtf8(std::string(toString(s->settings.kind)).c_str()));
    if (const auto* a = dynamic_cast<const EmArrayComponent*>(c))
        return QStringLiteral("RF array (%1 elements)").arg(a->settings.elementCount());
    if (dynamic_cast<const GuideComponent*>(c))
        return QStringLiteral("Waveguide / cavity");
    return {};
}

EmEditor::EmEditor(Id id, AcousticController& acoustics, EmController& em, DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), acoustics, units, parent), ui_(std::make_unique<Ui::EmEditor>()), em_(em)
{
    ui_->setupUi(this);
    const auto edited = [this] { commit(); };
    for (QComboBox* combo : {ui_->sourceKindCombo, ui_->layoutCombo, ui_->elementPatternCombo})
        connect(combo, &QComboBox::activated, this, edited);
    for (QDoubleSpinBox* spin : {ui_->antennaFrequencySpin, ui_->currentSpin, ui_->feedPhaseSpin, ui_->antennaDelaySpin, ui_->referenceSpin,
                                 ui_->sourceFrequencySpin, ui_->sourceAmplitudeSpin, ui_->sourcePhaseSpin, ui_->sourceDelaySpin, ui_->spacingXSpin,
                                 ui_->spacingYSpin, ui_->radiusSpin, ui_->arrayFrequencySpin, ui_->arrayAmplitudeSpin, ui_->guideFrequencySpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, edited);
    for (QSpinBox* spin : {ui_->countXSpin, ui_->countYSpin})
        connect(spin, &QSpinBox::valueChanged, this, edited);
    for (QCheckBox* box : {ui_->antennaActiveCheck, ui_->sourceActiveCheck, ui_->arrayActiveCheck})
        connect(box, &QCheckBox::clicked, this, edited);
    connect(ui_->elementTable, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (!updating_)
            commitElement(row, column);
    });
    connect(ui_->clearOverridesButton, &QPushButton::clicked, this, [this] {
        const SceneObject* o = object();
        const auto* a = o ? dynamic_cast<const EmArrayComponent*>(o->state.component.get()) : nullptr;
        if (!a)
            return;
        EmArraySettings s = a->settings;
        s.overrides.clear();
        report(em_.setComponent(id_, std::make_shared<EmArrayComponent>(s), QStringLiteral("Clear element settings of")));
    });
    connect(&em_, &EmController::settingsChanged, this, [this] { refresh(); });
    refresh();
}

EmEditor::~EmEditor() = default;

QString EmEditor::antennaText() const
{
    return ui_->antennaText->toPlainText();
}

void EmEditor::refresh()
{
    const SceneObject* o = object();
    if (!o)
        return;
    updating_ = true;
    const Component* c = o->state.component.get();
    if (const auto* a = dynamic_cast<const AntennaComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->antennaPage);
        ui_->group->setTitle(QStringLiteral("Antenna"));
        const AntennaSettings& s = a->settings;
        show(ui_->antennaFrequencySpin, s.frequencyHz, 1e-6);
        show(ui_->currentSpin, s.feedCurrent, 1.0);
        show(ui_->feedPhaseSpin, s.feedPhase, 1.0 / kDeg);
        show(ui_->antennaDelaySpin, s.delay, 1e9);
        show(ui_->referenceSpin, s.referenceImpedance, 1.0);
        ui_->antennaActiveCheck->setChecked(s.active);
        const AntennaReport r = em_.antennaReport(id_);
        QString text = QString::fromStdString(r.model);
        if (r.impedance)
            text += QStringLiteral("\nZin = %1 %2 j%3 ohm").arg(r.impedance->input.real(), 0, 'f', 2).arg(r.impedance->input.imag() < 0 ? '-' : '+')
                        .arg(std::abs(r.impedance->input.imag()), 0, 'f', 2);
        if (r.matching)
            text += QStringLiteral(", VSWR %1 re %2 ohm").arg(r.matching->vswr, 0, 'f', 2).arg(s.referenceImpedance);
        if (r.directivity > 0)
            text += QStringLiteral("\nDirectivity %1 dBi, realized gain %2 dBi").arg(10 * std::log10(r.directivity), 0, 'f', 2)
                        .arg(10 * std::log10(std::max(r.realizedGain, 1e-12)), 0, 'f', 2);
        for (const std::string& e : r.estimates)
            text += QStringLiteral("\n") + QString::fromStdString(e);
        ui_->antennaText->setPlainText(text);
    } else if (const auto* src = dynamic_cast<const SourceComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->sourcePage);
        ui_->group->setTitle(QStringLiteral("Electromagnetic source"));
        const SourceSettings& s = src->settings;
        ui_->sourceKindCombo->setCurrentIndex(indexOf(kSources, s.kind));
        show(ui_->sourceFrequencySpin, s.frequencyHz, 1e-6);
        ui_->sourceAmplitudeSpin->setSuffix(s.kind == SourceKind::ElectricDipole ? QStringLiteral(" A m") : s.kind == SourceKind::MagneticDipole
                                                                                                              ? QStringLiteral(" A m^2")
                                                                                                              : QStringLiteral(" V/m (peak)"));
        show(ui_->sourceAmplitudeSpin, s.amplitude, 1.0);
        show(ui_->sourcePhaseSpin, s.phase, 1.0 / kDeg);
        show(ui_->sourceDelaySpin, s.delay, 1e9);
        ui_->sourceActiveCheck->setChecked(s.active);
    } else if (const auto* arr = dynamic_cast<const EmArrayComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->arrayPage);
        ui_->group->setTitle(QStringLiteral("RF array"));
        const EmArraySettings& s = arr->settings;
        ui_->layoutCombo->setCurrentIndex(indexOf(kLayouts, s.layout));
        ui_->countXSpin->setValue(s.countX);
        ui_->countYSpin->setValue(s.countY);
        ui_->countYSpin->setEnabled(s.layout == EmArrayLayout::Planar);
        showLength(ui_->spacingXSpin, s.spacingX);
        showLength(ui_->spacingYSpin, s.spacingY);
        showLength(ui_->radiusSpin, s.radius);
        ui_->spacingYSpin->setEnabled(s.layout == EmArrayLayout::Planar);
        ui_->radiusSpin->setEnabled(s.layout == EmArrayLayout::Circular);
        ui_->elementPatternCombo->setCurrentIndex(indexOf(kPatterns, s.pattern));
        show(ui_->arrayFrequencySpin, s.frequencyHz, 1e-6);
        show(ui_->arrayAmplitudeSpin, s.amplitude, 1.0);
        ui_->arrayActiveCheck->setChecked(s.active);
        const ArrayModel model = arrayModelOf(s);
        const QSignalBlocker block(ui_->elementTable);
        ui_->elementTable->setRowCount(static_cast<int>(model.elements.size()));
        for (int i = 0; i < static_cast<int>(model.elements.size()); ++i) {
            const ArrayElement& e = model.elements[static_cast<size_t>(i)];
            const auto fixed = [](QString text) {
                auto* item = new QTableWidgetItem(std::move(text));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                return item;
            };
            ui_->elementTable->setItem(i, 0, fixed(QString::number(i)));
            ui_->elementTable->setItem(i, 1, fixed(QStringLiteral("%1, %2, %3 %4").arg(units_.lengthFromSI(e.position.x()), 0, 'g', 4)
                                                       .arg(units_.lengthFromSI(e.position.y()), 0, 'g', 4)
                                                       .arg(units_.lengthFromSI(e.position.z()), 0, 'g', 4).arg(units_.lengthSymbol())));
            ui_->elementTable->setItem(i, 2, new QTableWidgetItem(QString::number(std::abs(e.weight), 'g', 6)));
            ui_->elementTable->setItem(i, 3, new QTableWidgetItem(QString::number(std::arg(e.weight) / kDeg, 'f', 2)));
            ui_->elementTable->setItem(i, 4, new QTableWidgetItem(QString::number(e.delay * 1e9, 'g', 6)));
            ui_->elementTable->setItem(i, 5, new QTableWidgetItem(e.active ? QStringLiteral("on") : QStringLiteral("off")));
        }
    } else if (const auto* g = dynamic_cast<const GuideComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->guidePage);
        ui_->group->setTitle(QStringLiteral("Waveguide / cavity"));
        show(ui_->guideFrequencySpin, g->settings.frequencyHz, 1e-6);
    }
    updating_ = false;
}

void EmEditor::commit()
{
    if (updating_)
        return;
    const SceneObject* o = object();
    if (!o)
        return;
    const Component* c = o->state.component.get();
    if (const auto* a = dynamic_cast<const AntennaComponent*>(c)) {
        AntennaSettings s = a->settings;
        take(s.frequencyHz, ui_->antennaFrequencySpin, 1e-6);
        take(s.feedCurrent, ui_->currentSpin, 1.0);
        take(s.feedPhase, ui_->feedPhaseSpin, 1.0 / kDeg);
        take(s.delay, ui_->antennaDelaySpin, 1e9);
        take(s.referenceImpedance, ui_->referenceSpin, 1.0);
        s.active = ui_->antennaActiveCheck->isChecked();
        if (Status status = s.validate(); !status)
            return report(status);
        if (!(s == a->settings))
            report(em_.setComponent(id_, std::make_shared<AntennaComponent>(s), QStringLiteral("Edit antenna")));
    } else if (const auto* src = dynamic_cast<const SourceComponent*>(c)) {
        SourceSettings s = src->settings;
        s.kind = kSources[std::clamp(ui_->sourceKindCombo->currentIndex(), 0, 2)];
        take(s.frequencyHz, ui_->sourceFrequencySpin, 1e-6);
        take(s.amplitude, ui_->sourceAmplitudeSpin, 1.0);
        take(s.phase, ui_->sourcePhaseSpin, 1.0 / kDeg);
        take(s.delay, ui_->sourceDelaySpin, 1e9);
        s.active = ui_->sourceActiveCheck->isChecked();
        if (Status status = s.validate(); !status)
            return report(status);
        if (!(s == src->settings))
            report(em_.setComponent(id_, std::make_shared<SourceComponent>(s), QStringLiteral("Edit source")));
    } else if (const auto* arr = dynamic_cast<const EmArrayComponent*>(c)) {
        EmArraySettings s = arr->settings;
        s.layout = kLayouts[std::clamp(ui_->layoutCombo->currentIndex(), 0, 3)];
        s.countX = ui_->countXSpin->value();
        s.countY = ui_->countYSpin->value();
        takeLength(s.spacingX, ui_->spacingXSpin);
        takeLength(s.spacingY, ui_->spacingYSpin);
        takeLength(s.radius, ui_->radiusSpin);
        s.pattern = kPatterns[std::clamp(ui_->elementPatternCombo->currentIndex(), 0, 3)];
        take(s.frequencyHz, ui_->arrayFrequencySpin, 1e-6);
        take(s.amplitude, ui_->arrayAmplitudeSpin, 1.0);
        s.active = ui_->arrayActiveCheck->isChecked();
        if (s.layout == EmArrayLayout::Custom && s.customPositions.empty())
            s.customPositions = {Vec3(-0.5 * s.spacingX, 0, 0), Vec3(0.5 * s.spacingX, 0, 0)};
        if (Status status = s.validate(); !status)
            return report(status);
        if (!(s == arr->settings))
            report(em_.setComponent(id_, std::make_shared<EmArrayComponent>(s), QStringLiteral("Edit array")));
    } else if (const auto* g = dynamic_cast<const GuideComponent*>(c)) {
        GuideSettings s = g->settings;
        take(s.frequencyHz, ui_->guideFrequencySpin, 1e-6);
        if (Status status = s.validate(); !status)
            return report(status);
        if (!(s == g->settings))
            report(em_.setComponent(id_, std::make_shared<GuideComponent>(s), QStringLiteral("Edit guide")));
    }
}

void EmEditor::commitElement(int row, int column)
{
    const SceneObject* o = object();
    const auto* arr = o ? dynamic_cast<const EmArrayComponent*>(o->state.component.get()) : nullptr;
    if (!arr || column < 2)
        return;
    const QString text = ui_->elementTable->item(row, column)->text().trimmed();
    EmArraySettings s = arr->settings;
    ElementExcitation& e = s.overrides[row];
    bool ok = true;
    switch (column) {
    case 2: e.amplitude = text.toDouble(&ok); break;
    case 3: e.phase = text.toDouble(&ok) * kDeg; break;
    case 4: e.delay = text.toDouble(&ok) * 1e-9; break;
    case 5: e.active = text.compare(QLatin1String("off"), Qt::CaseInsensitive) != 0 && text != QLatin1String("0"); break;
    default: break;
    }
    if (!ok || (column == 2 && *e.amplitude < 0.0)) {
        report(Status::failure("enter a number (amplitude not negative)"));
        refresh();
        return;
    }
    report(em_.setComponent(id_, std::make_shared<EmArrayComponent>(s), QStringLiteral("Edit element %1 of").arg(row)));
}

PropertiesPanel::ComponentEditorFactory makeEmEditorFactory(AcousticController& acoustics, EmController& em)
{
    return [&acoustics, &em](const SceneObject& object, DocumentController&, DisplayUnits& units, QWidget* parent) -> QWidget* {
        const Component* c = object.state.component.get();
        if (dynamic_cast<const AntennaComponent*>(c) || dynamic_cast<const SourceComponent*>(c) || dynamic_cast<const EmArrayComponent*>(c)
            || dynamic_cast<const GuideComponent*>(c))
            return new EmEditor(object.id(), acoustics, em, units, parent);
        return nullptr;
    };
}
