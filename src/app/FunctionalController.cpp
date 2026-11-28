#include "FunctionalController.h"

#include "DocumentController.h"

#include "services/Services.h"
#include "simulation/Job.h"

using nlohmann::json;
using resonant::Result;
using resonant::Status;

FunctionalController::FunctionalController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::projectReplaced, this, &FunctionalController::structuresChanged);
    connect(document_, &DocumentController::documentStateChanged, this, &FunctionalController::structuresChanged);
    // Registered after the document controller's own handler, which collects finished jobs first.
    connect(document_, &DocumentController::jobChanged, this, [this](const resonant::JobInfo& info) {
        const QString id = QString::fromStdString(info.id.str());
        if (!pending_.contains(id) || !resonant::isFinished(info.state))
            return;
        pending_.remove(id);
        auto view = document_->services().jobs.get(DocumentController::guiContext(), info.id.str());
        if (!view || view.value().state != "completed" || view.value().resultIds.empty()) {
            const std::string why = view && view.value().error ? view.value().error->message : info.message;
            emit studyFailed(QString::fromStdString(why.empty() ? std::string("the study did not complete") : why));
            return;
        }
        const json r = result(QString::fromStdString(view.value().resultIds[0]));
        emit resultReady(QString::fromStdString(view.value().resultIds[0]), QString::fromStdString(r.value("kind", "")));
    });
}

json FunctionalController::structures() const
{
    auto list = document_->services().functional.structures(DocumentController::guiContext());
    json out = json::array();
    if (list)
        for (const json& s : list.value())
            out.push_back(s);
    return out;
}

json FunctionalController::structure(const QString& id) const
{
    auto s = document_->services().functional.structure(DocumentController::guiContext(), id.toStdString());
    return s ? s.value() : json(nullptr);
}

json FunctionalController::analysisKind(const QString& kindId) const
{
    auto kinds = document_->services().functional.analysisKinds(DocumentController::guiContext());
    if (kinds)
        for (const json& k : kinds.value())
            if (k.value("id", "") == kindId.toStdString())
                return k;
    return json::object();
}

json FunctionalController::parameters(const QString& id) const
{
    auto p = document_->services().functional.parameters(DocumentController::guiContext(), id.toStdString());
    return p ? p.value() : json::array();
}

Status FunctionalController::setParameter(const QString& id, const QString& parameter, double value)
{
    auto m = document_->services().functional.setParameters(DocumentController::guiContext(), id.toStdString(), {{parameter.toStdString(), value}},
                                                             std::nullopt);
    if (!m)
        return Status::failure(m.error().message);
    return Status::success();
}

Result<QString> FunctionalController::analyze(const QString& id, const QString& model)
{
    auto job = document_->services().functional.analyze(DocumentController::guiContext(), id.toStdString(), model.toStdString());
    if (!job)
        return Result<QString>::failure(job.error().message);
    const QString jobId = QString::fromStdString(job.value().id);
    pending_.insert(jobId);
    return jobId;
}

Result<QString> FunctionalController::sweep(const QString& id, const QString& parameter, double from, double to, int points, const QString& model)
{
    const json spec{{"axes", {{{"parameter", parameter.toStdString()}, {"from", from}, {"to", to}, {"points", points}}}}, {"model", model.toStdString()}};
    auto job = document_->services().functional.sweep(DocumentController::guiContext(), id.toStdString(), spec);
    if (!job)
        return Result<QString>::failure(job.error().message);
    const QString jobId = QString::fromStdString(job.value().id);
    pending_.insert(jobId);
    return jobId;
}

json FunctionalController::result(const QString& resultId) const
{
    auto r = document_->services().functional.result(DocumentController::guiContext(), resultId.toStdString());
    return r ? r.value() : json::object();
}
