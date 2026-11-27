#include "manipulators/AcousticFieldRun.h"

#include "acoustics/Components.h"
#include "acoustics/InverseDesign.h"
#include "acoustics/Propagation.h"
#include "manipulators/Manipulator.h"
#include "manipulators/ReflectorModel.h"

#include <cmath>

namespace resonant::acoustics {

using manipulators::ReflectorCollection;
using manipulators::ReflectorOptions;

namespace {
ReflectorCollection reflectorsOf(const Project& project, const AcousticSettings& s)
{
    ReflectorOptions options;
    options.elementsPerWavelength = s.reflectorElementsPerWavelength;
    return manipulators::collectReflectors(project.scene, project.materials, s.medium.wavelength(s.frequencyHz), options);
}
}  // namespace

std::shared_ptr<const PropagationModel> fieldModelFor(const Project& project, const AcousticSettings& s)
{
    if (s.modelId == kReflectorModelId)
        return std::make_shared<const manipulators::ReflectorModel>(s.propagation, reflectorsOf(project, s).surfaces);
    return std::make_shared<const FreeFieldModel>(s.propagation);
}

std::shared_ptr<const FieldEvaluator> fieldEvaluatorFor(const Project& project, const AcousticSettings& s)
{
    const auto sources = collectSources(project.scene);
    if (s.modelId == kReflectorModelId) {
        auto model = std::make_shared<const manipulators::ReflectorModel>(s.propagation, reflectorsOf(project, s).surfaces);
        return std::make_shared<const manipulators::ReflectorFieldEvaluator>(sources, s.frequencyHz, s.medium, std::move(model));
    }
    return std::make_shared<const FieldEvaluator>(sources, s.frequencyHz, s.medium, std::make_shared<FreeFieldModel>(s.propagation));
}

std::vector<std::string> fieldModelNotes(const Project& project, const AcousticSettings& s)
{
    std::vector<std::string> notes;
    if (s.modelId == kReflectorModelId) {
        const ReflectorCollection collection = reflectorsOf(project, s);
        if (collection.surfaces.empty())
            notes.push_back("There is no reflector in the scene: the result equals the free field.");
        for (const std::string& note : collection.notes)
            notes.push_back(note);
    } else {
        bool manipulators = false;
        project.scene.forEach([&](const SceneObject& object) {
            manipulators = manipulators || dynamic_cast<const manipulators::ManipulatorComponent*>(object.state.component.get()) != nullptr;
        });
        if (manipulators)
            notes.push_back("The scene has wave manipulators, but the free-field model ignores them. Select the reflector model to include "
                            "reflectors.");
    }
    return notes;
}

Result<FieldEstimate> estimateFieldRun(const Project& project, const Id& regionId, const AcousticSettings& s)
{
    const SceneObject* object = project.scene.find(regionId);
    const auto* region = object ? componentOf<RegionComponent>(*object) : nullptr;
    if (!region)
        return Result<FieldEstimate>::failure("choose a field region");
    size_t sourceCount = 0;
    for (const PointSource& source : collectSources(project.scene))
        if (std::abs(source.frequencyHz - s.frequencyHz) <= 1e-9 * s.frequencyHz)
            ++sourceCount;
    FieldEstimate estimate =
        estimateField(gridOf(region->settings, project.scene.worldMatrix(*object)), sourceCount, s.medium, s.frequencyHz, s.memoryLimitBytes);
    if (s.modelId == kReflectorModelId) {
        // Every sample also costs one pass over the reflector surface elements.
        ReflectorOptions options;
        options.elementsPerWavelength = s.reflectorElementsPerWavelength;
        const size_t elements = manipulators::estimateReflectorElements(project.scene, s.medium.wavelength(s.frequencyHz), options);
        estimate.evaluations += static_cast<double>(estimate.points) * static_cast<double>(elements);
        estimate.warnings.push_back("Reflector model: " + std::to_string(elements) + " surface element(s) at " +
                                    std::to_string(s.reflectorElementsPerWavelength).substr(0, 4) +
                                    " per wavelength (Kirchhoff approximation, not a full-wave solution).");
        if (estimate.evaluations > 2e11 && estimate.allowed) {
            estimate.allowed = false;
            estimate.refusal = "This calculation needs more than 200 billion element evaluations and would run for "
                               "hours. Reduce the region's resolution, the reflector size or the surface resolution.";
        } else if (estimate.evaluations > 5e9) {
            estimate.warnings.push_back("This is a large calculation and may take minutes.");
        }
    }
    for (const std::string& note : fieldModelNotes(project, s))
        estimate.warnings.push_back(note);
    return estimate;
}

Result<FieldRun> prepareFieldRun(const Project& project, const Id& regionId, int runNumber)
{
    using R = Result<FieldRun>;
    const AcousticSettings s = settingsOf(project);
    auto estimated = estimateFieldRun(project, regionId, s);
    if (!estimated)
        return R::failure(estimated.message());
    if (!estimated.value().allowed)
        return R::failure(estimated.value().refusal);
    const SceneObject* object = project.scene.find(regionId);
    const auto* region = componentOf<RegionComponent>(*object);
    const auto evaluator = fieldEvaluatorFor(project, s);

    // Everything needed to reproduce the run, as it is right now.
    ResultMetadata metadata;
    metadata.projectId = project.id;
    nlohmann::json sources = nlohmann::json::array();
    for (const PointSource& source : evaluator->sources())
        sources.push_back({{"label", source.label},
                           {"object", source.objectId.str()},
                           {"element", source.elementIndex},
                           {"position", {source.position.x(), source.position.y(), source.position.z()}},
                           {"forward", {source.forward.x(), source.forward.y(), source.forward.z()}},
                           {"frequencyHz", source.frequencyHz},
                           {"amplitude", {source.amplitude.real(), source.amplitude.imag()}},
                           {"directivity", std::string(toString(source.directivity.model))},
                           {"radius", source.radius}});
    nlohmann::json targets = nlohmann::json::array();
    for (const TargetPoint& target : collectTargets(project.scene).points)
        targets.push_back({{"label", target.label},
                           {"position", {target.position.x(), target.position.y(), target.position.z()}},
                           {"desired", {target.desired.real(), target.desired.imag()}},
                           {"weight", target.weight}});
    nlohmann::json reflectors = nlohmann::json::array();
    const std::shared_ptr<const PropagationModel> propagationModel = fieldModelFor(project, s);  // alive while its surfaces are read
    if (const auto* model = dynamic_cast<const manipulators::ReflectorModel*>(propagationModel.get()))
        for (const manipulators::ReflectingSurface& surface : model->surfaces())
            reflectors.push_back({{"object", surface.objectId.str()},
                                  {"name", surface.label},
                                  {"boundary", std::string(manipulators::toString(surface.boundary))},
                                  {"impedance", surface.impedance},
                                  {"surfaceElements", surface.elements.size()}});
    metadata.configuration = {{"settings", toJson(s)},
                              {"reflectors", std::move(reflectors)},
                              {"region", {{"object", regionId.str()}, {"name", object->state.name}, {"component", region->toJson()}}},
                              {"sources", std::move(sources)},
                              {"targets", std::move(targets)}};
    FieldRun run;
    run.name = object->state.name + " @ " + std::to_string(static_cast<long long>(s.frequencyHz)) + " Hz, run " + std::to_string(runNumber);
    run.estimate = estimated.take();
    run.job = std::make_shared<FieldJob>(run.name, evaluator, gridOf(region->settings, project.scene.worldMatrix(*object)), std::move(metadata));
    return run;
}

}  // namespace resonant::acoustics
