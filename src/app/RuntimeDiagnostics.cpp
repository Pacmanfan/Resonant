#include "RuntimeDiagnostics.h"

#include "PythonEnvironment.h"
#include "compute/Compute.h"
#include "foundation/Version.h"
#include "help/HelpIndex.h"
#include "plugins/PluginManager.h"
#include "render/Renderer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QProcess>
#include <QSysInfo>
#include <QVersionNumber>

#include <gnu/libc-version.h>
#include <link.h>

#include <algorithm>
#include <filesystem>
#include <map>

namespace runtime_diagnostics {
namespace {

void add(Report& r, const QString& name, Status status, const QString& detail)
{
    r.checks.push_back({name, status, detail});
}

QJsonObject readManifest(const QString& file)
{
    QFile f(file);
    if (file.isEmpty() || !f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool inside(const QString& path, const QString& root)
{
    if (root.isEmpty() || path.isEmpty())
        return false;
    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString r = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    return p == r || p.startsWith(r + QLatin1Char('/'));
}

QStringList loadedLibraries()
{
    QStringList out;
    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void* data) {
            if (info->dlpi_name && *info->dlpi_name)
                static_cast<QStringList*>(data)->push_back(QString::fromLocal8Bit(info->dlpi_name));
            return 0;
        },
        &out);
    return out;
}

QString shown(const QString& path)
{
    return path.isEmpty() ? QStringLiteral("(not found)") : redact(QDir::toNativeSeparators(path));
}

void checkPackage(Report& r, const ResourcePaths& paths, const QJsonObject& manifest)
{
    QString build = QStringLiteral("development build");
    if (!manifest.isEmpty())
        build = QStringLiteral("commit %1, %2, built %3")
                    .arg(manifest.value(QStringLiteral("commit")).toString(QStringLiteral("?")),
                         manifest.value(QStringLiteral("buildType")).toString(QStringLiteral("?")),
                         manifest.value(QStringLiteral("built")).toString(QStringLiteral("?")));
    add(r, QStringLiteral("Application"), Status::Info,
        QStringLiteral("%1 %2 (%3)").arg(QString::fromLatin1(resonant::kApplicationName), QString::fromLatin1(resonant::kApplicationVersion), build));
    const QString where = paths.installRoot.isEmpty() ? paths.executableDirectory : paths.installRoot;
    add(r, QStringLiteral("Layout"), paths.layout == ResourcePaths::Layout::Unknown ? Status::Warn : Status::Info,
        QStringLiteral("%1 at %2").arg(ResourcePaths::layoutName(paths.layout), shown(where)));
}

void checkSystem(Report& r, const QJsonObject& manifest)
{
    const QString arch = QSysInfo::currentCpuArchitecture();
    const QString wanted = manifest.value(QStringLiteral("architecture")).toString();
    add(r, QStringLiteral("System"), !wanted.isEmpty() && wanted != arch ? Status::Fail : Status::Info,
        QStringLiteral("%1, %2 %3, %4").arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion(), arch));
    const QString libc = QString::fromLatin1(gnu_get_libc_version());
    const QString minimum = manifest.value(QStringLiteral("minimumGlibc")).toString();
    if (minimum.isEmpty())
        add(r, QStringLiteral("C library"), Status::Info, QStringLiteral("glibc %1").arg(libc));
    else {
        const bool ok = QVersionNumber::fromString(libc) >= QVersionNumber::fromString(minimum);
        add(r, QStringLiteral("C library"), ok ? Status::Pass : Status::Fail,
            ok ? QStringLiteral("glibc %1 (needs %2 or newer)").arg(libc, minimum)
               : QStringLiteral("glibc %1 is older than the %2 this package was built for: use a newer distribution").arg(libc, minimum));
    }
}

void checkQt(Report& r, const ResourcePaths& paths, const QStringList& libraries)
{
    QStringList plugins;
    for (const QString& p : QCoreApplication::libraryPaths())
        plugins << shown(p);
    add(r, QStringLiteral("Qt"), Status::Info,
        QStringLiteral("runtime %1 (built with %2), platform plugin '%3', plugin paths %4")
            .arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR), QGuiApplication::platformName(), plugins.join(QStringLiteral(", "))));
    if (!paths.isBundle())
        return;
    QStringList foreign;
    for (const QString& lib : libraries)
        if (QFileInfo(lib).fileName().startsWith(QStringLiteral("libQt6")) && !inside(lib, paths.installRoot))
            foreign << shown(lib);
    add(r, QStringLiteral("Qt libraries"), foreign.isEmpty() ? Status::Pass : Status::Fail,
        foreign.isEmpty() ? QStringLiteral("every Qt library and plugin is the package's own")
                          : QStringLiteral("Qt libraries loaded from outside the package: %1 (unset LD_LIBRARY_PATH / LD_PRELOAD and start with ./resonant)")
                                .arg(foreign.join(QStringLiteral(", "))));
}

void checkOpenGl(Report& r)
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal")) {
        add(r, QStringLiteral("OpenGL"), Status::Warn, QStringLiteral("not checked: no display (platform '%1')").arg(platform));
        return;
    }
    const QSurfaceFormat wanted = resonant::render::requiredSurfaceFormat();
    QOpenGLContext context;
    context.setFormat(wanted);
    QOffscreenSurface surface;
    surface.setFormat(wanted);
    surface.create();
    if (!context.create() || !surface.isValid() || !context.makeCurrent(&surface)) {
        add(r, QStringLiteral("OpenGL"), Status::Fail,
            QStringLiteral("no OpenGL %1.%2 core context could be created: update the graphics driver, or try ./resonant --software-gl")
                .arg(wanted.majorVersion())
                .arg(wanted.minorVersion()));
        return;
    }
    const QSurfaceFormat got = context.format();
    QOpenGLFunctions* gl = context.functions();
    const auto text = [gl](GLenum name) {
        const auto* s = reinterpret_cast<const char*>(gl->glGetString(name));
        return s ? QString::fromLatin1(s) : QStringLiteral("?");
    };
    const QString detail = QStringLiteral("%1.%2 %3 (%4; %5)")
                               .arg(got.majorVersion())
                               .arg(got.minorVersion())
                               .arg(got.profile() == QSurfaceFormat::CoreProfile ? QStringLiteral("core") : QStringLiteral("compatibility"),
                                    text(GL_RENDERER), text(GL_VENDOR));
    const bool enough = std::make_pair(got.majorVersion(), got.minorVersion()) >= std::make_pair(wanted.majorVersion(), wanted.minorVersion());
    context.doneCurrent();
    add(r, QStringLiteral("OpenGL"), enough ? Status::Pass : Status::Fail,
        enough ? detail : QStringLiteral("%1: the viewport needs OpenGL %2.%3 core").arg(detail).arg(wanted.majorVersion()).arg(wanted.minorVersion()));
}

void checkPython(Report& r, const ResourcePaths& paths)
{
    const PythonLaunch launch = pythonLaunch(paths);
    if (!launch.usable() || !launch.error.isEmpty()) {
        add(r, QStringLiteral("Python"), Status::Warn, QStringLiteral("unavailable: %1 (the console, scripts and Python plugins are disabled)").arg(launch.error));
        return;
    }
    QProcess p;
    p.setProcessEnvironment(launch.environment);
    p.setProgram(launch.program);
    p.setArguments({QStringLiteral("-c"), QStringLiteral("import sys, resonant; v = sys.version.split()[0]\n"
                                                         "try:\n import numpy; n = numpy.__version__\n"
                                                         "except ImportError: n = 'not installed'\n"
                                                         "print(v, resonant.API_VERSION, n)")});
    p.start();
    const bool finished = p.waitForFinished(30000);
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed().split(QLatin1Char('\n')).last();
    if (!finished)
        p.kill();
    const bool ok = finished && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    const QStringList parts = out.split(QLatin1Char(' '));
    const QString origin = launch.bundled ? QStringLiteral("bundled") : launch.origin;
    if (ok && parts.size() >= 3)
        add(r, QStringLiteral("Python"), Status::Pass,
            QStringLiteral("%1 %2 (%3), resonant API %4, NumPy %5").arg(shown(launch.program), parts[0], origin, parts[1], parts.mid(2).join(QLatin1Char(' '))));
    else
        add(r, QStringLiteral("Python"), launch.bundled ? Status::Fail : Status::Warn,
            QStringLiteral("%1 (%2) cannot import resonant: %3").arg(shown(launch.program), origin, err.isEmpty() ? QStringLiteral("no output") : err));
}

void checkResources(Report& r, const ResourcePaths& paths)
{
    if (paths.help.isEmpty())
        add(r, QStringLiteral("Help"), Status::Fail, QStringLiteral("not found (the help folder is missing from the package)"));
    else if (auto index = resonant::help::HelpIndex::load(paths.help.toStdString()); !index)
        add(r, QStringLiteral("Help"), Status::Fail, QStringLiteral("%1: %2").arg(shown(paths.help), QString::fromStdString(index.message())));
    else
        add(r, QStringLiteral("Help"), Status::Pass,
            QStringLiteral("%1 topics, %2%3")
                .arg(index.value().topics().size())
                .arg(shown(paths.help))
                .arg(QFileInfo::exists(paths.help + QStringLiteral("/index.html")) ? QStringLiteral(" (index.html for browsers)") : QString()));
    int projects = 0;
    if (!paths.resourceRoot.isEmpty())
        for (QDirIterator it(paths.resourceRoot + QStringLiteral("/examples"), {QStringLiteral("*.resonant")}, QDir::Files, QDirIterator::Subdirectories);
             it.hasNext(); it.next())
            ++projects;
    const int demos = paths.resourceRoot.isEmpty() ? 0 : QDir(paths.resourceRoot + QStringLiteral("/demos/profiles")).entryList({QStringLiteral("*.json")}, QDir::Files).size();
    add(r, QStringLiteral("Examples and demonstrations"), paths.resourceRoot.isEmpty() ? Status::Fail : Status::Pass,
        paths.resourceRoot.isEmpty() ? QStringLiteral("not found") : QStringLiteral("%1 example projects, %2 demonstrations in %3").arg(projects).arg(demos).arg(shown(paths.resourceRoot)));
    const QString host = QDir(paths.executableDirectory).absoluteFilePath(QStringLiteral("resonant-automation"));
    add(r, QStringLiteral("Automation"), paths.apiDescriptors.isEmpty() ? Status::Warn : Status::Info,
        QStringLiteral("descriptor set %1; headless host %2").arg(shown(paths.apiDescriptors), QFileInfo::exists(host) ? shown(host) : QStringLiteral("(not built)")));
}

void checkComputeAndPlugins(Report& r, const ResourcePaths& paths, bool python)
{
    add(r, QStringLiteral("Compute"), Status::Info, QString::fromStdString(resonant::compute::detectCompute().summary()));
    const std::filesystem::path examples =
        paths.resourceRoot.isEmpty() ? std::filesystem::path() : std::filesystem::path(paths.resourceRoot.toStdString()) / "plugins" / "examples";
    resonant::plugins::PluginManager manager(resonant::plugins::PluginManager::standardSearchPaths(examples),
                                             resonant::plugins::PluginManager::standardConfigFile(), resonant::kApplicationVersion);
    std::vector<std::string> capabilities;
    if (python)
        capabilities.push_back("python");
    manager.discover(capabilities);
    std::map<std::string, int> states;
    for (const auto& p : manager.plugins())
        ++states[std::string(resonant::plugins::toString(p.state))];
    QStringList parts;
    for (const auto& [state, count] : states)
        parts << QStringLiteral("%1 %2").arg(count).arg(QString::fromStdString(state));
    add(r, QStringLiteral("Plugins"), Status::Info,
        QStringLiteral("%1 discovered%2").arg(manager.plugins().size()).arg(parts.isEmpty() ? QString() : QStringLiteral(" (") + parts.join(QStringLiteral(", ")) + QLatin1Char(')')));
}

void checkUserDirectories(Report& r, const ResourcePaths& paths)
{
    const std::pair<const char*, QString> dirs[] = {{"configuration", paths.userConfig}, {"data", paths.userData}, {"cache", paths.userCache}, {"documents", paths.userDocuments}};
    QStringList shownDirs, insideInstall;
    for (const auto& [name, dir] : dirs) {
        shownDirs << QStringLiteral("%1 %2").arg(QString::fromLatin1(name), shown(dir));
        if (inside(dir, paths.installRoot))
            insideInstall << QString::fromLatin1(name);
    }
    add(r, QStringLiteral("User directories"), insideInstall.isEmpty() ? Status::Pass : Status::Fail,
        insideInstall.isEmpty() ? shownDirs.join(QStringLiteral("; "))
                                : QStringLiteral("inside the installation (%1): %2").arg(insideInstall.join(QStringLiteral(", ")), shownDirs.join(QStringLiteral("; "))));
}

void checkIntegrity(Report& r, const ResourcePaths& paths)
{
    if (!paths.isBundle())
        return;
    int checked = 0;
    const QStringList problems = verifyBundleFiles(paths.installRoot, 10, &checked);
    add(r, QStringLiteral("Package files"), problems.isEmpty() && checked > 0 ? Status::Pass : Status::Fail,
        problems.isEmpty() ? (checked > 0 ? QStringLiteral("%1 files match diagnostics/FILES.sha256").arg(checked) : QStringLiteral("diagnostics/FILES.sha256 is missing or empty"))
                           : QStringLiteral("the package is incomplete or damaged (extract the ZIP again): %1").arg(problems.join(QStringLiteral("; "))));
}

void checkEnvironment(Report& r)
{
    static const char* const names[] = {"LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM", "QT_QPA_PLATFORM_PLUGIN_PATH", "PYTHONHOME", "PYTHONPATH",
                                        "RESONANT_PYTHON", "RESONANT_HELP_DIR", "RESONANT_RESOURCE_ROOT", "RESONANT_PLUGIN_PATH", "LIBGL_ALWAYS_SOFTWARE"};
    QStringList set;
    for (const char* name : names)
        if (qEnvironmentVariableIsSet(name))
            set << QString::fromLatin1(name);
    const bool preload = qEnvironmentVariableIsSet("LD_PRELOAD");
    add(r, QStringLiteral("Environment"), preload ? Status::Warn : Status::Info,
        set.isEmpty() ? QStringLiteral("no variable that changes how Resonant loads is set")
                      : QStringLiteral("set (values not shown): %1%2").arg(set.join(QStringLiteral(", ")), preload ? QStringLiteral("; LD_PRELOAD can break the package") : QString()));
}

}  // namespace

int Report::failures() const
{
    return static_cast<int>(std::count_if(checks.begin(), checks.end(), [](const Check& c) { return c.status == Status::Fail; }));
}

int Report::warnings() const
{
    return static_cast<int>(std::count_if(checks.begin(), checks.end(), [](const Check& c) { return c.status == Status::Warn; }));
}

QString statusName(Status status)
{
    switch (status) {
    case Status::Pass:
        return QStringLiteral("PASS");
    case Status::Warn:
        return QStringLiteral("WARN");
    case Status::Fail:
        return QStringLiteral("FAIL");
    case Status::Info:
        break;
    }
    return QStringLiteral("INFO");
}

QString redact(const QString& text)
{
    const QString home = QDir::cleanPath(QDir::homePath());
    if (home.isEmpty() || home == QLatin1String("/"))
        return text;
    QString out = text;
    out.replace(home + QLatin1Char('/'), QStringLiteral("~/"));
    if (out == home)
        out = QStringLiteral("~");
    return out;
}

QStringList verifyBundleFiles(const QString& root, int limit, int* checked)
{
    QStringList problems;
    if (checked)
        *checked = 0;
    QFile list(QDir(root).absoluteFilePath(QStringLiteral("diagnostics/FILES.sha256")));
    if (!list.open(QIODevice::ReadOnly | QIODevice::Text))
        return {QStringLiteral("diagnostics/FILES.sha256 cannot be read")};
    while (!list.atEnd()) {
        const QString line = QString::fromUtf8(list.readLine()).trimmed();
        if (line.size() < 67 || line.at(64) != QLatin1Char(' '))
            continue;
        const QString expected = line.left(64);
        const QString name = line.mid(66);
        QFile f(QDir(root).absoluteFilePath(name));
        QString problem;
        if (!f.open(QIODevice::ReadOnly))
            problem = QStringLiteral("%1 is missing").arg(name);
        else {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(&f);
            if (QString::fromLatin1(hash.result().toHex()) != expected)
                problem = QStringLiteral("%1 has changed").arg(name);
        }
        if (checked)
            ++*checked;
        if (!problem.isEmpty() && problems.size() < limit)
            problems << problem;
    }
    return problems;
}

Report run(const ResourcePaths& paths, const Options& options)
{
    Report r;
    const QJsonObject manifest = readManifest(paths.runtimeManifest);
    const QStringList libraries = loadedLibraries();
    checkPackage(r, paths, manifest);
    checkSystem(r, manifest);
    checkQt(r, paths, libraries);
    if (options.openGl)
        checkOpenGl(r);
    if (options.python)
        checkPython(r, paths);
    checkResources(r, paths);
    checkComputeAndPlugins(r, paths, options.python && pythonLaunch(paths).usable());
    checkUserDirectories(r, paths);
    if (options.verifyFiles)
        checkIntegrity(r, paths);
    checkEnvironment(r);
    if (options.libraries)
        for (const QString& lib : libraries)
            r.libraries << QStringLiteral("%1 [%2]").arg(shown(lib), paths.isBundle() && inside(lib, paths.installRoot) ? QStringLiteral("package") : QStringLiteral("system"));
    return r;
}

QString format(const Report& report, bool details)
{
    QString out;
    for (const Check& c : report.checks)
        out += QStringLiteral("%1  %2: %3\n").arg(statusName(c.status), c.name, c.detail);
    if (details && !report.libraries.isEmpty()) {
        out += QStringLiteral("\nLoaded libraries (%1):\n").arg(report.libraries.size());
        for (const QString& lib : report.libraries)
            out += QStringLiteral("  %1\n").arg(lib);
    }
    out += QStringLiteral("\nResult: %1 failed, %2 warning(s)\n").arg(report.failures()).arg(report.warnings());
    return out;
}

}  // namespace runtime_diagnostics
