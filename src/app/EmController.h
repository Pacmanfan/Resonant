#pragma once

// The electromagnetic domain in the application: antennas, sources, arrays
// and guides; their analytical reports, patterns and sweeps; array steering,
// tapering and beam synthesis; field evaluation on regions (analytical or
// with the Yee full-wave solver); measured one-port data; and how a field or
// a pattern is shown.
//
// Every number shown comes from a calculation in src/core/electromagnetics,
// labelled with the model it comes from (analytical, array factor, full-wave
// numerical, design estimate or imported measurement).
#include "electromagnetics/EmFullWave.h"
#include "electromagnetics/Touchstone.h"
#include "render/FieldLayer.h"
#include "render/Layers.h"
#include "render/ScalarMeshLayer.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <optional>
#include <vector>

class DocumentController;

class EmController : public QObject {
    Q_OBJECT
public:
    explicit EmController(DocumentController* document, QObject* parent = nullptr);

    resonant::em::EmSettings settings() const;
    resonant::Status setSettings(const resonant::em::EmSettings& settings);
    resonant::em::EmFullWaveSettings fullWaveSettings() const;
    resonant::Status setFullWaveSettings(const resonant::em::EmFullWaveSettings& settings);

    // --- objects ---
    // A new antenna of a geometry kind ("antenna.dipole", ...), sized for 1 GHz
    // where a size follows from the frequency; wire antennas get copper.
    resonant::Result<resonant::Id> addAntenna(const QString& kind);
    resonant::Result<resonant::Id> addSource(resonant::em::SourceKind kind);
    resonant::Result<resonant::Id> addArray(resonant::em::EmArrayLayout layout);
    resonant::Result<resonant::Id> addGuide(const QString& kind);  // "guide.rectangular", ..., "cavity.rectangular.em"
    resonant::Status setComponent(const resonant::Id& id, resonant::ComponentPtr component, const QString& what);

    enum class Kind { None, Antenna, Source, Array, Guide };
    Kind kindOf(const resonant::Id& id) const;
    std::vector<std::pair<resonant::Id, QString>> objects() const;  // every electromagnetic object
    std::vector<std::pair<resonant::Id, QString>> regions() const;  // field regions (shared with acoustics)

    // --- analysis ---
    resonant::em::AntennaReport antennaReport(const resonant::Id& id) const;
    double referenceImpedance(const resonant::Id& antennaId) const;  // the antenna's own, 50 ohm otherwise
    resonant::em::GuideReport guideReport(const resonant::Id& id) const;
    std::vector<std::pair<double, resonant::em::AntennaImpedance>> sweep(const resonant::Id& id) const;
    resonant::Result<resonant::em::PatternGrid> pattern(const resonant::Id& id) const;
    // Array-factor analysis of an array object: main lobe, beamwidths,
    // sidelobes, and grating-lobe warnings for regular lattices.
    struct ArrayAnalysis {
        resonant::em::PatternAnalysis pattern;
        std::vector<resonant::Vec3> predictedGratingLobes;  // local directions
        QStringList warnings;
        double frequencyHz = 0.0;
    };
    resonant::Result<ArrayAnalysis> analyzeArray(const resonant::Id& id) const;
    // Writes element phases (or delays) that point the main lobe along the
    // settings' steering direction (undoable).
    resonant::Status steerArray(const resonant::Id& id);
    // Dolph-Chebyshev amplitudes for a linear array (undoable).
    resonant::Status applyChebyshev(const resonant::Id& id);
    // Least-squares weights for the settings' beams and nulls; applied when `apply`.
    resonant::Result<resonant::em::BeamSynthesis> synthesize(const resonant::Id& id, bool apply);

    // --- fields ---
    // Analytical fields of all active radiators on a region.
    resonant::Status evaluateRegion(const resonant::Id& regionId);
    // The TE10 field over a rectangular guide's cross-section.
    resonant::Status showGuideField(const resonant::Id& guideId);
    // E and H at a world point (analytical radiators).
    resonant::Result<std::shared_ptr<resonant::em::EmFieldResult>> probe(const resonant::Vec3& world) const;
    resonant::em::EmPrepared planFullWave(const resonant::Id& regionId) const;
    resonant::Result<resonant::Id> runFullWave(const resonant::Id& regionId);
    bool isRunning() const { return pending_ != nullptr; }
    void cancel();
    const std::vector<std::shared_ptr<const resonant::em::EmFieldResult>>& results() const { return results_; }
    void removeResult(int index);

    // --- measured data ---
    resonant::Status importTouchstone(const QString& path);
    resonant::Status exportTouchstone(const resonant::Id& antennaId, const QString& path) const;
    std::vector<std::pair<QString, resonant::em::OnePortData>> measurements() const;

    // --- display ---
    enum class Quantity { Magnitude, Phase, Instantaneous, Decibels };  // Decibels: 40 dB below the colour-scale top
    void display(int index);  // -1 hides
    int displayedIndex() const { return displayed_; }
    void setComponentShown(resonant::em::FieldComponent component);
    resonant::em::FieldComponent componentShown() const { return component_; }
    void setQuantity(Quantity quantity);
    void setNormalized(bool normalized, double absoluteScale);
    void setAnimating(bool animating);
    bool isAnimating() const { return timer_.isActive(); }
    QString displayDescription() const;
    resonant::render::ComplexFieldGrid displayGrid() const;
    const resonant::render::FieldDisplay& fieldDisplay() const { return fieldDisplay_; }
    // A 3D pattern surface around an antenna or array (radius ~ |E| / max, linear).
    resonant::Status showPattern3D(const resonant::Id& id);
    void hidePattern3D();
    std::shared_ptr<const resonant::render::ScalarMesh> patternMesh() const { return patternMesh_; }
    std::vector<resonant::render::LineItem> patternLines() const { return patternLines_; }

signals:
    void settingsChanged();
    void resultsChanged();
    void displayChanged();         // the field shown changed
    void patternChanged();         // the 3D pattern shown changed
    void runningChanged();
    void measurementsChanged();
    void message(const QString& text);

private:
    void onJobChanged(const resonant::JobInfo& info);
    void addResult(std::shared_ptr<const resonant::em::EmFieldResult> result);
    void rebuildDisplay();
    resonant::Result<resonant::Id> insert(const char* type, const QString& name, resonant::ComponentPtr component,
                                          std::optional<resonant::Geometry> geometry, const resonant::Id& material = {});
    resonant::Result<resonant::em::SampleGrid> regionGrid(const resonant::Id& regionId) const;

    DocumentController* document_;
    std::vector<std::shared_ptr<const resonant::em::EmFieldResult>> results_;
    std::shared_ptr<resonant::em::EmFullWaveJob> pending_;
    resonant::Id pendingId_;
    int displayed_ = -1;
    resonant::em::FieldComponent component_ = resonant::em::FieldComponent::EMagnitude;
    Quantity quantity_ = Quantity::Decibels;  // fields span decades, near sources especially
    bool normalized_ = true;
    double absoluteScale_ = 1.0;
    std::shared_ptr<std::vector<std::complex<float>>> samples_;  // the shown component
    resonant::render::FieldDisplay fieldDisplay_;
    QTimer timer_;
    std::shared_ptr<const resonant::render::ScalarMesh> patternMesh_;
    std::vector<resonant::render::LineItem> patternLines_;
};
