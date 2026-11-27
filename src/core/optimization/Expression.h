#pragma once

// A small arithmetic expression language for relations between design
// parameters: derived variables ("0.5 * wavelength"), dependent dimensions
// ("aperture / 4") and relational constraints ("aperture - 4 * focalLength").
//
//   expression := term (('+' | '-') term)*
//   term       := factor (('*' | '/') factor)*
//   factor     := ('+' | '-') factor | power
//   power      := primary ('^' factor)?
//   primary    := number | name | '[' id ']' | name '(' arguments ')' | '(' expression ')'
//
// Names are design-variable ids or constants the adapter provides (for
// example "wavelength"): letters, digits, '_' and '.'; an id with other
// characters is written in square brackets, [object/1f2e/geometry/aperture].
// Functions: sqrt, abs,
// sin, cos, tan, exp, log, min, max, pow. "pi" is predefined. Everything is
// evaluated in double precision with SI values; the language has no units,
// so the expression's author is responsible for dimensional consistency.
#include "foundation/Result.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace resonant::optimization {

class Expression {
public:
    Expression() = default;
    // Parses the text; fails with the position and reason of the first error.
    static Result<Expression> parse(const std::string& text);

    const std::string& text() const { return text_; }
    bool empty() const { return !root_; }
    // The names the expression refers to (excluding functions and "pi").
    std::vector<std::string> names() const;
    // Evaluates with the given values; fails for an unknown name or a
    // non-finite result.
    Result<double> evaluate(const std::map<std::string, double, std::less<>>& values) const;

    struct Node;

private:
    std::string text_;
    std::shared_ptr<const Node> root_;
};

}  // namespace resonant::optimization
