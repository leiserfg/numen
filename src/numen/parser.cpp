#include "parser.hpp"
#include "numen/numen.hpp"
#include "numen/unit.hpp"
#include "timezone.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <charconv>
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

const auto &operators() {
  // clang-format off
  static const auto OPERATORS = std::to_array<OperatorDefinition>({
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
     OperatorDefinition{.id = "^", .aliases = {"^", "**", "pow", "power"}, .precedence = EXPONENT_PRECEDENCE,
                        .rightAssociative = true}
  });
  // clang-format on

  return OPERATORS;
}

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
  return std::ranges::any_of(operators(), [&](auto &&op) { return std::ranges::contains(op.aliases, tok); });
}

std::optional<double> parseConstant(std::string_view tok) {
  auto it = std::ranges::find_if(CONSTANTS, [&](const ConstantDef &def) {
    return def.caseSensitive ? (tok == def.name) : equalsIgnoreCase(def.name, tok);
  });

  if (it == CONSTANTS.end()) return std::nullopt;
  return it->n;
}

std::unique_ptr<Expression> makeNumberExpr(numen::Value n) {
  return std::make_unique<Expression>(NumberString{n});
}

std::unique_ptr<Expression> makeBinExpr(std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs,
                                        const std::string &op) {
  auto expr = std::make_unique<Expression>();

  return std::make_unique<Expression>(
      BinaryExpression{.op = op, .lhs = std::move(lhs), .rhs = std::move(rhs)});
}

std::unique_ptr<Expression> makeUnit(std::unique_ptr<Expression> inner, const NamedUnit &unit) {
  return std::make_unique<Expression>(UnitExpression{
      .unit = unit,
      .expr = std::move(inner),
  });
}

// a clock component only reads as one inside its own range, so anything else is
// not a time and must not be narrowed into one
std::optional<unsigned> clockComponent(double value, unsigned limit) {
  if (value < 0 || value > limit || value != std::trunc(value)) return std::nullopt;
  return static_cast<unsigned>(value);
}

std::optional<std::chrono::year> asYear(double value) {
  // year's conversion to int is explicit, hence the casts
  constexpr int lowest = static_cast<int>(std::chrono::year::min());
  constexpr int highest = static_cast<int>(std::chrono::year::max());

  if (value < lowest || value > highest || value != std::trunc(value)) return std::nullopt;
  return std::chrono::year{static_cast<int>(value)};
}

bool isRelativeDateToken(std::string_view name) {
  // handle more relative expressions, e.g "last week" etc...
  return false;
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

std::optional<NamedUnit> Parser::parseUnit(bool validate) {
  auto head = m_lexer.peak();
  auto start = m_lexer.cursor();

  if (!head) return std::nullopt;

  if ((validate || head->type != Lexer::TokenType::String) && m_unitDb.findCompounds(head->raw).empty()) {
    return std::nullopt;
  }

  m_lexer.next();
  NamedUnit target{.terms = {NamedUnitTerm{.name = head->raw}}};

  // shorthand squaring/cubing. We don't support other exponents are they are not typically
  // used as shorthand.
  if (auto num = m_lexer.peakAs<Lexer::Number>(); num && num->n >= 2 && num->n <= 3) {
    target.terms.front().exponent = num->n.to<int>();
    m_lexer.next();
  }

  if (auto tok = m_lexer.peak(); tok && tok->raw == "(") {
    m_lexer.setCursor(start);
    return std::nullopt;
  }

  return target;
}

// only a real unit token may follow the slash, so "1 km to m / 2" still divides
// the result rather than naming metres per two
std::optional<NamedUnit> Parser::parseConversionTarget() {
  auto target = parseUnit(false); // FIXME: we should not need to disable validation here

  if (!target) return std::nullopt;

  while (auto op = m_lexer.peak()) {
    if (op->raw != "/" && op->raw != "*") break;

    auto next = m_lexer.peak(1);
    if (!next || next->type != Lexer::TokenType::String) break;
    if (m_unitDb.findCompounds(next->raw).empty()) break;

    m_lexer.next();
    m_lexer.next();
    target->terms.push_back(NamedUnitTerm{.name = next->raw, .exponent = op->raw == "/" ? -1 : 1});
  }

  return target;
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

      if (auto dayOfMonth = clockComponent(value, 31); dayOfMonth && value >= 1) {
        day = std::chrono::day{*dayOfMonth};
      } else if (auto parsed = asYear(value)) {
        year = *parsed;
      } else {
        break;
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
  constexpr auto isDateDelim = [](auto c) { return c == "/" || c == "-"; };

  if (!ns1 || ns1->type != Lexer::TokenType::Number) { return std::nullopt; }

  if (auto tok = m_lexer.peak(1); !tok || !ns1->isAdjacent(*tok) || !isDateDelim(tok->raw)) {
    return std::nullopt;
  }

  auto ns2 = m_lexer.peak(2);

  if (!ns2 || ns2->type != Lexer::TokenType::Number) { return std::nullopt; }

  if (auto tok = m_lexer.peak(3); !tok || !ns2->isAdjacent(*tok) || !isDateDelim(tok->raw)) {
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

constexpr auto RESERVED_TIME_TOKENS =
    std::to_array<std::string_view>({"tomorrow", "yesterday", "tomorrow", "now", "time", "date", "ago"});

std::optional<RelativeDateTimeLiteral> Parser::parseRelativeDateTimeLiteral() {
  if (auto duration = scanDuration()) {
    if (auto s = m_lexer.peak(duration->tokenCount); s && s->type == Lexer::TokenType::String) {
      auto word = s->raw;
      if (equalsIgnoreCase(word, std::string_view{"ago"})) {
        m_lexer.advance(duration->tokenCount + 1);
        return RelativeDateTimeLiteral{
            .delta = duration->data,
            .direction = RelativeDateTimeLiteral::Direction::Past,
        };
      }
    }
  }

  if (auto s = m_lexer.peakAs<Lexer::String>()) {
    if (s->data == "yesterday") {
      m_lexer.next();

      return RelativeDateTimeLiteral{
          .delta = Duration{.seconds = std::chrono::days{1}},
          .direction = RelativeDateTimeLiteral::Direction::Past,
          .precision = DateTimePrecision::Date,
      };
    }

    if (s->data == "tomorrow") {
      m_lexer.next();
      return RelativeDateTimeLiteral{
          .delta = Duration{.seconds = std::chrono::days{1}},
          .direction = RelativeDateTimeLiteral::Direction::Future,
          .precision = DateTimePrecision::Date,
      };
    }

    if (s->data == "today" || s->data == "date") {
      m_lexer.next();
      return RelativeDateTimeLiteral{
          .precision = DateTimePrecision::Date,
      };
    }

    if (s->data == "now" || s->data == "time") {
      m_lexer.next();
      return RelativeDateTimeLiteral{
          .precision = DateTimePrecision::DateTime,
      };
    }
  }

  return std::nullopt;
}

std::optional<DateTimeLiteral> Parser::parseDate() {
  if (auto d = parseYYYYMMDD()) {
    d->time = parseTime(true);
    return d;
  }

  if (auto d = parseNaturalDateLiteral()) {
    d->time = parseTime(true);
    return d;
  }

  // 12 Jan 2026 18:50
  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    if (auto m = m_lexer.peak(1)) {

      if (auto month = m_dateStringVocab.asMonth(m->raw)) {
        auto dayOfMonth = clockComponent(toNumber(tok->raw), 31);
        if (!dayOfMonth) return std::nullopt;

        DateTimeLiteral d;
        d.day = std::chrono::day{*dayOfMonth};
        d.month = month;

        m_lexer.next();
        m_lexer.next();
        if (auto year = m_lexer.peakIf(Lexer::TokenType::Number)) {
          auto parsed = asYear(toNumber(year->raw));
          if (!parsed) return std::nullopt;

          d.year = *parsed;
          m_lexer.next();
        }

        d.time = parseTime(true);
        return d;
      }
    }

    if (auto time = parseTime()) { return DateTimeLiteral{.time = time}; }
  }

  return std::nullopt;
}

std::optional<ParsedTime> Parser::parseTime(bool afterDate) {
  if (auto tok = m_lexer.peakAs<Lexer::String>(); tok && tok->data == "at") { m_lexer.next(); }

  std::chrono::hours hrs{};

  auto head = m_lexer.peakIf(Lexer::TokenType::Number);

  if (!head) return std::nullopt;

  if (head) {
    auto value = clockComponent(toNumber(head->raw), 23);
    if (!value) return std::nullopt;
    hrs = std::chrono::hours{*value};
  }

  ParsedTime time;

  time.hours = hrs;

  const auto commitTime = [&]() {
    if (auto tok = m_lexer.peakAs<Lexer::String>()) {
      const bool am = equalsIgnoreCase(tok->data, std::string_view{"am"});
      const bool pm = equalsIgnoreCase(tok->data, std::string_view{"pm"});
      // TODO: if using 12h we need to make sure the clock component actually make sense...

      if (am || pm) {
        m_lexer.next();
        if (pm) time.hours = time.hours.value_or(std::chrono::hours{0}) + std::chrono::hours{12};
      }
    }
    return time;
  };

  using IV = std::initializer_list<std::string_view>;

  auto sep = m_lexer.peak(1);
  if (!sep || !std::ranges::contains(IV{":", "h"}, sep->raw)) {
    if (afterDate) {
      m_lexer.next();
      return commitTime();
    }
    return std::nullopt;
  }

  // "h" also names the hour unit, so it only separates a clock when glued
  if (sep->raw == "h") {
    if (!head || !head->isAdjacent(*sep)) return std::nullopt;

    // on its own "12h" is twelve hours far more often than noon, so it needs
    // its minutes. after a date there is nothing else it could mean
    if (!afterDate) {
      auto mins = m_lexer.peak(2);
      if (!mins || mins->type != Lexer::TokenType::Number || !sep->isAdjacent(*mins)) return std::nullopt;
    }
  }

  m_lexer.next();
  m_lexer.next();

  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    auto value = clockComponent(toNumber(tok->raw), 59);
    if (!value) return std::nullopt;

    time.minutes = std::chrono::minutes{*value};
    m_lexer.next();
  }

  if (auto tok = m_lexer.peak(); !tok || tok->raw != ":") return commitTime();
  m_lexer.next();

  if (auto tok = m_lexer.peakIf(Lexer::TokenType::Number)) {
    auto value = clockComponent(toNumber(tok->raw), 59);
    if (!value) return std::nullopt;

    time.seconds = std::chrono::seconds{*value};
    m_lexer.next();
  }

  return commitTime();
}

std::optional<DateString> Parser::parseRFC3339() {
  // 2026-08-17T14:30:00Z
  using L = Lexer;
  auto parsed = m_lexer.peakForward<L::Number, L::Operator, L::Number, L::Operator, L::Number, L::String,
                                    L::Number, L::Operator, L::Number, L::Operator, L::Number>();

  if (!parsed) return std::nullopt;

  auto &[year, s1, month, s2, day, tsep, hour, s3, min, s4, s] = *parsed;

  if (!(s1.op == "-" && s2.op == "-" && s3.op == ":" && s4.op == ":")) return std::nullopt;

  // the shape alone does not make it a timestamp: every component has to be
  // one a clock could read, or the cast below has nothing to land on
  auto yr = asYear(year.n.toDouble());
  auto mon = clockComponent(month.n.toDouble(), 12);
  auto dayOfMonth = clockComponent(day.n.toDouble(), 31);
  auto hours = clockComponent(hour.n.toDouble(), 23);
  auto minutes = clockComponent(min.n.toDouble(), 59);
  // :60 is a leap second, which is a reading RFC 3339 allows
  auto seconds = clockComponent(s.n.toDouble(), 60);

  if (!yr || !mon || !dayOfMonth || !hours || !minutes || !seconds) return std::nullopt;
  if (*mon < 1 || *dayOfMonth < 1) return std::nullopt;

  DateString ds;

  ds.value = DateTimeLiteral{.day = std::chrono::day{*dayOfMonth},
                             .month = std::chrono::month{*mon},
                             .year = *yr,
                             .time = ParsedTime{
                                 .hours = std::chrono::hours{*hours},
                                 .minutes = std::chrono::minutes{*minutes},
                                 .seconds = std::chrono::seconds{*seconds},
                             }};

  m_lexer.advance(std::tuple_size_v<decltype(parsed)::value_type>);

  if (auto s = m_lexer.peakAs<Lexer::String>(); s && s->data == "Z") {
    m_lexer.next();
    ds.timezone = TimezoneOffset{.name = "UTC"};
    return ds;
  } else if (auto offset = parseTimezoneOffset()) {
    ds.timezone = TimezoneOffset{.name = "UTC", .offset = *offset};
    return ds;
  }

  return std::nullopt;
}

std::optional<std::chrono::seconds> Parser::parseTimezoneOffset() {
  constexpr auto isValidOffset = [](auto offset) { return offset >= 0 && offset <= 23; };
  std::chrono::seconds offset{0};

  if (auto str = m_lexer.peakIf(Lexer::TokenType::Operator)) {
    if (str->raw == "+" || str->raw == "-") {
      int sign = str->raw == "+" ? 1 : -1;
      m_lexer.next();

      if (auto n = m_lexer.peakAs<Lexer::Number>(); n && n->n.isInteger() && isValidOffset(n->n)) {
        m_lexer.next();
        offset += std::chrono::hours(static_cast<int>(n->n.toDouble()) * sign);

        if (auto tok = m_lexer.peak(); tok && tok->raw == ":") {
          m_lexer.next();
          if (auto n = m_lexer.peakAs<Lexer::Number>(); n && n->n.isInteger() && n->n >= 0 && n->n < 60) {
            m_lexer.next();
            offset += std::chrono::minutes(static_cast<int>(n->n.toDouble()) * sign);
          }
        }
      }
    }
    return offset;
  };
  return std::nullopt;
}

std::optional<TimezoneOffset> Parser::parseTimezone() {
  if (auto str = m_lexer.peakIf(Lexer::TokenType::String)) {
    if (std::ranges::contains(RESERVED_TIME_TOKENS, str->raw)) return std::nullopt;

    auto isOffsettableTz = std::ranges::any_of(std::initializer_list<std::string_view>({"gmt", "utc"}),
                                               [&](auto &&s) { return equalsIgnoreCase(s, str->raw); });

    if (isOffsettableTz) {
      m_lexer.next();
      return TimezoneOffset{.name = str->raw,
                            .offset = parseTimezoneOffset().value_or(std::chrono::seconds{0})};
    }
  }

  return greedyParse(4, [&](std::string_view word) { return isTimezoneToken(word); })
      .transform([](auto &&str) { return TimezoneOffset(str); });
}

std::optional<NamedNumberFormat> Parser::parseNumberFormat() {
  return greedyParse(3,
                     [&](std::string_view word) {
                       return std::ranges::contains(
                           std::initializer_list{"hex", "octal", "binary", "hexadecimal"}, word);
                     })
      .transform([](auto &&str) { return NamedNumberFormat(str); });
}

std::optional<numen::Value> Parser::parseNumber() {
  constexpr auto CONTEXT_AWARE_THOUSAND_SEP = ",";
  std::string ns;
  size_t count = 0;

  while (true) {
    auto n = m_lexer.peak();
    if (!n || n->type != Lexer::TokenType::Number) break;
    auto &nb = std::get<Lexer::Number>(n->data);
    double nn;

    ns += n->raw;
    m_lexer.next();

    if (std::from_chars(n->raw.data(), n->raw.data() + n->raw.size(), nn).ptr !=
            n->raw.data() + n->raw.size() &&
        count == 0) {
      return nb.n;
    }

    if (auto tok = m_lexer.peak(); m_inFunction || !tok || tok->raw != CONTEXT_AWARE_THOUSAND_SEP) {
      if (count == 0) return nb.n; // do not bother stringifying, there is only one part so pass the number
      break;
    };

    ++count;
    m_lexer.next();
  }

  if (ns.empty()) return std::nullopt;

  double n;
  std::from_chars(ns.data(), ns.data() + ns.size(), n);
  return n;
};

std::unique_ptr<Expression> Parser::parseTerm() {
  if (!m_lexer.peak()) {
    throw std::runtime_error("Expected EOF, looks like there is nothing we can parse!");
  }

  auto expr = std::unique_ptr<Expression>();
  auto frontUnit = parseUnit();

  if (auto tok = m_lexer.peak()) {

    if (auto constant = parseConstant(tok->raw)) {
      m_lexer.next();

      auto lhs = makeNumberExpr(numen::Value{*constant});

      // pi2 = pi * 2
      if (auto n = m_lexer.peak(); n && !isOperatorToken(n->raw) && n->raw != ")") {
        return makeBinExpr(std::move(lhs), parseMul(), "*");
      }

      return std::move(lhs);
    }

    if (auto date = parseRFC3339()) { return std::make_unique<Expression>(*date); }

    {
      auto tz = parseTimezone();

      if (auto date = parseRelativeDateTimeLiteral()) {
        DateString ds{.value = *date, .timezone = tz};

        if (!tz) {
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

    if (auto tz = parseTimezone()) {
      if (auto date = parseDate()) {
        DateString ds{.value = *date, .timezone = *tz};
        return std::make_unique<Expression>(ds);
      }
    }

    if (auto date = parseDate()) {
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

  if (auto tok = m_lexer.peak()) {
    bool unary = std::ranges::contains(std::initializer_list<std::string_view>{"+", "-"}, tok->raw);
    if (!frontUnit && unary) {
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
      } else {
        // implict multiplication, e.g "(150 * 2)4
        expr = makeBinExpr(std::move(expr), parseMul(), "*");
      }
    }
  }

  if (auto tok = m_lexer.peak()) {
    if (auto n = parseNumber()) {
      expr = makeNumberExpr(*n);
    } else if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
      if (auto next = m_lexer.peak(1); next && next->raw == "(") {
        m_lexer.next();
        m_lexer.next();

        FunctionCall fn{.name = tok->raw};

        constexpr auto unterminated = "Expected ) to close the argument list";

        m_inFunction = true;

        while (true) {
          if (m_lexer.peakOrThrow(unterminated).raw == ")") break;

          fn.args.emplace_back(pratParse());

          auto tok = m_lexer.peakOrThrow(unterminated);
          if (tok.raw == ")") { break; }
          if (tok.raw != ",") { throw std::runtime_error("Expected , to add another argument"); }

          m_lexer.next();
        }

        m_inFunction = false;

        m_lexer.next();
        // falls through to the unit check so that "sqrt(4) km" carries a unit
        expr = std::make_unique<Expression>(std::move(fn));
      }
    }
  }

  // unit can be found after any term.
  if (auto unit = parseUnit()) {
    if (!expr)
      expr = makeUnit(makeNumberExpr(1), unit.value());
    else
      expr = makeUnit(std::move(expr), unit.value());
  }

  if (frontUnit) {
    if (!expr)
      expr = makeUnit(makeNumberExpr(1), frontUnit.value());
    else
      expr = makeUnit(std::move(expr), frontUnit.value());
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

        auto part = numen::durationFrom(n.n.toDouble(), *unit);
        if (!part) break;

        d.tokenCount += 2;
        d.data = d.data + *part;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (d.tokenCount) { return d; }

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

      ConversionExpression expr{.lhs = std::move(left)};

      auto cursor = m_lexer.cursor();
      size_t maxCursor = 0;

      const auto stampChoice = [&](auto &&fn) {
        auto r = fn();
        maxCursor = std::max(maxCursor, m_lexer.cursor());
        m_lexer.setCursor(cursor);
        return r;
      };

      expr.target.tz = stampChoice([&]() { return parseTimezone(); });
      expr.target.unit = stampChoice([&]() { return parseConversionTarget(); });
      expr.target.fmt = stampChoice([&]() { return parseNumberFormat(); });
      m_lexer.setCursor(maxCursor);

      left = std::make_unique<Expression>(std::move(expr));

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

    auto it = std::ranges::find_if(operators(), [&](const OperatorDefinition &op) {
      return std::ranges::contains(op.aliases, tok->raw);
    });

    if (it != operators().end()) {
      if (it->precedence < minPrec) break;
      m_lexer.next();
      auto right = pratParse(it->rightAssociative ? it->precedence : it->precedence + 1);
      left = makeBinExpr(std::move(left), std::move(right), std::string{it->id});
    } else {
      // FIXME: ugly, hacky...
      if (!(3 < minPrec)) {
        if (auto constant = parseConstant(tok->raw)) {
          auto rhs = parseTerm();
          left = makeBinExpr(std::move(left), std::move(rhs), std::string{"*"});
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
