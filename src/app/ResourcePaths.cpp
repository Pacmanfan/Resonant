#include "ResourcePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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
}  // namespace

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
    }
    const QDir exe(executableDirectory);

    // An installation: <prefix>/bin -> <prefix>/share/resonant.
    const QDir share(exe.absoluteFilePath(QStringLiteral("../share/resonant")));
    if (share.exists()) {
        if (p.help.isEmpty() && share.exists(QStringLiteral("help/index.json")))
            p.help = share.absoluteFilePath(QStringLiteral("help"));
        if (p.resourceRoot.isEmpty() && isResourceRoot(share))
            p.resourceRoot = share.absolutePath();
        if (p.pythonPackage.isEmpty() && share.exists(QStringLiteral("python/resonant/__init__.py")))
            p.pythonPackage = share.absoluteFilePath(QStringLiteral("python"));
    }

    // A build tree: walk up from the executable (build/<tree>/src/main, tests/app, ...).
    QDir up = exe;
    for (int level = 0; level < 5; ++level) {
        if (p.help.isEmpty() && up.exists(QStringLiteral("help/index.json")))
            p.help = up.absoluteFilePath(QStringLiteral("help"));
        if (p.pythonModule.isEmpty() && hasNativeModule(QDir(up.absoluteFilePath(QStringLiteral("src/python")))))
            p.pythonModule = up.absoluteFilePath(QStringLiteral("src/python"));
        if (p.resourceRoot.isEmpty() && isResourceRoot(up))
            p.resourceRoot = up.absolutePath();
        if (!up.cdUp())
            break;
    }
#ifdef RESONANT_SOURCE_DIR
    // The source tree this build was configured from (development builds).
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
