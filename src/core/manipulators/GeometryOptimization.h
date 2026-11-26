// A first, deliberately small optimizer for parametric geometry: it varies
// chosen parameters of one geometry within bounds to maximize an objective
// that is computed by a physical model, and keeps the full history.
//
// Method: cyclic coordinate search; along each variable a golden-section
// search of the bounded interval. Derivative-free and robust, but LOCAL:
// golden-section finds the maximum only when the objective has a single
// peak in the interval, and coordinate search can stop at a ridge. The
// outcome says which method produced it; it is a candidate to be checked,
// not a proof of optimality.
//
// The optimizer never changes a scene. It returns the best geometry it
// found; applying it is the caller's (undoable) decision.
#pragma once

#include "manipulators/Manipulator.h"

#include <functional>

namespace resonant::manipulators {

struct DesignVariable {
    std::string parameter;  // key of a geometry parameter
    double lower = 0.0;
    double upper = 1.0;
};

struct OptimizationSettings {
    int maxEvaluations = 80;
    // A line search stops when its interval is this fraction of the bounds.
    double tolerance = 1e-3;
    ManufacturingConstraints constraints;  // candidates violating them are infeasible
};

struct Candidate {
    std::vector<double> values;   // one per design variable
    double objective = 0.0;
    bool feasible = true;
    std::string reason;           // why it is infeasible
};

struct OptimizationOutcome {
    bool improved = false;
    std::string method;
    std::vector<DesignVariable> variables;
    Candidate initial;
    Candidate best;
    std::vector<Candidate> history;       // every candidate evaluated, in order
    std::optional<Geometry> bestGeometry;
    std::vector<std::string> notes;
};

// The objective of a candidate geometry, to be maximized. It must run a
// physical model on the geometry it is given.
using GeometryObjective = std::function<double(const Geometry& candidate)>;

OptimizationOutcome optimizeGeometry(const Geometry& start, const std::vector<DesignVariable>& variables,
                                     const GeometryObjective& objective, const OptimizationSettings& settings = {});

}  // namespace resonant::manipulators
