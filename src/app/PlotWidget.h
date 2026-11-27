#pragma once

// A plain line plot: one curve, linear axes, with the axis ranges written
// out. It draws data it is given and nothing else.
#include <QString>
#include <QVector>
#include <QWidget>

class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget* parent = nullptr);

    void setCurve(QVector<double> x, QVector<double> y, const QString& title, const QString& xLabel, const QString& yLabel);
    // Vertical marker lines at these x values (resonance candidates).
    void setMarkers(QVector<double> x);
    void clear();

    int pointCount() const { return static_cast<int>(x_.size()); }
    const QString& title() const { return title_; }
    const QVector<double>& xValues() const { return x_; }
    const QVector<double>& yValues() const { return y_; }
    const QVector<double>& markers() const { return markers_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return {240, 120}; }

private:
    QVector<double> x_, y_, markers_;
    QString title_, xLabel_, yLabel_;
};
