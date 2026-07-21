#pragma once
#include "abacus/unit.hpp"
#include "lexer.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

struct Expression;

using NumberString = double;

struct BinaryExpression {
  std::string op;
  std::unique_ptr<Expression> lhs;
  std::unique_ptr<Expression> rhs;
};

// we keep the target unit as a simple string
// because unit names are not unique and may
// be interpreted differenty depending on what
// is being done with them.
// for instance, "1m to s" should be "1 minute to second"
// while "1m to in" should be "1 meter to inches"
using OpaqueUnit = std::string_view;

struct ConversionExpression {
  std::unique_ptr<Expression> b;
  OpaqueUnit target;
};

struct UnaryExpression {
  std::string_view op;
  std::unique_ptr<Expression> lhs;
};

struct UnitExpression {
  OpaqueUnit unit;
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
  std::vector<std::string_view> aliases;
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
public:
  Parser(std::string_view data, const UnitDatabase &unitDb)
      : m_lexer(data), m_unitDb(unitDb) {}

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
                                              std::string_view unit) {
    return std::make_unique<Expression>(UnitExpression{
        .unit = unit,
        .expr = std::move(inner),
    });
  }

  std::optional<std::string_view> parseUnit() {
    if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
      if (auto unit = m_unitDb.findUnit(std::string{tok->raw})) {
        return tok->raw;
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
      if (!expr)
        expr = makeUnit(makeNumberExpr(1), unit.value());
      else
        expr = makeUnit(std::move(expr), unit.value());
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
            ConversionExpression{.b = std::move(left), .target = *unit});

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
