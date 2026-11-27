// Runtime paths, the bundled Python's isolation and the diagnostics of the
// Linux ZIP (RES-DIST-004, RES-DIST-006, RES-DIST-008, ADR-000218).
#include "HelpWindow.h"
#include "PythonEnvironment.h"
#include "ResourcePaths.h"
#include "RuntimeDiagnostics.h"

#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>

#include <gtest/gtest.h>

namespace {

void touch(const QString& path, const QByteArray& content = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(content);
}

// A minimal bundle as the packager lays it out: <root>/bin beside resonant-runtime.json.
QString makeBundle(const QString& root)
{
    for (const char* file : {"bin/resonant", "resonant-runtime.json", "help/index.json", "demos/profiles/a.json", "examples/x.resonant",
                             "python/resonant/__init__.py", "python/runtime/bin/python3", "python/site-packages/numpy/__init__.py",
                             "automation/resonant_api.desc"})
        touch(root + QLatin1Char('/') + QString::fromLatin1(file));
    return root + QStringLiteral("/bin");
}

bool inside(const QString& path, const QString& root)
{
    return QDir::cleanPath(path).startsWith(QDir::cleanPath(root) + QLatin1Char('/'));
}

}  // namespace

TEST(RuntimePathsTest, ABundleIsFoundFromItsExecutableWhereverItIs)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString root = tmp.path() + QStringLiteral("/folder with spaces/RESONANT-Linux-x86_64-0.1.0");
    const ResourcePaths p = ResourcePaths::locateFrom(makeBundle(root), false);
    EXPECT_EQ(p.layout, ResourcePaths::Layout::Bundle);
    EXPECT_EQ(p.installRoot, QDir(root).absolutePath());
    EXPECT_EQ(p.runtimeManifest, root + QStringLiteral("/resonant-runtime.json"));
    EXPECT_EQ(p.help, root + QStringLiteral("/help"));
    EXPECT_EQ(p.resourceRoot, root);
    EXPECT_EQ(p.pythonPackage, root + QStringLiteral("/python"));
    EXPECT_EQ(p.pythonHome, root + QStringLiteral("/python/runtime"));
    EXPECT_EQ(p.pythonInterpreter, root + QStringLiteral("/python/runtime/bin/python3"));
    EXPECT_EQ(p.pythonSitePackages, root + QStringLiteral("/python/site-packages"));
    EXPECT_EQ(p.apiDescriptors, root + QStringLiteral("/automation/resonant_api.desc"));
    EXPECT_TRUE(p.pythonModule.isEmpty());

    // Renamed and moved: everything follows the executable.
    const QString moved = tmp.path() + QStringLiteral("/elsewhere/Resonant renamed");
    ASSERT_TRUE(QDir().mkpath(tmp.path() + QStringLiteral("/elsewhere")));
    ASSERT_TRUE(QDir().rename(root, moved));
    const ResourcePaths q = ResourcePaths::locateFrom(moved + QStringLiteral("/bin"), false);
    EXPECT_EQ(q.layout, ResourcePaths::Layout::Bundle);
    EXPECT_EQ(q.help, moved + QStringLiteral("/help"));
    EXPECT_EQ(q.pythonInterpreter, moved + QStringLiteral("/python/runtime/bin/python3"));
}

TEST(RuntimePathsTest, ABundleNeverLooksOutsideItself)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    // A help folder above the bundle (as in a build tree) must not be picked up.
    touch(tmp.path() + QStringLiteral("/help/index.json"));
    touch(tmp.path() + QStringLiteral("/src/python/_resonant.cpython-312-x86_64-linux-gnu.so"));
    const QString root = tmp.path() + QStringLiteral("/pkg");
    touch(root + QStringLiteral("/bin/resonant"));
    touch(root + QStringLiteral("/resonant-runtime.json"));
    const ResourcePaths p = ResourcePaths::locateFrom(root + QStringLiteral("/bin"), false);
    EXPECT_EQ(p.layout, ResourcePaths::Layout::Bundle);
    EXPECT_TRUE(p.help.isEmpty()) << p.help.toStdString();
    EXPECT_TRUE(p.pythonModule.isEmpty()) << p.pythonModule.toStdString();
    EXPECT_TRUE(p.resourceRoot.isEmpty());
}

TEST(RuntimePathsTest, AnInstallationUsesShareResonant)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    touch(tmp.path() + QStringLiteral("/bin/resonant"));
    touch(tmp.path() + QStringLiteral("/share/resonant/help/index.json"));
    touch(tmp.path() + QStringLiteral("/share/resonant/python/resonant/__init__.py"));
    const ResourcePaths p = ResourcePaths::locateFrom(tmp.path() + QStringLiteral("/bin"), false);
    EXPECT_EQ(p.layout, ResourcePaths::Layout::Installation);
    EXPECT_EQ(p.help, tmp.path() + QStringLiteral("/share/resonant/help"));
    EXPECT_EQ(p.pythonPackage, tmp.path() + QStringLiteral("/share/resonant/python"));
    EXPECT_TRUE(p.pythonInterpreter.isEmpty());
}

TEST(RuntimePathsTest, UserDirectoriesAreNeverInsideTheBundle)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString root = tmp.path() + QStringLiteral("/RESONANT");
    const ResourcePaths p = ResourcePaths::locateFrom(makeBundle(root), false);
    for (const QString& dir : {p.userConfig, p.userData, p.userCache, p.userDocuments}) {
        EXPECT_FALSE(dir.isEmpty());
        EXPECT_FALSE(inside(dir, root)) << dir.toStdString();
    }
}

TEST(RuntimePathsTest, TheBundledPythonIsIsolated)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString root = tmp.path() + QStringLiteral("/RESONANT");
    const ResourcePaths p = ResourcePaths::locateFrom(makeBundle(root), false);
    QProcessEnvironment base;
    base.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
    base.insert(QStringLiteral("PYTHONPATH"), QStringLiteral("/foreign/site"));
    base.insert(QStringLiteral("PYTHONHOME"), QStringLiteral("/foreign"));
    base.insert(QStringLiteral("PYTHONSTARTUP"), QStringLiteral("/foreign/startup.py"));
    base.insert(QStringLiteral("VIRTUAL_ENV"), QStringLiteral("/foreign/venv"));
    const PythonLaunch launch = pythonLaunch(p, base);
    EXPECT_TRUE(launch.bundled);
    EXPECT_TRUE(launch.error.isEmpty()) << launch.error.toStdString();
    EXPECT_EQ(launch.program, p.pythonInterpreter);
    const QProcessEnvironment& env = launch.environment;
    EXPECT_EQ(env.value(QStringLiteral("PYTHONHOME")), p.pythonHome);
    EXPECT_EQ(env.value(QStringLiteral("PYTHONPATH")), p.pythonPackage + QLatin1Char(':') + p.pythonSitePackages);
    EXPECT_EQ(env.value(QStringLiteral("PYTHONNOUSERSITE")), QStringLiteral("1"));
    EXPECT_EQ(env.value(QStringLiteral("PYTHONDONTWRITEBYTECODE")), QStringLiteral("1"));
    EXPECT_FALSE(env.contains(QStringLiteral("PYTHONSTARTUP")));
    EXPECT_FALSE(env.contains(QStringLiteral("VIRTUAL_ENV")));
    EXPECT_EQ(env.value(QStringLiteral("PATH")), QStringLiteral("/usr/bin"));  // the rest of the environment is kept
}

TEST(RuntimePathsTest, AnExplicitInterpreterWinsAndKeepsTheUsersPath)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const ResourcePaths p = ResourcePaths::locateFrom(makeBundle(tmp.path() + QStringLiteral("/R")), false);
    QProcessEnvironment base;
    base.insert(QStringLiteral("RESONANT_PYTHON"), QStringLiteral("/opt/python/bin/python3"));
    base.insert(QStringLiteral("PYTHONPATH"), QStringLiteral("/mine"));
    const PythonLaunch launch = pythonLaunch(p, base);
    EXPECT_FALSE(launch.bundled);
    EXPECT_EQ(launch.origin, QStringLiteral("RESONANT_PYTHON"));
    EXPECT_EQ(launch.program, QStringLiteral("/opt/python/bin/python3"));
    EXPECT_EQ(launch.environment.value(QStringLiteral("PYTHONPATH")), p.pythonPackage + QStringLiteral(":/mine"));
    EXPECT_FALSE(launch.environment.contains(QStringLiteral("PYTHONHOME")));
}

TEST(RuntimePathsTest, TheDiagnosticReportRedactsTheHomeDirectoryAndEnvironmentValues)
{
    EXPECT_EQ(runtime_diagnostics::redact(QDir::homePath() + QStringLiteral("/projects/x")), QStringLiteral("~/projects/x"));
    EXPECT_EQ(runtime_diagnostics::redact(QStringLiteral("/opt/resonant")), QStringLiteral("/opt/resonant"));
    const QByteArray saved = qgetenv("LD_PRELOAD");
    qputenv("LD_PRELOAD", "/secret/probe-value.so");
    runtime_diagnostics::Options options;
    options.openGl = false;
    options.python = false;
    options.verifyFiles = false;
    options.libraries = true;
    const runtime_diagnostics::Report report = runtime_diagnostics::run(ResourcePaths::locate(), options);
    if (saved.isEmpty())
        qunsetenv("LD_PRELOAD");
    else
        qputenv("LD_PRELOAD", saved);
    const QString text = runtime_diagnostics::format(report, true);
    EXPECT_TRUE(text.contains(QStringLiteral("LD_PRELOAD"))) << text.toStdString();
    EXPECT_FALSE(text.contains(QStringLiteral("probe-value"))) << text.toStdString();
    EXPECT_FALSE(text.contains(QDir::homePath() + QLatin1Char('/'))) << text.toStdString();
    EXPECT_TRUE(text.contains(QStringLiteral("Loaded libraries")));
    EXPECT_TRUE(text.contains(QStringLiteral("Help")));
}

TEST(RuntimePathsTest, BundleIntegrityFindsChangedAndMissingFiles)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString root = tmp.path();
    touch(root + QStringLiteral("/a.txt"), "alpha");
    touch(root + QStringLiteral("/dir with space/b.bin"), "beta");
    QByteArray list;
    for (const char* name : {"a.txt", "dir with space/b.bin"}) {
        QFile f(root + QLatin1Char('/') + QString::fromLatin1(name));
        ASSERT_TRUE(f.open(QIODevice::ReadOnly));
        list += QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex() + "  " + name + "\n";
    }
    touch(root + QStringLiteral("/diagnostics/FILES.sha256"), list);
    int checked = 0;
    EXPECT_TRUE(runtime_diagnostics::verifyBundleFiles(root, 20, &checked).isEmpty());
    EXPECT_EQ(checked, 2);
    touch(root + QStringLiteral("/a.txt"), "changed");
    QFile::remove(root + QStringLiteral("/dir with space/b.bin"));
    const QStringList problems = runtime_diagnostics::verifyBundleFiles(root);
    ASSERT_EQ(problems.size(), 2);
    EXPECT_TRUE(problems[0].contains(QStringLiteral("a.txt has changed")));
    EXPECT_TRUE(problems[1].contains(QStringLiteral("b.bin is missing")));
}

TEST(RuntimePathsTest, AScreenshotLinkInTheHelpOpensItFullSize)
{
    HelpWindow help;
    ASSERT_TRUE(help.load(ResourcePaths::locate().help)) << help.loadError().toStdString();
    ASSERT_TRUE(help.showTopic(QStringLiteral("ui.main-window")));
    // The page links its screenshot as a plain relative image (a browser shows it full size).
    EXPECT_TRUE(help.articleHtml().contains(QStringLiteral("href=\"images/screenshots/main-window-overview.png\"")));
    auto* view = help.findChild<QTextBrowser*>();
    ASSERT_NE(view, nullptr);
    emit view->anchorClicked(QUrl(QStringLiteral("images/screenshots/main-window-overview.png")));
    auto* viewer = help.findChild<QDialog*>();
    ASSERT_NE(viewer, nullptr);
    EXPECT_TRUE(viewer->isVisible());
    viewer->close();
}
