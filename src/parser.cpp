#include "parser.hpp"
#include "timezone.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <initializer_list>
#include <numbers>
#include <string_view>

struct OperatorDefinition {
  std::string_view id;
  std::vector<std::string_view> aliases;
  int precedence;
};

const auto OPERATORS = std::to_array<OperatorDefinition>(
    {OperatorDefinition{.id = ">>", .aliases = {">>"}, .precedence = 1},
     OperatorDefinition{.id = "<<", .aliases = {"<<"}, .precedence = 1},
     OperatorDefinition{.id = "|", .aliases = {"|"}, .precedence = 1},
     OperatorDefinition{.id = "&", .aliases = {"&"}, .precedence = 1},
     OperatorDefinition{.id = "+", .aliases = {"+", "add", "plus"}, .precedence = 2},
     OperatorDefinition{.id = "-", .aliases = {"-", "minus"}, .precedence = 2},
     OperatorDefinition{.id = "*", .aliases = {"*", "mul"}, .precedence = 3},
     OperatorDefinition{.id = "/", .aliases = {"/", "div"}, .precedence = 3},
     OperatorDefinition{.id = "%", .aliases = {"%", "mod", "modulo"}, .precedence = 3},
     OperatorDefinition{.id = "^", .aliases = {"^", "pow", "power"}, .precedence = 4}});

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

std::unique_ptr<Expression> makeNumberExpr(double n) { return std::make_unique<Expression>(NumberString{n}); }

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

std::optional<std::chrono::weekday> parseWeekday(std::string_view s) {
  std::istringstream is{std::string{s}};
  std::chrono::weekday m;
  is >> std::chrono::parse("{:L%a}", m);
  if (!is) return std::nullopt;
  return m;
}

std::optional<std::chrono::month> parseMonth(std::string_view s) {
  std::istringstream is{std::string{s}};
  std::chrono::month m;
  is >> std::chrono::parse("%b", m);
  if (!is) return std::nullopt;
  return m;
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

std::optional<std::string_view> Parser::parseUnit() {
  if (auto tok = m_lexer.peakIf(Lexer::TokenType::String)) {
    if (auto unit = m_unitDb.findUnit(std::string{tok->raw})) { return tok->raw; }
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

std::optional<DateTimeLiteral> Parser::parseDate() {
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
  auto expr = std::unique_ptr<Expression>();

  if (auto tok = m_lexer.peak()) {
    if (auto constant = parseConstant(tok->raw)) {
      m_lexer.next();

      auto lhs = makeNumberExpr(*constant);

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

    if (auto tok = m_lexer.peak(); tok && tok->type != Lexer::TokenType::Operator) {
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

        while (true) {
          auto tok = m_lexer.peak();
          if (tok->raw == ")") break;
          fn.args.emplace_back(pratParse());
          tok = m_lexer.peak();

          if (tok->raw == ")") { break; }
          if (tok->raw != ",") { throw std::runtime_error("Expected , to add another argument"); }
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

  if (!expr) { throw std::runtime_error("Expected term"); }

  return expr;
}

std::unique_ptr<Expression> Parser::pratParse(int minPrec) {
  auto left = parseTerm();

  while (auto tok = m_lexer.peak()) {
    if (tok->raw == "to" || tok->raw == "in" | tok->raw == "->") {
      if (minPrec > 0) break;

      m_lexer.next();

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

    auto it = std::ranges::find_if(
        OPERATORS, [&](const OperatorDefinition &op) { return std::ranges::contains(op.aliases, tok->raw); });

    if (it != OPERATORS.end()) {
      if (it->precedence < minPrec) break;
      m_lexer.next();
      auto right = pratParse(it->precedence + 1);
      left = makeBinExpr(std::move(left), std::move(right), std::string{it->id});
    } else {
      if (!(3 < minPrec)) {
        if (auto constant = parseConstant(tok->raw)) {
          m_lexer.next();
          left = makeBinExpr(std::move(left), std::move(makeNumberExpr(*constant)), std::string{"*"});
          continue;
        } else if (tok->raw == "(") {
          left = makeBinExpr(std::move(left), std::move(parseTerm()), std::string{"*"});
          continue;
        }
      }
      break;
    }
  }

  return left;
}
