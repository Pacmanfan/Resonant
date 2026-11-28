// The "Material Scales" panel (REQ-CHAT-20261126-020, RES-MSC-014): the project's material
// representations by scale with their model cards, relationships and applicable
// derivations; a derivation run as a job and assigned to a library material; and the
// lineage of a material's properties back to the representations they came from.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <memory>

class MultiscaleController;
namespace Ui {
class MaterialScalesPanel;
}
class QPushButton;

class MaterialScalesPanel : public QWidget {
    Q_OBJECT
public:
    explicit MaterialScalesPanel(QWidget* parent = nullptr);
    ~MaterialScalesPanel() override;

    void setController(MultiscaleController* controller);

    // For tests.
    int representationCount() const;
    void selectRepresentation(const QString& id);
    QString currentRepresentation() const;
    QStringList methodChoices() const;
    void chooseMethod(const QString& id);
    void chooseTarget(const QString& materialId);
    void showLineage(const QString& materialId);
    QPushButton* deriveButton() const;
    QPushButton* assignButton() const;
    QString statusText() const;
    QString detailsText() const;
    int lineageRowCount() const;
    QString lineageCell(int row, int column) const;

public slots:
    void refresh();

private:
    void showRepresentation();
    void fillLineage();

    std::unique_ptr<Ui::MaterialScalesPanel> ui_;
    MultiscaleController* controller_ = nullptr;
    QString resultId_;
    bool filling_ = false;
};
