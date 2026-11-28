#include "EnergyFlowView.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

using nlohmann::json;

namespace {

constexpr double kMaxBand = 28.0;  // px for the largest amount

QString number(double v)
{
    return QString::number(v, 'g', 4);
}

}  // namespace

EnergyFlowView::EnergyFlowView(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setAutoFillBackground(true);
}

void EnergyFlowView::clear()
{
    nodes_.clear();
    edges_.clear();
    update();
}

void EnergyFlowView::setFlows(const json& flows, const QString& family, const QString& unit)
{
    nodes_.clear();
    edges_.clear();
    unit_ = unit;
    const std::string fam = family.toStdString();
    double largest = 0.0;
    for (const json& e : flows.value("edges", json::array()))
        if (e.value("family", std::string()) == fam)
            largest = std::max(largest, e.value("magnitude", 0.0));
    std::vector<std::string> used;
    for (const json& e : flows.value("edges", json::array())) {
        if (e.value("family", std::string()) != fam)
            continue;
        Edge edge;
        edge.from = QString::fromStdString(e.value("from", std::string()));
        edge.to = QString::fromStdString(e.value("to", std::string()));
        edge.role = QString::fromStdString(e.value("role", std::string()));
        edge.magnitude = e.value("magnitude", 0.0);
        edge.width = largest > 0.0 ? std::max(1.0, kMaxBand * edge.magnitude / largest) : 1.0;
        const json& a = e["amount"];
        edge.label = a.is_number() ? number(a.get<double>()) + QLatin1Char(' ') + unit : number(edge.magnitude) + QStringLiteral(" (|vector|) ") + unit;
        edges_.push_back(edge);
        used.push_back(edge.from.toStdString());
        used.push_back(edge.to.toStdString());
    }
    for (const json& n : flows.value("nodes", json::array())) {
        const std::string id = n.value("id", std::string());
        const bool member = n.value("member", false);
        if (!member && std::find(used.begin(), used.end(), id) == used.end())
            continue;
        Node node;
        node.id = QString::fromStdString(id);
        node.name = QString::fromStdString(n.value("name", id));
        node.kind = QString::fromStdString(n.value("kind", std::string()));
        node.member = member;
        node.stored = n.value("storedChange", 0.0);
        if (!member) {
            // Outside: on the left when it only gives, else on the right.
            bool gives = false, takes = false;
            for (const Edge& e : edges_) {
                gives = gives || e.from == node.id;
                takes = takes || e.to == node.id;
            }
            node.column = gives && !takes ? 0 : 2;
        }
        nodes_.push_back(node);
    }
    update();
}

double EnergyFlowView::bandWidth(const QString& from, const QString& to) const
{
    for (const Edge& e : edges_)
        if (e.from == from && e.to == to)
            return e.width;
    return 0.0;
}

QString EnergyFlowView::edgeLabel(const QString& from, const QString& to) const
{
    for (const Edge& e : edges_)
        if (e.from == from && e.to == to)
            return e.label;
    return {};
}

void EnergyFlowView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    p.fillRect(rect(), pal.base());
    if (nodes_.empty()) {
        p.setPen(pal.color(QPalette::PlaceholderText));
        p.drawText(rect(), Qt::AlignCenter, tr("Run a study to see where its energy goes."));
        return;
    }
    // Layout: three columns, nodes spread vertically.
    std::vector<QRectF> boxes(nodes_.size());
    const double w = width(), h = height(), boxW = std::min(150.0, w / 4.0), boxH = 26.0;
    for (int col = 0; col < 3; ++col) {
        std::vector<size_t> idx;
        for (size_t i = 0; i < nodes_.size(); ++i)
            if (nodes_[i].column == col)
                idx.push_back(i);
        const double x = col == 0 ? 8.0 : col == 1 ? (w - boxW) / 2.0 : w - boxW - 8.0;
        for (size_t k = 0; k < idx.size(); ++k) {
            const double y = (k + 1) * h / (idx.size() + 1) - boxH / 2.0;
            boxes[idx[k]] = QRectF(x, y, boxW, boxH);
        }
    }
    const auto box = [&](const QString& id) -> const QRectF* {
        for (size_t i = 0; i < nodes_.size(); ++i)
            if (nodes_[i].id == id)
                return &boxes[i];
        return nullptr;
    };
    // Bands.
    for (const Edge& e : edges_) {
        const QRectF* a = box(e.from);
        const QRectF* b = box(e.to);
        if (!a || !b)
            continue;
        QColor c = e.role == QLatin1String("input") ? QColor(52, 120, 200) : e.role == QLatin1String("dissipated") ? QColor(210, 110, 40)
                   : e.role == QLatin1String("output")                                                            ? QColor(120, 120, 120)
                                                                                                                  : QColor(60, 160, 100);
        c.setAlpha(150);
        const QPointF s(a->center().x() < b->center().x() ? a->right() : a->left(), a->center().y());
        const QPointF t(a->center().x() < b->center().x() ? b->left() : b->right(), b->center().y());
        QPainterPath path;
        if (std::abs(a->center().x() - b->center().x()) < 1.0) {
            // Two members in the same column: an arc beside them.
            path.moveTo(a->right(), a->center().y());
            path.cubicTo(QPointF(a->right() + 60, a->center().y()), QPointF(b->right() + 60, b->center().y()), QPointF(b->right(), b->center().y()));
        } else {
            const double mx = (s.x() + t.x()) / 2.0;
            path.moveTo(s);
            path.cubicTo(QPointF(mx, s.y()), QPointF(mx, t.y()), t);
        }
        p.setPen(QPen(c, e.width, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(pal.color(QPalette::Text));
        p.drawText(path.pointAtPercent(0.5) + QPointF(4, -4), e.label);
    }
    // Nodes.
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const Node& n = nodes_[i];
        p.setPen(pal.color(QPalette::Text));
        p.setBrush(n.member ? pal.color(QPalette::AlternateBase) : pal.color(QPalette::Window));
        p.drawRoundedRect(boxes[i], 4, 4);
        QString text = n.name;
        if (n.member && n.stored != 0.0)
            text += QStringLiteral("  (stored %1)").arg(number(n.stored));
        p.drawText(boxes[i].adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(text, Qt::ElideRight, boxes[i].width() - 8));
    }
}
