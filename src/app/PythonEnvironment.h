// How Resonant starts Python (RES-DIST-008, ADR-000218): the console's and
// the script editor's worker, Python plugins and the gRPC bridge all go
// through pythonLaunch(), so they use the same interpreter and see the same
// packages.
//
// The interpreter is, in order: $RESONANT_PYTHON (an explicit choice), the
// bundled runtime of a Linux ZIP, `python3` on PATH. A bundled runtime is
// isolated from the user's environment: PYTHONHOME is the runtime,
// PYTHONPATH holds only the package's directories, the user site directory is
// off and no bytecode is written into the package (it is precompiled). Any
// other interpreter keeps the user's PYTHONPATH after the package.
#pragma once

#include "ResourcePaths.h"

#include <QProcessEnvironment>
#include <QString>

struct PythonLaunch {
    QString program;  // empty: no interpreter (see error)
    QProcessEnvironment environment;
    bool bundled = false;
    QString origin;   // "RESONANT_PYTHON", "bundled", "PATH"
    QString error;    // why there is no program, or why the package is missing

    bool usable() const { return !program.isEmpty(); }
};

PythonLaunch pythonLaunch(const ResourcePaths& paths, const QProcessEnvironment& base = QProcessEnvironment::systemEnvironment());
