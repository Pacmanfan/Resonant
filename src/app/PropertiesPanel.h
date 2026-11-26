// The properties of the selected object: name, type, transform, geometry
// parameters, material and visibility. Lengths are shown in the display
// unit and rotations in degrees; values are converted to SI at this
// boundary through DisplayUnits.
#pragma once

#include "foundation/Id.h"
#include "scene/Scene.h"

#include <QWidget>

#include <functional>
#include <memory>

class DisplayUnits;
class DocumentController;
class QDoubleSpinBox;
class QVBoxLayout;
namespace Ui {
class PropertiesPanel;
}

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    // Builds the editor of a domain component for an object, or returns
    // nullptr when the object has none. Installed by domain user interfaces.
    using ComponentEditorFactory =
        std::function<QWidget*(const resonant::SceneObject& object, DocumentController& controller,
                               DisplayUnits& units, QWidget* parent)>;

    explicit PropertiesPanel(QWidget* parent = nullptr);
    ~PropertiesPanel() override;

    void setController(DocumentController* controller, DisplayUnits* units);
    void addComponentEditorFactory(ComponentEditorFactory factory) { factories_.push_back(std::move(factory)); }

    resonant::Id shownObject() const { return shown_; }

signals:
    void operationFailed(const QString& message);

private:
    void showSelection();
    void refreshValues();
    void rebuildGeometryEditors(const resonant::SceneObject& object);
    void rebuildComponentEditors(const resonant::SceneObject& object);
    void refreshMaterialChoices();
    void connectTransformSpin(QDoubleSpinBox* spin, int group, int axis);
    void report(const resonant::Status& status);

    std::unique_ptr<Ui::PropertiesPanel> ui_;
    DocumentController* controller_ = nullptr;
    DisplayUnits* units_ = nullptr;
    resonant::Id shown_;
    std::string shownGeometryKind_;
    std::string shownComponentType_;
    std::vector<std::pair<std::string, QDoubleSpinBox*>> geometrySpins_;
    std::vector<ComponentEditorFactory> factories_;
    bool updating_ = false;
};
