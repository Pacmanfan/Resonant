#include "optimization/Expression.h"

#include <cctype>
#include <cmath>
#include <numbers>
#include <set>

namespace resonant::optimization {

struct Expression::Node {
    enum class Kind { Number, Name, Unary, Binary, Call };
    Kind kind = Kind::Number;
    double number = 0.0;
    std::string name;  // Name, Call
    char op = 0;       // Unary, Binary
    std::vector<std::shared_ptr<const Node>> children;
};

namespace {

using NodePtr = std::shared_ptr<const Expression::Node>;

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Result<NodePtr> parse()
    {
        NodePtr node = expression();
        skip();
        if (!error_.empty())
            return Result<NodePtr>::failure(error_);
        if (pos_ != text_.size())
            return Result<NodePtr>::failure(at("unexpected '" + std::string(1, text_[pos_]) + "'"));
        return node;
    }

private:
    std::string at(const std::string& what) const { return what + " at position " + std::to_string(pos_ + 1) + " of '" + text_ + "'"; }
    void fail(const std::string& what)
    {
        if (error_.empty())
            error_ = at(what);
    }
    void skip()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])))
            ++pos_;
    }
    bool accept(char c)
    {
        skip();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }
    static NodePtr make(Expression::Node node) { return std::make_shared<const Expression::Node>(std::move(node)); }
    static NodePtr binary(char op, NodePtr a, NodePtr b)
    {
        Expression::Node n;
        n.kind = Expression::Node::Kind::Binary;
        n.op = op;
        n.children = {std::move(a), std::move(b)};
        return make(std::move(n));
    }

    NodePtr expression()
    {
        NodePtr left = term();
        while (error_.empty()) {
            if (accept('+'))
                left = binary('+', left, term());
            else if (accept('-'))
                left = binary('-', left, term());
            else
                break;
        }
        return left;
    }
    NodePtr term()
    {
        NodePtr left = factor();
        while (error_.empty()) {
            if (accept('*'))
                left = binary('*', left, factor());
            else if (accept('/'))
                left = binary('/', left, factor());
            else
                break;
        }
        return left;
    }
    NodePtr factor()
    {
        if (accept('-')) {
            Expression::Node n;
            n.kind = Expression::Node::Kind::Unary;
            n.op = '-';
            n.children = {factor()};
            return make(std::move(n));
        }
        if (accept('+'))
            return factor();
        NodePtr base = primary();
        if (accept('^'))
            return binary('^', base, factor());
        return base;
    }
    static bool nameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }
    NodePtr primary()
    {
        skip();
        if (pos_ >= text_.size()) {
            fail("unexpected end");
            return make({});
        }
        const char c = text_[pos_];
        if (accept('(')) {
            NodePtr inner = expression();
            if (!accept(')'))
                fail("missing ')'");
            return inner;
        }
        if (c == '[') {
            const size_t close = text_.find(']', pos_);
            if (close == std::string::npos || close == pos_ + 1) {
                fail("an id in brackets needs its closing ']'");
                return make({});
            }
            Expression::Node n;
            n.kind = Expression::Node::Kind::Name;
            n.name = text_.substr(pos_ + 1, close - pos_ - 1);
            pos_ = close + 1;
            return make(std::move(n));
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            const char* begin = text_.c_str() + pos_;
            char* end = nullptr;
            const double value = std::strtod(begin, &end);
            if (end == begin) {
                fail("bad number");
                return make({});
            }
            pos_ += static_cast<size_t>(end - begin);
            Expression::Node n;
            n.number = value;
            return make(std::move(n));
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const size_t start = pos_;
            while (pos_ < text_.size() && nameChar(text_[pos_]))
                ++pos_;
            std::string name = text_.substr(start, pos_ - start);
            if (accept('(')) {
                Expression::Node n;
                n.kind = Expression::Node::Kind::Call;
                n.name = name;
                if (!accept(')')) {
                    do
                        n.children.push_back(expression());
                    while (error_.empty() && accept(','));
                    if (!accept(')'))
                        fail("missing ')' after the arguments of " + name);
                }
                static const std::map<std::string, size_t> arity{{"sqrt", 1}, {"abs", 1}, {"sin", 1}, {"cos", 1}, {"tan", 1},
                                                                  {"exp", 1},  {"log", 1}, {"min", 2}, {"max", 2}, {"pow", 2}};
                const auto known = arity.find(name);
                if (known == arity.end())
                    fail("unknown function '" + name + "'");
                else if (known->second != n.children.size())
                    fail(name + " takes " + std::to_string(known->second) + " argument(s)");
                return make(std::move(n));
            }
            Expression::Node n;
            n.kind = Expression::Node::Kind::Name;
            n.name = std::move(name);
            return make(std::move(n));
        }
        fail("unexpected '" + std::string(1, c) + "'");
        return make({});
    }

    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;
};

void collect(const Expression::Node& node, std::set<std::string>& out)
{
    if (node.kind == Expression::Node::Kind::Name && node.name != "pi")
        out.insert(node.name);
    for (const auto& child : node.children)
        collect(*child, out);
}

Result<double> eval(const Expression::Node& node, const std::map<std::string, double, std::less<>>& values)
{
    using Kind = Expression::Node::Kind;
    switch (node.kind) {
    case Kind::Number: return node.number;
    case Kind::Name: {
        if (node.name == "pi")
            return std::numbers::pi;
        const auto it = values.find(node.name);
        if (it == values.end())
            return Result<double>::failure("unknown name '" + node.name + "'");
        return it->second;
    }
    case Kind::Unary: {
        auto a = eval(*node.children[0], values);
        if (!a)
            return a;
        return -a.value();
    }
    case Kind::Binary: {
        auto a = eval(*node.children[0], values);
        if (!a)
            return a;
        auto b = eval(*node.children[1], values);
        if (!b)
            return b;
        switch (node.op) {
        case '+': return a.value() + b.value();
        case '-': return a.value() - b.value();
        case '*': return a.value() * b.value();
        case '/': return a.value() / b.value();
        case '^': return std::pow(a.value(), b.value());
        default: return Result<double>::failure("unknown operator");
        }
    }
    case Kind::Call: {
        std::vector<double> args;
        for (const auto& child : node.children) {
            auto v = eval(*child, values);
            if (!v)
                return v;
            args.push_back(v.value());
        }
        const std::string& f = node.name;
        if (f == "sqrt") return std::sqrt(args[0]);
        if (f == "abs") return std::abs(args[0]);
        if (f == "sin") return std::sin(args[0]);
        if (f == "cos") return std::cos(args[0]);
        if (f == "tan") return std::tan(args[0]);
        if (f == "exp") return std::exp(args[0]);
        if (f == "log") return std::log(args[0]);
        if (f == "min") return std::min(args[0], args[1]);
        if (f == "max") return std::max(args[0], args[1]);
        if (f == "pow") return std::pow(args[0], args[1]);
        return Result<double>::failure("unknown function '" + f + "'");
    }
    }
    return Result<double>::failure("bad expression");
}

}  // namespace

Result<Expression> Expression::parse(const std::string& text)
{
    Parser parser(text);
    auto root = parser.parse();
    if (!root)
        return Result<Expression>::failure(root.message());
    Expression e;
    e.text_ = text;
    e.root_ = root.take();
    return e;
}

std::vector<std::string> Expression::names() const
{
    std::set<std::string> out;
    if (root_)
        collect(*root_, out);
    return {out.begin(), out.end()};
}

Result<double> Expression::evaluate(const std::map<std::string, double, std::less<>>& values) const
{
    if (!root_)
        return Result<double>::failure("empty expression");
    auto v = eval(*root_, values);
    if (v && !std::isfinite(v.value()))
        return Result<double>::failure("'" + text_ + "' is not finite for these values");
    return v;
}

}  // namespace resonant::optimization
