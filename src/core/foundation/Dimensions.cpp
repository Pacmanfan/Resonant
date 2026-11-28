#include "foundation/Dimensions.h"

#include <cctype>
#include <cmath>
#include <map>
#include <vector>

namespace resonant {

Dimension Dimension::base(int index)
{
    Dimension d;
    d.e[static_cast<size_t>(index)] = 1;
    return d;
}

Dimension Dimension::operator*(const Dimension& o) const
{
    Dimension d;
    for (size_t i = 0; i < 7; ++i)
        d.e[i] = e[i] + o.e[i];
    return d;
}

Dimension Dimension::operator/(const Dimension& o) const
{
    Dimension d;
    for (size_t i = 0; i < 7; ++i)
        d.e[i] = e[i] - o.e[i];
    return d;
}

Dimension Dimension::power(int n) const
{
    Dimension d;
    for (size_t i = 0; i < 7; ++i)
        d.e[i] = e[i] * n;
    return d;
}

std::string Dimension::toString() const
{
    static const char* names[7] = {"kg", "m", "s", "K", "A", "mol", "cd"};
    std::string out;
    for (size_t i = 0; i < 7; ++i) {
        if (e[i] == 0)
            continue;
        if (!out.empty())
            out += ' ';
        out += names[i];
        if (e[i] != 1)
            out += "^" + std::to_string(e[i]);
    }
    return out.empty() ? "1" : out;
}

namespace {

enum { KG, M, S, K, A, MOL, CD };

Dimension dim(std::initializer_list<std::pair<int, int>> exps)
{
    Dimension d;
    for (auto [i, n] : exps)
        d.e[static_cast<size_t>(i)] = n;
    return d;
}

struct Named {
    Dimension d;
    double factor;
    bool prefixable;
};

const std::map<std::string, Named>& units()
{
    static const std::map<std::string, Named> u = [] {
        std::map<std::string, Named> m;
        m["1"] = {Dimension::none(), 1.0, false};
        m["%"] = {Dimension::none(), 0.01, false};
        m["rad"] = {Dimension::none(), 1.0, true};
        m["sr"] = {Dimension::none(), 1.0, false};
        m["g"] = {dim({{KG, 1}}), 1e-3, true};
        m["m"] = {dim({{M, 1}}), 1.0, true};
        m["s"] = {dim({{S, 1}}), 1.0, true};
        m["K"] = {dim({{K, 1}}), 1.0, true};
        m["A"] = {dim({{A, 1}}), 1.0, true};
        m["mol"] = {dim({{MOL, 1}}), 1.0, true};
        m["cd"] = {dim({{CD, 1}}), 1.0, false};
        m["Hz"] = {dim({{S, -1}}), 1.0, true};
        m["N"] = {dim({{KG, 1}, {M, 1}, {S, -2}}), 1.0, true};
        m["Pa"] = {dim({{KG, 1}, {M, -1}, {S, -2}}), 1.0, true};
        m["J"] = {dim({{KG, 1}, {M, 2}, {S, -2}}), 1.0, true};
        m["W"] = {dim({{KG, 1}, {M, 2}, {S, -3}}), 1.0, true};
        m["C"] = {dim({{A, 1}, {S, 1}}), 1.0, true};
        m["V"] = {dim({{KG, 1}, {M, 2}, {S, -3}, {A, -1}}), 1.0, true};
        m["F"] = {dim({{KG, -1}, {M, -2}, {S, 4}, {A, 2}}), 1.0, true};
        m["ohm"] = {dim({{KG, 1}, {M, 2}, {S, -3}, {A, -2}}), 1.0, true};
        m["\xCE\xA9"] = m["ohm"];  // the Greek capital omega
        m["S"] = {dim({{KG, -1}, {M, -2}, {S, 3}, {A, 2}}), 1.0, true};
        m["Wb"] = {dim({{KG, 1}, {M, 2}, {S, -2}, {A, -1}}), 1.0, true};
        m["T"] = {dim({{KG, 1}, {S, -2}, {A, -1}}), 1.0, true};
        m["H"] = {dim({{KG, 1}, {M, 2}, {S, -2}, {A, -2}}), 1.0, true};
        m["min"] = {dim({{S, 1}}), 60.0, false};
        m["h"] = {dim({{S, 1}}), 3600.0, false};
        m["Wh"] = {dim({{KG, 1}, {M, 2}, {S, -2}}), 3600.0, true};
        m["eV"] = {dim({{KG, 1}, {M, 2}, {S, -2}}), 1.602176634e-19, true};
        m["L"] = {dim({{M, 3}}), 1e-3, true};
        m["bar"] = {dim({{KG, 1}, {M, -1}, {S, -2}}), 1e5, true};
        m["atm"] = {dim({{KG, 1}, {M, -1}, {S, -2}}), 101325.0, false};
        return m;
    }();
    return u;
}

const std::vector<std::pair<std::string, double>>& prefixes()
{
    static const std::vector<std::pair<std::string, double>> p = {
        {"P", 1e15}, {"T", 1e12}, {"G", 1e9}, {"M", 1e6}, {"k", 1e3}, {"c", 1e-2}, {"m", 1e-3},
        {"u", 1e-6}, {"\xC2\xB5", 1e-6}, {"\xCE\xBC", 1e-6}, {"n", 1e-9}, {"p", 1e-12}, {"f", 1e-15}};
    return p;
}

Result<UnitValue> atom(const std::string& symbol)
{
    using R = Result<UnitValue>;
    if (symbol == "degC" || symbol == "\xC2\xB0" "C" || symbol == "degF" || symbol == "\xC2\xB0" "F")
        return R::failure("'" + symbol + "' is an affine temperature scale: use K (a temperature difference in degC is the same in K)");
    if (symbol == "kg")
        return UnitValue{dim({{KG, 1}}), 1.0};
    const auto& u = units();
    if (auto it = u.find(symbol); it != u.end())
        return UnitValue{it->second.d, it->second.factor};
    for (const auto& [p, f] : prefixes())
        if (symbol.size() > p.size() && symbol.compare(0, p.size(), p) == 0) {
            auto it = u.find(symbol.substr(p.size()));
            if (it != u.end() && it->second.prefixable)
                return UnitValue{it->second.d, f * it->second.factor};
        }
    return R::failure("unknown unit '" + symbol + "'");
}

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}

    Result<UnitValue> parse()
    {
        auto r = product();
        if (!r)
            return r;
        skipSpace();
        if (pos_ != s_.size())
            return Result<UnitValue>::failure("unexpected '" + std::string(s_.substr(pos_)) + "' in the unit");
        return r;
    }

private:
    void skipSpace()
    {
        while (pos_ < s_.size() && s_[pos_] == ' ')
            ++pos_;
    }
    bool multiplication()
    {
        // '*', a middle dot (U+00B7, U+22C5), or a space before another factor.
        skipSpace();
        if (pos_ < s_.size() && s_[pos_] == '*') {
            ++pos_;
            return true;
        }
        for (const char* dot : {"\xC2\xB7", "\xE2\x8B\x85"}) {
            const std::string_view d(dot);
            if (s_.substr(pos_, d.size()) == d) {
                pos_ += d.size();
                return true;
            }
        }
        return false;
    }
    Result<UnitValue> product()
    {
        auto first = factor();
        if (!first)
            return first;
        UnitValue v = first.value();
        for (;;) {
            const size_t before = pos_;
            skipSpace();
            if (pos_ >= s_.size() || s_[pos_] == ')')
                break;
            if (s_[pos_] == '/') {
                ++pos_;
                auto f = factor();
                if (!f)
                    return f;
                v.dimension = v.dimension / f.value().dimension;
                v.factor /= f.value().factor;
                continue;
            }
            const bool explicitTimes = multiplication();
            if (!explicitTimes && pos_ == before)
                break;
            auto f = factor();
            if (!f)
                return f;
            v.dimension = v.dimension * f.value().dimension;
            v.factor *= f.value().factor;
        }
        return v;
    }
    Result<UnitValue> factor()
    {
        skipSpace();
        Result<UnitValue> base = Result<UnitValue>::failure("");
        if (pos_ < s_.size() && s_[pos_] == '(') {
            ++pos_;
            base = product();
            if (!base)
                return base;
            skipSpace();
            if (pos_ >= s_.size() || s_[pos_] != ')')
                return Result<UnitValue>::failure("a parenthesis is not closed in the unit");
            ++pos_;
        } else {
            const size_t start = pos_;
            while (pos_ < s_.size()) {
                const unsigned char c = static_cast<unsigned char>(s_[pos_]);
                if (c == ' ' || c == '*' || c == '/' || c == '^' || c == '(' || c == ')')
                    break;
                if (c == 0xC2 && pos_ + 1 < s_.size() && static_cast<unsigned char>(s_[pos_ + 1]) == 0xB7)
                    break;  // middle dot
                if (c == 0xE2 && s_.substr(pos_, 3) == "\xE2\x8B\x85")
                    break;
                ++pos_;
            }
            if (pos_ == start)
                return Result<UnitValue>::failure("a unit is missing in '" + std::string(s_) + "'");
            base = atom(std::string(s_.substr(start, pos_ - start)));
            if (!base)
                return base;
        }
        UnitValue v = base.value();
        if (pos_ < s_.size() && s_[pos_] == '^') {
            ++pos_;
            const size_t start = pos_;
            if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+'))
                ++pos_;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_])))
                ++pos_;
            const std::string digits(s_.substr(start, pos_ - start));
            if (digits.empty() || digits == "-" || digits == "+")
                return Result<UnitValue>::failure("an exponent is missing in '" + std::string(s_) + "'");
            const int n = std::stoi(digits);
            v.dimension = v.dimension.power(n);
            v.factor = std::pow(v.factor, n);
        }
        return v;
    }

    std::string_view s_;
    size_t pos_ = 0;
};

}  // namespace

Result<UnitValue> parseUnit(std::string_view expression)
{
    if (expression.empty())
        return UnitValue{};
    return Parser(expression).parse();
}

Result<double> convertUnit(double value, std::string_view from, std::string_view to)
{
    auto a = parseUnit(from);
    if (!a)
        return Result<double>::failure(a.message());
    auto b = parseUnit(to);
    if (!b)
        return Result<double>::failure(b.message());
    if (a.value().dimension != b.value().dimension)
        return Result<double>::failure("cannot convert " + std::string(from) + " [" + a.value().dimension.toString() + "] to " + std::string(to) + " [" +
                                       b.value().dimension.toString() + "]: the dimensions differ");
    return value * a.value().factor / b.value().factor;
}

bool hasDimension(std::string_view unit, const Dimension& d)
{
    auto u = parseUnit(unit);
    return u && u.value().dimension == d;
}

}  // namespace resonant
