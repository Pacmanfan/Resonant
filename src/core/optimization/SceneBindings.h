#pragma once

// Domain-independent parts of an adapter (RES-OPT-010, RES-OPT-018):
// binding a snapshot, reconstructing an independent project per evaluation,
// and the bindings every scene object has.
//
//   object/<id>/position/{x,y,z}    m, the object's local translation
//   object/<id>/rotation/{x,y,z}    rad, its local Euler angles
//   object/<id>/geometry/<param>    the geometry parameter in its SI unit
//   object/<id>/material            categorical: index into the variable's
//                                   choices (material ids of the library)
//
// Geometry parameters of one object are applied together and the geometry's
// own rules are checked after all of them are set, so a design whose
// parameters are individually valid but jointly not (an aperture larger
// than twice the radius of curvature) is detected and reported as an
// invalid design - never evaluated as if it were valid.
#include "foundation/Result.h"
#include "foundation/Types.h"
#include "optimization/Evaluator.h"

#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace resonant {
class Project;
class SceneObject;
}  // namespace resonant

namespace resonant::optimization {

std::string objectBinding(const std::string& objectId, const std::string& what);

struct SceneBinding {
    enum class Kind { Position, Rotation, Geometry, Material };
    Kind kind = Kind::Position;
    std::string objectId;
    std::string field;  // geometry parameter, or "x"/"y"/"z"
};
std::optional<SceneBinding> parseSceneBinding(std::string_view key);

// Adds the generic variables of an object: its geometry parameters (bounds:
// the parameter's range within a quarter and four times the present value),
// its position (+/- 0.1 m) and, when `materialDomain` is given, the choice
// among library materials that have properties of that domain.
void appendSceneVariables(const Project& project, const SceneObject& object, std::vector<DesignVariable>& out, bool geometry, bool position,
                          std::optional<PhysicsDomain> materialDomain);

class SnapshotAdapter : public DesignEvaluator {
public:
    SnapshotAdapter(nlohmann::json snapshot, nlohmann::json configuration);
    const Capabilities& capabilities() const override { return capabilities_; }
    const std::string& snapshotKey() const override { return key_; }
    Status apply(const std::map<std::string, double, std::less<>>& bindings, Project& project) const override;

protected:
    // An independent project with the bindings applied; fails (an invalid
    // design) when one cannot be applied or the result is not valid.
    Result<std::unique_ptr<Project>> build(const std::map<std::string, double, std::less<>>& bindings) const;
    // Applies one binding that is not a scene binding; returns false when
    // the key is not the adapter's. Called after the scene bindings.
    virtual Result<bool> applyDomainBinding(Project& project, std::string_view key, double value) const;
    // Checks a built design beyond geometry rules (overlaps, clearances).
    virtual Status checkDesign(const Project& project) const;
    // Computes the key after the derived constructor has filled capabilities.
    void finishBinding();

    nlohmann::json snapshot_;
    nlohmann::json configuration_;
    Capabilities capabilities_;
    std::string key_;
};

}  // namespace resonant::optimization
