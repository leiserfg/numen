#include "parser.hpp"
#include "abacus/unit.hpp"
#include "timezone.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

struct OperatorDefinition {
  std::string_view id;
  std::vector<std::string_view> aliases;
  int precedence;
  bool rightAssociative = false;
};

constexpr int EXPONENT_PRECEDENCE = 5;

// clang-format off
const auto OPERATORS = std::to_array<OperatorDefinition>({
     OperatorDefinition{.id = "==", .aliases = {"=="}, .precedence = 1},
     OperatorDefinition{.id = "!=", .aliases = {"!="}, .precedence = 1},
     OperatorDefinition{.id = ">", .aliases = {">"}, .precedence = 1},
     OperatorDefinition{.id = ">=", .aliases = {">="}, .precedence = 1},
     OperatorDefinition{.id = "<", .aliases = {"<"}, .precedence = 1},
     OperatorDefinition{.id = "<=", .aliases = {"<="}, .precedence = 1},

     OperatorDefinition{.id = ">>", .aliases = {">>"}, .precedence = 2},
     OperatorDefinition{.id = ">>", .aliases = {">>"}, .precedence = 2},
     OperatorDefinition{.id = "<<", .aliases = {"<<"}, .precedence = 2},
     OperatorDefinition{.id = "|", .aliases = {"|"}, .precedence = 2},
     OperatorDefinition{.id = "&", .aliases = {"&"}, .precedence = 2},
     OperatorDefinition{.id = "+", .aliases = {"+", "add", "plus"}, .precedence = 3},
     OperatorDefinition{.id = "-", .aliases = {"-", "minus"}, .precedence = 3},
     OperatorDefinition{.id = "*", .aliases = {"*", "mul"}, .precedence = 4},
     OperatorDefinition{.id = "/", .aliases = {"/", "div"}, .precedence = 4},
     OperatorDefinition{.id = "%", .aliases = {"%", "mod", "modulo"}, .precedence = 4},
     OperatorDefinition{.id = "^", .aliases = {"^", "pow", "power"}, .precedence = EXPONENT_PRECEDENCE,
                        .rightAssociative = true}
});
// clang-format on

struct ConstantDef {
  std::string_view name;
  double n;
  bool caseSensitive = false;
};

// clang-format off
constexpr auto CONSTANTS = std::to_array<ConstantDef>({
		{"pi", std::numbers::pi},
		{"e", std::numbers::e, false},
		{"phi", std::numbers::phi},
});
// clang-format on

namespace {

bool isOperatorToken(std::string_view tok) {
  return std::ranges::any_of(OPERATORS, [&](auto &&op) { return std::ranges::contains(op.aliases, tok); });
}

std::optional<double> parseConstant(std::string_view tok) {
  auto it = std::ranges::find_if(CONSTANTS, [&](const ConstantDef &def) {
    return def.caseSensitive ? (tok == def.name) : equalsIgnoreCase(def.name, tok);
  });

  if (it == CONSTANTS.end()) return std::nullopt;
  return it->n;
}

std::unique_ptr<Expression> makeNumberExpr(abacus::Value n) { return std::make_unique<Expression>(NumberString{n}); }

std::unique_ptr<Expression> makeBinExpr(std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs,
                                        const std::string &op) {
  auto expr = std::make_unique<Expression>();

  return std::make_unique<Expression>(
      BinaryExpression{.op = op, .lhs = std::move(lhs), .rhs = std::move(rhs)});
}

std::unique_ptr<Expression> makeUnit(std::unique_ptr<Expression> inner, std::string_view unit) {
  return std::make_unique<Expression>(UnitExpression{
      .unit = unit,
      .expr = std::move(inner),
  });
}

bool isRelativeDateToken(std::string_view name) {
  // handle more relative expressions, e.g "last week" etc...
  return name == "time" || name == "date" || name == "now" || name == "yesterday";
}
}; // namespace

Parser::Parser(std::string_view data, const UnitDatabase &unitDb) : m_lexer(data), m_unitDb(unitDb) {}

AST Parser::parse() {
  AST ast;
  ast.root = pratParse();
  return ast;
}

bool Parser::isTimezoneToken(std::string_view name) {
  // TODO: do something thorough
  return TimezoneDB{}.query(name);
}

std::optional<OpaqueUnit> Parser::parseUnit() {
  if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
    if (!m_unitDb.findUnitCandidates(tok->raw).empty()) { return tok->raw; }
  }
  return std::nullopt;
}

// Jan 18 2021
// 18 Jan 2021
// 18 Jan
// Jan 18
// a month can never appear in its numeric form here (very convenient)
// <weekday_name | day_of_month> <month_name> <year> => 18 Jan 2024
// <month_name> <weekday_name | day_of_month> <year> => Jan 18 2024
std::optional<DateTimeLiteral> Parser::parseNaturalDateLiteral() {
  std::optional<std::chrono::weekday> weekday;
  std::optional<std::chrono::day> day;
  std::optional<std::chrono::month> month;
  std::optional<std::chrono::year> year;

  int i = 0;

  while (i < 3) {
    auto tok = m_lexer.peak(i);
    if (!tok) break;

    if (auto it = std::get_if<Lexer::Number>(&tok->data)) {
      auto value = it->n.toDouble();
      if (value >= 1 && value <= 31) {
        day = std::chrono::day{static_cast<unsigned>(value)};
      } else {
        year = std::chrono::year{static_cast<int>(value)};
      }
    } else if (auto m = m_dateStringVocab.asMonth(tok->raw)) {
      month = *m;
    } else if (auto week = m_dateStringVocab.asWeekday(tok->raw)) {
      weekday = *week;
    } else {
      break;
    }

    ++i;
  }

  const bool hasDay = weekday || day;

  if (month && year && !hasDay) { day = std::chrono::day{1}; }

  // the month is what makes this a date at all: without it "0 9" and "2001 5"
  // are just two numbers that happen to sit next to each other
  if (month && (hasDay || year)) {
    DateTimeLiteral lit;

    lit.month = month;
    lit.year = year;

    if (weekday) lit.day = weekday;
    if (day) lit.day = day;

    m_lexer.advance(i);

    return lit;
  }

  return std::nullopt;
}

// parse YYYY/MM/DD or MM/DD/YYYY or DD/MM/YYYY
// requires that the '/' separators are strictly adjacent
// to the numbers (no spaces) in order to be considered a date literal
std::optional<DateTimeLiteral> Parser::parseYYYYMMDD() {
  auto ns1 = m_lexer.peak(0);
  constexpr auto dateDelim = "/";

  if (!ns1 || ns1->type != Lexer::TokenType::Number) { return std::nullopt; }

  if (auto tok = m_lexer.peak(1); !tok || !ns1->isAdjacent(*tok) || tok->raw != dateDelim) {
    return std::nullopt;
  }

  auto ns2 = m_lexer.peak(2);

  if (!ns2 || ns2->type != Lexer::TokenType::Number) { return std::nullopt; }

  if (auto tok = m_lexer.peak(3); !tok || !ns2->isAdjacent(*tok) || tok->raw != dateDelim) {
    return std::nullopt;
  }

  auto ns3 = m_lexer.peak(4);

  if (!ns3 || ns3->type != Lexer::TokenType::Number) { return std::nullopt; }

  auto [n1, n2, n3] = std::tuple{toNumber(ns1->raw), toNumber(ns2->raw), toNumber(ns3->raw)};

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

std::optional<RelativeDateTimeLiteral> Parser::parseRelativeDateTimeLiteral() {
  if (auto duration = scanDuration()) {
    if (auto s = m_lexer.peak(duration->tokenCount); s && s->type == Lexer::TokenType::String) {
      auto word = s->raw;
      if (equalsIgnoreCase(word, std::string_view{"ago"})) {
        m_lexer.advance(duration->tokenCount + 1);
        return RelativeDateTimeLiteral{
            .anchor = duration->data,
            .direction = RelativeDateTimeLiteral::Direction::Past,
        };
      }
    }
  }

  if (auto tok = m_lexer.peak(); tok && tok->raw == "yesterday") {
    m_lexer.next();
    return RelativeDateTimeLiteral{
        .anchor = Duration{.seconds = std::chrono::days{1}},
        .direction = RelativeDateTimeLiteral::Direction::Past,
    };
  }

  if (auto tok = m_lexer.peak(); tok && tok->raw == "tomorrow") {
    m_lexer.next();
    return RelativeDateTimeLiteral{
        .anchor = Duration{.seconds = std::chrono::days{1}},
        .direction = RelativeDateTimeLiteral::Direction::Future,
    };
  }

  return std::nullopt;
}

std::optional<DateTimeLiteral> Parser::parseDate() {
  if (auto d = parseYYYYMMDD()) {
    d->time = parseTime();
    return d;
  }

  if (auto d = parseNaturalDateLiteral()) {
    d->time = parseTime();
    return d;
  }

  // 12 Jan 2026 18:50
  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    if (auto m = m_lexer.peak(1)) {

      if (auto month = m_dateStringVocab.asMonth(m->raw)) {
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

    if (auto time = parseTime()) { return DateTimeLiteral{.time = time}; }
  }

  return std::nullopt;
}

std::optional<ParsedTime> Parser::parseTime() {
  std::chrono::hours hrs;

  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    hrs = std::chrono::hours{static_cast<unsigned>(toNumber(tok->raw))};
  }

  using IV = std::initializer_list<std::string_view>;

  if (auto tok = m_lexer.peak(1); !tok || !std::ranges::contains(IV{":", "h"}, tok->raw)) return std::nullopt;
  m_lexer.next();

  ParsedTime time;

  time.hours = hrs;

  m_lexer.next();

  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    time.minutes = std::chrono::minutes{static_cast<unsigned>(toNumber(tok->raw))};
    m_lexer.next();
  }

  if (auto tok = m_lexer.peak(); !tok || tok->raw != ":") return time;
  m_lexer.next();

  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    time.seconds = std::chrono::seconds{static_cast<unsigned>(toNumber(tok->raw))};
    m_lexer.next();
  }

  return time;
}

std::optional<TimezoneLike> Parser::parseTimezone() {
  if (auto str = m_lexer.peakIf(Lexer::TokenType::String)) {
    auto isRelativeOffsetTz = std::ranges::any_of(std::initializer_list<std::string_view>({"gmt", "utc"}),
                                                  [&](auto &&s) { return equalsIgnoreCase(s, str->raw); });

    if (isRelativeOffsetTz) {
      TimezoneOffset tz;
      tz.name = str->raw;
      m_lexer.next();
      if (auto str = m_lexer.peakIf(Lexer::TokenType::Operator)) {
        if (str->raw == "+" || str->raw == "-") {
          m_lexer.next();
          if (auto time = parseTime()) {
            if (auto h = time->hours) { tz.offset += std::chrono::hours(*h); }
            if (auto m = time->minutes) tz.offset += std::chrono::minutes(*m);
            if (auto s = time->seconds) tz.offset += std::chrono::seconds(*s);
          }
        }
      }

      return tz;
    }
  }

  return greedyParse(3, [&](std::string_view word) { return isTimezoneToken(word); })
      .transform([](auto &&str) { return NamedTimezone(str); });
}

std::optional<NamedNumberFormat> Parser::parseNumberFormat() {

  return greedyParse(3,
                     [&](std::string_view word) {
                       return std::ranges::contains(
                           std::initializer_list{"hex", "octal", "binary", "hexadecimal"}, word);
                     })
      .transform([](auto &&str) { return NamedNumberFormat(str); });
}

std::unique_ptr<Expression> Parser::parseTerm() {
  if (!m_lexer.peak()) {
    throw std::runtime_error("Expected EOF, looks like there is nothing we can parse!");
  }

  auto expr = std::unique_ptr<Expression>();

  if (auto tok = m_lexer.peak()) {
    if (auto constant = parseConstant(tok->raw)) {
      m_lexer.next();

      auto lhs = makeNumberExpr(abacus::Value{*constant});

      // pi2 = pi * 2
      if (auto n = m_lexer.peak(); n && !isOperatorToken(n->raw) && n->raw != ")") {
        return makeBinExpr(std::move(lhs), parseMul(), "*");
      }

      return lhs;
    }

    if (auto date = parseDate()) {
      DateString ds{.value = *date};
      if (auto result = parseTimezone()) { ds.timezone = result.value(); }
      return std::make_unique<Expression>(ds);
    }

    if (auto date = parseRelativeDateTimeLiteral()) {
      DateString ds{.value = *date};
      if (auto result = parseTimezone()) { ds.timezone = result.value(); }
      return std::make_unique<Expression>(ds);
    }

    if (isRelativeDateToken(tok->raw)) {
      m_lexer.next();
      DateString ds{.value = tok->raw};

      if (auto tz = m_lexer.peak()) {
        if (auto result = parseTimezone()) { ds.timezone = result.value(); }
      }

      return std::make_unique<Expression>(ds);
    }
  }

  // less than 2 tokens means likely unit
  if (auto duration = scanDuration(); duration && duration->tokenCount > 2) {
    for (int i = 0; i != duration->tokenCount; ++i) {
      m_lexer.next();
    }
    return std::make_unique<Expression>(duration->data);
  }

  if (auto tok = m_lexer.peak()) {
    bool unary = std::ranges::contains(std::initializer_list<std::string_view>{"+", "-"}, tok->raw);
    if (unary) {
      m_lexer.next();
      return std::make_unique<Expression>(UnaryExpression{
          .op = tok->raw,
          .lhs = pratParse(EXPONENT_PRECEDENCE),
      });
    }
  }

  if (auto open = m_lexer.peak(); open && open->raw == "(") {
    m_lexer.next();
    expr = pratParse();

    if (auto close = m_lexer.peak(); !close || close->raw != ")") {
      throw std::runtime_error("Expected ) to close the group");
    }
    m_lexer.next();

    if (auto tok = m_lexer.peak();
        tok && tok->type != Lexer::TokenType::Operator && tok->raw != "to" && tok->raw != "in") {
      if (auto unit = parseUnit()) {
        if (!expr)
          expr = makeUnit(makeNumberExpr(1), unit.value());
        else
          expr = makeUnit(std::move(expr), unit.value());
        m_lexer.next();
      } else {
        // implict multiplication, e.g "(150 * 2)4
        expr = makeBinExpr(std::move(expr), parseMul(), "*");
      }
    }
  }

  if (auto tok = m_lexer.peak()) {
    if (auto n = tok->asNumber()) {
      m_lexer.next();
      expr = makeNumberExpr(n->n);
    } else if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
      if (auto next = m_lexer.peak(1); next && next->raw == "(") {
        m_lexer.next();
        m_lexer.next();

        FunctionCall fn{.name = tok->raw};

        constexpr auto unterminated = "Expected ) to close the argument list";

        while (true) {
          if (m_lexer.peakOrThrow(unterminated).raw == ")") break;

          fn.args.emplace_back(pratParse());

          auto tok = m_lexer.peakOrThrow(unterminated);
          if (tok.raw == ")") { break; }
          if (tok.raw != ",") { throw std::runtime_error("Expected , to add another argument"); }

          m_lexer.next();
        }

        m_lexer.next();
        return std::make_unique<Expression>(std::move(fn));
      }
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
    m_lexer.next();
    return parseTerm();
  }

  return expr;
}

std::optional<Scanned<Duration>> Parser::scanDuration() {
  // 1 year 2 weeks 2 minutes
  Scanned<Duration> d;

  while (true) {
    if (auto pk = m_lexer.peakForward<Lexer::Number, Lexer::String>(d.tokenCount)) {
      auto &[n, u] = *pk;
      if (auto unit = m_unitDb.findUnit(std::string{u.data});
          unit && unit->dimension == dimensions::DURATION) {

        d.tokenCount += 2;

        if (unit->id == "year") {
          d.data.years = std::chrono::years{static_cast<unsigned>(n.n.toDouble())};
        } else if (unit->id == "month") {
          d.data.months = std::chrono::months{static_cast<unsigned>(n.n.toDouble())};
        } else {
          d.data.seconds = d.data.seconds.value_or(std::chrono::seconds{0}) +
                           std::chrono::seconds{static_cast<unsigned>(n.n.toDouble() * unit->factor)};
        }
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (d.data.years || d.data.months || d.data.seconds) { return d; }

  return std::nullopt;
}

// "7 % -3" and "10% - 3" are the same token sequence, only spacing tells them apart
bool Parser::isPostfixPercent(const Lexer::Token &pct) {
  auto next = m_lexer.peak(1);

  if (!next) return true;
  if (next->raw == "+" || next->raw == "-") return m_lexer.isGluedLeft(pct);
  if (next->type == Lexer::TokenType::Number || next->raw == "(") return false;

  return next->type != Lexer::TokenType::String || !parseConstant(next->raw);
}

std::unique_ptr<Expression> Parser::pratParse(int minPrec) {
  auto left = parseTerm();

  while (auto tok = m_lexer.peak()) {
    if (tok->raw == "to" || tok->raw == "in" | tok->raw == "->") {
      if (minPrec > 0) break;

      m_lexer.next();

      // in is equivalent to addition for durations.
      // now in 2 days 5minutes
      if (auto duration = scanDuration(); duration && tok->raw == "in") {
        auto rhs = std::make_unique<Expression>(duration->data);
        left = std::make_unique<Expression>(BinaryExpression{
            .op = "+",
            .lhs = std::move(left),
            .rhs = std::move(rhs),
        });
        m_lexer.advance(duration->tokenCount);
        continue;
      }

      if (auto tz = parseTimezone()) {
        left = std::make_unique<Expression>(ConversionExpression{.b = std::move(left), .target = tz.value()});
        continue;
      }

      if (auto fmt = parseNumberFormat()) {
        left =
            std::make_unique<Expression>(ConversionExpression{.b = std::move(left), .target = fmt.value()});
        continue;
      }

      auto unit = m_lexer.peak();

      if (!unit || unit->type != Lexer::TokenType::String)
        throw std::runtime_error("expected unit after conversion operator");

      m_lexer.next();

      left = std::make_unique<Expression>(
          ConversionExpression{.b = std::move(left), .target = NamedUnit{unit->raw}});

      continue;
    }

    if (tok->raw == "%" && isPostfixPercent(*tok)) {
      auto next = m_lexer.peak(1);
      bool ofFollows = next && next->raw == "of";

      if (ofFollows && 3 < minPrec) { break; }

      m_lexer.next();
      left = std::make_unique<Expression>(PercentExpression{.expr = std::move(left)});

      // "of" is the same percentage, applied to what comes after it
      if (ofFollows) {
        m_lexer.next();
        left = makeBinExpr(std::move(left), parseMul(), "*");
      }

      continue;
    }

    auto it = std::ranges::find_if(
        OPERATORS, [&](const OperatorDefinition &op) { return std::ranges::contains(op.aliases, tok->raw); });

    if (it != OPERATORS.end()) {
      if (it->precedence < minPrec) break;
      m_lexer.next();
      auto right = pratParse(it->rightAssociative ? it->precedence : it->precedence + 1);
      left = makeBinExpr(std::move(left), std::move(right), std::string{it->id});
    } else {
      if (!(3 < minPrec)) {
        if (auto constant = parseConstant(tok->raw)) {
          m_lexer.next();
          left = makeBinExpr(std::move(left), makeNumberExpr(abacus::Value{*constant}), std::string{"*"});
          continue;
        } else if (tok->raw == "(") {
          left = makeBinExpr(std::move(left), std::move(parseTerm()), std::string{"*"});
          continue;
        }
      }

      if (tok->raw == "(" || tok->raw == ")" || tok->raw == "," || parseConstant(tok->raw)) break;

      m_lexer.next();
      continue;
    }
  }

  return left;
}
