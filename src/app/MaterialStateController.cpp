#include "MaterialStateController.h"

#include "DocumentController.h"

#include "services/Services.h"
#include "simulation/Job.h"

using nlohmann::json;
using resonant::Result;
using resonant::Status;

MaterialStateController::MaterialStateController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::projectReplaced, this, &MaterialStateController::modelChanged);
    connect(document_, &DocumentController::documentStateChanged, this, &MaterialStateController::modelChanged);
    connect(document_, &DocumentController::jobChanged, this, [this](const resonant::JobInfo& info) {
        const QString id = QString::fromStdString(info.id.str());
        if (!pending_.contains(id) || !resonant::isFinished(info.state))
            return;
        pending_.remove(id);
        auto view = document_->services().jobs.get(DocumentController::guiContext(), info.id.str());
        if (!view || view.value().state != "completed" || view.value().resultIds.empty()) {
            const std::string why = view && view.value().error ? view.value().error->message : info.message;
            emit runFailed(QString::fromStdString(why.empty() ? std::string("the run did not complete") : why));
            return;
        }
        emit resultReady(QString::fromStdString(view.value().resultIds[0]));
    });
}

json MaterialStateController::models() const
{
    auto m = document_->services().materialState.models(DocumentController::guiContext());
    return m ? m.value() : json::array();
}

json MaterialStateController::studies() const
{
    auto m = document_->services().materialState.studies(DocumentController::guiContext());
    return m ? m.value() : json::array();
}

json MaterialStateController::records() const
{
    json out = json::array();
    if (auto list = document_->services().materialState.records(DocumentController::guiContext()))
        for (const json& r : list.value())
            out.push_back(r);
    return out;
}

json MaterialStateController::materials() const
{
    json out = json::array();
    if (auto list = document_->services().materials.list(DocumentController::guiContext(), ""))
        for (const json& m : list.value())
            out.push_back({{"id", m.value("id", std::string())}, {"name", m.value("name", std::string())}});
    return out;
}

Result<QString> MaterialStateController::run(const QString& studyId, const json& study)
{
    auto job = document_->services().materialState.run(DocumentController::guiContext(), studyId.toStdString(), study);
    if (!job)
        return Result<QString>::failure(job.error().message);
    const QString id = QString::fromStdString(job.value().id);
    pending_.insert(id);
    return id;
}

json MaterialStateController::result(const QString& resultId) const
{
    auto r = document_->services().materialState.result(DocumentController::guiContext(), resultId.toStdString());
    return r ? r.value() : json::object();
}

Result<QString> MaterialStateController::saveRecord(const QString& resultId, const QString& name)
{
    const std::string id = "rec-" + resonant::Id::generate().str().substr(0, 8);
    auto m = document_->services().materialState.saveRecord(DocumentController::guiContext(), resultId.toStdString(), id, name.toStdString());
    if (!m)
        return Result<QString>::failure(m.error().message);
    return QString::fromStdString(id);
}

Status MaterialStateController::commit(const QString& recordId, const QString& materialId)
{
    auto m = document_->services().materialState.commit(DocumentController::guiContext(), recordId.toStdString(), {{"materialId", materialId.toStdString()}});
    if (!m)
        return Status::failure(m.error().message);
    return Status::success();
}
