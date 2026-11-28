#pragma once

// Physical models for the laboratory (REQ-CHAT-20261126-023, RES-LAB-011, -014,
// ADR-000227): predictions from the existing solvers as functions of named physical
// parameters, for comparison with measurements and for calibration. Only parameters with a
// physical meaning in the model can be varied; there is no correction factor.
//
//   resonator.lumped            the driven response of a lumped resonator (oscillators): receptance,
//                               mobility or accelerance at the measured frequencies; a single
//                               mass-spring-damper from mass, stiffness and damping, or a network
//   em.layered-reflection       S11 of a layered stack at normal incidence (electromagnetics)
//   thermal.evolution-probe     the temperature at a position of a physical system's slab over
//                               time, by its time-stepped coupled evolution, with material
//                               properties as parameters ("<material>.thermal.<key>")
//   material-state.j2-uniaxial  the stress of a J2 material point along a measured strain path
//   functional.metric           a metric of a functional structure's analysis (one value),
//                               with structure parameters as parameters
//   material.property           a library material's property value (with its provenance and
//                               derivation: measured, homogenized, computed ...)
//   design-revision.metric      a promoted optimization candidate's evaluated quantity

#include "experiments/ExperimentTypes.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <map>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
class Project;
}  // namespace resonant

namespace resonant::coupling {

struct LabModelParameter {
    std::string id, label, unit, meaning;
    double value = 0.0, lower = 0.0, upper = 0.0;
};

struct LabModel {
    std::string id, title, domain, observable, axis, assumptions;
    bool complex = false, scalar = false;
    std::vector<LabModelParameter> parameters;   // the parameters of the default configuration
    std::vector<std::string> configuration;      // configuration keys
    nlohmann::json toJson() const;
};

const std::vector<LabModel>& laboratoryModels();
const LabModel* findLaboratoryModel(std::string_view id);

struct Prediction {
    std::vector<double> axis;
    std::vector<std::complex<double>> values;  // real models: imaginary part zero
    bool complex = false;
    bool scalar = false;  // one value (the axis is ignored)
    experiments::QuantitySpec quantity;
    std::string unit, model, solver;
    double standard = 0.0;  // a scalar's stated uncertainty (material.property), else 0
    nlohmann::json details = nlohmann::json::object();
};

// The parameters a configuration exposes (with their current values), and the prediction.
Result<std::vector<LabModelParameter>> laboratoryModelParameters(const std::string& modelId, const nlohmann::json& configuration, const Project& project);
Result<Prediction> predictLaboratoryModel(const std::string& modelId, const nlohmann::json& configuration, const std::map<std::string, double>& parameters,
                                          const std::vector<double>& axis, const Project& project, JobContext* context);

}  // namespace resonant::coupling
