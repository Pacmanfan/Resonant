#pragma once

// The mechanical interpretation of experiments: frequency response
// functions of structures, resonances identified from them, mode-shape
// correlation (MAC), comparison with the simulated modes, stress inferred
// from measured strain, and templates for the mechanical workflows (modal
// testing, vibration characterization, actuator calibration, vibration
// mapping).
//
// Resonances identified here are peaks of a measured response with their
// half-power bandwidth: estimates of the structure's modes at the measured
// points, not computed eigenmodes. Stress is never measured by an
// accelerometer or a strain gauge; it is inferred from strain with a
// stated material model.
#include "experiments/Experiment.h"
#include "experiments/Processing.h"
#include "mechanics/Analyses.h"

#include <complex>
#include <vector>

namespace resonant::mechanics {

struct IdentifiedMode {
    double frequencyHz = 0.0;
    double dampingRatio = 0.0;         // from the half-power bandwidth (light damping, separated modes)
    double qualityFactor = 0.0;        // 1 / (2 zeta)
    double peakMagnitude = 0.0;        // |H| at the peak
    double coherence = 0.0;            // at the peak
    double frequencyUncertaintyHz = 0.0;  // half the spectral resolution
    bool bandwidthResolved = true;     // false when the half-power points are closer than the resolution
    std::string note;
};

// Peaks of |H| that reach `minRelative` of the largest, where the coherence is
// at least `minCoherence`, strongest first.
Result<std::vector<IdentifiedMode>> identifyModes(const experiments::ProcessingItem& transferFunction, double minRelative = 0.1,
                                                  double minCoherence = 0.8, double minHz = 0.0);

// Modal assurance criterion |a^H b|^2 / ((a^H a)(b^H b)): 1 for proportional shapes.
double modalAssuranceCriterion(const std::vector<std::complex<double>>& a, const std::vector<std::complex<double>>& b);

// The operating deflection shape at one frequency from several FRFs of the
// same excitation (the complex values at that frequency, interpolated).
Result<std::vector<std::complex<double>>> operatingShape(const std::vector<experiments::ProcessingItem>& transferFunctions,
                                                         double frequencyHz);

struct ModePair {
    size_t measured = 0, simulated = 0;
    double measuredHz = 0.0, simulatedHz = 0.0;
    double frequencyErrorPercent = 0.0;  // (simulated - measured) / measured
    double mac = -1.0;                   // -1 = no shapes to compare
};

// Pairs identified with simulated modes: by MAC when shapes are given for
// both (each simulated shape sampled at the measured points, same order),
// otherwise by nearest frequency.
std::vector<ModePair> pairModes(const std::vector<IdentifiedMode>& measured, const std::vector<std::vector<std::complex<double>>>& measuredShapes,
                                const std::vector<double>& simulatedHz, const std::vector<std::vector<std::complex<double>>>& simulatedShapes);

// The mode shape of a simulated mode at measurement points (nearest nodes,
// along the sensors' directions).
std::vector<std::complex<double>> simulatedShapeAt(const StructuralModel& model, const Mode& mode, const std::vector<Vec3>& points,
                                                   const std::vector<Vec3>& directions);

// The processing step that infers stress from measured strain (Hooke's law, uniaxial).
experiments::ProcessingStep inferStressStep(double youngsModulusPa, const std::string& materialName);

// --- templates (on the simulated vibration DAQ) ---

experiments::Experiment modalTestExperiment(const std::string& id);
experiments::Experiment vibrationCharacterizationExperiment(const std::string& id, double frequencyHz);
experiments::Experiment shakerCalibrationExperiment(const std::string& id);

}  // namespace resonant::mechanics
