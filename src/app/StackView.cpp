#include "StackView.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <functional>

using nlohmann::json;

namespace {

constexpr int kMinimumHeight = 3;

QColor roleColor(const std::string& role)
{
    if (role == "substrate")
        return {150, 155, 165};
    if (role == "transduction")
        return {225, 170, 60};
    if (role == "electrode")
        return {210, 185, 90};
    if (role == "adhesion")
        return {120, 170, 120};
    if (role == "encapsulation")
        return {120, 165, 215};
    if (role == "isolation")
        return {180, 140, 200};
    if (role == "thermal-barrier")
        return {215, 120, 110};
    const auto h = static_cast<int>(std::hash<std::string>{}(role) % 360);
    return QColor::fromHsl(h, 90, 160);
}

}  // namespace

StackView::StackView(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(160);
}

void StackView::setStack(const json& layers, const json& interfaces)
{
    layers_ = layers.is_array() ? layers : json::array();
    interfaces_ = interfaces.is_array() ? interfaces : json::array();
    update();
}

int StackView::thinLayerCount() const
{
    double total = 0.0;
    for (const json& l : layers_)
        total += l.value("thickness", 0.0);
    const double drawable = std::max(1, height() - 40);
    int thin = 0;
    for (const json& l : layers_)
        if (total > 0.0 && l.value("thickness", 0.0) / total * drawable < kMinimumHeight)
            ++thin;
    return thin;
}

QString StackView::caption() const
{
    const int thin = thinLayerCount();
    return tr("Idealized model, thicknesses to scale%1").arg(thin > 0 ? tr(" (%1 thin layers drawn at %2 px)").arg(thin).arg(kMinimumHeight) : QString());
}

void StackView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    if (layers_.empty()) {
        p.drawText(rect(), Qt::AlignCenter, tr("No stack"));
        return;
    }
    double total = 0.0;
    for (const json& l : layers_)
        total += l.value("thickness", 0.0);
    const int left = 16, right = width() - 150, top = 8, bottom = height() - 28;
    const double drawable = std::max(1, bottom - top);
    // Heights to scale, the thin ones at the minimum, the rest scaled to fill.
    std::vector<double> h;
    double fixed = 0.0, scalable = 0.0;
    for (const json& l : layers_) {
        const double hh = total > 0.0 ? l.value("thickness", 0.0) / total * drawable : drawable / layers_.size();
        h.push_back(hh);
        (hh < kMinimumHeight ? fixed : scalable) += hh < kMinimumHeight ? kMinimumHeight : hh;
    }
    const double scale = scalable > 0.0 ? (drawable - fixed) / scalable : 1.0;
    double y = bottom;
    std::vector<double> boundaries;
    for (size_t i = 0; i < layers_.size(); ++i) {
        const double hh = h[i] < kMinimumHeight ? kMinimumHeight : h[i] * scale;
        const json& l = layers_[i];
        const QRectF r(left, y - hh, right - left, hh);
        QColor c = roleColor(l.value("role", std::string()));
        const std::string pattern = l.value("pattern", std::string("uniform"));
        if (pattern == "segmented") {
            const double end = l.value("coverage", 1.0);
            p.fillRect(QRectF(r.left(), r.top(), r.width() * end, r.height()), c);
            p.fillRect(QRectF(r.left() + r.width() * end, r.top(), r.width() * (1.0 - end), r.height()), palette().base());
        } else {
            p.fillRect(r, c);
        }
        if (h[i] < kMinimumHeight) {
            p.setPen(QPen(palette().text().color(), 1, Qt::DotLine));
            p.drawRect(r);
        }
        p.setPen(palette().text().color());
        p.drawText(QRectF(right + 6, r.center().y() - 8, 144, 16), Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(l.value("id", std::string())) + QStringLiteral(" (%1 um)").arg(l.value("thickness", 0.0) * 1e6, 0, 'g', 3));
        y -= hh;
        boundaries.push_back(y);
    }
    // The interfaces between layers i and i + 1, at boundaries[i].
    for (const json& in : interfaces_) {
        const int below = in.value("below", -1);
        if (below < 0 || below + 1 >= static_cast<int>(boundaries.size()) + 1 || below >= static_cast<int>(boundaries.size()))
            continue;
        const std::string model = in.value("model", std::string());
        QPen pen(palette().text().color(), 1.2);
        if (model.empty())
            pen = QPen(Qt::black, 1.6, Qt::DashLine);
        else if (model.find("contact-resistance") != std::string::npos)
            pen = QPen(QColor(230, 110, 20), 2.0);
        else if (model.find("cohesive") != std::string::npos)
            pen = QPen(QColor(200, 30, 30), 1.6, Qt::DashDotLine);
        p.setPen(pen);
        p.drawLine(QPointF(left, boundaries[static_cast<size_t>(below)]), QPointF(right, boundaries[static_cast<size_t>(below)]));
    }
    p.setPen(palette().text().color());
    p.drawText(QRectF(left, height() - 22, width() - left, 18), Qt::AlignLeft | Qt::AlignVCenter, caption());
}
