#include "coupling/StructuralRadiation.h"

#include <cmath>

namespace resonant::coupling {

Result<std::shared_ptr<acoustics::FieldResult>> radiateFromPlate(const mechanics::StructuralModel& model, const Eigen::VectorXcd& u,
                                                                 double frequencyHz, const acoustics::Medium& medium,
                                                                 const acoustics::GridFrame& frame, const std::string& name)
{
    using Failure = Result<std::shared_ptr<acoustics::FieldResult>>;
    using mechanics::ElementKind;
    if (model.kind != ElementKind::Plate && model.kind != ElementKind::Membrane)
        return Failure::failure("sound radiation is calculated for plates and membranes only (the Rayleigh integral needs a flat "
                                "surface in a baffle)");
    if (static_cast<size_t>(u.size()) != model.dofCount())
        return Failure::failure("the displacement does not belong to this structure");
    if (!(frequencyHz > 0.0))
        return Failure::failure("the frequency must be positive");
    const double omega = 2.0 * 3.14159265358979323846 * frequencyHz, k = omega / medium.soundSpeed;
    const std::complex<double> j(0.0, 1.0);

    // Elements as point radiators: centre, area, volume velocity.
    std::vector<Vec3> centres;
    std::vector<std::complex<double>> strength;  // v A
    double largest = 0.0;
    for (const mechanics::Element& e : model.elements) {
        Vec3 c = Vec3::Zero();
        std::complex<double> w(0.0, 0.0);
        for (int q = 0; q < 4; ++q) {
            const auto node = static_cast<size_t>(e.nodes[static_cast<size_t>(q)]);
            c += 0.25 * model.nodes[node];
            w += 0.25 * u(static_cast<Eigen::Index>(model.dof(node, 0)));
        }
        const Vec3 d1 = model.nodes[static_cast<size_t>(e.nodes[1])] - model.nodes[static_cast<size_t>(e.nodes[0])];
        const Vec3 d2 = model.nodes[static_cast<size_t>(e.nodes[3])] - model.nodes[static_cast<size_t>(e.nodes[0])];
        const double area = d1.cross(d2).norm();
        largest = std::max(largest, std::max(d1.norm(), d2.norm()));
        centres.push_back(c);
        strength.push_back(j * omega * w * area);
    }
    const Vec3 origin = model.nodes.front();
    auto field = std::make_shared<acoustics::FieldResult>();
    field->id = Id::generate();
    field->name = name;
    field->frame = frame;
    field->frequencyHz = frequencyHz;
    field->medium = medium;
    field->pressure.resize(frame.pointCount());
    field->nearSource.assign(frame.pointCount(), 0);
    double sum = 0.0;
    size_t counted = 0;
    for (int w = 0; w < frame.nw; ++w)
        for (int v = 0; v < frame.nv; ++v)
            for (int i = 0; i < frame.nu; ++i) {
                const size_t index = frame.index(i, v, w);
                const Vec3 x = frame.point(i, v, w);
                const double side = (x - origin).dot(model.normal);
                if (side <= 0.5 * largest) {
                    // Behind the baffle, or closer to the surface than an element: not calculated.
                    field->nearSource[index] = 1;
                    field->pressure[index] = {0.0f, 0.0f};
                    ++field->statistics.nearSourceSamples;
                    continue;
                }
                std::complex<double> p(0.0, 0.0);
                for (size_t e = 0; e < centres.size(); ++e) {
                    const double r = (x - centres[e]).norm();
                    p += strength[e] * std::exp(-j * k * r) / r;
                }
                p *= j * omega * medium.density / (2.0 * 3.14159265358979323846);
                field->pressure[index] = {static_cast<float>(p.real()), static_cast<float>(p.imag())};
                const double magnitude = std::abs(p);
                sum += magnitude;
                ++counted;
                if (magnitude > field->statistics.maxMagnitude) {
                    field->statistics.maxMagnitude = magnitude;
                    field->statistics.maxPoint = x;
                }
            }
    field->statistics.meanMagnitude = counted ? sum / static_cast<double>(counted) : 0.0;
    field->assumptions = {
        "One-way coupling: the structure's vibration is calculated in vacuo and taken as given; the fluid does not load the structure.",
        "Rayleigh integral of a flat vibrating surface in an infinite rigid baffle; the field is calculated only in front of the "
        "baffle, at least half an element away from the surface (other samples are excluded).",
        "Each element radiates as a point with its centre's normal velocity: valid where the element is small against the acoustic "
        "wavelength.",
        "Linear acoustics in a homogeneous medium at rest; time convention e^{+j omega t}, peak amplitudes."};
    if (largest > medium.wavelength(frequencyHz) / 6.0)
        field->assumptions.push_back("WARNING: the elements are larger than a sixth of the acoustic wavelength; the radiated field is inaccurate.");
    field->metadata.domain = PhysicsDomain::Acoustic;
    field->metadata.solverId = "coupling.structure-to-acoustic.rayleigh";
    field->metadata.solverVersion = "1";
    field->metadata.assumptions = field->assumptions;
    return field;
}

}  // namespace resonant::coupling
