// Where the application finds its read-only resources and where the user's
// writable data goes (ADR-000215, ADR-000218, RES-DIST-004): the help, the
// demonstrations, the example projects, the example plugins, the Python
// package, a bundled Python runtime and the automation descriptor set.
// Looked up, in order, from
//   1. environment variables (RESONANT_HELP_DIR, RESONANT_RESOURCE_ROOT,
//      RESONANT_PYTHON_PACKAGE_DIR, RESONANT_PYTHON_MODULE_DIR, RESONANT_API_DESCRIPTORS);
//   2. a bundle (the Linux ZIP): <root>/bin/resonant beside <root>/resonant-runtime.json,
//      with the resources at the top of <root>;
//   3. an installation: <prefix>/bin/resonant -> <prefix>/share/resonant/...;
//   4. a build tree: the executable's ancestors hold help/index.json (the
//      generated help) and the source tree is found from the build tree's
//      recorded source directory (never in a bundle).
// An empty path means "not found"; callers say so to the user. The user's
// directories are the XDG ones (QStandardPaths) and are never inside the
// installation: nothing is written into a bundle or a prefix.
#pragma once

#include <QString>
#include <QStringList>

struct ResourcePaths {
    enum class Layout { Unknown, BuildTree, Installation, Bundle };

    Layout layout = Layout::Unknown;
    QString executableDirectory;
    QString installRoot;      // the bundle's root or the installation prefix (empty for a build tree)
    QString runtimeManifest;  // <root>/resonant-runtime.json (bundles only)

    // Read-only resources.
    QString help;          // directory with index.json, search.json, html/
    QString resourceRoot;  // directory with demos/, examples/, plugins/examples
    QString pythonPackage; // directory that contains the `resonant` package
    QString pythonModule;  // directory with the native module _resonant*.so (empty: in the package)
    QString apiDescriptors; // the automation API's descriptor set resonant_api.desc (the gRPC bridge reads it)
    QString pythonHome;          // a bundled Python runtime (bin/python3, lib/python3.x); empty: none
    QString pythonInterpreter;   // that runtime's interpreter
    QString pythonSitePackages;  // the bundle's third-party Python packages (NumPy, protobuf, grpcio)

    // The user's writable directories (not created here).
    QString userConfig;     // ~/.config/Resonant/Resonant (settings, plugins.json beside it)
    QString userData;       // ~/.local/share/Resonant/Resonant (scripts, demonstration runs)
    QString userCache;      // ~/.cache/Resonant/Resonant
    QString userDocuments;  // where projects and exports are proposed (~/Documents)

    bool isBundle() const { return layout == Layout::Bundle; }
    static QString layoutName(Layout layout);

    // From the running executable (QCoreApplication::applicationDirPath()).
    static ResourcePaths locate();
    // From an explicit executable directory (tests: an installation or bundle layout).
    static ResourcePaths locateFrom(const QString& executableDirectory, bool useEnvironment = true);
};
