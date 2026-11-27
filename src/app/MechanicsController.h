#pragma once

// The mechanical domain in the application: structures, actuators, sensors
// and targets, the analyses, their results and how a result is shown.
//
// Deformation in the viewport is always magnified by a visual scale that is
// reported separately from the physical displacement.
#include "mechanics/Bloch.h"
#include "mechanics/Measurement.h"
#include "mechanics/Study.h"
#include "mechanics/Targeting.h"
#include "render/Layers.h"
#include "render/ScalarMeshLayer.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <vector>

class AcousticController;
class DocumentController;

class MechanicsController : public QObject {
    Q_OBJECT
public:
    MechanicsController(DocumentController* document, AcousticController* acoustics, QObject* parent = nullptr);

    resonant::mechanics::MechanicsSettings settings() const;
    resonant::Status setSettings(const resonant::mechanics::MechanicsSettings& settings);

    // Gives an object with geometry a structural idealization (undoable).
    resonant::Status makeStructural(const resonant::Id& object, resonant::mechanics::Idealization idealization);
    resonant::Status setComponent(const resonant::Id& object, resonant::ComponentPtr component, const QString& what);
    // New objects, placed on the given structure (or at the origin).
    resonant::Result<resonant::Id> addActuator(const resonant::Id& structure = {});
    resonant::Result<resonant::Id> addSensor(const resonant::Id& structure = {});
    resonant::Result<resonant::Id> addTarget(const resonant::Id& structure = {});
    resonant::Result<resonant::Id> addRod(const char* kind);  // "rod.horn" or "rod.periodic", made a beam of steel

    std::vector<std::pair<resonant::Id, QString>> structures() const;
    resonant::mechanics::StructureStudy study(const resonant::Id& structure) const;

    // Starts an analysis as a background job; false with the reason.
    resonant::Result<resonant::Id> run(const resonant::Id& structure, resonant::mechanics::Analysis analysis);
    bool isRunning() const { return pending_ != nullptr; }
    void cancel();
    const std::vector<std::shared_ptr<const resonant::mechanics::StructuralResult>>& results() const { return results_; }
    void removeResult(int index);

    // --- display ---
    enum class Quantity { DisplacementMagnitude, NormalComponent, X, Y, Z, VelocityMagnitude, AccelerationMagnitude, VonMises };
    // Whether a quantity exists for a result (stress only where calculated, velocity only for harmonic results).
    static bool available(const resonant::mechanics::StructuralResult& result, Quantity quantity);
    static QString quantityName(Quantity quantity);
    // Shows item `item` of result `index`: a mode, a frequency, a recorded instant; -1 hides.
    void display(int index, int item = 0);
    int displayedIndex() const { return displayed_; }
    int displayedItem() const { return item_; }
    int itemCount(int index) const;
    QString itemLabel(int index, int item) const;
    void setQuantity(Quantity quantity);
    Quantity quantity() const { return quantity_; }
    void setScale(bool automatic, double factor);
    bool automaticScale() const { return automaticScale_; }
    double scaleFactor() const { return scale_; }  // the magnification in use
    void setAnimating(bool animating);
    bool isAnimating() const { return timer_.isActive(); }
    void setPhase(double radians);
    QString displayDescription() const;
    std::shared_ptr<const resonant::render::ScalarMesh> displayMesh() const { return mesh_; }
    std::vector<resonant::render::LineItem> displayLines() const { return lines_; }
    resonant::Id displayedStructure() const;

    // --- targeting, band structure, radiation, measurements ---
    resonant::Result<resonant::mechanics::TargetingSolution> solveTargets(const resonant::Id& structure) const;
    resonant::Status applyTargets(const resonant::Id& structure, const resonant::mechanics::TargetingSolution& solution);
    resonant::Result<resonant::mechanics::BandStructure> bandStructure(const resonant::Id& structure, int bands = 4) const;
    // Sound radiated by a plate's harmonic result at one of its frequencies,
    // onto an acoustic field region; added to the acoustic results.
    resonant::Status radiate(int resultIndex, int frequencyIndex, const resonant::Id& region);
    resonant::Status importMeasurement(const QString& path);
    std::vector<resonant::mechanics::MeasuredResponse> measurements() const;

signals:
    void settingsChanged();
    void resultsChanged();
    void displayChanged();
    void runningChanged();
    void message(const QString& text);

private:
    void onJobChanged(const resonant::JobInfo& info);
    void rebuildMesh();
    resonant::Result<resonant::Id> addMarker(const char* type, const QString& name, resonant::ComponentPtr component, const resonant::Id& structure);

    DocumentController* document_;
    AcousticController* acoustics_;
    std::vector<std::shared_ptr<const resonant::mechanics::StructuralResult>> results_;
    std::shared_ptr<resonant::mechanics::StructuralJob> pending_;
    resonant::Id pendingId_;
    int displayed_ = -1, item_ = 0;
    Quantity quantity_ = Quantity::DisplacementMagnitude;
    bool automaticScale_ = true;
    double manualScale_ = 1.0, scale_ = 1.0, phase_ = 0.0;
    QTimer timer_;
    std::shared_ptr<const resonant::render::ScalarMesh> mesh_;
    std::vector<resonant::render::LineItem> lines_;
};
