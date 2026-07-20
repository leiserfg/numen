#pragma once
#include "tinyexpr/tinyexpr.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <expected>
#include <iostream>
#include <memory>
#include <numbers>
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

struct Expression;

using NumberString = std::string;

struct BinaryExpression {
  std::string_view op;
  std::unique_ptr<Expression> lhs;
  std::unique_ptr<Expression> rhs;
};

struct UnaryExpression {
  std::string_view op;
  Expression *lhs;
};

enum class ExpressionType {
  Date,
  Currency,
  Real,
};

struct Expression {
  std::variant<BinaryExpression, UnaryExpression, NumberString> data;
  ExpressionType type = ExpressionType::Real;
};

struct AST {
  std::unique_ptr<Expression> root;
};

class Parser {
public:
  Parser(std::string_view data) : m_lexer(data) {}

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

  static std::unique_ptr<Expression> makeNumberExpr(auto n) {
    return std::make_unique<Expression>(NumberString{n});
  }

  template <typename T>
  std::unique_ptr<Expression>
  parseHigher(const T &fn, std::initializer_list<std::string_view> operators) {
    auto left = fn();

    while (m_lexer.peak() &&
           std::ranges::contains(operators, m_lexer.peak()->raw)) {
      auto tok = m_lexer.peak();
      m_lexer.next();
      auto right = fn();
      left = std::make_unique<Expression>(BinaryExpression({
          .op = tok->raw,
          .lhs = std::move(left),
          .rhs = std::move(right),
      }));
    }

    return left;
  }

  std::unique_ptr<Expression> parseTerm() {
    if (m_lexer.peak() && m_lexer.peak()->raw == "(") {
      m_lexer.next();
      auto inner = parseAddition();
      if (m_lexer.peak() && m_lexer.peak()->raw != ")") {
        throw std::runtime_error("expected closing parenthesis");
      }
      m_lexer.next();
      return inner;
    }

    if (m_lexer.peakIf(Lexer::TokenType::Number)) {
      return parseNumber();
    }

    throw std::runtime_error("Expected term");
  }

  std::unique_ptr<Expression> parseNumber() {
    if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
      m_lexer.next();
      return makeNumberExpr(tok->raw);
    }
    throw std::runtime_error("Expected a number");
  }

  std::unique_ptr<Expression> parseExp() {
    auto left = parseTerm();

    while (auto tok = m_lexer.peak()) {
      std::initializer_list<std::string_view> longForm{"to", "the", "power",
                                                       "of"};
      bool isLongForm = m_lexer.isWordSequence(longForm);

      if (tok->raw == "power" || tok->raw == "^" || tok->raw == "exp" ||
          isLongForm) {
        size_t nskip = isLongForm ? longForm.size() : 1;

        for (auto i = 0; i != nskip; ++i) {
          m_lexer.next();
        }

        auto right = parseTerm();
        left = std::make_unique<Expression>(BinaryExpression({
            .op = "^",
            .lhs = std::move(left),
            .rhs = std::move(right),
        }));
      } else {
        break;
      }
    }

    return left;
  }

  // 100 + 40% of 100 * 5

  std::unique_ptr<Expression> parseMul() {
    auto left = parseExp();

    while (auto tok = m_lexer.peak()) {
      if (isModuloOperator(tok.value())) {
        m_lexer.next();
        bool isPctOf = tok->raw == "%" && m_lexer.peak()->raw == "of";

        if (isPctOf) {
          m_lexer.next();

          // X% of Y should transform to (X/100 * Y)

          auto factor = std::make_unique<Expression>(
              BinaryExpression({.op = "/",
                                .lhs = std::move(left),
                                .rhs = makeNumberExpr("100")}));

          auto right = parseExp();

          left = std::make_unique<Expression>(BinaryExpression({
              .op = "*",
              .lhs = std::move(factor),
              .rhs = std::move(right),
          }));
        } else {

          auto right = parseExp();
          left = std::make_unique<Expression>(BinaryExpression({
              .op = "%",
              .lhs = std::move(left),
              .rhs = std::move(right),
          }));
        }
      } else if (isMulOperator(tok.value())) {
        m_lexer.next();
        auto right = parseExp();
        left = std::make_unique<Expression>(BinaryExpression({
            .op = "*",
            .lhs = std::move(left),
            .rhs = std::move(right),
        }));
      } else if (isDivOperator(tok.value())) {
        m_lexer.next();
        auto right = parseExp();
        left = std::make_unique<Expression>(BinaryExpression({
            .op = "/",
            .lhs = std::move(left),
            .rhs = std::move(right),
        }));
      } else {
        break;
      }
    }

    return left;
  }

  std::unique_ptr<Expression> parseAddition() {
    return parseHigher([this]() { return parseMul(); }, {"+", "-"});
  }

  AST parse() {
    AST ast;
    ast.root = parseAddition();
    return ast;
  }

private:
  Lexer m_lexer;
};

namespace abacus {
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

  static std::string computeExpression(const Expression &expr) {
    if (auto be = std::get_if<BinaryExpression>(&expr.data)) {
      return tinyexprEvalString(std::format("({} {} {})",
                                            computeExpression(*be->lhs), be->op,
                                            computeExpression(*be->rhs)));
    }

    if (auto n = std::get_if<NumberString>(&expr.data)) {
      return *n;
    }
    return "";
  }

  std::expected<std::string, std::string> evaluate(const std::string &expr) {
    Parser parser{expr};
    auto ast = parser.parse();
    auto result = computeExpression(*ast.root);

    return result;
  }
};

}; // namespace abacus
