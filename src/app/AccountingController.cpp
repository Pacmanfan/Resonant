#include "AccountingController.h"

#include "DocumentController.h"

#include "services/Services.h"
#include "simulation/Job.h"

using nlohmann::json;
using resonant::Result;

AccountingController::AccountingController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::projectReplaced, this, &AccountingController::modelChanged);
    connect(document_, &DocumentController::documentStateChanged, this, &AccountingController::modelChanged);
    connect(document_, &DocumentController::jobChanged, this, [this](const resonant::JobInfo& info) {
        const QString id = QString::fromStdString(info.id.str());
        if (!pending_.contains(id) || !resonant::isFinished(info.state))
            return;
        pending_.remove(id);
        auto view = document_->services().jobs.get(DocumentController::guiContext(), info.id.str());
        if (!view || view.value().state != "completed" || view.value().resultIds.empty()) {
            const std::string why = view && view.value().error ? view.value().error->message : info.message;
            emit runFailed(QString::fromStdString(why.empty() ? std::string("the accounting did not complete") : why));
            return;
        }
        emit resultReady(QString::fromStdString(view.value().resultIds[0]));
    });
}

json AccountingController::studies() const
{
    auto m = document_->services().accounting.studies(DocumentController::guiContext());
    return m ? m.value() : json::array();
}

Result<QString> AccountingController::run(const QString& studyId)
{
    auto job = document_->services().accounting.run(DocumentController::guiContext(), studyId.toStdString(), json());
    if (!job)
        return Result<QString>::failure(job.error().message);
    const QString id = QString::fromStdString(job.value().id);
    pending_.insert(id);
    return id;
}

json AccountingController::result(const QString& resultId) const
{
    auto r = document_->services().accounting.result(DocumentController::guiContext(), resultId.toStdString());
    return r ? r.value() : json::object();
}

Result<json> AccountingController::report(const QString& resultId, const json& system) const
{
    auto r = document_->services().accounting.report(DocumentController::guiContext(), resultId.toStdString(), system);
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value();
}

Result<QString> AccountingController::exportResult(const QString& resultId, const QString& format) const
{
    auto r = document_->services().accounting.exportResult(DocumentController::guiContext(), resultId.toStdString(), format.toStdString());
    if (!r)
        return Result<QString>::failure(r.error().message);
    return QString::fromStdString(r.value());
}

Result<QString> AccountingController::saveRecord(const QString& resultId, const QString& name)
{
    const std::string id = "acc-" + resonant::Id::generate().str().substr(0, 8);
    auto m = document_->services().accounting.saveRecord(DocumentController::guiContext(), resultId.toStdString(), id, name.toStdString());
    if (!m)
        return Result<QString>::failure(m.error().message);
    return QString::fromStdString(id);
}

json AccountingController::records() const
{
    json out = json::array();
    if (auto list = document_->services().accounting.records(DocumentController::guiContext()))
        for (const json& r : list.value())
            out.push_back(r);
    return out;
}
