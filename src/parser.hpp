#pragma once
#include "abacus/unit.hpp"
#include "lexer.hpp"
#include "timezone.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct Expression;

using NumberString = double;

namespace {
double toNumber(std::string_view s) {
  double n;
  std::from_chars(s.data(), s.data() + s.size(), n);
  return n;
};
} // namespace

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

struct NamedTimezone {
  std::string_view name;
};

struct TimezoneOffset {
  std::string_view name;
  std::chrono::seconds offset = std::chrono::seconds(0);
};

using TimezoneLike = std::variant<NamedTimezone, TimezoneOffset>;

struct NamedUnit {
  std::string_view name;
};

struct NamedNumberFormat {
  std::string_view name;
};

using ConversionTarget =
    std::variant<NamedUnit, TimezoneLike, NamedNumberFormat>;

struct ConversionExpression {
  std::unique_ptr<Expression> b;
  ConversionTarget target;
};

// now in new york
// 5:03pm utc

enum class Meridiem { AM, PM };

// now = "time" | "date" | "now"
// time_lit = <hour>[:<min>:<sec>][am|pm]
// date = [now|time_lit] [in <TZ>]

struct RelativeDateString {};

struct ParsedTime {
  std::optional<std::chrono::hours> hours;
  std::optional<std::chrono::minutes> minutes;
  std::optional<std::chrono::seconds> seconds;
};

struct DateTimeLiteral {
  std::optional<std::chrono::day> day;
  std::optional<std::chrono::month> month;
  std::optional<std::chrono::year> year;
  std::optional<ParsedTime> time;
};

struct DateString {
  std::variant<DateTimeLiteral, std::string_view> value;
  std::optional<TimezoneLike> timezone;
};

struct UnaryExpression {
  std::string_view op;
  std::unique_ptr<Expression> lhs;
};

struct FunctionCall {
  std::string_view name;
  std::vector<std::unique_ptr<Expression>> args;
};

struct UnitExpression {
  OpaqueUnit unit;
  std::unique_ptr<Expression> expr;
};

struct Expression {
  std::variant<BinaryExpression, UnaryExpression, NumberString, DateString,
               UnitExpression, ConversionExpression, FunctionCall>
      data;

  const BinaryExpression *asBinaryExpression() const {
    return std::get_if<BinaryExpression>(&data);
  }

  const UnaryExpression *asUnaryExpression() const {
    return std::get_if<UnaryExpression>(&data);
  }

  const ConversionExpression *asConversion() const {
    return std::get_if<ConversionExpression>(&data);
  }

  const FunctionCall *asFunction() const {
    return std::get_if<FunctionCall>(&data);
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

static auto operators = std::to_array<OperatorDefinition>(
    {OperatorDefinition{.id = ">>", .aliases = {">>"}, .precedence = 1},
     OperatorDefinition{.id = "<<", .aliases = {"<<"}, .precedence = 1},
     OperatorDefinition{.id = "|", .aliases = {"|"}, .precedence = 1},
     OperatorDefinition{.id = "&", .aliases = {"&"}, .precedence = 1},
     OperatorDefinition{
         .id = "+", .aliases = {"+", "add", "plus"}, .precedence = 2},
     OperatorDefinition{.id = "-", .aliases = {"-", "minus"}, .precedence = 2},
     OperatorDefinition{.id = "*", .aliases = {"*", "mul"}, .precedence = 3},
     OperatorDefinition{.id = "/", .aliases = {"/", "div"}, .precedence = 3},
     OperatorDefinition{
         .id = "%", .aliases = {"%", "mod", "modulo"}, .precedence = 3},
     OperatorDefinition{
         .id = "^", .aliases = {"^", "pow", "power"}, .precedence = 4}});

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

  std::optional<std::chrono::weekday> parseWeekday(std::string_view s) {
    std::istringstream is{std::string{s}};
    std::chrono::weekday m;
    is >> std::chrono::parse("{:L%a}", m);
    if (!is)
      return std::nullopt;
    return m;
  }

  std::optional<std::chrono::month> parseMonth(std::string_view s) {
    std::istringstream is{std::string{s}};
    std::chrono::month m;
    is >> std::chrono::parse("%b", m);
    if (!is)
      return std::nullopt;
    return m;
  }

  bool isRelativeDateToken(std::string_view name) const {
    // handle more relative expressions, e.g "last week" etc...
    return name == "time" || name == "date" || name == "now" ||
           name == "yesterday";
  }

  bool isTimezoneToken(std::string_view name) {
    // TODO: do something thorough
    return TimezoneDB{}.query(name);
  }

  std::optional<DateTimeLiteral> parseYYYYMMDD() {
    auto ns1 = m_lexer.peak(0);

    if (!ns1 || ns1->type != Lexer::TokenType::Number) {
      return std::nullopt;
    }

    if (auto tok = m_lexer.peak(1);
        !tok || !ns1->isAdjacent(*tok) || tok->raw != "/") {
      return std::nullopt;
    }

    auto ns2 = m_lexer.peak(2);

    if (!ns2 || ns2->type != Lexer::TokenType::Number) {
      return std::nullopt;
    }

    if (auto tok = m_lexer.peak(3);
        !tok || !ns2->isAdjacent(*tok) || tok->raw != "/") {
      return std::nullopt;
    }

    auto ns3 = m_lexer.peak(4);

    if (!ns3 || ns3->type != Lexer::TokenType::Number) {
      return std::nullopt;
    }

    auto [n1, n2, n3] =
        std::tuple{toNumber(ns1->raw), toNumber(ns2->raw), toNumber(ns3->raw)};

    const auto isMonth = [](auto n) { return n >= 1 && n <= 12; };
    const auto isDay = [](auto n) { return n >= 1 && n <= 31; };
    const auto isYear = [](auto n) { return n >= 0 && n <= 2100; };
    const auto commit = [&](DateTimeLiteral lit) {
      for (int i = 0; i != 5; ++i) {
        m_lexer.next();
      }
      return lit;
    };

    DateTimeLiteral d;

    // YYYY/MM/DD
    if (isYear(n1) && isMonth(n2) && isDay(n3)) {
      d.year = std::chrono::year(n1);
      d.month = std::chrono::month(n2);
      d.day = std::chrono::day(n3);
      return commit(d);
    }

    // DD/MM/YYYY
    if (isDay(n1) && isMonth(n2) && isYear(n3)) {
      d.day = std::chrono::day(n1);
      d.month = std::chrono::month(n2);
      d.year = std::chrono::year(n3);
      return commit(d);
    }

    // MM/DD/YYYY
    if (isMonth(n1) && isDay(n2) && isYear(n3)) {
      d.month = std::chrono::month(n1);
      d.day = std::chrono::day(n2);
      d.year = std::chrono::year(n3);
      return commit(d);
    }

    return std::nullopt;
  }

  std::optional<DateTimeLiteral> parseDate() {
    if (auto d = parseYYYYMMDD()) {
      d->time = parseTime();
      return d;
    }

    // 12 Jan 2026 18:50
    if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
      if (auto m = m_lexer.peak(1)) {

        if (auto month = parseMonth(m->raw)) {
          DateTimeLiteral d;
          d.day = std::chrono::day{static_cast<unsigned>(toNumber(tok->raw))};
          d.month = month;

          m_lexer.next();
          m_lexer.next();
          if (auto year = m_lexer.peakIf(Lexer::TokenType::Number)) {
            d.year = std::chrono::year{static_cast<int>(toNumber(year->raw))};
            m_lexer.next();
          }

          d.time = parseTime();
          return d;
        }
      }

      if (auto time = parseTime()) {
        return DateTimeLiteral{.time = time};
      }
    }

    return std::nullopt;
  }

  // [<hour>]:[minute]:[second]
  std::optional<ParsedTime> parseTime() {
    std::chrono::hours hrs;

    if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
      hrs = std::chrono::hours{static_cast<unsigned>(toNumber(tok->raw))};
    }

    using IV = std::initializer_list<std::string_view>;

    if (auto tok = m_lexer.peak(1);
        !tok || !std::ranges::contains(IV{":", "h"}, tok->raw))
      return std::nullopt;
    m_lexer.next();

    ParsedTime time;

    time.hours = hrs;

    m_lexer.next();

    if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
      time.minutes =
          std::chrono::minutes{static_cast<unsigned>(toNumber(tok->raw))};
      m_lexer.next();
    }

    if (auto tok = m_lexer.peak(); !tok || tok->raw != ":")
      return time;
    m_lexer.next();

    if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
      time.seconds =
          std::chrono::seconds{static_cast<unsigned>(toNumber(tok->raw))};
      m_lexer.next();
    }

    return time;
  }

  std::optional<TimezoneLike> parseTimezone() {
    if (auto str = m_lexer.peakIf(Lexer::TokenType::String)) {
      auto isRelativeOffsetTz = std::ranges::any_of(
          std::initializer_list<std::string_view>({"gmt", "utc"}),
          [&](auto &&s) { return equalsIgnoreCase(s, str->raw); });

      if (isRelativeOffsetTz) {
        TimezoneOffset tz;
        tz.name = str->raw;
        m_lexer.next();
        if (auto str = m_lexer.peakIf(Lexer::TokenType::Operator)) {
          if (str->raw == "+" || str->raw == "-") {
            m_lexer.next();
            if (auto time = parseTime()) {
              if (auto h = time->hours) {
                tz.offset += std::chrono::hours(*h);
              }
              if (auto m = time->minutes)
                tz.offset += std::chrono::minutes(*m);
              if (auto s = time->seconds)
                tz.offset += std::chrono::seconds(*s);
            }
          }
        }

        return tz;
      }
    }

    return greedyParse(
               3, [&](std::string_view word) { return isTimezoneToken(word); })
        .transform([](auto &&str) { return NamedTimezone(str); });
  }

  std::optional<NamedNumberFormat> parseNumberFormat() {

    return greedyParse(3,
                       [&](std::string_view word) {
                         return std::ranges::contains(
                             std::initializer_list{"hex", "octal", "binary",
                                                   "hexadecimal"},
                             word);
                       })
        .transform([](auto &&str) { return NamedNumberFormat(str); });
  }

  template <typename F>
  std::optional<std::string_view> greedyParse(int n, F fn) {
    assert(n >= 0);
    for (int i = 0; i != n; ++i) {
      auto str = m_lexer.peakString(n - i);
      if (str && fn(*str)) {
        int consumable = n - i;
        for (int j = 0; j != consumable; ++j)
          m_lexer.next();
        return str;
      }
    }
    return std::nullopt;
  }

  std::unique_ptr<Expression> parseTerm() {
    auto expr = std::unique_ptr<Expression>();

    if (auto tok = m_lexer.peak()) {
      if (auto date = parseDate()) {
        DateString ds{.value = *date};
        if (auto result = parseTimezone()) {
          ds.timezone = result.value();
        }
        return std::make_unique<Expression>(ds);
      }

      if (isRelativeDateToken(tok->raw)) {
        m_lexer.next();
        DateString ds{.value = tok->raw};

        if (auto tz = m_lexer.peak()) {
          if (auto result = parseTimezone()) {
            ds.timezone = result.value();
          }
        }

        return std::make_unique<Expression>(ds);
      }
    }

    if (auto tok = m_lexer.peak()) {
      bool unary = std::ranges::contains(
          std::initializer_list<std::string_view>{"+", "-"}, tok->raw);
      if (unary) {
        m_lexer.next();
        return std::make_unique<Expression>(UnaryExpression{
            .op = tok->raw,
            .lhs = parseTerm(),
        });
      }
    }

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

    else if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
      if (auto next = m_lexer.peak(1); next && next->raw == "(") {
        m_lexer.next();
        m_lexer.next();

        FunctionCall fn{.name = tok->raw};

        while (true) {
          auto tok = m_lexer.peak();
          if (tok->raw == ")")
            break;
          fn.args.emplace_back(pratParse());
          tok = m_lexer.peak();

          if (tok->raw == ")") {
            break;
          }
          if (tok->raw != ",") {
            throw std::runtime_error("Expected , to add another argument");
          }
          m_lexer.next();
        }

        m_lexer.next();
        return std::make_unique<Expression>(std::move(fn));
      }
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
        if (minPrec > 0)
          break;

        m_lexer.next();

        if (auto tz = parseTimezone()) {
          left = std::make_unique<Expression>(
              ConversionExpression{.b = std::move(left), .target = tz.value()});
          continue;
        }

        if (auto fmt = parseNumberFormat()) {
          left = std::make_unique<Expression>(ConversionExpression{
              .b = std::move(left), .target = fmt.value()});
          continue;
        }

        auto unit = m_lexer.peak();

        if (!unit || unit->type != Lexer::TokenType::String)
          throw std::runtime_error("expected unit after conversion operator");

        m_lexer.next();

        left = std::make_unique<Expression>(ConversionExpression{
            .b = std::move(left), .target = NamedUnit{unit->raw}});

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
