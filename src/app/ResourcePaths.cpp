#include "ResourcePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
QString env(const char* name)
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? QString() : QDir(QString::fromLocal8Bit(v)).absolutePath();
}

bool isResourceRoot(const QDir& d)
{
    return d.exists(QStringLiteral("demos/profiles")) && d.exists(QStringLiteral("examples"));
}

bool hasNativeModule(const QDir& d)
{
    return !d.entryList({QStringLiteral("_resonant*.so")}, QDir::Files).isEmpty();
}

// The resources of a data root: the bundle's root or an installation's share/resonant.
void fillFromDataRoot(ResourcePaths& p, const QDir& data)
{
    if (p.help.isEmpty() && data.exists(QStringLiteral("help/index.json")))
        p.help = data.absoluteFilePath(QStringLiteral("help"));
    if (p.resourceRoot.isEmpty() && isResourceRoot(data))
        p.resourceRoot = data.absolutePath();
    if (p.pythonPackage.isEmpty() && data.exists(QStringLiteral("python/resonant/__init__.py")))
        p.pythonPackage = data.absoluteFilePath(QStringLiteral("python"));
    if (p.apiDescriptors.isEmpty() && data.exists(QStringLiteral("automation/resonant_api.desc")))
        p.apiDescriptors = data.absoluteFilePath(QStringLiteral("automation/resonant_api.desc"));
}

QString writable(QStandardPaths::StandardLocation location)
{
    return QStandardPaths::writableLocation(location);
}
}  // namespace

QString ResourcePaths::layoutName(Layout layout)
{
    switch (layout) {
    case Layout::BuildTree:
        return QStringLiteral("build tree");
    case Layout::Installation:
        return QStringLiteral("installation");
    case Layout::Bundle:
        return QStringLiteral("bundle");
    case Layout::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

ResourcePaths ResourcePaths::locate()
{
    return locateFrom(QCoreApplication::applicationDirPath());
}

ResourcePaths ResourcePaths::locateFrom(const QString& executableDirectory, bool useEnvironment)
{
    ResourcePaths p;
    if (useEnvironment) {
        p.help = env("RESONANT_HELP_DIR");
        p.resourceRoot = env("RESONANT_RESOURCE_ROOT");
        p.pythonPackage = env("RESONANT_PYTHON_PACKAGE_DIR");
        p.pythonModule = env("RESONANT_PYTHON_MODULE_DIR");
        p.apiDescriptors = env("RESONANT_API_DESCRIPTORS");
    }
    const QDir exe(executableDirectory);
    p.executableDirectory = exe.absolutePath();
    p.userConfig = writable(QStandardPaths::AppConfigLocation);
    p.userData = writable(QStandardPaths::AppDataLocation);
    p.userCache = writable(QStandardPaths::CacheLocation);
    p.userDocuments = writable(QStandardPaths::DocumentsLocation);

    // A bundle: <root>/bin beside <root>/resonant-runtime.json, the resources at the top of <root>.
    const QDir root(QDir::cleanPath(exe.absoluteFilePath(QStringLiteral(".."))));
    if (root.exists(QStringLiteral("resonant-runtime.json"))) {
        p.layout = Layout::Bundle;
        p.installRoot = root.absolutePath();
        p.runtimeManifest = root.absoluteFilePath(QStringLiteral("resonant-runtime.json"));
        fillFromDataRoot(p, root);
        const QDir runtime(root.absoluteFilePath(QStringLiteral("python/runtime")));
        if (runtime.exists(QStringLiteral("bin/python3"))) {
            p.pythonHome = runtime.absolutePath();
            p.pythonInterpreter = runtime.absoluteFilePath(QStringLiteral("bin/python3"));
        }
        if (root.exists(QStringLiteral("python/site-packages")))
            p.pythonSitePackages = root.absoluteFilePath(QStringLiteral("python/site-packages"));
        return p;  // a bundle never looks outside itself
    }

    // An installation: <prefix>/bin -> <prefix>/share/resonant.
    const QDir share(exe.absoluteFilePath(QStringLiteral("../share/resonant")));
    if (share.exists()) {
        p.layout = Layout::Installation;
        p.installRoot = root.absolutePath();
        fillFromDataRoot(p, share);
    }

    // A build tree: walk up from the executable (build/<tree>/src/main, tests/app, ...).
    QDir up = exe;
    for (int level = 0; level < 5; ++level) {
        if (p.help.isEmpty() && up.exists(QStringLiteral("help/index.json"))) {
            p.help = up.absoluteFilePath(QStringLiteral("help"));
            if (p.layout == Layout::Unknown)
                p.layout = Layout::BuildTree;
        }
        if (p.pythonModule.isEmpty() && hasNativeModule(QDir(up.absoluteFilePath(QStringLiteral("src/python")))))
            p.pythonModule = up.absoluteFilePath(QStringLiteral("src/python"));
        if (p.apiDescriptors.isEmpty() && up.exists(QStringLiteral("src/automation_api/resonant_api.desc")))
            p.apiDescriptors = up.absoluteFilePath(QStringLiteral("src/automation_api/resonant_api.desc"));
        if (p.resourceRoot.isEmpty() && isResourceRoot(up))
            p.resourceRoot = up.absolutePath();
        if (!up.cdUp())
            break;
    }
#ifdef RESONANT_SOURCE_DIR
    // The source tree this build was configured from (development builds; never compiled into a bundle).
    const QDir source(QStringLiteral(RESONANT_SOURCE_DIR));
    if (p.resourceRoot.isEmpty() && isResourceRoot(source))
        p.resourceRoot = source.absolutePath();
    if (p.pythonPackage.isEmpty() && source.exists(QStringLiteral("python/resonant/__init__.py")))
        p.pythonPackage = source.absoluteFilePath(QStringLiteral("python"));
#endif
    if (p.pythonPackage.isEmpty() && !p.resourceRoot.isEmpty() && QDir(p.resourceRoot).exists(QStringLiteral("python/resonant/__init__.py")))
        p.pythonPackage = QDir(p.resourceRoot).absoluteFilePath(QStringLiteral("python"));
    return p;
}
