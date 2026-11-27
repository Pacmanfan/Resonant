// Runtime diagnostics (RES-DIST-006, ADR-000218): what `resonant
// --check-runtime` and `resonant --diagnostics` report. The checks need a
// QApplication (the platform plugin is one of them) but no main window.
//
// A report is for the user to read and, if they choose, to share: the home
// directory is shown as "~", environment variables by name only, and no
// project content, credential or setting value is included. Nothing is sent
// anywhere.
#pragma once

#include "ResourcePaths.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace runtime_diagnostics {

enum class Status { Pass, Warn, Fail, Info };

struct Check {
    QString name;
    Status status = Status::Info;
    QString detail;
};

struct Options {
    bool openGl = true;       // create an OpenGL 4.5 core context (needs a display)
    bool python = true;       // start the interpreter and import resonant
    bool verifyFiles = true;  // a bundle's files against diagnostics/FILES.sha256
    bool libraries = false;   // list every loaded shared library (--diagnostics)
};

struct Report {
    QList<Check> checks;
    QStringList libraries;  // "<path> [bundle|system]" when Options::libraries
    int failures() const;
    int warnings() const;
};

Report run(const ResourcePaths& paths, const Options& options);
QString format(const Report& report, bool details);
QString statusName(Status status);

// The home directory as "~" (and nothing else changed).
QString redact(const QString& text);

// A bundle's integrity: every file listed in <root>/diagnostics/FILES.sha256
// exists and has its checksum. Problems (at most `limit`) are returned.
QStringList verifyBundleFiles(const QString& root, int limit = 20, int* checked = nullptr);

}  // namespace runtime_diagnostics
