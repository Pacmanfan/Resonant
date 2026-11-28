// The "Matter" panel (REQ-CHAT-20261127-001, RES-FAM-019): an architecture's regions (role,
// place, material source, process), its layer stacks drawn as an idealized model with their
// interfaces, the analyses with their results and assumptions, the manufacturability check,
// the exploration (findings apart from hypotheses) and the mechanisms between domains, a
// side-by-side comparison of the alternatives, and a stack sent to the coupled engine.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <memory>

class MatterController;
class StackView;
class QPushButton;
namespace Ui {
class MatterPanel;
}

class MatterPanel : public QWidget {
    Q_OBJECT
public:
    explicit MatterPanel(QWidget* parent = nullptr);
    ~MatterPanel() override;

    void setController(MatterController* controller);

    // For tests.
    int architectureCount() const;
    void selectArchitecture(const QString& id);
    void selectAnalysis(const QString& id);
    void setOptions(const QString& options);
    int regionRows() const;
    int interfaceRows() const;
    StackView* stackView() const;
    QPushButton* runButton() const;
    QPushButton* manufactureButton() const;
    QPushButton* exploreButton() const;
    QPushButton* mechanismsButton() const;
    QPushButton* duplicateButton() const;
    QPushButton* compareButton() const;
    QPushButton* systemButton() const;
    QString resultText() const;
    int plottedPoints() const;
    QString manufactureStatus() const;
    int manufactureRows() const;
    QString exploreText() const;
    int compareRows() const;
    int compareColumns() const;
    QString statusText() const;
    void setMechanismDomains(const QString& from, const QString& to);

public slots:
    void refresh();

private:
    void showArchitecture();
    void showStack();
    QString currentId() const;
    nlohmann::json current_ = nlohmann::json::object();
    std::unique_ptr<Ui::MatterPanel> ui_;
    MatterController* controller_ = nullptr;
};
