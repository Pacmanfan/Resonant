// The application side of dynamic material state (REQ-CHAT-20261126-021, RES-DMS-014): the
// Material State panel's access to MaterialStateService through the GUI's own session, and the
// runs it started. No physics here.
#pragma once

#include "foundation/Result.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <nlohmann/json.hpp>

class DocumentController;

class MaterialStateController : public QObject {
    Q_OBJECT
public:
    explicit MaterialStateController(DocumentController* document, QObject* parent = nullptr);

    nlohmann::json models() const;
    nlohmann::json studies() const;
    nlohmann::json records() const;
    nlohmann::json materials() const;  // [{id, name}]
    // A stored study (id) or an inline one.
    resonant::Result<QString> run(const QString& studyId, const nlohmann::json& study);
    nlohmann::json result(const QString& resultId) const;
    resonant::Result<QString> saveRecord(const QString& resultId, const QString& name);  // the record's id
    resonant::Status commit(const QString& recordId, const QString& materialId);
    bool busy() const { return !pending_.isEmpty(); }

signals:
    void modelChanged();
    void resultReady(const QString& resultId);
    void runFailed(const QString& message);

private:
    DocumentController* document_;
    QSet<QString> pending_;
};
