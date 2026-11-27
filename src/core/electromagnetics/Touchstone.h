#pragma once

// Touchstone files: S-parameters of 1 ... N ports (.s1p, .s2p, .s3p, .s4p,
// .sNp), versions 1.1 and 2.0.
//
// Reading keeps what the file states: the frequency unit, the number format
// (RI, MA, DB), the reference impedance of every port, the port ordering and
// the comments. Values are held as complex S_ij. Touchstone 1 writes a
// two-port's data as S11 S21 S12 S22 and larger networks row by row
// (S11 S12 ... S1N, S21 ...), several lines per frequency; Touchstone 2 states
// the two-port order with [Two-Port Data Order]. Only full S matrices are read
// (Y, Z, H, G parameters, mixed-mode data and lower / upper triangular
// matrices are refused, not converted).
#include "electromagnetics/Conventions.h"
#include "foundation/Result.h"

#include <string>
#include <vector>

namespace resonant::em {

struct OnePortData {
    std::vector<double> frequencyHz;
    std::vector<Complex> reflection;  // S11
    double referenceImpedance = 50.0;
    std::string comments;
    // Input impedance Z0 (1 + S) / (1 - S).
    std::vector<Complex> impedance() const;
};

Result<OnePortData> parseTouchstone1(const std::string& text);
// Written as GHz, RI, with the comments as "!" lines.
std::string writeTouchstone1(const OnePortData& data);

struct NetworkParameters {
    int ports = 1;
    std::vector<double> frequencyHz;
    // Per frequency, row-major: s[f][(i - 1) * ports + (j - 1)] = S_ij.
    std::vector<std::vector<Complex>> s;
    std::vector<double> referenceImpedance;  // per port, ohm
    std::string version = "1.1";             // as read
    std::string sourceFormat = "MA";         // RI, MA or DB, as read
    std::string sourceFrequencyUnit = "GHZ"; // as read
    std::string comments;
    Complex at(size_t frequency, int i, int j) const { return s[frequency][static_cast<size_t>((i - 1) * ports + (j - 1))]; }
};

// The port count of a file name ending in .sNp (case-insensitive), or 0.
int portsFromFileName(const std::string& name);
// ports = 0: from [Number of Ports] (version 2) or from a one- or two-port
// data line; three or more ports in a version-1 file need the count (from
// the file name).
Result<NetworkParameters> parseTouchstone(const std::string& text, int ports = 0);
// Version 1.1 (Hz, RI) when every port has the same reference impedance,
// version 2.0 with [Reference] otherwise.
std::string writeTouchstone(const NetworkParameters& data);
// S11 of a network as one-port data.
OnePortData reflectionOf(const NetworkParameters& data, int port = 1);

}  // namespace resonant::em
