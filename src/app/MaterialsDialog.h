// Material browser and editor. The dialog edits a working copy of the
// project's library; nothing reaches the project until OK, when the whole
// edit becomes one undoable step.
#pragma once

#include "materials/Material.h"

#include <QDialog>

#include <memory>

namespace Ui {
class MaterialsDialog;
}

class MaterialsDialog : public QDialog {
    Q_OBJECT
public:
    explicit MaterialsDialog(const resonant::MaterialLibrary& library, QWidget* parent = nullptr);
    ~MaterialsDialog() override;

    // The edited library (valid after exec() returned Accepted).
    const resonant::MaterialLibrary& library() const { return library_; }

    // Operations behind the buttons, public so tests can drive the dialog.
    void selectMaterial(const resonant::Id& id);
    resonant::Id currentMaterialId() const;
    resonant::Id addMaterial();
    resonant::Id duplicateCurrent();
    void deleteCurrent();
    // Sets one property of the current material and domain from text, as
    // typing into the table does. Returns false (and shows the reason) when
    // the value is refused.
    bool setPropertyText(const QString& key, const QString& valueText);
    void setCurrentDomain(resonant::PhysicsDomain domain);
    QString errorText() const;

private:
    resonant::Material* current();
    resonant::PhysicsDomain domain() const;
    void refreshList();
    void showCurrent();
    void fillPropertyTable();
    void onCellEdited(int row, int column);
    void showError(const QString& message);

    std::unique_ptr<Ui::MaterialsDialog> ui_;
    resonant::MaterialLibrary library_;
    bool updating_ = false;
};
