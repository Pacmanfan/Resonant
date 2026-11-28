#include "MultiscaleController.h"

#include "DocumentController.h"

#include "services/Services.h"
#include "simulation/Job.h"

using nlohmann::json;
using resonant::Result;
using resonant::Status;

MultiscaleController::MultiscaleController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::projectReplaced, this, &MultiscaleController::modelChanged);
    connect(document_, &DocumentController::documentStateChanged, this, &MultiscaleController::modelChanged);
    connect(document_, &DocumentController::jobChanged, this, [this](const resonant::JobInfo& info) {
        const QString id = QString::fromStdString(info.id.str());
        if (!pending_.contains(id) || !resonant::isFinished(info.state))
            return;
        pending_.remove(id);
        auto view = document_->services().jobs.get(DocumentController::guiContext(), info.id.str());
        if (!view || view.value().state != "completed" || view.value().resultIds.empty()) {
            const std::string why = view && view.value().error ? view.value().error->message : info.message;
            emit derivationFailed(QString::fromStdString(why.empty() ? std::string("the derivation did not complete") : why));
            return;
        }
        emit resultReady(QString::fromStdString(view.value().resultIds[0]));
    });
}

json MultiscaleController::representations() const
{
    auto list = document_->services().multiscale.representations(DocumentController::guiContext());
    json out = json::array();
    if (list)
        for (const json& r : list.value())
            out.push_back(r);
    return out;
}

json MultiscaleController::representation(const QString& id) const
{
    auto r = document_->services().multiscale.representation(DocumentController::guiContext(), id.toStdString());
    return r ? r.value() : json(nullptr);
}

json MultiscaleController::materials() const
{
    json out = json::array();
    if (auto list = document_->services().materials.list(DocumentController::guiContext(), ""))
        for (const json& m : list.value())
            out.push_back({{"id", m.value("id", std::string())}, {"name", m.value("name", std::string())}});
    return out;
}

Result<QString> MultiscaleController::derive(const QString& representation, const QString& method)
{
    auto job = document_->services().multiscale.derive(DocumentController::guiContext(), representation.toStdString(), method.toStdString());
    if (!job)
        return Result<QString>::failure(job.error().message);
    const QString id = QString::fromStdString(job.value().id);
    pending_.insert(id);
    return id;
}

json MultiscaleController::result(const QString& resultId) const
{
    auto r = document_->services().multiscale.result(DocumentController::guiContext(), resultId.toStdString());
    return r ? r.value() : json::object();
}

Status MultiscaleController::assign(const QString& resultId, const QString& materialId)
{
    auto m = document_->services().multiscale.assign(DocumentController::guiContext(), resultId.toStdString(), {{"materialId", materialId.toStdString()}});
    if (!m)
        return Status::failure(m.error().message);
    return Status::success();
}

json MultiscaleController::lineage(const QString& materialId) const
{
    auto l = document_->services().multiscale.lineage(DocumentController::guiContext(), materialId.toStdString());
    return l ? l.value() : json(nullptr);
}
