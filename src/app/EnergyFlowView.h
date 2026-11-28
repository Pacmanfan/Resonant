// The energy flow view of the Accounting panel (REQ-CHAT-20261126-022, RES-ACC-016): the
// flow graph of a report - what comes from outside on the left, the system's members in the
// middle, what leaves and what is dissipated on the right, internal transfers between
// members - with each band's width proportional to its calculated amount (the largest is
// the widest; nothing is decorative) and its value and unit written on it.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

class EnergyFlowView : public QWidget {
    Q_OBJECT
public:
    explicit EnergyFlowView(QWidget* parent = nullptr);

    // `flows`: the report's {"nodes", "edges"}; `family`: which edges to draw; `unit`: their unit.
    void setFlows(const nlohmann::json& flows, const QString& family, const QString& unit);
    void clear();

    // For tests.
    int nodeCount() const { return static_cast<int>(nodes_.size()); }
    int edgeCount() const { return static_cast<int>(edges_.size()); }
    // The drawn width of the band from -> to (0 when there is none).
    double bandWidth(const QString& from, const QString& to) const;
    QString edgeLabel(const QString& from, const QString& to) const;

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return {480, 260}; }

private:
    struct Node {
        QString id, name, kind;
        bool member = false;
        int column = 1;  // 0 outside sources, 1 members, 2 outputs and sinks
        double stored = 0.0;
    };
    struct Edge {
        QString from, to, role;
        double magnitude = 0.0, width = 0.0;
        QString label;
    };
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    QString unit_;
};
