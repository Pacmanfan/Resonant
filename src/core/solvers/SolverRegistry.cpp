#include "solvers/SolverRegistry.h"

#include <algorithm>
#include <cstdio>

namespace resonant::solvers {

std::string_view toString(Phenomenon p)
{
    switch (p) {
    case Phenomenon::DirectRadiation: return "direct radiation";
    case Phenomenon::Interference: return "interference";
    case Phenomenon::SourceDirectivity: return "source directivity";
    case Phenomenon::SingleReflection: return "single reflection";
    case Phenomenon::MultipleReflection: return "multiple reflections";
    case Phenomenon::Diffraction: return "diffraction";
    case Phenomenon::Shadowing: return "shadowing";
    case Phenomenon::Transmission: return "transmission through materials";
    case Phenomenon::Refraction: return "refraction";
    case Phenomenon::Absorption: return "absorption";
    case Phenomenon::Resonance: return "resonance";
    case Phenomenon::TransientSignals: return "transient signals";
    case Phenomenon::Broadband: return "broadband signals";
    case Phenomenon::HeterogeneousMedia: return "heterogeneous media";
    case Phenomenon::ParticleVelocity: return "particle velocity and intensity";
    case Phenomenon::ShearWaves: return "shear waves in solids";
    case Phenomenon::Nonlinearity: return "nonlinear propagation";
    case Phenomenon::Flow: return "moving media";
    }
    return "";
}

bool SolverDescriptor::represents_(Phenomenon phenomenon) const
{
    return std::find(represents.begin(), represents.end(), phenomenon) != represents.end();
}

void SolverRegistry::add(SolverDescriptor descriptor)
{
    solvers_.push_back(std::move(descriptor));
}

const SolverDescriptor* SolverRegistry::find(std::string_view id) const
{
    for (const SolverDescriptor& s : solvers_)
        if (s.id == id)
            return &s;
    return nullptr;
}

std::vector<Assessment> SolverRegistry::assess(const ProblemTraits& t, PhysicsDomain domain) const
{
    std::vector<Assessment> all;
    for (const SolverDescriptor& s : solvers_) {
        if (s.domain != domain)
            continue;
        Assessment a;
        a.solver = &s;
        const auto missing = [&](std::string text) {
            a.complete = false;
            a.leftOut.push_back(std::move(text));
        };
        const auto count = [](size_t n, const char* what) { return std::to_string(n) + " " + what + (n == 1 ? "" : "s"); };

        if (t.reflectors > 0) {
            if (!s.represents_(Phenomenon::SingleReflection))
                missing(count(t.reflectors, "reflector") + " would be ignored");
            else if (!s.represents_(Phenomenon::MultipleReflection) && (t.reflectors > 1 || t.closedDomain))
                missing("reflections between surfaces (only one reflection per path is calculated)");
        }
        if (t.otherManipulators > 0 && !s.represents_(Phenomenon::Diffraction))
            missing(count(t.otherManipulators, "lens, waveguide, cavity or diffraction structure") + " would be ignored");
        else if (t.otherManipulators > 0 && !s.represents_(Phenomenon::Transmission))
            missing(count(t.otherManipulators, "lens, waveguide, cavity or diffraction structure") + " would be ignored");
        if (t.plainObjects > 0 && !s.represents_(Phenomenon::Shadowing))
            missing(count(t.plainObjects, "object") + " with geometry would be ignored (no scattering or shadow)");
        if (t.penetrableObjects > 0 && !s.represents_(Phenomenon::Transmission))
            missing("transmission into " + count(t.penetrableObjects, "material object"));
        if (t.transientSignal && !s.represents_(Phenomenon::TransientSignals))
            missing("the transient signal (this solver is single-frequency and steady-state)");
        if (t.closedDomain && !s.represents_(Phenomenon::Resonance))
            missing("the reflecting walls of the domain and any resonance between them");
        if (t.mediumAttenuation && !s.represents_(Phenomenon::Absorption))
            missing("attenuation in the medium");
        if (t.directionalSources && !s.represents_(Phenomenon::SourceDirectivity))
            missing("emitter directivity (every emitter would radiate as a monopole)");
        if (t.needsIntensity && !s.represents_(Phenomenon::ParticleVelocity))
            missing("particle velocity and intensity");
        if (t.dimensions == 2 && !s.supports2D && !t.planeRegionMeansSlice)
            missing("a two-dimensional calculation");
        if (t.dimensions == 3 && !s.supports3D)
            missing("a three-dimensional calculation");

        if (s.timeDomain && t.dimensions == 2)
            a.remarks.push_back("on a plane region this solver calculates a TWO-DIMENSIONAL problem (line sources, objects as "
                                "infinite cylinders), not a slice of the three-dimensional field");
        if (s.timeDomain && t.gridCellUpdates > 0.0) {
            char buffer[160];
            std::snprintf(buffer, sizeof buffer, "about %.2g cell updates for this region and frequency", t.gridCellUpdates);
            a.remarks.push_back(buffer);
            if (t.gridCellUpdates > 5e10)
                a.remarks.push_back("that is a very long calculation; a smaller region, a lower frequency or a 2D plane makes it practical");
        }
        if (!s.fullWave && !a.complete)
            a.remarks.push_back("it would still run, and its result would not include the items above");
        if (!s.fullWave && a.complete && t.reflectors + t.otherManipulators + t.plainObjects == 0)
            a.remarks.push_back("exact for this problem within its model, and by far the cheapest");
        if (s.id == "acoustic.reflector.kirchhoff" && t.reflectors > 0 && t.domainWavelengths > 0.0)
            a.remarks.push_back("a high-frequency approximation: accurate when reflectors and their curvature radii are several "
                                "wavelengths large");
        all.push_back(std::move(a));
    }
    return all;
}

void registerAcousticSolvers(SolverRegistry& registry)
{
    using P = Phenomenon;
    registry.add({"acoustic.freefield.analytical", "Analytical free field", "1", PhysicsDomain::Acoustic,
                  "closed-form superposition of point sources", false, false, false, true,
                  {P::DirectRadiation, P::Interference, P::SourceDirectivity, P::Absorption, P::ParticleVelocity},
                  "samples x sources",
                  "Emitters and arrays radiating into open, homogeneous space at one frequency. Exact within that model.",
                  {"Objects in the scene have no effect.", "One frequency, steady state.",
                   "The baffled-piston directivity is a far-field formula."}});
    registry.add({"acoustic.reflector.kirchhoff", "Free field + reflectors (Kirchhoff approximation)", "1", PhysicsDomain::Acoustic,
                  "Helmholtz-Kirchhoff surface integral with tangent-plane boundary values", false, false, false, true,
                  {P::DirectRadiation, P::Interference, P::SourceDirectivity, P::Absorption, P::SingleReflection, P::Diffraction},
                  "samples x surface elements",
                  "One reflection from reflectors that are several wavelengths large, at one frequency.",
                  {"One reflection per path; no resonance between surfaces.", "No shadowing of one surface by another.",
                   "Only reflector manipulators take part; other objects have no effect.",
                   "No particle velocity or intensity.", "Not a full-wave solution."}});
    registry.add({"acoustic.fdtd.staggered", "Full-wave time domain (FDTD)", "1", PhysicsDomain::Acoustic,
                  "finite differences in the time domain on a uniform staggered grid, second order", true, true, true, true,
                  {P::DirectRadiation, P::Interference, P::SingleReflection, P::MultipleReflection, P::Diffraction, P::Shadowing,
                   P::Transmission, P::Refraction, P::Absorption, P::Resonance, P::TransientSignals, P::Broadband,
                   P::HeterogeneousMedia, P::ParticleVelocity},
                  "cells x time steps; cells grow with (size / wavelength)^2 in 2D and ^3 in 3D",
                  "Reflection, diffraction, scattering, transmission and resonance in domains up to some tens of wavelengths; "
                  "pulses and broadband signals.",
                  {"Emitters are monopoles: directivity models are not applied.",
                   "Curved surfaces are stepped at the cell size.",
                   "Solids are fluids with their density and speed of sound: no shear waves.",
                   "Attenuation is frequency-independent.",
                   "Numerical dispersion: the wave is slightly slow, by an amount the preflight states.",
                   "Open space needs absorbing layers, which reflect a small, tested amount.",
                   "Linear acoustics in a medium at rest."}});
}

const SolverRegistry& acousticSolvers()
{
    static const SolverRegistry registry = [] {
        SolverRegistry r;
        registerAcousticSolvers(r);
        return r;
    }();
    return registry;
}

std::string describeAssessments(const std::vector<Assessment>& assessments)
{
    std::string text;
    for (const Assessment& a : assessments) {
        text += a.solver->name + (a.solver->fullWave ? " [full-wave]" : " [approximate model]") + ": ";
        if (a.complete) {
            text += "represents everything in this problem.";
        } else {
            text += "would LEAVE OUT ";
            for (size_t i = 0; i < a.leftOut.size(); ++i)
                text += (i ? "; " : "") + a.leftOut[i];
            text += ".";
        }
        for (const std::string& remark : a.remarks)
            text += " Note: " + remark + ".";
        text += "\n";
    }
    return text;
}

}  // namespace resonant::solvers
