// The application side of functional structures (REQ-CHAT-20261126-017,
// RES-FUNC-015): the panel's access to FunctionalStructureService through the
// GUI's own session, and the jobs it started. No physics here: every value
// comes from the services.
#pragma once

#include "foundation/Result.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

class DocumentController;

class FunctionalController : public QObject {
    Q_OBJECT
public:
    explicit FunctionalController(DocumentController* document, QObject* parent = nullptr);

    // The structures of the project: [{id, name, analysis, ...}].
    nlohmann::json structures() const;
    nlohmann::json structure(const QString& id) const;
    nlohmann::json analysisKind(const QString& kindId) const;
    nlohmann::json parameters(const QString& id) const;
    // An undoable change of one parameter (bound: the scene object; free: the structure).
    resonant::Status setParameter(const QString& id, const QString& parameter, double value);
    // Jobs; the result arrives through resultReady.
    resonant::Result<QString> analyze(const QString& id, const QString& model);
    resonant::Result<QString> sweep(const QString& id, const QString& parameter, double from, double to, int points, const QString& model);
    nlohmann::json result(const QString& resultId) const;
    bool busy() const { return !pending_.isEmpty(); }

signals:
    void structuresChanged();
    void resultReady(const QString& resultId, const QString& kind);
    void studyFailed(const QString& message);

private:
    DocumentController* document_;
    QSet<QString> pending_;
};
