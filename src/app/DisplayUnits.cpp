#include "DisplayUnits.h"

using namespace resonant;

DisplayUnits::DisplayUnits(QObject* parent) : QObject(parent) {}

void DisplayUnits::setLengthUnit(units::LengthUnit unit)
{
    if (unit == lengthUnit_)
        return;
    lengthUnit_ = unit;
    emit changed();
}

QString DisplayUnits::lengthSymbol() const
{
    const auto symbol = units::symbol(lengthUnit_);
    return QString::fromUtf8(symbol.data(), static_cast<qsizetype>(symbol.size()));
}

double DisplayUnits::fromSI(double value, const QString& siUnit) const
{
    if (siUnit == QLatin1String("m"))
        return lengthFromSI(value);
    if (siUnit == QLatin1String("rad"))
        return units::radiansToDegrees(value);
    return value;
}

double DisplayUnits::toSI(double value, const QString& siUnit) const
{
    if (siUnit == QLatin1String("m"))
        return lengthToSI(value);
    if (siUnit == QLatin1String("rad"))
        return units::degreesToRadians(value);
    return value;
}

QString DisplayUnits::symbolFor(const QString& siUnit) const
{
    if (siUnit == QLatin1String("m"))
        return lengthSymbol();
    if (siUnit == QLatin1String("rad"))
        return QStringLiteral("deg");
    return siUnit;
}
