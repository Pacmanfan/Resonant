#include "atomistic/StructureImport.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace resonant::atomistic {

namespace {
constexpr double kAngstrom = 1e-10;

std::vector<std::string> lines(const std::string& text)
{
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string l;
    while (std::getline(ss, l)) {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        out.push_back(l);
    }
    return out;
}

std::vector<std::string> words(const std::string& l)
{
    std::vector<std::string> w;
    std::stringstream ss(l);
    std::string x;
    while (ss >> x)
        w.push_back(x);
    return w;
}

bool number(const std::string& s, double& v)
{
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end && *end == '\0' && std::isfinite(v);
}
}  // namespace

Result<Crystal> parseXyz(const std::string& text)
{
    using R = Result<Crystal>;
    const auto ls = lines(text);
    if (ls.size() < 2)
        return R::failure("an XYZ file starts with the atom count and a comment line");
    const auto first = words(ls[0]);
    double count = 0.0;
    if (first.size() != 1 || !number(first[0], count) || count < 1 || count != std::floor(count))
        return R::failure("the first XYZ line must be the number of atoms");
    const size_t n = static_cast<size_t>(count);
    Crystal c;
    c.name = "imported XYZ";
    c.periodic = false;
    // Extended XYZ: Lattice="..." in the comment line.
    const std::string& comment = ls[1];
    const auto at = comment.find("Lattice=\"");
    if (at != std::string::npos) {
        const auto close = comment.find('"', at + 9);
        if (close == std::string::npos)
            return R::failure("the Lattice=\"...\" field is not closed");
        const auto v = words(comment.substr(at + 9, close - at - 9));
        if (v.size() != 9)
            return R::failure("Lattice= needs nine numbers");
        for (int k = 0; k < 9; ++k) {
            double x = 0.0;
            if (!number(v[static_cast<size_t>(k)], x))
                return R::failure("Lattice= holds a value that is not a number");
            c.lattice(k % 3, k / 3) = x * kAngstrom;
        }
        c.periodic = true;
    }
    if (ls.size() < 2 + n)
        return R::failure("the XYZ file lists fewer atoms than its count (" + std::to_string(n) + ")");
    std::vector<Eigen::Vector3d> cart;
    for (size_t i = 0; i < n; ++i) {
        const auto w = words(ls[2 + i]);
        double x, y, z;
        if (w.size() < 4 || !number(w[1], x) || !number(w[2], y) || !number(w[3], z))
            return R::failure("XYZ atom line " + std::to_string(i + 1) + " must be: species x y z");
        c.atoms.push_back({w[0], Eigen::Vector3d(x, y, z) * kAngstrom});
    }
    for (size_t i = 2 + n; i < ls.size(); ++i)
        if (!words(ls[i]).empty())
            return R::failure("the XYZ file has more lines than its atom count: a multi-frame file is not supported");
    if (c.periodic) {
        if (!(std::abs(c.lattice.determinant()) > 0.0))
            return R::failure("the lattice vectors are not independent");
        const Eigen::Matrix3d inv = c.lattice.inverse();
        for (Atom& a : c.atoms)
            a.position = inv * a.position;  // to fractional
    }
    if (Status v = c.validate(); !v)
        return R::failure(v.message());
    return c;
}

Result<Crystal> parsePoscar(const std::string& text)
{
    using R = Result<Crystal>;
    const auto ls = lines(text);
    if (ls.size() < 8)
        return R::failure("a POSCAR file needs a comment, a scale, three lattice vectors, species, counts and a coordinate mode");
    Crystal c;
    c.name = ls[0].empty() ? "imported POSCAR" : ls[0];
    double scale = 0.0;
    const auto s = words(ls[1]);
    if (s.size() != 1 || !number(s[0], scale) || !(scale > 0.0))
        return R::failure("the POSCAR scale must be one positive number (negative volumes are not supported)");
    for (int k = 0; k < 3; ++k) {
        const auto w = words(ls[static_cast<size_t>(2 + k)]);
        double x, y, z;
        if (w.size() < 3 || !number(w[0], x) || !number(w[1], y) || !number(w[2], z))
            return R::failure("POSCAR lattice vector " + std::to_string(k + 1) + " needs three numbers");
        c.lattice.col(k) = Eigen::Vector3d(x, y, z) * scale * kAngstrom;
    }
    const auto species = words(ls[5]);
    double probe = 0.0;
    if (species.empty() || number(species[0], probe))
        return R::failure("the POSCAR file has no species line (VASP 4 format): species names are needed and are not guessed");
    const auto counts = words(ls[6]);
    if (counts.size() != species.size())
        return R::failure("POSCAR: as many counts as species");
    std::vector<int> n;
    size_t total = 0;
    for (const auto& w : counts) {
        double x = 0.0;
        if (!number(w, x) || x < 1 || x != std::floor(x))
            return R::failure("POSCAR counts must be positive integers");
        n.push_back(static_cast<int>(x));
        total += static_cast<size_t>(x);
    }
    size_t line = 7;
    std::string mode = ls[line];
    if (!mode.empty() && (mode[0] == 'S' || mode[0] == 's'))
        mode = ls[++line];  // selective dynamics: the flags are ignored
    const bool cartesian = !mode.empty() && (mode[0] == 'C' || mode[0] == 'c' || mode[0] == 'K' || mode[0] == 'k');
    const bool direct = !mode.empty() && (mode[0] == 'D' || mode[0] == 'd');
    if (!cartesian && !direct)
        return R::failure("POSCAR: the coordinate mode must be Direct or Cartesian");
    ++line;
    if (ls.size() < line + total)
        return R::failure("POSCAR: fewer coordinate lines than atoms");
    const Eigen::Matrix3d inv = c.lattice.inverse();
    size_t k = 0;
    for (size_t sp = 0; sp < species.size(); ++sp)
        for (int i = 0; i < n[sp]; ++i, ++k) {
            const auto w = words(ls[line + k]);
            double x, y, z;
            if (w.size() < 3 || !number(w[0], x) || !number(w[1], y) || !number(w[2], z))
                return R::failure("POSCAR coordinate line " + std::to_string(k + 1) + " needs three numbers");
            Eigen::Vector3d p(x, y, z);
            if (cartesian)
                p = inv * (p * scale * kAngstrom);
            c.atoms.push_back({species[sp], p});
        }
    if (Status v = c.validate(); !v)
        return R::failure(v.message());
    return c;
}

Result<Crystal> parseStructure(const std::string& text, const std::string& format)
{
    if (format == "xyz")
        return parseXyz(text);
    if (format == "poscar")
        return parsePoscar(text);
    return Result<Crystal>::failure("unknown structure format '" + format + "' (xyz, poscar; CIF and PDB are not supported)");
}

std::string writeXyz(const Crystal& c)
{
    std::string out = std::to_string(c.atoms.size()) + "\n";
    char buf[256];
    if (c.periodic) {
        out += "Lattice=\"";
        for (int k = 0; k < 9; ++k) {
            std::snprintf(buf, sizeof buf, "%s%.10g", k ? " " : "", c.lattice(k % 3, k / 3) / kAngstrom);
            out += buf;
        }
        out += "\" Properties=species:S:1:pos:R:3\n";
    } else {
        out += c.name + "\n";
    }
    const auto x = c.cartesian();
    for (size_t i = 0; i < c.atoms.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%s %.10g %.10g %.10g\n", c.atoms[i].species.c_str(), x[i].x() / kAngstrom, x[i].y() / kAngstrom, x[i].z() / kAngstrom);
        out += buf;
    }
    return out;
}

}  // namespace resonant::atomistic
