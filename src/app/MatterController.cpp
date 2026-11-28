#include "MatterController.h"

#include "DocumentController.h"

#include "services/Services.h"

using nlohmann::json;
using resonant::Result;

namespace {

template <typename T>
Result<T> unwrap(const resonant::services::Outcome<T>& o)
{
    if (!o)
        return Result<T>::failure(o.error().message);
    return o.value();
}

}  // namespace

MatterController::MatterController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::projectReplaced, this, &MatterController::modelChanged);
    connect(document_, &DocumentController::documentStateChanged, this, &MatterController::modelChanged);
}

json MatterController::architectures() const
{
    auto r = document_->services().matter.architectures(DocumentController::guiContext());
    return r ? r.value() : json::array();
}

json MatterController::architecture(const QString& id) const
{
    auto r = document_->services().matter.architecture(DocumentController::guiContext(), id.toStdString());
    return r ? r.value() : json::object();
}

Result<json> MatterController::analyze(const QString& architecture, const QString& stack, const QString& analysis, const json& options) const
{
    return unwrap(document_->services().matter.analyze(DocumentController::guiContext(), architecture.toStdString(), stack.toStdString(), analysis.toStdString(), options));
}

Result<json> MatterController::manufacturability(const QString& architecture) const
{
    return unwrap(document_->services().matter.manufacturability(DocumentController::guiContext(), architecture.toStdString()));
}

Result<json> MatterController::explore(const QString& architecture) const
{
    return unwrap(document_->services().matter.explore(DocumentController::guiContext(), architecture.toStdString()));
}

Result<json> MatterController::mechanisms(const QString& from, const QString& to, const QString& architecture) const
{
    return unwrap(document_->services().matter.mechanisms(DocumentController::guiContext(), from.toStdString(), to.toStdString(), architecture.toStdString()));
}

Result<json> MatterController::compare(const json& architectures) const
{
    auto r = document_->services().matter.compare(DocumentController::guiContext(), {{"architectures", architectures}});
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<json> MatterController::duplicate(const QString& architecture, const QString& newId)
{
    auto r = document_->services().matter.duplicateArchitecture(DocumentController::guiContext(), architecture.toStdString(), newId.toStdString(), "");
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<json> MatterController::createPhysicalSystem(const QString& architecture, const QString& stack)
{
    auto r = document_->services().matter.createPhysicalSystem(DocumentController::guiContext(), architecture.toStdString(), stack.toStdString(), json::object());
    if (!r)
        return Result<json>::failure(r.error().message);
    return r.value().result;
}

Result<QString> MatterController::report(const QString& architecture, const QString& format) const
{
    auto r = document_->services().matter.report(DocumentController::guiContext(), architecture.toStdString(), format.toStdString());
    if (!r)
        return Result<QString>::failure(r.error().message);
    return QString::fromStdString(r.value());
}
