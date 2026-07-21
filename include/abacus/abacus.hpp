#pragma once
#include "tinyexpr/tinyexpr.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <exception>
#include <expected>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

// Very ugly code for now.
// This is meant to be a calculator backend that with extended natural language
// capabilities. Supporting stuff like '40 % of 150', 'current time now in unix
// epoch' etc...
// For now the math stuff is delegated to tinyexpr.

class Lexer {
public:
  struct Number {
    std::string_view n;
  };

  enum class OperatorType { Add, Subtract, Multiply, Divide, Pow };
  enum class State { Reset, Number, Operator, String };

  enum class TokenType { String, Number, Operator };

  struct String {
    std::string_view data;
  };
  struct Operator {
    char c;
  };

  struct Token {
    std::string_view raw;
    TokenType type;
    std::variant<Number, String, Operator> data;
  };

  Lexer(std::string_view data) : m_data(data), m_cursor(0) {}

  std::optional<Token> peakIf(TokenType type) {
    if (auto tok = peak(); tok && tok->type == type) {
      return tok;
    }
    return std::nullopt;
  }

  std::optional<Token> peak() {
    auto old = m_cursor;
    auto tok = next();
    m_cursor = old;
    return tok;
  }

  // e.g can check whether "to the power of" is next
  bool isWordSequence(std::span<const std::string_view> words) {
    auto old = m_cursor;
    size_t i = 0;

    while (auto tok = peakIf(TokenType::String)) {
      if (i == words.size()) {
        break;
      }
      if (tok->raw != words[i]) {
        break;
      }
      ++i;
      next();
    }

    m_cursor = old;
    return i == words.size();
  }

  std::optional<Token> next() {
    State state = State::Reset;
    size_t startPos = m_cursor;

    const auto getSelection = [&]() -> std::string_view {
      return m_data.substr(startPos, m_cursor - startPos);
    };

    const auto makeToken = [&](TokenType type) {
      return Token{getSelection(), type};
    };

    auto tryCommit = [&]() -> std::optional<Token> {
      switch (state) {
      case State::Number:
        return makeToken(TokenType::Number);
      case State::String:
        return makeToken(TokenType::String);
      case State::Operator:
        return makeToken(TokenType::Operator);
      default:
        return std::nullopt;
      }
    };

    while (m_cursor < m_data.size()) {
      char c = m_data[m_cursor];

      switch (state) {
      case State::Reset: {
        if (std::isdigit(c)) {
          state = State::Number;
          continue;
        }
        if (std::isspace(c)) {
          startPos += 1;
          break;
        }
        if (std::isalpha(c)) {
          state = State::String;
          continue;
        }
        if (!std::isalnum(c)) {
          state = State::Operator;
          break;
        }
        break;
      }
      case State::Number: {
        // TODO: we probably want to only allow one '.' for a valid digit of
        // course. And we will also want to localize it at some point.
        if (!std::isdigit(c) && c != '.') {
          return tryCommit();
        }
        break;
      }
        // TODO: we will probably want multichar operators...
      case State::Operator: {
        return tryCommit();
      }
      case State::String: {
        if (!std::isalpha(c)) {
          return tryCommit();
        }
        break;
      }
      }

      ++m_cursor;
    }

    return tryCommit();
  }

private:
  std::string_view m_data;
  std::string_view::size_type m_cursor;
};

struct UnitDerivative {};
enum class UnitType { Date, Currency, Distance, Temperature };

struct UnitDef {
  std::string id;
  std::vector<std::string> aliases;
  double factor;
  std::string family;
  UnitType type;
};

struct UnitBaseRelation {
  std::string lhs;
  std::string rhs;
  double factor;
};

class UnitDatabase {
public:
  UnitDatabase() {
    registerUnit(UnitDef{
        .id = "inch",
        .aliases = {"in"},
        .factor = 0,
        .family = "imperial",
    });
    registerUnit(UnitDef{
        .id = "meter",
        .aliases = {"m"},
        .factor = 1,
        .family = "metric",
    });
    registerUnit(UnitDef{
        .id = "kilometer",
        .aliases = {"km"},
        .factor = 1e3,
        .family = "metric",
    });
    registerRelation(UnitBaseRelation{
        .lhs = "imperial", .rhs = "metric", .factor = 39.3701});

    registerUnit(UnitDef{.id = "usd", .family = "currency"});
    registerUnit(UnitDef{.id = "gbp", .family = "currency"});
    registerUnit(UnitDef{.id = "eur", .family = "currency"});
  }
  //.factor = 39.3701

  void registerUnit(UnitDef unit) { m_units.emplace_back(unit); }
  void registerRelation(UnitBaseRelation rel) { m_relations.emplace_back(rel); }

  const UnitDef *findUnit(const std::string &id) const {
    auto it = std::ranges::find_if(m_units, [&](const UnitDef &u) {
      return u.id == id || std::ranges::contains(u.aliases, id);
    });

    return it != m_units.end() ? &*it : nullptr;
  }

  std::expected<double, std::string> convert(double n, const UnitDef &from,
                                             const UnitDef &to) {
    // 1km to m
    // 1000m to km

    if (from.family == to.family) {
      return n * from.factor / to.factor;
    }

    auto rel =
        std::ranges::find_if(m_relations, [&](const UnitBaseRelation &rel) {
          return (rel.lhs == from.family && rel.rhs == to.family) ||
                 (rel.lhs == to.family && rel.rhs == from.family);
        });

    if (rel == m_relations.end()) {
      return std::unexpected(
          std::format("No idea how to convert {} to {}. You probably forgot to "
                      "register the unit relation.",
                      from.family, to.family));
    }

    return n * from.factor * rel->factor;
  }

private:
  std::vector<UnitDef> m_units;
  std::vector<UnitBaseRelation> m_relations;
};

struct Expression;

using NumberString = double;

struct BinaryExpression {
  std::string op;
  std::unique_ptr<Expression> lhs;
  std::unique_ptr<Expression> rhs;
};

struct ConversionExpression {
  std::unique_ptr<Expression> b;
  UnitDef target;
};

struct UnaryExpression {
  std::string_view op;
  std::unique_ptr<Expression> lhs;
};

struct UnitExpression {
  UnitDef unit;
  std::unique_ptr<Expression> expr;
};

struct Expression {
  std::variant<BinaryExpression, UnaryExpression, NumberString, UnitExpression,
               ConversionExpression>
      data;

  const BinaryExpression *asBinaryExpression() const {
    return std::get_if<BinaryExpression>(&data);
  }

  const ConversionExpression *asConversion() const {
    return std::get_if<ConversionExpression>(&data);
  }
};

struct AST {
  std::unique_ptr<Expression> root;
};

struct OperatorDefinition {
  std::string_view id;
  std::vector<std::string> aliases;
  int precedence;
};

static std::vector<OperatorDefinition> operators{
    OperatorDefinition{.id = "to", .aliases = {"to", "in"}, .precedence = 1},
    OperatorDefinition{
        .id = "+", .aliases = {"+", "add", "plus"}, .precedence = 2},
    OperatorDefinition{.id = "-", .aliases = {"-", "minus"}, .precedence = 2},
    OperatorDefinition{.id = "*", .aliases = {"*", "mul"}, .precedence = 3},
    OperatorDefinition{.id = "/", .aliases = {"/", "div"}, .precedence = 3},
    OperatorDefinition{
        .id = "%", .aliases = {"%", "mod", "modulo"}, .precedence = 3},
    OperatorDefinition{
        .id = "^", .aliases = {"^", "pow", "power"}, .precedence = 4}};

class Parser {
  // hardcoded list, obviously not desirable
  static constexpr auto CURRENCIES =
      std::to_array<std::string_view>({"usd", "eur", "gbp", "yen", "kwon"});

public:
  Parser(std::string_view data, const UnitDatabase &unitDb)
      : m_lexer(data), m_unitDb(unitDb) {}

  static constexpr bool isModuloOperator(const Lexer::Token &tok) {
    return std::ranges::contains(
        std::initializer_list<std::string_view>{"%", "mod", "modulo"}, tok.raw);
  }

  static constexpr bool isMulOperator(const Lexer::Token &tok) {
    return std::ranges::contains(
        std::initializer_list<std::string_view>{"*", "mul", "multiply"},
        tok.raw);
  }
  static constexpr bool isDivOperator(const Lexer::Token &tok) {
    return std::ranges::contains(
        std::initializer_list<std::string_view>{"/", "div"}, tok.raw);
  }

  static constexpr bool isAdditionOperator(const Lexer::Token &tok) {
    return std::ranges::contains(std::initializer_list{"+", "plus"}, tok.raw);
  }

  static constexpr bool isSubtractOperator(const Lexer::Token &tok) {
    return std::ranges::contains(std::initializer_list{"-", "minus"}, tok.raw);
  }

  static std::unique_ptr<Expression> makeNumberExpr(double n) {
    return std::make_unique<Expression>(NumberString{n});
  }

  static std::unique_ptr<Expression> makeNumberExpr(const std::string &ns) {
    double n;
    std::from_chars(ns.data(), ns.data() + ns.size(), n);
    return makeNumberExpr(n);
  }

  std::unique_ptr<Expression> makeBinExpr(std::unique_ptr<Expression> lhs,
                                          std::unique_ptr<Expression> rhs,
                                          const std::string &op) {
    auto expr = std::make_unique<Expression>();

    return std::make_unique<Expression>(BinaryExpression{
        .op = op, .lhs = std::move(lhs), .rhs = std::move(rhs)});
  }

  static std::unique_ptr<Expression> makeUnit(std::unique_ptr<Expression> inner,
                                              const UnitDef &unit) {
    return std::make_unique<Expression>(UnitExpression{
        .unit = unit,
        .expr = std::move(inner),
    });
  }

  std::optional<UnitDef> parseUnit() {
    if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
      if (auto unit = m_unitDb.findUnit(std::string{tok->raw})) {
        return *unit;
      }
    }
    return std::nullopt;
  }

  std::unique_ptr<Expression> parseTerm() {
    auto expr = std::unique_ptr<Expression>();

    if (m_lexer.peak() && m_lexer.peak()->raw == "(") {
      m_lexer.next();
      expr = pratParse();
      if (m_lexer.peak() && m_lexer.peak()->raw != ")") {
        throw std::runtime_error("expected closing parenthesis");
      }
      m_lexer.next();
    }

    if (m_lexer.peakIf(Lexer::TokenType::Number)) {
      expr = parseNumber();
    }

    // unit can be found after any term.
    if (auto unit = parseUnit()) {
      expr = makeUnit(std::move(expr), *unit);
      m_lexer.next();
    }

    if (!expr) {
      throw std::runtime_error("Expected term");
    }

    return expr;
  }

  std::unique_ptr<Expression> parseNumber() {
    if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
      m_lexer.next();
      return makeNumberExpr(std::string{tok->raw});
    }
    throw std::runtime_error("Expected a number");
  }

  std::unique_ptr<Expression> pratParse(int minPrec = 0) {
    auto left = parseTerm();

    while (auto tok = m_lexer.peak()) {
      if (tok->raw == "to" || tok->raw == "in" | tok->raw == "->") {
        m_lexer.next();
        auto unit = parseUnit();

        if (!unit)
          throw std::runtime_error("expected unit after conversion operator");

        m_lexer.next();

        left = std::make_unique<Expression>(
            ConversionExpression{.b = std::move(left), .target = unit.value()});

        continue;
      }

      auto it =
          std::ranges::find_if(operators, [&](const OperatorDefinition &op) {
            return std::ranges::contains(op.aliases, tok->raw);
          });

      if (it != operators.end()) {
        if (it->precedence < minPrec)
          break;
        m_lexer.next();
        auto right = pratParse(it->precedence + 1);
        left =
            makeBinExpr(std::move(left), std::move(right), std::string{it->id});
      } else {
        break;
      }
    }

    return left;
  }

  AST parse() {
    AST ast;
    ast.root = pratParse();
    return ast;
  }

private:
  Lexer m_lexer;
  const UnitDatabase &m_unitDb;
};

namespace abacus {
struct ComputedValue {
  double n;
  // TODO: maybe we want the pointer in the db, idk...
  // optional is convenient, and no lifetime issues by copying...
  std::optional<UnitDef> unit;

  ComputedValue operator+(const ComputedValue &rhs) const {
    return output(n + rhs.n, rhs);
  }

  ComputedValue operator-(const ComputedValue &rhs) const {
    return output(n - rhs.n, rhs);
  }

  ComputedValue operator*(const ComputedValue &rhs) const {
    return output(n * rhs.n, rhs);
  }

  ComputedValue operator/(const ComputedValue &rhs) const {
    return output(n / rhs.n, rhs);
  }

  ComputedValue operator%(const ComputedValue &rhs) const {
    return output(static_cast<int>(n) % static_cast<int>(rhs.n), rhs);
  }

  ComputedValue pow(const ComputedValue &rhs) const {
    return output(std::pow(n, rhs.n), rhs);
  }

private:
  ComputedValue output(double n, const ComputedValue &rhs) const {
    return ComputedValue{
        .n = n,
        .unit = foldUnit(rhs),
    };
  }

  std::optional<UnitDef> foldUnit(const ComputedValue &rhs) const {
    return rhs.unit.or_else([&]() { return unit; });
  }
};

class Abacus {
public:
  Abacus() = default;

  static std::string tinyexprEvalString(std::string_view expr) {
    te_parser parser;
    auto res = parser.evaluate(expr);

    if (std::isnan(res)) {
      return "";
    }
    return std::format("{:.6g}", res);
  }

  // we may not need tinyexpr after all...
  ComputedValue computeExpr(const Expression &expr) {
    if (auto be = expr.asBinaryExpression()) {
      if (be->op == "+") {
        return computeExpr(*be->lhs) + computeExpr(*be->rhs);
      }
      if (be->op == "-") {
        return computeExpr(*be->lhs) - computeExpr(*be->rhs);
      }
      if (be->op == "*") {
        return computeExpr(*be->lhs) * computeExpr(*be->rhs);
      }
      if (be->op == "/") {
        return computeExpr(*be->lhs) / computeExpr(*be->rhs);
      }
      if (be->op == "%") {
        return computeExpr(*be->lhs) % computeExpr(*be->rhs);
      }
      if (be->op == "^") {
        return computeExpr(*be->lhs).pow(computeExpr(*be->rhs));
      }
      throw std::runtime_error(std::format("Unhandled operator {}", be->op));
    }

    else if (auto conv = expr.asConversion()) {
      auto value = computeExpr(*conv->b);

      // if converted expression has no unit there is nothing to do, just tag it
      // with the target unit...
      if (!value.unit)
        return {.n = value.n, .unit = conv->target};

      if (value.unit->type != conv->target.type) {
        throw std::runtime_error(std::format("Incompatible units ({} to {})",
                                             value.unit->id, conv->target.id));
      }

      std::cout << "converting " << value.unit->id << " to " << conv->target.id
                << std::endl;

      auto res = m_unitDb.convert(value.n, *value.unit, conv->target);

      if (!res)
        throw std::runtime_error(res.error());

      return {.n = res.value(), .unit = conv->target};
    }

    else if (auto n = std::get_if<NumberString>(&expr.data)) {
      return ComputedValue{.n = *n};
    }

    else if (auto ue = std::get_if<UnitExpression>(&expr.data)) {
      double n = computeExpr(*ue->expr).n;
      // since we unitify the expression, we discard any unit the expr might
      // have had
      return ComputedValue{.n = n, .unit = ue->unit};
    }

    throw std::runtime_error("Unhandled expression type");
  }

  /*
  std::string computeExpression(const Expression &expr) {
    if (auto be = std::get_if<BinaryExpression>(&expr.data)) {
      auto str = std::format("({} {} {})", computeExpression(*be->lhs), be->op,
                             computeExpression(*be->rhs));

      std::cout << str << std::endl;

      return tinyexprEvalString(str);
    }

    if (auto n = std::get_if<NumberString>(&expr.data)) {
      return *n;
    }

    throw std::runtime_error("Unhandled expression type");
  }
  */

  /**
   * Parse the expression and return its AST.
   */
  std::expected<AST, std::string> parse(const std::string &expr) {
    return Parser{expr, m_unitDb}.parse();
  }

  std::expected<ComputedValue, std::string> compute(const std::string &expr) {
    try {
      Parser parser{expr, m_unitDb};
      auto ast = parser.parse();

      return computeExpr(*ast.root);
    } catch (const std::exception &e) {
      return std::unexpected(e.what());
    }
  }

  std::expected<std::string, std::string> evaluate(const std::string &expr) {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    auto result = computeExpr(*ast.root);
    return std::format("{:.6g}", result.n);
  }

private:
  UnitDatabase m_unitDb;
};

}; // namespace abacus
