#pragma once

// Import of atomic structures (RES-MSC-008): XYZ (plain: no cell, so not periodic;
// extended: Lattice="ax ay az bx by bz cx cy cz") and VASP 5 POSCAR (with the species
// line). Coordinates in angstrom are converted to m. Nothing missing is invented: a
// POSCAR without species names, or an XYZ whose atom count disagrees, is refused.

#include "atomistic/Crystal.h"

#include <string>

namespace resonant::atomistic {

Result<Crystal> parseXyz(const std::string& text);
Result<Crystal> parsePoscar(const std::string& text);
// "xyz" or "poscar".
Result<Crystal> parseStructure(const std::string& text, const std::string& format);
std::string writeXyz(const Crystal& crystal);

}  // namespace resonant::atomistic
