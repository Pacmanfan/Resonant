#include "PlotWidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PlotWidget::setCurve(QVector<double> x, QVector<double> y, const QString& title, const QString& xLabel, const QString& yLabel)
{
    x_ = std::move(x);
    y_ = std::move(y);
    title_ = title;
    xLabel_ = xLabel;
    yLabel_ = yLabel;
    markers_.clear();
    update();
}

void PlotWidget::setMarkers(QVector<double> x)
{
    markers_ = std::move(x);
    update();
}

void PlotWidget::clear()
{
    x_.clear();
    y_.clear();
    markers_.clear();
    title_.clear();
    update();
}

void PlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    const QRectF area = QRectF(rect()).adjusted(70, 22, -12, -34);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(area);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(0, 2, width(), 18), Qt::AlignCenter, title_);
    if (x_.size() < 2 || y_.size() != x_.size() || area.width() < 10 || area.height() < 10) {
        painter.drawText(area, Qt::AlignCenter, x_.isEmpty() ? QStringLiteral("No data") : QString());
        return;
    }
    const auto [xMinIt, xMaxIt] = std::minmax_element(x_.begin(), x_.end());
    const auto [yMinIt, yMaxIt] = std::minmax_element(y_.begin(), y_.end());
    double xMin = *xMinIt, xMax = *xMaxIt, yMin = *yMinIt, yMax = *yMaxIt;
    if (xMax <= xMin)
        xMax = xMin + 1.0;
    if (yMax <= yMin) {
        yMax = yMin + 1.0;
        yMin -= 1.0;
    }
    const auto px = [&](double x) { return area.left() + (x - xMin) / (xMax - xMin) * area.width(); };
    const auto py = [&](double y) { return area.bottom() - (y - yMin) / (yMax - yMin) * area.height(); };

    if (yMin < 0.0 && yMax > 0.0) {
        painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DashLine));
        painter.drawLine(QPointF(area.left(), py(0.0)), QPointF(area.right(), py(0.0)));
    }
    painter.setPen(QPen(QColor(200, 120, 0), 1, Qt::DotLine));
    for (const double marker : markers_)
        if (marker >= xMin && marker <= xMax)
            painter.drawLine(QPointF(px(marker), area.top()), QPointF(px(marker), area.bottom()));

    // At most a few points per pixel column: keep the extremes of each column.
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(30, 110, 200), 1.2));
    QPainterPath path;
    const qsizetype stride = std::max<qsizetype>(1, x_.size() / static_cast<qsizetype>(4 * area.width()));
    bool started = false;
    for (qsizetype i = 0; i < x_.size(); i += stride) {
        const qsizetype end = std::min(i + stride, x_.size());
        const auto [lo, hi] = std::minmax_element(y_.begin() + i, y_.begin() + end);
        for (const auto it : {std::min(lo, hi), std::max(lo, hi)}) {
            const QPointF point(px(x_[it - y_.begin()]), py(*it));
            if (!started) {
                path.moveTo(point);
                started = true;
            } else {
                path.lineTo(point);
            }
        }
    }
    painter.drawPath(path);

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(palette().color(QPalette::Text));
    const auto number = [](double v) { return QString::number(v, 'g', 4); };
    painter.drawText(QRectF(0, area.top() - 8, 66, 16), Qt::AlignRight | Qt::AlignVCenter, number(yMax));
    painter.drawText(QRectF(0, area.bottom() - 8, 66, 16), Qt::AlignRight | Qt::AlignVCenter, number(yMin));
    painter.drawText(QRectF(area.left() - 40, area.bottom() + 2, 80, 14), Qt::AlignCenter, number(xMin));
    painter.drawText(QRectF(area.right() - 60, area.bottom() + 2, 72, 14), Qt::AlignRight | Qt::AlignVCenter, number(xMax));
    painter.drawText(QRectF(area.left(), area.bottom() + 16, area.width(), 16), Qt::AlignCenter, xLabel_);
    painter.save();
    painter.translate(12, area.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2, -8, area.height(), 16), Qt::AlignCenter, yLabel_);
    painter.restore();
}
