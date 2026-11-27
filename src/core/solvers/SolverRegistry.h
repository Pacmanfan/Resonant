#pragma once

// What each solver can and cannot represent, and which of them fit a given
// problem.
//
// Solvers stay separate implementations with their own formulations; this
// registry only holds their declarations. Assessing a problem never selects
// or starts a solver: the user chooses, and a solver that would leave out
// part of the problem says which part.
#include "foundation/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace resonant::solvers {

enum class Phenomenon {
    DirectRadiation,        // sound from sources through a homogeneous medium
    Interference,           // phase-correct superposition
    SourceDirectivity,      // directivity models of emitters
    SingleReflection,       // one reflection from a surface
    MultipleReflection,     // repeated reflections, standing waves between surfaces
    Diffraction,            // around edges and through apertures
    Shadowing,              // of one object by another
    Transmission,           // into and through penetrable materials
    Refraction,             // change of direction at an interface
    Absorption,             // losses in the medium or in materials
    Resonance,              // of enclosures and cavities
    TransientSignals,       // pulses, bursts, true time delays
    Broadband,              // many frequencies in one run
    HeterogeneousMedia,     // spatially varying density and speed of sound
    ParticleVelocity,       // and therefore intensity
    ShearWaves,             // elastic waves in solids
    Nonlinearity,           // finite-amplitude effects
    Flow                    // moving media
};

std::string_view toString(Phenomenon phenomenon);

struct SolverDescriptor {
    std::string id;
    std::string name;
    std::string version;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    std::string method;                  // the numerical method, in a few words
    bool fullWave = false;               // solves the wave equation without high-frequency approximations
    bool timeDomain = false;
    bool supports2D = false, supports3D = true;
    std::vector<Phenomenon> represents;
    std::string costScaling;             // how the work grows
    std::string applicability;           // when to use it
    std::vector<std::string> limitations;

    bool represents_(Phenomenon phenomenon) const;
};

// What a problem contains and asks for.
struct ProblemTraits {
    int dimensions = 3;                  // of the selected region: 2 (plane) or 3 (volume)
    bool planeRegionMeansSlice = true;   // analytical solvers sample a 3D field on a plane; FDTD solves a 2D problem
    size_t reflectors = 0;               // reflector manipulators
    size_t otherManipulators = 0;        // lenses, waveguides, cavities, diffraction structures
    size_t plainObjects = 0;             // other objects with geometry
    size_t penetrableObjects = 0;        // of all of those: objects simulated as materials
    bool transientSignal = false;        // the requested signal is not a steady sinusoid
    bool closedDomain = false;           // reflecting domain faces were requested
    bool mediumAttenuation = false;
    bool directionalSources = false;
    bool needsIntensity = false;
    double domainWavelengths = 0.0;      // largest extent of the region in wavelengths
    double gridCellUpdates = 0.0;        // estimated work of a grid solver; 0 = unknown
};

struct Assessment {
    const SolverDescriptor* solver = nullptr;
    bool complete = true;                    // represents everything the problem contains
    std::vector<std::string> leftOut;        // what it would ignore or approximate away
    std::vector<std::string> remarks;        // cost and accuracy notes
};

class SolverRegistry {
public:
    void add(SolverDescriptor descriptor);
    const SolverDescriptor* find(std::string_view id) const;
    const std::vector<SolverDescriptor>& solvers() const { return solvers_; }

    // One assessment per registered solver of the domain, in registration
    // order. Nothing is selected.
    std::vector<Assessment> assess(const ProblemTraits& traits, PhysicsDomain domain = PhysicsDomain::Acoustic) const;

private:
    std::vector<SolverDescriptor> solvers_;
};

// The acoustic solvers this build contains.
void registerAcousticSolvers(SolverRegistry& registry);
const SolverRegistry& acousticSolvers();

// Text for the user: every solver with what it would leave out.
std::string describeAssessments(const std::vector<Assessment>& assessments);

}  // namespace resonant::solvers
