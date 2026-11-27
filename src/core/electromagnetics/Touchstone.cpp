#include "electromagnetics/Touchstone.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
