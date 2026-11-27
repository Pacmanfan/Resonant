// Where the application finds its shared data (ADR-000215): the help, the
// demonstrations, the example projects, the example plugins and the Python
// package. Looked up, in order, from
//   1. environment variables (RESONANT_HELP_DIR, RESONANT_RESOURCE_ROOT,
//      RESONANT_PYTHON_PACKAGE_DIR, RESONANT_PYTHON_MODULE_DIR);
//   2. an installation: <prefix>/bin/resonant -> <prefix>/share/resonant/...;
//   3. a build tree: the executable's ancestors hold help/index.json (the
//      generated help) and the source tree is found from the build tree's
//      recorded source directory.
// An empty path means "not found"; callers say so to the user.
#pragma once

#include <QString>

struct ResourcePaths {
    QString help;          // directory with index.json, search.json, html/
    QString resourceRoot;  // directory with demos/, examples/, plugins/examples
    QString pythonPackage; // directory that contains the `resonant` package
    QString pythonModule;  // directory with the native module _resonant*.so (empty: in the package)

    // From the running executable (QCoreApplication::applicationDirPath()).
    static ResourcePaths locate();
    // From an explicit executable directory (tests: an installation layout).
    static ResourcePaths locateFrom(const QString& executableDirectory, bool useEnvironment = true);
};
