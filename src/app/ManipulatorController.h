// The application side of wave manipulators: creating them, describing what
// is and is not calculated for them, analytical estimates as text, the
// geometry optimization for reflectors, and how they are drawn (focal
// markers, optional ray paths).
#pragma once

#include "SceneRenderBridge.h"
#include "manipulators/GeometryOptimization.h"
#include "manipulators/Manipulator.h"

#include <QObject>
#include <QString>

class AcousticController;
class DisplayUnits;
class DocumentController;

class ManipulatorController : public QObject {
    Q_OBJECT
public:
    ManipulatorController(DocumentController* document, AcousticController* acoustics, QObject* parent = nullptr);

    resonant::Result<resonant::Id> addManipulator(const QString& geometryKind);
    resonant::Status setSettings(const resonant::Id& id, const resonant::manipulators::ManipulatorSettings& settings);

    // Characteristic points, analytical estimates and their assumptions for
    // a manipulator, as plain text. Uses the current medium and the
    // object's assigned material.
    QString estimatesText(const resonant::SceneObject& object, const DisplayUnits& units) const;

    // Varies one geometry parameter of a reflector to maximize |P| at its
    // intended focus, with the reflector field model and the scene's
    // current sources. Changes nothing; returns the outcome.
    resonant::Result<resonant::manipulators::OptimizationOutcome> optimizeReflector(const resonant::Id& id,
                                                                                     const std::string& parameter,
                                                                                     double lower, double upper) const;
    // Replaces the reflector's geometry by an optimized one (undoable).
    resonant::Status applyGeometry(const resonant::Id& id, const resonant::Geometry& geometry);

    bool showRays() const { return showRays_; }
    void setShowRays(bool show);

    // Draws focal points, intended focus / source markers and ray paths.
    SceneRenderBridge::Decorator makeDecorator();

signals:
    void displayChanged();

private:
    DocumentController* document_;
    AcousticController* acoustics_;
    bool showRays_ = false;
};
