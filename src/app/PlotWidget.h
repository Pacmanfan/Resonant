#pragma once

// Line plots: one or more curves, linear axes (optionally logarithmic x),
// with the axis ranges written out. It draws the data it is given and
// nothing else.
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget* parent = nullptr);

    // Replaces everything with one curve.
    void setCurve(QVector<double> x, QVector<double> y, const QString& title, const QString& xLabel, const QString& yLabel);
    // Adds a further curve (with a legend entry).
    void addCurve(QVector<double> x, QVector<double> y, const QString& label, const QColor& color, bool pointsOnly = false);
    // Vertical marker lines at these x values (resonance candidates).
    void setMarkers(QVector<double> x);
    void setLogX(bool logX);
    void clear();

    int pointCount() const { return curves_.isEmpty() ? 0 : static_cast<int>(curves_.front().x.size()); }
    int curveCount() const { return static_cast<int>(curves_.size()); }
    const QString& title() const { return title_; }
    const QVector<double>& xValues() const;
    const QVector<double>& yValues() const;
    const QVector<double>& yValues(int curve) const { return curves_[curve].y; }
    const QVector<double>& markers() const { return markers_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return {240, 120}; }

private:
    struct Curve {
        QVector<double> x, y;
        QString label;
        QColor color;
        bool pointsOnly = false;
    };
    QVector<Curve> curves_;
    QVector<double> markers_;
    QString title_, xLabel_, yLabel_;
    bool logX_ = false;
};
