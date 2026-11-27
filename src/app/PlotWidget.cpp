#include "PlotWidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const QVector<double> kEmpty;
}

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PlotWidget::setCurve(QVector<double> x, QVector<double> y, const QString& title, const QString& xLabel, const QString& yLabel)
{
    curves_.clear();
    curves_.push_back({std::move(x), std::move(y), QString(), QColor(30, 110, 200), false});
    title_ = title;
    xLabel_ = xLabel;
    yLabel_ = yLabel;
    markers_.clear();
    update();
}

void PlotWidget::addCurve(QVector<double> x, QVector<double> y, const QString& label, const QColor& color, bool pointsOnly)
{
    curves_.push_back({std::move(x), std::move(y), label, color, pointsOnly});
    update();
}

void PlotWidget::setMarkers(QVector<double> x)
{
    markers_ = std::move(x);
    update();
}

void PlotWidget::setLogX(bool logX)
{
    logX_ = logX;
    update();
}

void PlotWidget::clear()
{
    curves_.clear();
    markers_.clear();
    title_.clear();
    update();
}

const QVector<double>& PlotWidget::xValues() const { return curves_.isEmpty() ? kEmpty : curves_.front().x; }
const QVector<double>& PlotWidget::yValues() const { return curves_.isEmpty() ? kEmpty : curves_.front().y; }

void PlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    const QRectF area = QRectF(rect()).adjusted(70, 22, -12, -34);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(area);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(0, 2, width(), 18), Qt::AlignCenter, title_);
    double xMin = std::numeric_limits<double>::infinity(), xMax = -xMin, yMin = xMin, yMax = -xMin;
    for (const Curve& c : curves_)
        for (qsizetype i = 0; i < std::min(c.x.size(), c.y.size()); ++i) {
            if (!std::isfinite(c.y[i]) || (logX_ && !(c.x[i] > 0.0)))
                continue;
            xMin = std::min(xMin, c.x[i]);
            xMax = std::max(xMax, c.x[i]);
            yMin = std::min(yMin, c.y[i]);
            yMax = std::max(yMax, c.y[i]);
        }
    if (!std::isfinite(xMin) || area.width() < 10 || area.height() < 10) {
        painter.drawText(area, Qt::AlignCenter, QStringLiteral("No data"));
        return;
    }
    if (xMax <= xMin)
        xMax = xMin + 1.0;
    if (yMax <= yMin) {
        yMax = yMin + 1.0;
        yMin -= 1.0;
    }
    const auto fx = [&](double x) { return logX_ ? std::log10(x) : x; };
    const double x0 = fx(xMin), x1 = fx(xMax);
    const auto px = [&](double x) { return area.left() + (fx(x) - x0) / (x1 - x0) * area.width(); };
    const auto py = [&](double y) { return area.bottom() - (y - yMin) / (yMax - yMin) * area.height(); };

    if (yMin < 0.0 && yMax > 0.0) {
        painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DashLine));
        painter.drawLine(QPointF(area.left(), py(0.0)), QPointF(area.right(), py(0.0)));
    }
    painter.setPen(QPen(QColor(200, 120, 0), 1, Qt::DotLine));
    for (const double marker : markers_)
        if (marker >= xMin && marker <= xMax)
            painter.drawLine(QPointF(px(marker), area.top()), QPointF(px(marker), area.bottom()));

    painter.setClipRect(area);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const Curve& c : curves_) {
        painter.setPen(QPen(c.color, 1.2));
        if (c.pointsOnly) {
            painter.setBrush(c.color);
            for (qsizetype i = 0; i < std::min(c.x.size(), c.y.size()); ++i)
                if (std::isfinite(c.y[i]) && (!logX_ || c.x[i] > 0.0))
                    painter.drawEllipse(QPointF(px(c.x[i]), py(c.y[i])), 2.0, 2.0);
            painter.setBrush(Qt::NoBrush);
            continue;
        }
        QPainterPath path;
        bool started = false;
        for (qsizetype i = 0; i < std::min(c.x.size(), c.y.size()); ++i) {
            if (!std::isfinite(c.y[i]) || (logX_ && !(c.x[i] > 0.0))) {
                started = false;
                continue;
            }
            const QPointF point(px(c.x[i]), py(c.y[i]));
            if (!started) {
                path.moveTo(point);
                started = true;
            } else {
                path.lineTo(point);
            }
        }
        painter.drawPath(path);
    }
    painter.setClipping(false);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Legend.
    int row = 0;
    for (const Curve& c : curves_) {
        if (c.label.isEmpty())
            continue;
        const QRectF entry(area.right() - 230, area.top() + 4 + 15 * row++, 226, 14);
        painter.fillRect(QRectF(entry.left(), entry.center().y() - 1, 14, 3), c.color);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(entry.adjusted(18, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, c.label);
    }

    painter.setPen(palette().color(QPalette::Text));
    const auto number = [](double v) { return QString::number(v, 'g', 4); };
    painter.drawText(QRectF(0, area.top() - 8, 66, 16), Qt::AlignRight | Qt::AlignVCenter, number(yMax));
    painter.drawText(QRectF(0, area.bottom() - 8, 66, 16), Qt::AlignRight | Qt::AlignVCenter, number(yMin));
    painter.drawText(QRectF(area.left() - 40, area.bottom() + 2, 80, 14), Qt::AlignCenter, number(xMin));
    painter.drawText(QRectF(area.right() - 60, area.bottom() + 2, 72, 14), Qt::AlignRight | Qt::AlignVCenter, number(xMax));
    painter.drawText(QRectF(area.left(), area.bottom() + 16, area.width(), 16), Qt::AlignCenter,
                     xLabel_ + (logX_ ? QStringLiteral(" (logarithmic)") : QString()));
    painter.save();
    painter.translate(12, area.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2, -8, area.height(), 16), Qt::AlignCenter, yLabel_);
    painter.restore();
}
