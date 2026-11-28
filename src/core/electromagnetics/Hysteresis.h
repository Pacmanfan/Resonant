#pragma once

// Magnetic hysteresis by the Preisach model (RES-DMS-008): relay hysterons on a grid of
// switching fields alpha >= beta in [-Hs, Hs], each up when H >= alpha and down when
// H <= beta, weighted by a density (uniform, or Gaussian in the coercive field
// (alpha - beta) / 2 and the interaction field (alpha + beta) / 2). The relay states are
// the memory (irreversible); M = Ms sum(w gamma) + chi_r H adds a reversible part.
// Return-point memory (wiping out) and congruent minor loops follow from the model;
// the loss per cycle per volume is the loop area mu0 closed-integral H dM.
// Uniform density, from negative saturation: M_irr / Ms = -1 + (H + Hs)^2 / (2 Hs^2), and
// the major loop's area is (4/3) Ms Hs (continuum limit; the grid converges as 1/N).

#include "foundation/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace resonant::em {

struct PreisachParameters {
    double saturationMagnetization = 0.0;  // Ms, A/m (the irreversible part)
    double saturationField = 0.0;          // Hs, A/m: every relay switches within [-Hs, Hs]
    double reversibleSusceptibility = 0.0; // chi_r
    int levels = 100;                      // relays per field axis
    std::string density = "uniform";       // or "gaussian"
    double coercivity = 0.0;               // mean coercive field (gaussian), A/m
    double coerciveSpread = 0.0, interactionSpread = 0.0;  // A/m (gaussian)
    Status validate() const;
};

class PreisachModel {
public:
    static Result<PreisachModel> create(const PreisachParameters& p);
    // Negative saturation (every relay down) and H = -Hs.
    void reset();
    // Applies a field and returns M (A/m).
    double apply(double H);
    double field() const { return H_; }
    double magnetization() const;
    double irreversibleFraction() const;  // sum(w gamma) in [-1, 1]
    const std::vector<std::int8_t>& relays() const { return state_; }
    // The alternating extrema that define the present state (the "staircase" memory).
    const std::vector<double>& memory() const { return memory_; }
    const PreisachParameters& parameters() const { return p_; }

private:
    PreisachParameters p_;
    std::vector<double> alpha_, beta_, weight_;
    std::vector<std::int8_t> state_;
    std::vector<double> memory_;
    double H_ = 0.0;
};

}  // namespace resonant::em
