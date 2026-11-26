#include "ObjectLabels.h"

#include "AcousticMarkers.h"

using namespace resonant;

QString objectTypeLabel(const SceneObject& object)
{
    if (object.type() == object_types::Group)
        return QStringLiteral("Group");
    if (object.state.geometry)
        return QString::fromStdString(object.state.geometry->kind().displayName);
    if (const QString label = acousticTypeLabel(object); !label.isEmpty())
        return label;
    return QString::fromStdString(object.type());
}
