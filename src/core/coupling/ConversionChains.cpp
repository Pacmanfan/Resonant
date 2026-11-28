#include "coupling/ConversionChains.h"

#include <cmath>
#include <cstdio>

namespace resonant::coupling {
namespace {

using Complex = std::complex<double>;

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

}  // namespace

Result<DuctHarvesterResult> ductPiezoelectricHarvester(const DuctPiezoelectricHarvester& spec)
{
    using R = Result<DuctHarvesterResult>;
    if (!(spec.fluidDensity > 0.0 && spec.soundSpeed > 0.0 && spec.incidentPressure > 0.0 && spec.frequency > 0.0))
        return R::failure("the duct needs a positive fluid density, sound speed, incident pressure and frequency");
    const mechanics::Terminal* load = nullptr;
    for (const auto& t : spec.body.terminals)
        if (t.name == spec.loadTerminal && t.kind == mechanics::Terminal::Kind::Load)
            load = &t;
    if (!load)
        return R::failure("the chain needs the resistive load terminal '" + spec.loadTerminal + "'");
    if (spec.body.body.surface.kind != mechanics::ReferenceSurface::Kind::Plane)
        return R::failure("the duct chain takes a flat plate closing the duct");
    // The coupled plate under a unit uniform pressure on its outer face.
    mechanics::PiezoHarmonicSettings unit;
    unit.frequency = spec.frequency;
    unit.loads.outerPressure = 1.0;
    unit.lossFactor = spec.lossFactor;
    auto solved = mechanics::solvePiezoHarmonic(spec.body, spec.supports, unit);
    if (!solved)
        return R::failure(solved.message());
    const mechanics::PiezoHarmonicResult& one = solved.value();
    // The volume velocity into the plate: U = sum f_i v_i for the unit pressure's nodal forces f.
    auto f = mechanics::layeredLoadVector(spec.body.body, one.mesh, unit.loads);
    if (!f)
        return R::failure(f.message());
    const double w = 2 * 3.14159265358979323846 * spec.frequency;
    const Complex j(0.0, 1.0);
    // The face's area is the unit pressure's total force on a flat plate.
    Complex volumeVelocity = 0.0;
    Eigen::Vector3d total = Eigen::Vector3d::Zero();
    for (Eigen::Index i = 0; i < f.value().size(); ++i) {
        volumeVelocity += f.value()(i) * j * w * one.displacement(i);
        total(i % 3) += f.value()(i);
    }
    const double area = total.norm();
    if (!(std::abs(volumeVelocity) > 0.0))
        return R::failure("the plate does not move under the pressure: no absorption");
    DuctHarvesterResult out;
    out.area = area;
    const double rhoC = spec.fluidDensity * spec.soundSpeed;
    out.surfaceImpedance = area / volumeVelocity;  // p / (U / A) for p = 1 Pa
    const Complex pi(spec.incidentPressure, 0.0);
    out.surfacePressure = 2.0 * pi * out.surfaceImpedance / (out.surfaceImpedance + rhoC);
    const Complex pr = out.surfacePressure - pi;
    out.reflectionCoefficient = pr / pi;
    out.incidentPower = std::norm(pi) * area / (2 * rhoC);
    out.reflectedPower = std::norm(pr) * area / (2 * rhoC);
    // The coupled solution scales with p: powers with |p|^2.
    const double scale = std::norm(out.surfacePressure);
    out.absorbedPower = scale * one.inputPower;
    out.dampingPower = scale * one.dampingPower;
    const auto& term = one.terminals.at(spec.loadTerminal);
    out.voltage = out.surfacePressure * term.voltage;
    const Complex current = out.surfacePressure * term.current;
    out.terminalPower = 0.5 * std::real(out.voltage * std::conj(current));
    out.loadPower = 0.5 * std::norm(out.voltage) / load->resistance;
    // The chain.
    using conversion::EnergyForm;
    conversion::ConversionStage acoustic;
    acoustic.id = "acoustic";
    acoustic.name = "Plane wave at the plate";
    acoustic.mechanism = "acoustic-absorption";
    acoustic.model = "duct plane-wave relation + the coupled plate's impedance";
    acoustic.inputForm = EnergyForm::Acoustic;
    acoustic.outputForm = EnergyForm::Mechanical;
    acoustic.input = out.incidentPower;
    acoustic.output = out.absorbedPower;
    acoustic.otherOutputs = {{"reflected wave", EnergyForm::Acoustic, out.reflectedPower}};
    acoustic.coupling = {{"reflectionMagnitude", std::abs(out.reflectionCoefficient)}};
    acoustic.boundaryConditions = {"incident plane wave " + num(spec.incidentPressure) + " Pa at " + num(spec.frequency) + " Hz in a rigid duct"};
    acoustic.assumptions = {"one-dimensional plane wave (below the duct's first cross mode)", "no fluid or cavity behind the plate"};
    acoustic.validity = {"wavelength large against the cross-section's dimensions"};
    conversion::ConversionStage transducer;
    transducer.id = "transducer";
    transducer.name = "Piezoelectric plate";
    transducer.mechanism = "piezoelectric";
    transducer.model = "mechanics.piezoelectric-solid";
    transducer.inputForm = EnergyForm::Mechanical;
    transducer.outputForm = EnergyForm::Electrical;
    transducer.input = out.absorbedPower;
    transducer.output = out.terminalPower;
    transducer.dissipated = out.dampingPower;
    transducer.coupling = {{"lossFactor", spec.lossFactor}};
    transducer.boundaryConditions = {"the plate's supports", "the load terminal '" + spec.loadTerminal + "' of " + num(load->resistance) + " ohm"};
    transducer.assumptions = one.assumptions;
    transducer.validity = {"linear piezoelectricity and small displacements"};
    conversion::ConversionStage delivery;
    delivery.id = "delivery";
    delivery.name = "Terminals to the load";
    delivery.mechanism = "electrical conduction";
    delivery.model = "the terminal's voltage and current";
    delivery.inputForm = EnergyForm::Electrical;
    delivery.outputForm = EnergyForm::Electrical;
    delivery.input = out.terminalPower;
    delivery.output = out.loadPower;
    delivery.assumptions = {"ideal leads: the load's voltage is the terminal's"};
    out.chain.id = "acoustic-to-electrical";
    out.chain.title = "Acoustic excitation to an electrical load (duct harvester)";
    out.chain.stages = {acoustic, transducer, delivery};
    out.report = conversion::evaluateChain(out.chain);
    out.assumptions = acoustic.assumptions;
    out.assumptions.push_back("the coupled plate solved once per unit pressure; the response scales linearly with the surface pressure");
    return out;
}

}  // namespace resonant::coupling
