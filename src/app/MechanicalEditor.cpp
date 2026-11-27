#include "MechanicalEditor.h"

#include "DisplayUnits.h"
#include "MechanicsController.h"
#include "ui_MechanicalEditor.h"

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr Idealization kIdealizations[] = {Idealization::Beam, Idealization::Plate, Idealization::Membrane, Idealization::Solid};
constexpr dsp::WaveformKind kWaveforms[] = {dsp::WaveformKind::Continuous, dsp::WaveformKind::ToneBurst, dsp::WaveformKind::GaussianPulse,
                                            dsp::WaveformKind::Ricker, dsp::WaveformKind::Chirp};
constexpr double kDegree = 3.14159265358979323846 / 180.0;

template <typename E, size_t N>
int indexOf(const E (&values)[N], E value)
{
    for (size_t i = 0; i < N; ++i)
        if (values[i] == value)
            return static_cast<int>(i);
    return 0;
}

}  // namespace

MechanicalEditor::MechanicalEditor(Id id, AcousticController& acoustics, MechanicsController& mechanics, DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), acoustics, units, parent), ui_(std::make_unique<Ui::MechanicalEditor>()), mechanics_(mechanics)
{
    ui_->setupUi(this);
    const auto edited = [this] { commit(); };
    for (QComboBox* combo : {ui_->idealizationCombo, ui_->axisCombo, ui_->support1Combo, ui_->support2Combo, ui_->support3Combo, ui_->support4Combo,
                             ui_->support5Combo, ui_->support6Combo, ui_->excitationCombo, ui_->waveformCombo, ui_->actuatorStructureCombo,
                             ui_->quantityCombo, ui_->sensorStructureCombo, ui_->targetStructureCombo})
        connect(combo, &QComboBox::activated, this, edited);
    for (QDoubleSpinBox* spin : {ui_->elementSizeSpin, ui_->thicknessSpin, ui_->tensionSpin, ui_->amplitudeSpin, ui_->phaseSpin, ui_->delaySpin,
                                 ui_->radiusSpin, ui_->signalFrequencySpin, ui_->cyclesSpin, ui_->sensitivitySpin, ui_->targetAmplitudeSpin,
                                 ui_->targetPhaseSpin, ui_->weightSpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, edited);
    for (QCheckBox* box : {ui_->activeCheck, ui_->physicalCheck, ui_->suppressCheck})
        connect(box, &QCheckBox::clicked, this, edited);
    for (QLineEdit* line : {ui_->profileEdit, ui_->sensitivityUnitEdit, ui_->sensorModelEdit})
        connect(line, &QLineEdit::editingFinished, this, edited);
    connect(&mechanics_, &MechanicsController::settingsChanged, this, [this] { refresh(); });
    refresh();
}

MechanicalEditor::~MechanicalEditor() = default;

QString MechanicalEditor::studyText() const
{
    return ui_->studyText->toPlainText();
}

void MechanicalEditor::fillStructures(QComboBox* combo, const Id& selected)
{
    combo->clear();
    combo->addItem(QStringLiteral("The nearest structure"), QString());
    for (const auto& [id, name] : mechanics_.structures())
        combo->addItem(name, QString::fromStdString(id.str()));
    const int index = combo->findData(QString::fromStdString(selected.str()));
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void MechanicalEditor::refresh()
{
    const SceneObject* o = object();
    if (!o)
        return;
    updating_ = true;
    const Component* c = o->state.component.get();
    const QString length = units_.lengthSymbol();
    if (const auto* s = dynamic_cast<const StructureComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->structurePage);
        ui_->group->setTitle(QStringLiteral("Structure"));
        const StructureSettings& st = s->settings;
        ui_->idealizationCombo->setCurrentIndex(indexOf(kIdealizations, st.idealization));
        ui_->axisCombo->setCurrentIndex(st.axis + 1);
        ui_->elementSizeSpin->setSuffix(QLatin1Char(' ') + length);
        ui_->thicknessSpin->setSuffix(QLatin1Char(' ') + length);
        showLength(ui_->elementSizeSpin, st.elementSize);
        showLength(ui_->thicknessSpin, st.thickness);
        ui_->tensionSpin->setValue(st.tension);
        ui_->tensionSpin->setEnabled(st.idealization == Idealization::Membrane);
        ui_->thicknessSpin->setEnabled(st.idealization == Idealization::Plate || st.idealization == Idealization::Membrane);
        QComboBox* supports[6] = {ui_->support1Combo, ui_->support2Combo, ui_->support3Combo, ui_->support4Combo, ui_->support5Combo, ui_->support6Combo};
        QLabel* labels[6] = {ui_->support1Label, ui_->support2Label, ui_->support3Label, ui_->support4Label, ui_->support5Label, ui_->support6Label};
        QStringList names;
        switch (st.idealization) {
        case Idealization::Beam: names = {QStringLiteral("Start of the axis"), QStringLiteral("End of the axis")}; break;
        case Idealization::Plate:
            names = {QStringLiteral("Edge at -axis 1"), QStringLiteral("Edge at +axis 1"), QStringLiteral("Edge at -axis 2"), QStringLiteral("Edge at +axis 2")};
            break;
        case Idealization::Membrane: names = {}; break;
        case Idealization::Solid:
            names = {QStringLiteral("Face -X"), QStringLiteral("Face +X"), QStringLiteral("Face -Y"), QStringLiteral("Face +Y"), QStringLiteral("Face -Z"),
                     QStringLiteral("Face +Z")};
            break;
        }
        for (int i = 0; i < 6; ++i) {
            const bool used = i < names.size();
            supports[i]->setVisible(used);
            labels[i]->setVisible(used);
            if (used) {
                labels[i]->setText(names[i]);
                supports[i]->setCurrentIndex(static_cast<int>(st.supports[static_cast<size_t>(i)]));
            }
        }
        // How the object will be analysed, with the reasons.
        const StructureStudy study = mechanics_.study(o->id());
        QStringList lines;
        if (!study.allowed)
            lines << QStringLiteral("CANNOT BE ANALYSED: ") + QString::fromStdString(study.refusal);
        else
            lines << QString::fromStdString(study.summary());
        for (const auto& [name, value] : study.properties)
            lines << QString::fromStdString(name + ": " + value);
        for (const std::string& n : study.notes)
            lines << QString::fromStdString(n);
        for (const std::string& w : study.warnings)
            lines << QStringLiteral("WARNING: ") + QString::fromStdString(w);
        if (st.idealization == Idealization::Membrane)
            lines << QStringLiteral("A membrane is held at every edge; supports do not apply.");
        ui_->studyText->setPlainText(lines.join(QLatin1Char('\n')));
    } else if (const auto* a = dynamic_cast<const ActuatorComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->actuatorPage);
        ui_->group->setTitle(QStringLiteral("Actuator"));
        const ActuatorSettings& as = a->settings;
        ui_->excitationCombo->setCurrentIndex(as.excitation == Excitation::PointForce ? 0 : 1);
        ui_->amplitudeSpin->setValue(as.amplitude);
        ui_->phaseSpin->setValue(as.phase / kDegree);
        ui_->delaySpin->setValue(as.delay * 1e3);
        ui_->radiusSpin->setSuffix(QLatin1Char(' ') + length);
        showLength(ui_->radiusSpin, as.radius);
        ui_->radiusSpin->setEnabled(as.excitation == Excitation::DistributedForce);
        ui_->waveformCombo->setCurrentIndex(indexOf(kWaveforms, as.waveform.kind));
        ui_->signalFrequencySpin->setValue(as.waveform.frequencyHz);
        ui_->cyclesSpin->setValue(as.waveform.cycles);
        ui_->cyclesSpin->setEnabled(as.waveform.kind == dsp::WaveformKind::ToneBurst);
        ui_->activeCheck->setChecked(as.active);
        fillStructures(ui_->actuatorStructureCombo, as.structure);
        ui_->physicalCheck->setChecked(as.physicalDevice);
        ui_->profileEdit->setText(QString::fromStdString(as.deviceProfile));
        ui_->profileEdit->setEnabled(as.physicalDevice);
    } else if (const auto* se = dynamic_cast<const SensorComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->sensorPage);
        ui_->group->setTitle(QStringLiteral("Sensor"));
        ui_->quantityCombo->setCurrentIndex(static_cast<int>(se->settings.quantity));
        fillStructures(ui_->sensorStructureCombo, se->settings.structure);
        ui_->sensitivitySpin->setValue(se->settings.sensitivity);
        ui_->sensitivityUnitEdit->setText(QString::fromStdString(se->settings.sensitivityUnit));
        ui_->sensorModelEdit->setText(QString::fromStdString(se->settings.sensorModel));
    } else if (const auto* t = dynamic_cast<const VibrationTargetComponent*>(c)) {
        ui_->pages->setCurrentWidget(ui_->targetPage);
        ui_->group->setTitle(QStringLiteral("Vibration target"));
        ui_->targetAmplitudeSpin->setValue(t->settings.amplitude * 1e6);
        ui_->targetPhaseSpin->setValue(t->settings.phase / kDegree);
        ui_->weightSpin->setValue(t->settings.weight);
        ui_->suppressCheck->setChecked(t->settings.suppress);
        ui_->targetAmplitudeSpin->setEnabled(!t->settings.suppress);
        fillStructures(ui_->targetStructureCombo, t->settings.structure);
    }
    updating_ = false;
}

void MechanicalEditor::commit()
{
    if (updating_)
        return;
    const SceneObject* o = object();
    if (!o)
        return;
    const Component* c = o->state.component.get();
    const auto idOf = [](const QComboBox* combo) {
        const QString text = combo->currentData().toString();
        return text.isEmpty() ? Id() : Id(text.toStdString());
    };
    Status status = Status::success();
    if (const auto* s = dynamic_cast<const StructureComponent*>(c)) {
        StructureSettings st = s->settings;
        st.idealization = kIdealizations[ui_->idealizationCombo->currentIndex()];
        st.axis = ui_->axisCombo->currentIndex() - 1;
        takeLength(st.elementSize, ui_->elementSizeSpin);
        takeLength(st.thickness, ui_->thicknessSpin);
        st.tension = ui_->tensionSpin->value();
        QComboBox* supports[6] = {ui_->support1Combo, ui_->support2Combo, ui_->support3Combo, ui_->support4Combo, ui_->support5Combo, ui_->support6Combo};
        for (int i = 0; i < 6; ++i)
            if (supports[i]->isVisible() || st.idealization != s->settings.idealization)
                st.supports[static_cast<size_t>(i)] = static_cast<Support>(supports[i]->currentIndex());
        status = mechanics_.setComponent(o->id(), std::make_shared<StructureComponent>(st), QStringLiteral("Edit the structure of"));
    } else if (const auto* a = dynamic_cast<const ActuatorComponent*>(c)) {
        ActuatorSettings as = a->settings;
        as.excitation = ui_->excitationCombo->currentIndex() == 0 ? Excitation::PointForce : Excitation::DistributedForce;
        take(as.amplitude, ui_->amplitudeSpin, 1.0);
        take(as.phase, ui_->phaseSpin, 1.0 / kDegree);
        take(as.delay, ui_->delaySpin, 1e3);
        takeLength(as.radius, ui_->radiusSpin);
        as.waveform.kind = kWaveforms[ui_->waveformCombo->currentIndex()];
        take(as.waveform.frequencyHz, ui_->signalFrequencySpin, 1.0);
        take(as.waveform.cycles, ui_->cyclesSpin, 1.0);
        as.active = ui_->activeCheck->isChecked();
        as.structure = idOf(ui_->actuatorStructureCombo);
        as.physicalDevice = ui_->physicalCheck->isChecked();
        as.deviceProfile = ui_->profileEdit->text().toStdString();
        if (as.waveform.kind == dsp::WaveformKind::Chirp && as.waveform.endFrequencyHz <= as.waveform.frequencyHz)
            as.waveform.endFrequencyHz = 2.0 * as.waveform.frequencyHz;
        status = as.validate();
        if (status)
            status = mechanics_.setComponent(o->id(), std::make_shared<ActuatorComponent>(as), QStringLiteral("Edit actuator"));
    } else if (const auto* se = dynamic_cast<const SensorComponent*>(c)) {
        SensorSettings ss = se->settings;
        ss.quantity = static_cast<mechanics::Quantity>(ui_->quantityCombo->currentIndex());
        ss.structure = idOf(ui_->sensorStructureCombo);
        take(ss.sensitivity, ui_->sensitivitySpin, 1.0);
        ss.sensitivityUnit = ui_->sensitivityUnitEdit->text().toStdString();
        ss.sensorModel = ui_->sensorModelEdit->text().toStdString();
        status = mechanics_.setComponent(o->id(), std::make_shared<SensorComponent>(ss), QStringLiteral("Edit sensor"));
    } else if (const auto* t = dynamic_cast<const VibrationTargetComponent*>(c)) {
        VibrationTargetSettings ts = t->settings;
        take(ts.amplitude, ui_->targetAmplitudeSpin, 1e6);
        take(ts.phase, ui_->targetPhaseSpin, 1.0 / kDegree);
        take(ts.weight, ui_->weightSpin, 1.0);
        ts.suppress = ui_->suppressCheck->isChecked();
        ts.structure = idOf(ui_->targetStructureCombo);
        status = mechanics_.setComponent(o->id(), std::make_shared<VibrationTargetComponent>(ts), QStringLiteral("Edit vibration target"));
    }
    report(status);
    refresh();
}

PropertiesPanel::ComponentEditorFactory makeMechanicalEditorFactory(AcousticController& acoustics, MechanicsController& mechanics)
{
    return [&acoustics, &mechanics](const SceneObject& object, DocumentController&, DisplayUnits& units, QWidget* parent) -> QWidget* {
        const Component* c = object.state.component.get();
        if (dynamic_cast<const StructureComponent*>(c) || dynamic_cast<const ActuatorComponent*>(c) || dynamic_cast<const SensorComponent*>(c)
            || dynamic_cast<const VibrationTargetComponent*>(c))
            return new MechanicalEditor(object.id(), acoustics, mechanics, units, parent);
        return nullptr;
    };
}
