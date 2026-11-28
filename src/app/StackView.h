// A layer stack drawn as an idealized model (REQ-CHAT-20261127-001, RES-FAM-019): the layers
// from the substrate (bottom) outwards, thicknesses to scale (a layer thinner than the minimum
// height is drawn at that height and marked), coloured by role; the interfaces as lines styled
// by their model (perfect, contact resistance, cohesive, none). The caption says it is an
// idealized model, not a micrograph.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

class StackView : public QWidget {
    Q_OBJECT
public:
    explicit StackView(QWidget* parent = nullptr);
    // layers: [{"id", "region", "role", "thickness", "pattern"}] from the bottom; interfaces:
    // [{"below" (layer index), "model"}] ("" when none is declared).
    void setStack(const nlohmann::json& layers, const nlohmann::json& interfaces);
    int layerCount() const { return static_cast<int>(layers_.size()); }
    int interfaceCount() const { return static_cast<int>(interfaces_.size()); }
    int thinLayerCount() const;  // drawn at the minimum height
    QString caption() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return {360, 240}; }

private:
    nlohmann::json layers_ = nlohmann::json::array();
    nlohmann::json interfaces_ = nlohmann::json::array();
};
