#include "FdtdController.h"

#include "AcousticController.h"
#include "DocumentController.h"
#include "acoustics/Settings.h"
#include "commands/SceneCommands.h"

#include <QFile>
#include <QTextStream>

using namespace resonant;
using namespace resonant::fdtd;

FdtdController::FdtdController(DocumentController* document, AcousticController* acoustics, QObject* parent)
    : QObject(parent), document_(document), acoustics_(acoustics)
{
    connect(document_, &DocumentController::jobChanged, this, &FdtdController::onJobChanged);
    connect(document_, &DocumentController::projectReplaced, this, [this] {
        results_.clear();
        pending_.reset();
        emit settingsChanged();
        emit resultsChanged();
        emit runningChanged();
    });
}

FdtdSettings FdtdController::settings() const
{
    return fdtdSettingsOf(document_->project());
}

Status FdtdController::setSettings(const FdtdSettings& newSettings)
{
    if (Status status = newSettings.validate(); !status)
        return status;
    if (newSettings == settings())
        return Status::success();
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(
        document_->project(), kFdtdSettingsKey, toJson(newSettings).dump(), "Change full-wave solver settings",
        [this] { emit settingsChanged(); }));
}

Prepared FdtdController::prepare(const Id& regionId) const
{
    return fdtd::prepare(document_->project(), regionId, acoustics_->settings().medium, settings());
}

QString FdtdController::assessment(const Id& regionId, const FdtdPlan& plan) const
{
    const solvers::ProblemTraits traits = traitsOf(document_->project(), regionId, acoustics_->settings().medium, settings(), plan);
    return QString::fromStdString(solvers::describeAssessments(solvers::acousticSolvers().assess(traits)))
           + QStringLiteral("The application never chooses or changes the solver for you.");
}

Result<Id> FdtdController::run(const Id& regionId)
{
    if (pending_)
        return Result<Id>::failure("a full-wave simulation is already running");
    Prepared prepared = prepare(regionId);
    if (!prepared.plan.allowed)
        return Result<Id>::failure(prepared.plan.refusal);
    const SceneObject* region = document_->scene().find(regionId);
    ResultMetadata metadata;
    metadata.projectId = document_->project().id;
    metadata.configuration = {{"region", {{"object", regionId.str()}, {"name", region->state.name}}}};
    const std::string name = region->state.name + ", full-wave run " + std::to_string(results_.size() + 1);
    pending_ = std::make_shared<FdtdJob>(name, std::move(prepared), settings(), acoustics_->settings().medium, std::move(metadata));
    pendingId_ = document_->submitJob(pending_);
    emit runningChanged();
    return pendingId_;
}

void FdtdController::cancel()
{
    if (pending_)
        document_->cancelJob(pendingId_);
}

void FdtdController::onJobChanged(const JobInfo& info)
{
    if (!pending_ || info.id != pendingId_ || !isFinished(info.state))
        return;
    const std::shared_ptr<FdtdJob> job = std::move(pending_);
    pending_.reset();
    emit runningChanged();
    if (info.state != JobState::Completed) {
        emit message(QStringLiteral("Full-wave simulation %1: %2").arg(QString::fromUtf8(toString(info.state).data()),
                                                                        QString::fromStdString(info.message)));
        return;
    }
    const std::shared_ptr<const FdtdResult> result = job->result();
    if (!result)
        return;
    results_.push_back(result);
    emit resultsChanged();
    if (result->harmonicField) {
        acoustics_->addResult(result->harmonicField);
    } else {
        // A transient run: keep its record, and show an instant from the
        // middle of the run, where the pulse is usually inside the region.
        nlohmann::json& runs = document_->project().extensions["acousticRuns"];
        if (!runs.is_array())
            runs = nlohmann::json::array();
        runs.push_back(acoustics::runRecord(*result));
        if (!result->output.snapshots.empty())
            showSnapshot(results_.size() - 1, result->output.snapshots.size() / 2);
    }
    emit message(QStringLiteral("Full-wave simulation finished: %1").arg(QString::fromStdString(result->name)));
}

bool FdtdController::showSnapshot(size_t resultIndex, size_t snapshotIndex)
{
    if (resultIndex >= results_.size())
        return false;
    const auto field = results_[resultIndex]->snapshotField(snapshotIndex);
    if (!field)
        return false;
    // An instant is a real pressure: show it as such, and do not animate a
    // phase it does not have.
    acoustics_->setAnimating(false);
    acoustics_->setQuantity(render::FieldQuantity::Instantaneous);
    acoustics_->displayTransient(field);
    return true;
}

bool FdtdController::showSteadyState(size_t resultIndex)
{
    if (resultIndex >= results_.size() || !results_[resultIndex]->harmonicField)
        return false;
    acoustics_->setQuantity(render::FieldQuantity::Magnitude);
    acoustics_->displayResult(results_[resultIndex]->harmonicField->id);
    return true;
}

Status FdtdController::exportProbes(size_t resultIndex, const QString& path) const
{
    if (resultIndex >= results_.size())
        return Status::failure("there is no such result");
    const FdtdResult& result = *results_[resultIndex];
    if (result.output.probes.empty())
        return Status::failure("the result has no probe recordings: add probes inside the region before running");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return Status::failure("could not write " + path.toStdString());
    QTextStream out(&file);
    out.setRealNumberPrecision(12);
    out << "# " << QString::fromStdString(result.name) << "; solver " << QString::fromStdString(result.metadata.solverId)
        << "; pressure in Pa; time step " << result.output.dt << " s\n";
    out << "time_s";
    for (const ProbeTrace& probe : result.output.probes)
        out << ",\"" << QString::fromStdString(probe.label).replace(QLatin1Char('"'), QStringLiteral("\"\"")) << "\"";
    out << "\n";
    for (size_t n = 0; n < result.output.probes.front().pressure.size(); ++n) {
        out << static_cast<double>(n + 1) * result.output.dt;
        for (const ProbeTrace& probe : result.output.probes)
            out << "," << probe.pressure[n];
        out << "\n";
    }
    return Status::success();
}
