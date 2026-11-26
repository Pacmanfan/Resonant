#include "acoustics/Focusing.h"

#include "foundation/Units.h"

#include <cmath>

namespace resonant::acoustics {

std::string_view toString(FocusControl control)
{
    return control == FocusControl::Phase ? "phase" : "timeDelay";
}

std::string_view toString(Taper taper)
{
    switch (taper) {
    case Taper::Uniform: return "uniform";
    case Taper::Hann: return "hann";
    case Taper::Hamming: return "hamming";
    case Taper::Cosine: return "cosine";
    }
    return "uniform";
}

namespace {
double taperWeight(Taper taper, double normalized)  // normalized distance from the centre in [0, 1)
{
    switch (taper) {
    case Taper::Uniform: return 1.0;
    case Taper::Hann: return 0.5 * (1.0 + std::cos(units::pi * normalized));
    case Taper::Hamming: return 0.54 + 0.46 * std::cos(units::pi * normalized);
    case Taper::Cosine: return std::cos(0.5 * units::pi * normalized);
    }
    return 1.0;
}
}  // namespace

FocusSolution computeFocus(const std::vector<ArrayElement>& elements, const Mat4& arrayWorld, const Vec3& target,
                           const Medium& medium, double frequencyHz, FocusControl control, Taper taper)
{
    FocusSolution solution;
    solution.control = control;
    solution.frequencyHz = frequencyHz;
    solution.soundSpeed = medium.soundSpeed;
    if (elements.empty())
        return solution;

    Vec3 centroid = Vec3::Zero();
    for (const ArrayElement& element : elements)
        centroid += element.localPosition;
    centroid /= static_cast<double>(elements.size());
    double maxFromCentre = 0.0;
    double maxDistance = 0.0;
    for (const ArrayElement& element : elements) {
        maxFromCentre = std::max(maxFromCentre, (element.localPosition - centroid).norm());
        maxDistance = std::max(maxDistance, (target - transformPoint(arrayWorld, element.localPosition)).norm());
    }
    // One element pitch beyond the edge, so edge weights are not exactly zero.
    const double taperExtent = maxFromCentre * (1.0 + 1.0 / std::max<size_t>(elements.size(), 2));

    const double k = medium.wavenumber(frequencyHz);
    for (const ArrayElement& element : elements) {
        FocusElement entry;
        entry.index = element.index;
        entry.distance = (target - transformPoint(arrayWorld, element.localPosition)).norm();
        if (control == FocusControl::Phase) {
            entry.phaseRad = std::fmod(k * entry.distance, 2.0 * units::pi);
        } else {
            entry.delaySec = (maxDistance - entry.distance) / medium.soundSpeed;
        }
        entry.weight = taperExtent > 0.0
                           ? taperWeight(taper, (element.localPosition - centroid).norm() / taperExtent)
                           : 1.0;
        solution.elements.push_back(entry);
    }
    return solution;
}

ArraySettings applyFocus(const ArraySettings& settings, const FocusSolution& solution, Taper taper)
{
    ArraySettings result = settings;
    for (const FocusElement& entry : solution.elements) {
        ElementOverride& o = result.overrides[entry.index];
        if (solution.control == FocusControl::Phase) {
            o.phaseRad = entry.phaseRad;
            o.delaySec.reset();
        } else {
            o.delaySec = entry.delaySec;
            o.phaseRad.reset();
        }
        if (taper != Taper::Uniform)
            o.amplitudePa = settings.element.amplitudePa * entry.weight;
    }
    return result;
}

}  // namespace resonant::acoustics
