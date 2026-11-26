#include "Application.h"

#include "foundation/Log.h"
#include "foundation/Version.h"
#include "render/Renderer.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QSysInfo>

#include <Eigen/Core>
#include <nlohmann/json.hpp>

namespace resonant_app {

void configureBeforeApplication()
{
    QCoreApplication::setOrganizationName(QStringLiteral("Resonant"));
    QCoreApplication::setApplicationName(QStringLiteral("Resonant"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(resonant::kApplicationVersion));
    QSurfaceFormat::setDefaultFormat(resonant::render::requiredSurfaceFormat());
}

void logStartup(const QApplication& application)
{
    using resonant::logInfo;
    logInfo("application", std::string(resonant::kApplicationName) + " " + resonant::kApplicationVersion + " starting");
    logInfo("application", "operating system: " + QSysInfo::prettyProductName().toStdString() + " ("
                               + QSysInfo::currentCpuArchitecture().toStdString() + ")");
    logInfo("application", "platform plugin: " + application.platformName().toStdString());
    logInfo("application",
            std::string("dependencies: Qt ") + qVersion() + ", Eigen " + std::to_string(EIGEN_WORLD_VERSION) + "."
                + std::to_string(EIGEN_MAJOR_VERSION) + "." + std::to_string(EIGEN_MINOR_VERSION) + ", nlohmann-json "
                + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." + std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "."
                + std::to_string(NLOHMANN_JSON_VERSION_PATCH));
}

}  // namespace resonant_app
