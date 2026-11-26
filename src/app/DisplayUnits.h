// The units the user interface shows. The core stores SI values only;
// every conversion between a displayed number and a stored one goes through
// this class, which is the single unit boundary of the application.
#pragma once

#include "foundation/Units.h"

#include <QObject>
#include <QString>

class DisplayUnits : public QObject {
    Q_OBJECT
public:
    explicit DisplayUnits(QObject* parent = nullptr);

    resonant::units::LengthUnit lengthUnit() const { return lengthUnit_; }
    void setLengthUnit(resonant::units::LengthUnit unit);

    double lengthFromSI(double meters) const { return resonant::units::fromSI(meters, lengthUnit_); }
    double lengthToSI(double value) const { return resonant::units::toSI(value, lengthUnit_); }
    QString lengthSymbol() const;

    // A displayed value of a parameter whose SI unit is `siUnit` ("m",
    // "rad", ...): lengths in the display unit, angles in degrees, anything
    // else unchanged.
    double fromSI(double value, const QString& siUnit) const;
    double toSI(double value, const QString& siUnit) const;
    QString symbolFor(const QString& siUnit) const;

signals:
    void changed();

private:
    resonant::units::LengthUnit lengthUnit_ = resonant::units::LengthUnit::Meter;
};
