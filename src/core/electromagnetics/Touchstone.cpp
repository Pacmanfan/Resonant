#include "electromagnetics/Touchstone.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace resonant::em {

std::vector<Complex> OnePortData::impedance() const
{
    std::vector<Complex> z;
    for (const Complex& s : reflection)
        z.push_back(referenceImpedance * (1.0 + s) / (1.0 - s));
    return z;
}

Result<OnePortData> parseTouchstone1(const std::string& text)
{
    using Failure = Result<OnePortData>;
    OnePortData d;
    double unit = 1e9;
    enum { RI, MA, DB } format = MA;  // Touchstone defaults: GHz, S, MA, R 50
    std::stringstream stream(text);
    std::string line;
    int number = 0;
    bool option = false;
    while (std::getline(stream, line)) {
        ++number;
        if (const auto bang = line.find('!'); bang != std::string::npos) {
            d.comments += line.substr(bang + 1) + "\n";
            line = line.substr(0, bang);
        }
        std::string upper = line;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        std::stringstream words(upper);
        std::string first;
        if (!(words >> first))
            continue;
        if (first == "#") {
            if (option)
                return Failure::failure("line " + std::to_string(number) + ": a second option line");
            option = true;
            std::string w;
            while (words >> w) {
                if (w == "HZ") unit = 1.0;
                else if (w == "KHZ") unit = 1e3;
                else if (w == "MHZ") unit = 1e6;
                else if (w == "GHZ") unit = 1e9;
                else if (w == "RI") format = RI;
                else if (w == "MA") format = MA;
                else if (w == "DB") format = DB;
                else if (w == "S") {}
                else if (w == "Y" || w == "Z" || w == "H" || w == "G")
                    return Failure::failure("only S parameters are read (the option line names " + w + ")");
                else if (w == "R") {
                    if (!(words >> d.referenceImpedance) || !(d.referenceImpedance > 0.0))
                        return Failure::failure("line " + std::to_string(number) + ": invalid reference impedance");
                }
            }
            continue;
        }
        std::stringstream values(line);
        double f, a, b;
        if (!(values >> f >> a >> b))
            return Failure::failure("line " + std::to_string(number) + ": expected frequency and two values of S11");
        double extra;
        if (values >> extra)
            return Failure::failure("line " + std::to_string(number) + ": more than one port; only .s1p files are read");
        Complex s;
        switch (format) {
        case RI: s = Complex(a, b); break;
        case MA: s = std::polar(a, b * kPi / 180.0); break;
        case DB: s = std::polar(std::pow(10.0, a / 20.0), b * kPi / 180.0); break;
        }
        if (!d.frequencyHz.empty() && f * unit <= d.frequencyHz.back())
            return Failure::failure("line " + std::to_string(number) + ": frequencies must increase");
        d.frequencyHz.push_back(f * unit);
        d.reflection.push_back(s);
    }
    if (d.frequencyHz.empty())
        return Failure::failure("the file contains no data");
    return d;
}

namespace {

std::string upperCase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string trim(const std::string& text)
{
    const auto a = text.find_first_not_of(" \t\r");
    if (a == std::string::npos)
        return {};
    const auto b = text.find_last_not_of(" \t\r");
    return text.substr(a, b - a + 1);
}

}  // namespace

int portsFromFileName(const std::string& name)
{
    const std::string u = upperCase(name);
    const auto dot = u.rfind(".S");
    if (dot == std::string::npos || u.size() < dot + 4 || u.back() != 'P')
        return 0;
    const std::string digits = u.substr(dot + 2, u.size() - dot - 3);
    if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
        return 0;
    return std::stoi(digits);
}

Result<NetworkParameters> parseTouchstone(const std::string& text, int ports)
{
    using Failure = Result<NetworkParameters>;
    NetworkParameters d;
    double unit = 1e9;
    std::string format = "MA";
    double z0 = 50.0;
    bool option = false, version2 = false, inNetworkData = false, twoPortOrder12 = false, ended = false;
    int declaredFrequencies = -1;
    std::vector<double> references;
    bool readingReferences = false;
    std::vector<double> pending;  // the values of the point being read
    std::stringstream stream(text);
    std::string line;
    int number = 0;
    auto groupSize = [&]() { return static_cast<size_t>(1 + 2 * ports * ports); };
    auto finishPoint = [&](int at) -> Status {
        const double f = pending[0] * unit;
        if (!std::isfinite(f) || f < 0.0)
            return Status::failure("line " + std::to_string(at) + ": invalid frequency");
        if (!d.frequencyHz.empty() && f <= d.frequencyHz.back())
            return Status::failure("line " + std::to_string(at) + ": frequencies must increase");
        std::vector<Complex> m(static_cast<size_t>(ports * ports));
        for (int k = 0; k < ports * ports; ++k) {
            const double a = pending[static_cast<size_t>(1 + 2 * k)], b = pending[static_cast<size_t>(2 + 2 * k)];
            if (!std::isfinite(a) || !std::isfinite(b))
                return Status::failure("line " + std::to_string(at) + ": a value is not finite");
            Complex v;
            if (format == "RI")
                v = Complex(a, b);
            else if (format == "MA")
                v = std::polar(a, b * kPi / 180.0);
            else
                v = std::polar(std::pow(10.0, a / 20.0), b * kPi / 180.0);
            int i = k / ports, j = k % ports;
            // The two-port order of Touchstone 1 (and of 2.0 with 21_12): S11 S21 S12 S22.
            if (ports == 2 && !twoPortOrder12)
                std::swap(i, j);
            m[static_cast<size_t>(i * ports + j)] = v;
        }
        d.frequencyHz.push_back(f);
        d.s.push_back(std::move(m));
        pending.clear();
        return Status::success();
    };
    while (std::getline(stream, line)) {
        ++number;
        if (const auto bang = line.find('!'); bang != std::string::npos) {
            d.comments += line.substr(bang + 1) + "\n";
            line = line.substr(0, bang);
        }
        line = trim(line);
        if (line.empty())
            continue;
        if (ended)
            return Failure::failure("line " + std::to_string(number) + ": data after [End]");
        const std::string upper = upperCase(line);
        if (upper[0] == '[') {
            readingReferences = false;
            const auto close = upper.find(']');
            if (close == std::string::npos)
                return Failure::failure("line " + std::to_string(number) + ": unterminated keyword");
            const std::string keyword = upper.substr(1, close - 1);
            const std::string rest = trim(line.substr(close + 1));
            if (keyword == "VERSION") {
                version2 = true;
                d.version = rest;
                if (rest.rfind("2.", 0) != 0)
                    return Failure::failure("Touchstone version " + rest + " is not supported (1.x and 2.x are)");
            } else if (keyword == "NUMBER OF PORTS") {
                const int n = std::atoi(rest.c_str());
                if (n < 1)
                    return Failure::failure("line " + std::to_string(number) + ": invalid number of ports");
                if (ports > 0 && ports != n)
                    return Failure::failure("the file declares " + std::to_string(n) + " ports, but " + std::to_string(ports) +
                                            " were expected");
                ports = n;
            } else if (keyword == "TWO-PORT DATA ORDER") {
                twoPortOrder12 = upperCase(rest) == "12_21";
            } else if (keyword == "NUMBER OF FREQUENCIES") {
                declaredFrequencies = std::atoi(rest.c_str());
            } else if (keyword == "REFERENCE") {
                readingReferences = true;
                std::stringstream values(rest);
                double r;
                while (values >> r)
                    references.push_back(r);
            } else if (keyword == "MATRIX FORMAT") {
                if (upperCase(rest) != "FULL")
                    return Failure::failure("only full matrices are read ([Matrix Format] " + rest + ")");
            } else if (keyword == "MIXED-MODE ORDER") {
                return Failure::failure("mixed-mode data is not read");
            } else if (keyword == "NETWORK DATA") {
                inNetworkData = true;
            } else if (keyword == "END") {
                ended = true;
            } else if (keyword == "NUMBER OF NOISE FREQUENCIES" || keyword == "NOISE DATA") {
                return Failure::failure("noise data is not read");
            }
            continue;
        }
        if (upper[0] == '#') {
            if (option)
                return Failure::failure("line " + std::to_string(number) + ": a second option line");
            option = true;
            std::stringstream words(upper.substr(1));
            std::string w;
            while (words >> w) {
                if (w == "HZ") { unit = 1.0; d.sourceFrequencyUnit = w; }
                else if (w == "KHZ") { unit = 1e3; d.sourceFrequencyUnit = w; }
                else if (w == "MHZ") { unit = 1e6; d.sourceFrequencyUnit = w; }
                else if (w == "GHZ") { unit = 1e9; d.sourceFrequencyUnit = w; }
                else if (w == "RI" || w == "MA" || w == "DB") format = w;
                else if (w == "S") {}
                else if (w == "Y" || w == "Z" || w == "H" || w == "G")
                    return Failure::failure("only S parameters are read (the option line names " + w + ")");
                else if (w == "R") {
                    if (!(words >> z0) || !(z0 > 0.0))
                        return Failure::failure("line " + std::to_string(number) + ": invalid reference impedance");
                }
            }
            d.sourceFormat = format;
            continue;
        }
        std::stringstream values(line);
        std::vector<double> tokens;
        std::string token;
        while (values >> token) {
            char* end = nullptr;
            const double v = std::strtod(token.c_str(), &end);
            if (end == token.c_str() || *end != '\0')
                return Failure::failure("line " + std::to_string(number) + ": '" + token + "' is not a number");
            tokens.push_back(v);
        }
        if (readingReferences) {
            references.insert(references.end(), tokens.begin(), tokens.end());
            continue;
        }
        if (version2 && !inNetworkData)
            return Failure::failure("line " + std::to_string(number) + ": data before [Network Data]");
        if (ports == 0) {
            if (tokens.size() == 3)
                ports = 1;
            else if (tokens.size() == 9)
                ports = 2;
            else
                return Failure::failure("the number of ports cannot be told from the data; give it (from the .sNp file name)");
        }
        if (pending.empty() && ports <= 2 && tokens.size() != groupSize())
            return Failure::failure("line " + std::to_string(number) + ": expected " + std::to_string(groupSize()) + " numbers for a " +
                                    std::to_string(ports) + "-port point, found " + std::to_string(tokens.size()));
        pending.insert(pending.end(), tokens.begin(), tokens.end());
        if (pending.size() > groupSize())
            return Failure::failure("line " + std::to_string(number) + ": too many values for one frequency point");
        if (pending.size() == groupSize())
            if (Status ok = finishPoint(number); !ok)
                return Failure::failure(ok.message());
    }
    if (!pending.empty())
        return Failure::failure("the last frequency point is incomplete");
    if (d.frequencyHz.empty())
        return Failure::failure("the file contains no data");
    if (declaredFrequencies >= 0 && static_cast<size_t>(declaredFrequencies) != d.frequencyHz.size())
        return Failure::failure("the file declares " + std::to_string(declaredFrequencies) + " frequencies but holds " +
                                std::to_string(d.frequencyHz.size()));
    d.ports = ports;
    if (!references.empty()) {
        if (references.size() != static_cast<size_t>(ports))
            return Failure::failure("[Reference] gives " + std::to_string(references.size()) + " impedances for " +
                                    std::to_string(ports) + " ports");
        d.referenceImpedance = references;
    } else {
        d.referenceImpedance.assign(static_cast<size_t>(ports), z0);
    }
    for (double r : d.referenceImpedance)
        if (!(r > 0.0))
            return Failure::failure("reference impedances must be positive");
    return d;
}

std::string writeTouchstone(const NetworkParameters& d)
{
    std::string out;
    std::stringstream comments(d.comments);
    std::string line;
    while (std::getline(comments, line))
        out += "! " + line + "\n";
    const bool sameReference = std::all_of(d.referenceImpedance.begin(), d.referenceImpedance.end(),
                                           [&](double r) { return r == d.referenceImpedance.front(); });
    const double z0 = d.referenceImpedance.empty() ? 50.0 : d.referenceImpedance.front();
    char buffer[200];
    if (!sameReference) {
        out += "[Version] 2.0\n";
        std::snprintf(buffer, sizeof buffer, "# HZ S RI R %.10g\n[Number of Ports] %d\n", z0, d.ports);
        out += buffer;
        if (d.ports == 2)
            out += "[Two-Port Data Order] 21_12\n";
        std::snprintf(buffer, sizeof buffer, "[Number of Frequencies] %zu\n[Reference]", d.frequencyHz.size());
        out += buffer;
        for (double r : d.referenceImpedance) {
            std::snprintf(buffer, sizeof buffer, " %.10g", r);
            out += buffer;
        }
        out += "\n[Network Data]\n";
    } else {
        std::snprintf(buffer, sizeof buffer, "# HZ S RI R %.10g\n", z0);
        out += buffer;
    }
    for (size_t f = 0; f < d.frequencyHz.size(); ++f) {
        std::snprintf(buffer, sizeof buffer, "%.12g", d.frequencyHz[f]);
        out += buffer;
        int onLine = 0;
        for (int i = 1; i <= d.ports; ++i)
            for (int j = 1; j <= d.ports; ++j) {
                // Two-port: S11 S21 S12 S22.
                const Complex v = d.ports == 2 ? d.at(f, j, i) : d.at(f, i, j);
                if (d.ports > 2 && onLine == 4) {
                    out += "\n";
                    onLine = 0;
                }
                std::snprintf(buffer, sizeof buffer, " %.12g %.12g", v.real(), v.imag());
                out += buffer;
                ++onLine;
            }
        out += "\n";
    }
    if (!sameReference)
        out += "[End]\n";
    return out;
}

OnePortData reflectionOf(const NetworkParameters& d, int port)
{
    OnePortData o;
    o.frequencyHz = d.frequencyHz;
    for (size_t f = 0; f < d.frequencyHz.size(); ++f)
        o.reflection.push_back(d.at(f, port, port));
    o.referenceImpedance = d.referenceImpedance.empty() ? 50.0 : d.referenceImpedance[static_cast<size_t>(port - 1)];
    o.comments = d.comments;
    return o;
}

std::string writeTouchstone1(const OnePortData& d)
{
    std::string out;
    std::stringstream comments(d.comments);
    std::string line;
    while (std::getline(comments, line))
        out += "! " + line + "\n";
    char buffer[160];
    std::snprintf(buffer, sizeof buffer, "# GHz S RI R %.10g\n", d.referenceImpedance);
    out += buffer;
    for (size_t i = 0; i < d.frequencyHz.size(); ++i) {
        std::snprintf(buffer, sizeof buffer, "%.12g %.12g %.12g\n", d.frequencyHz[i] / 1e9, d.reflection[i].real(), d.reflection[i].imag());
        out += buffer;
    }
    return out;
}

}  // namespace resonant::em
