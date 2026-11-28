// The application side of unified accounting (REQ-CHAT-20261126-022, RES-ACC-016): the
// Accounting panel's access to AccountingService through the GUI's own session, and the
// runs it started. No physics and no accounting here.
#pragma once

#include "foundation/Result.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <nlohmann/json.hpp>

class DocumentController;

class AccountingController : public QObject {
    Q_OBJECT
public:
    explicit AccountingController(DocumentController* document, QObject* parent = nullptr);

    nlohmann::json studies() const;
    resonant::Result<QString> run(const QString& studyId);
    nlohmann::json result(const QString& resultId) const;
    // The report for a system ({"id", "members"}; empty: the whole system), without running again.
    resonant::Result<nlohmann::json> report(const QString& resultId, const nlohmann::json& system) const;
    resonant::Result<QString> exportResult(const QString& resultId, const QString& format) const;
    resonant::Result<QString> saveRecord(const QString& resultId, const QString& name);
    nlohmann::json records() const;
    bool busy() const { return !pending_.isEmpty(); }

signals:
    void modelChanged();
    void resultReady(const QString& resultId);
    void runFailed(const QString& message);

private:
    DocumentController* document_;
    QSet<QString> pending_;
};
