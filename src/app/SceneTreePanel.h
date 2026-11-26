// The scene hierarchy: a tree of the scene's objects with selection,
// visibility check boxes, in-place renaming and a context menu. It mirrors
// the scene; every edit goes through the DocumentController.
#pragma once

#include "foundation/Id.h"
#include "scene/Scene.h"

#include <QHash>
#include <QWidget>

#include <memory>

class DocumentController;
class QMenu;
class QTreeWidgetItem;
namespace Ui {
class SceneTreePanel;
}

class SceneTreePanel : public QWidget {
    Q_OBJECT
public:
    explicit SceneTreePanel(QWidget* parent = nullptr);
    ~SceneTreePanel() override;

    void setController(DocumentController* controller);
    // The menu shown by a right click; the main window fills it with its
    // actions so the panel and the menu bar offer the same commands.
    void setContextMenu(QMenu* menu) { contextMenu_ = menu; }

    // Starts in-place editing of the primary selection's name.
    void editSelectedName();

    // For tests: the item of an object, or nullptr.
    QTreeWidgetItem* itemFor(const resonant::Id& id) const;
    int itemCount() const { return static_cast<int>(items_.size()); }

signals:
    void operationFailed(const QString& message);

private:
    void rebuild();
    void addSubtree(const resonant::SceneObject& object, QTreeWidgetItem* parentItem, int index);
    void updateItem(QTreeWidgetItem* item, const resonant::SceneObject& object);
    void removeItem(const resonant::Id& id);
    void onSceneChanged(const resonant::SceneEvent& event);
    void syncSelectionFromController();
    void syncSelectionToController();
    void applyFilter();
    void updateSummary();

    std::unique_ptr<Ui::SceneTreePanel> ui_;
    DocumentController* controller_ = nullptr;
    QMenu* contextMenu_ = nullptr;
    QHash<QString, QTreeWidgetItem*> items_;
    bool updating_ = false;
};
