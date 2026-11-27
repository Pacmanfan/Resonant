#include "ObjectLabels.h"

#include "AcousticMarkers.h"
#include "MechanicsMarkers.h"

QString structureSuffix(const resonant::SceneObject& object);

using namespace resonant;

QString objectTypeLabel(const SceneObject& object)
{
    if (object.type() == object_types::Group)
        return QStringLiteral("Group");
    if (object.state.geometry)
        return QString::fromStdString(object.state.geometry->kind().displayName) + structureSuffix(object);
    if (const QString label = acousticTypeLabel(object); !label.isEmpty())
        return label;
    if (const QString label = mechanicsTypeLabel(object); !label.isEmpty())
        return label;
    return QString::fromStdString(object.type());
}
