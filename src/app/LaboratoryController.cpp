#include "LaboratoryController.h"

#include "DocumentController.h"

#include "services/Services.h"
#include "simulation/Job.h"

using nlohmann::json;
using resonant::Result;


LaboratoryController::LaboratoryController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::projectReplaced, this, &LaboratoryController::modelChanged);
    connect(document_, &DocumentController::documentStateChanged, this, &LaboratoryController::modelChanged);
    connect(document_, &DocumentController::jobChanged, this, [this](const resonant::JobInfo& info) {
        const QString id = QString::fromStdString(info.id.str());
        if (!pending_.contains(id) || !resonant::isFinished(info.state))
            return;
        pending_.remove(id);
        auto view = document_->services().jobs.get(DocumentController::guiContext(), info.id.str());
        if (!view || view.value().state != "completed" || view.value().resultIds.empty()) {
            const std::string why = view && view.value().error ? view.value().error->message : info.message;
            emit calibrationFailed(QString::fromStdString(why.empty() ? std::string("the calibration did not complete") : why));
            return;
        }
        auto r = document_->services().laboratory.calibrationResult(DocumentController::guiContext(), view.value().resultIds[0]);
        emit calibrationReady(QString::fromStdString(view.value().resultIds[0]), r ? r.value() : json::object());
    });
}

json LaboratoryController::studies() const
{
    auto r = document_->services().laboratory.studies(DocumentController::guiContext());
    return r ? r.value() : json::array();
}

json LaboratoryController::models() const
{
    auto r = document_->services().laboratory.models(DocumentController::guiContext());
    return r ? r.value() : json::array();
}

json LaboratoryController::modelParameters(const QString& model, const json& configuration) const
{
    auto r = document_->services().laboratory.modelParameters(DocumentController::guiContext(), model.toStdString(), configuration);
    return r ? r.value() : json::array();
}

json LaboratoryController::datasets() const
{
    auto r = document_->services().laboratory.datasets(DocumentController::guiContext());
    return r ? r.value() : json::array();
}

json LaboratoryController::dataset(const QString& id, int maxPoints) const
{
    auto r = document_->services().laboratory.dataset(DocumentController::guiContext(), id.toStdString(), maxPoints);
    return r ? r.value() : json::object();
}

json LaboratoryController::records(const QString& section) const
{
    auto r = document_->services().laboratory.records(DocumentController::guiContext(), section.toStdString());
    return r ? r.value() : json::array();
}

Result<json> LaboratoryController::importFile(const QString& path, const QString& study, const QString& role)
{
    json spec{{"path", path.toStdString()}, {"role", role.toStdString()}};
    if (!study.isEmpty())
        spec["study"] = study.toStdString();
    auto r = document_->services().laboratory.importDataset(DocumentController::guiContext(), spec);
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<json> LaboratoryController::compare(const json& spec)
{
    auto r = document_->services().laboratory.compare(DocumentController::guiContext(), spec);
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<QString> LaboratoryController::calibrate(const json& spec)
{
    auto job = document_->services().laboratory.calibrate(DocumentController::guiContext(), spec);
    if (!job)
        return Result<QString>::failure(job.error().message);
    const QString id = QString::fromStdString(job.value().id);
    pending_.insert(id);
    return id;
}

Result<json> LaboratoryController::keep(const QString& resultId)
{
    auto r = document_->services().laboratory.acceptCalibration(DocumentController::guiContext(), resultId.toStdString());
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<json> LaboratoryController::validate(const QString& calibrationId, const json& datasets, const json& axisDatasets)
{
    auto r = document_->services().laboratory.validate(DocumentController::guiContext(), calibrationId.toStdString(),
                                                       datasets.get<std::vector<std::string>>(), "", axisDatasets.get<std::vector<std::string>>());
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<json> LaboratoryController::apply(const QString& calibrationId, const json& options)
{
    auto r = document_->services().laboratory.applyCalibration(DocumentController::guiContext(), calibrationId.toStdString(), options);
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<json> LaboratoryController::explain(const QString& recordId) const
{
    auto r = document_->services().laboratory.explain(DocumentController::guiContext(), recordId.toStdString());
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value();
}

Result<QString> LaboratoryController::report(const QString& studyId, const QString& format) const
{
    auto r = document_->services().laboratory.report(DocumentController::guiContext(), studyId.toStdString(), format.toStdString());
    if (!r)
        return Result<QString>::failure(r.error().message);
    return QString::fromStdString(r.value());
}
