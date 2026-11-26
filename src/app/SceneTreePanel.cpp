#include "SceneTreePanel.h"

#include "DocumentController.h"
#include "ObjectLabels.h"
#include "ui_SceneTreePanel.h"

#include <QHeaderView>
#include <QMenu>
#include <QSignalBlocker>

#include <functional>

using namespace resonant;

namespace {
constexpr int kIdRole = Qt::UserRole;

QString idOf(const QTreeWidgetItem* item)
{
    return item->data(0, kIdRole).toString();
}
}  // namespace

SceneTreePanel::SceneTreePanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::SceneTreePanel>())
{
    ui_->setupUi(this);
    // The name gets the room; a long type description is elided, with the
    // full text in its tool tip.
    ui_->tree->header()->setStretchLastSection(false);
    ui_->tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui_->tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ui_->tree->header()->resizeSection(1, 105);

    connect(ui_->tree, &QTreeWidget::itemSelectionChanged, this, &SceneTreePanel::syncSelectionToController);
    connect(ui_->tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (updating_ || !controller_ || column != 0)
            return;
        const Id id(idOf(item).toStdString());
        const SceneObject* object = controller_->scene().find(id);
        if (!object)
            return;
        Status status = Status::success();
        const bool visible = item->checkState(0) == Qt::Checked;
        if (visible != object->state.visible)
            status = controller_->setVisible(id, visible);
        else if (item->text(0) != QString::fromStdString(object->state.name))
            status = controller_->renameObject(id, item->text(0));
        if (!status)
            emit operationFailed(QString::fromStdString(status.message()));
        // Show what the scene really holds (an edit may have been refused).
        if (const SceneObject* current = controller_->scene().find(id))
            updateItem(item, *current);
    });
    connect(ui_->tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        if (contextMenu_)
            contextMenu_->exec(ui_->tree->viewport()->mapToGlobal(position));
    });
    connect(ui_->filterEdit, &QLineEdit::textChanged, this, &SceneTreePanel::applyFilter);
}

SceneTreePanel::~SceneTreePanel() = default;

void SceneTreePanel::setController(DocumentController* controller)
{
    controller_ = controller;
    connect(controller_, &DocumentController::projectReplaced, this, &SceneTreePanel::rebuild);
    connect(controller_, &DocumentController::sceneChanged, this, &SceneTreePanel::onSceneChanged);
    connect(controller_, &DocumentController::selectionChanged, this, &SceneTreePanel::syncSelectionFromController);
    rebuild();
}

QTreeWidgetItem* SceneTreePanel::itemFor(const Id& id) const
{
    return items_.value(QString::fromStdString(id.str()), nullptr);
}

void SceneTreePanel::editSelectedName()
{
    if (!controller_)
        return;
    if (QTreeWidgetItem* item = itemFor(controller_->primarySelection())) {
        ui_->tree->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
        ui_->tree->editItem(item, 0);
    }
}

void SceneTreePanel::updateItem(QTreeWidgetItem* item, const SceneObject& object)
{
    const bool wasUpdating = updating_;
    updating_ = true;
    item->setText(0, QString::fromStdString(object.state.name));
    item->setText(1, objectTypeLabel(object));
    item->setToolTip(1, objectTypeLabel(object));
    item->setCheckState(0, object.state.visible ? Qt::Checked : Qt::Unchecked);
    item->setToolTip(0, object.state.visible ? QString() : QStringLiteral("Hidden"));
    updating_ = wasUpdating;
}

void SceneTreePanel::addSubtree(const SceneObject& object, QTreeWidgetItem* parentItem, int index)
{
    auto* item = new QTreeWidgetItem();
    item->setData(0, kIdRole, QString::fromStdString(object.id().str()));
    item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
    updateItem(item, object);
    if (parentItem)
        parentItem->insertChild(index < 0 ? parentItem->childCount() : index, item);
    else
        ui_->tree->insertTopLevelItem(index < 0 ? ui_->tree->topLevelItemCount() : index, item);
    items_.insert(idOf(item), item);
    for (const auto& child : object.children())
        addSubtree(*child, item, -1);
    item->setExpanded(true);
}

void SceneTreePanel::removeItem(const Id& id)
{
    QTreeWidgetItem* item = itemFor(id);
    if (!item)
        return;
    // Forget the item and its descendants.
    QList<QTreeWidgetItem*> pending{item};
    while (!pending.isEmpty()) {
        QTreeWidgetItem* current = pending.takeLast();
        items_.remove(idOf(current));
        for (int i = 0; i < current->childCount(); ++i)
            pending.append(current->child(i));
    }
    delete item;
}

void SceneTreePanel::rebuild()
{
    updating_ = true;
    ui_->tree->clear();
    items_.clear();
    if (controller_)
        for (const auto& child : controller_->scene().root().children())
            addSubtree(*child, nullptr, -1);
    updating_ = false;
    syncSelectionFromController();
    applyFilter();
    updateSummary();
}

void SceneTreePanel::onSceneChanged(const SceneEvent& event)
{
    updating_ = true;
    switch (event.kind) {
    case SceneEvent::Kind::Added:
        if (const SceneObject* object = controller_->scene().find(event.id))
            addSubtree(*object, itemFor(event.parentId), object->indexInParent());
        break;
    case SceneEvent::Kind::Removed: removeItem(event.id); break;
    case SceneEvent::Kind::Changed:
        if (QTreeWidgetItem* item = itemFor(event.id))
            if (const SceneObject* object = controller_->scene().find(event.id))
                updateItem(item, *object);
        break;
    case SceneEvent::Kind::Moved:
        removeItem(event.id);
        if (const SceneObject* object = controller_->scene().find(event.id))
            addSubtree(*object, itemFor(event.parentId), object->indexInParent());
        break;
    case SceneEvent::Kind::Reset:
        updating_ = false;
        rebuild();
        return;
    }
    updating_ = false;
    if (event.kind != SceneEvent::Kind::Changed) {
        syncSelectionFromController();
        applyFilter();
        updateSummary();
    }
}

void SceneTreePanel::syncSelectionFromController()
{
    if (!controller_)
        return;
    const QSignalBlocker blocker(ui_->tree);
    ui_->tree->clearSelection();
    for (const Id& id : controller_->selection()) {
        if (QTreeWidgetItem* item = itemFor(id)) {
            item->setSelected(true);
            ui_->tree->scrollToItem(item);
        }
    }
}

void SceneTreePanel::syncSelectionToController()
{
    if (updating_ || !controller_)
        return;
    QList<Id> ids;
    for (const QTreeWidgetItem* item : ui_->tree->selectedItems())
        ids.append(Id(idOf(item).toStdString()));
    controller_->setSelection(ids);
}

void SceneTreePanel::applyFilter()
{
    const QString needle = ui_->filterEdit->text().trimmed();
    // An item is shown when it or any descendant matches.
    const std::function<bool(QTreeWidgetItem*)> visit = [&](QTreeWidgetItem* item) {
        bool anyChild = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyChild = visit(item->child(i)) || anyChild;
        const bool match = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive);
        item->setHidden(!(match || anyChild));
        return match || anyChild;
    };
    for (int i = 0; i < ui_->tree->topLevelItemCount(); ++i)
        visit(ui_->tree->topLevelItem(i));
}

void SceneTreePanel::updateSummary()
{
    const qsizetype count = items_.size();
    ui_->summaryLabel->setText(count == 0 ? QStringLiteral("No objects")
                                          : count == 1 ? QStringLiteral("1 object")
                                                       : QStringLiteral("%1 objects").arg(count));
}
