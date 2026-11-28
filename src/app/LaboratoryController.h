// The application side of the laboratory (REQ-CHAT-20261126-023, RES-LAB-017): the
// Laboratory panel's access to LaboratoryService through the GUI's own session, and the
// calibration jobs it started. No physics here.
#pragma once

#include "foundation/Result.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <nlohmann/json.hpp>

class DocumentController;

class LaboratoryController : public QObject {
    Q_OBJECT
public:
    explicit LaboratoryController(DocumentController* document, QObject* parent = nullptr);

    nlohmann::json studies() const;
    nlohmann::json models() const;
    nlohmann::json modelParameters(const QString& model, const nlohmann::json& configuration) const;
    nlohmann::json datasets() const;
    nlohmann::json dataset(const QString& id, int maxPoints) const;
    nlohmann::json records(const QString& section) const;
    // Imports a file (a path relative to the project, or absolute) for a study and a role; the import report.
    resonant::Result<nlohmann::json> importFile(const QString& path, const QString& study, const QString& role);
    resonant::Result<nlohmann::json> compare(const nlohmann::json& spec);
    resonant::Result<QString> calibrate(const nlohmann::json& spec);  // the job id
    resonant::Result<nlohmann::json> keep(const QString& resultId);
    resonant::Result<nlohmann::json> validate(const QString& calibrationId, const nlohmann::json& datasets, const nlohmann::json& axisDatasets);
    resonant::Result<nlohmann::json> apply(const QString& calibrationId, const nlohmann::json& options);
    resonant::Result<nlohmann::json> explain(const QString& recordId) const;
    resonant::Result<QString> report(const QString& studyId, const QString& format) const;
    bool busy() const { return !pending_.isEmpty(); }

signals:
    void modelChanged();
    void calibrationReady(const QString& resultId, const nlohmann::json& result);
    void calibrationFailed(const QString& message);

private:
    DocumentController* document_;
    QSet<QString> pending_;
};
