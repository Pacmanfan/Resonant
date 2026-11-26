// The application side of the acoustic module: creates acoustic objects,
// runs the engine's calculations on the document, keeps the results of the
// session and decides what the viewport shows. Panels call this class; it
// calls the core. No physics is implemented here.
#pragma once

#include "acoustics/FieldGrid.h"
#include "acoustics/Focusing.h"
#include "acoustics/InverseDesign.h"
#include "acoustics/Settings.h"
#include "manipulators/ReflectorModel.h"
#include "render/FieldLayer.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <optional>
#include <vector>

class DocumentController;

class AcousticController : public QObject {
    Q_OBJECT
public:
    explicit AcousticController(DocumentController* document, QObject* parent = nullptr);

    DocumentController& document() { return *document_; }

    // --- project-level settings (undoable) -----------------------------------
    resonant::acoustics::AcousticSettings settings() const;
    resonant::Status setSettings(const resonant::acoustics::AcousticSettings& settings);

    // --- objects (undoable) -----------------------------------------------------
    resonant::Result<resonant::Id> addEmitter();
    resonant::Result<resonant::Id> addArray(resonant::acoustics::ArrayLayout layout);
    resonant::Result<resonant::Id> addTarget();
    resonant::Result<resonant::Id> addProbe();
    resonant::Result<resonant::Id> addRegion(resonant::acoustics::RegionKind kind);
    // Replaces an object's component (emitter, array, target or region settings).
    resonant::Status setComponent(const resonant::Id& id, resonant::ComponentPtr component, const QString& what);

    // Ids and names of the scene's objects of one acoustic type.
    std::vector<std::pair<resonant::Id, QString>> objectsOfType(const char* type) const;

    // --- focusing ----------------------------------------------------------------
    // The focus settings for an array and a target point, without applying them.
    resonant::Result<resonant::acoustics::FocusSolution> computeFocus(const resonant::Id& arrayId,
                                                                      const resonant::Vec3& target,
                                                                      resonant::acoustics::FocusControl control,
                                                                      resonant::acoustics::Taper taper) const;
    resonant::Status applyFocus(const resonant::Id& arrayId, const resonant::acoustics::FocusSolution& solution,
                                resonant::acoustics::Taper taper);

    // --- inverse design -----------------------------------------------------------
    struct InverseRun {
        resonant::acoustics::InverseSolution solution;
        std::vector<resonant::acoustics::PointSource> sources;
        resonant::acoustics::TargetCollection targets;
    };
    // Solves for the strengths of one array's elements from the scene's point targets.
    InverseRun solveInverse(const resonant::Id& arrayId, const resonant::acoustics::InverseOptions& options) const;
    resonant::Status applyInverse(const resonant::Id& arrayId, const InverseRun& run);

    // --- field simulation -----------------------------------------------------------
    resonant::Result<resonant::acoustics::FieldEstimate> estimate(const resonant::Id& regionId) const;
    // Starts a background job; the result arrives through resultsChanged().
    resonant::Result<resonant::Id> runField(const resonant::Id& regionId);

    const std::vector<std::shared_ptr<const resonant::acoustics::FieldResult>>& results() const { return results_; }
    std::shared_ptr<const resonant::acoustics::FieldResult> displayedResult() const { return displayed_; }
    void displayResult(const resonant::Id& resultId);  // a null id hides the field
    void removeResult(const resonant::Id& resultId);

    // An evaluator for the scene as it is now, with the selected field model
    // (free field, or free field plus the scene's reflectors).
    std::shared_ptr<const resonant::acoustics::FieldEvaluator> currentEvaluator() const;
    // The selected model as a per-source propagation model.
    std::shared_ptr<const resonant::acoustics::PropagationModel> currentModel() const;
    bool reflectorModelSelected() const;
    // What the selected model leaves out of this scene (manipulators without
    // a field model, hidden reflectors, missing materials), in sentences.
    QStringList modelNotes() const;
    // Peak, width and displacement of a result; the intended point is the
    // first target point of the scene, else a reflector's intended focus.
    resonant::manipulators::FocusMetrics metricsFor(const resonant::acoustics::FieldResult& result) const;
    std::optional<resonant::Vec3> intendedPoint() const;
    std::vector<resonant::acoustics::PointReport> pointReports(bool includeTargets) const;
    resonant::Status exportPointReports(const QString& path) const;  // .csv or .json by extension

    // --- display ---------------------------------------------------------------------
    enum class Normalization {
        Absolute,   // the colour scale top is `absoluteScale`, in pascal
        Normalized, // the colour scale top is the displayed result's own maximum
    };
    const resonant::render::FieldDisplay& display() const { return display_; }
    Normalization normalization() const { return normalization_; }
    double absoluteScale() const { return absoluteScale_; }
    void setQuantity(resonant::render::FieldQuantity quantity);
    void setNormalization(Normalization normalization, double absoluteScalePa);
    void setSlices(double u, double v, double w, bool showU, bool showV, bool showW);
    void setFieldVisible(bool visible);
    // Animation of the instantaneous pressure: cycles per second of playback
    // (not the acoustic frequency, which is far too fast to watch).
    bool isAnimating() const { return timer_.isActive(); }
    void setAnimating(bool animating);
    void setAnimationRate(double cyclesPerSecond);
    double animationRate() const { return animationRate_; }
    // "Pa (peak), absolute, scale 12.3 Pa" - what the colours mean right now.
    QString displayDescription() const;
    // The field handed to the renderer, or an empty grid.
    resonant::render::ComplexFieldGrid displayGrid() const;

signals:
    void settingsChanged();
    void resultsChanged();
    void displayChanged();   // what is drawn changed: repaint
    void message(const QString& text);

private:
    void updateScale();
    void onJobChanged(const resonant::JobInfo& info);

    DocumentController* document_;
    std::vector<std::shared_ptr<const resonant::acoustics::FieldResult>> results_;
    std::shared_ptr<const resonant::acoustics::FieldResult> displayed_;
    std::vector<std::pair<resonant::Id, std::shared_ptr<resonant::acoustics::FieldJob>>> pending_;
    resonant::render::FieldDisplay display_;
    Normalization normalization_ = Normalization::Normalized;
    double absoluteScale_ = 1.0;
    QTimer timer_;
    double animationRate_ = 0.5;
};
