#include "PythonEnvironment.h"

#include <QDir>
#include <QStandardPaths>

namespace {
// Variables that would make the bundled interpreter load another Python's
// files or the user's packages.
const char* const kForeignPythonVariables[] = {"PYTHONHOME",       "PYTHONPATH",      "PYTHONSTARTUP", "PYTHONUSERBASE", "PYTHONEXECUTABLE",
                                               "PYTHONPLATLIBDIR", "PYTHONPYCACHEPREFIX", "PYTHONSAFEPATH", "VIRTUAL_ENV",    "CONDA_PREFIX"};
}  // namespace

PythonLaunch pythonLaunch(const ResourcePaths& paths, const QProcessEnvironment& base)
{
    PythonLaunch launch;
    launch.environment = base;
    const QString configured = base.value(QStringLiteral("RESONANT_PYTHON"));
    if (!configured.isEmpty()) {
        launch.program = configured;
        launch.origin = QStringLiteral("RESONANT_PYTHON");
    } else if (!paths.pythonInterpreter.isEmpty()) {
        launch.program = paths.pythonInterpreter;
        launch.bundled = true;
        launch.origin = QStringLiteral("bundled");
    } else {
        launch.program = QStandardPaths::findExecutable(QStringLiteral("python3"));
        launch.origin = QStringLiteral("PATH");
        if (launch.program.isEmpty())
            launch.error = QStringLiteral("Python 3 was not found (install python3, or set RESONANT_PYTHON to the interpreter).");
    }
    if (paths.pythonPackage.isEmpty() && launch.error.isEmpty())
        launch.error = QStringLiteral("The resonant Python package was not found (set RESONANT_PYTHON_PACKAGE_DIR to the directory that contains it).");

    QProcessEnvironment& env = launch.environment;
    if (launch.bundled) {
        for (const char* name : kForeignPythonVariables)
            env.remove(QString::fromLatin1(name));
        env.insert(QStringLiteral("PYTHONHOME"), paths.pythonHome);
        QStringList path;
        if (!paths.pythonPackage.isEmpty())
            path << paths.pythonPackage;
        if (!paths.pythonSitePackages.isEmpty())
            path << paths.pythonSitePackages;
        env.insert(QStringLiteral("PYTHONPATH"), path.join(QDir::listSeparator()));
        env.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
        env.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
    } else if (!paths.pythonPackage.isEmpty()) {
        const QString existing = env.value(QStringLiteral("PYTHONPATH"));
        env.insert(QStringLiteral("PYTHONPATH"), existing.isEmpty() ? paths.pythonPackage : paths.pythonPackage + QDir::listSeparator() + existing);
    }
    if (!paths.pythonModule.isEmpty())
        env.insert(QStringLiteral("RESONANT_PYTHON_MODULE_DIR"), paths.pythonModule);
    return launch;
}
