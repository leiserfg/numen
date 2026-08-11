#pragma once
#include "abacus/unit.hpp"
#include "abacus/abacus.hpp"
#include "lexer.hpp"
#include <bits/chrono.h>
#include <cassert>
#include "date-string.hpp"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

using namespace abacus;

struct Expression;

using NumberString = abacus::Value;

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

struct NamedUnitTerm {
  std::string_view name;
  int exponent = 1;
};

struct NamedUnit {
  std::vector<NamedUnitTerm> terms;

  // only a single token can be resolved against a sibling
  bool isSimple() const { return terms.size() == 1 && terms.front().exponent == 1; }
  std::string_view simpleName() const { return terms.front().name; }
};

struct NamedNumberFormat {
  std::string_view name;
};

using ConversionTarget = std::variant<NamedUnit, TimezoneLike, NamedNumberFormat>;

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

struct ParsedTime {
  std::optional<std::chrono::hours> hours;
  std::optional<std::chrono::minutes> minutes;
  std::optional<std::chrono::seconds> seconds;
};

struct DateTimeLiteral {
  std::optional<std::variant<std::chrono::day, std::chrono::weekday>> day;
  std::optional<std::chrono::month> month;
  std::optional<std::chrono::year> year;
  std::optional<ParsedTime> time;
};

template <typename T> struct Scanned {
  T data;
  std::size_t tokenCount = 0;
};

struct RelativeDateTimeLiteral {
  std::variant<Duration, std::chrono::weekday> anchor;
  enum class Direction : std::uint8_t { Past, Future } direction;
};

struct DateString {
  std::variant<DateTimeLiteral, RelativeDateTimeLiteral, std::string_view> value;
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

struct PercentExpression {
  std::unique_ptr<Expression> expr;
};

struct Expression {
  std::variant<BinaryExpression, UnaryExpression, NumberString, DateString, UnitExpression,
               ConversionExpression, Duration, FunctionCall, PercentExpression>
      data;

  const BinaryExpression *asBinaryExpression() const { return std::get_if<BinaryExpression>(&data); }

  const UnaryExpression *asUnaryExpression() const { return std::get_if<UnaryExpression>(&data); }

  const ConversionExpression *asConversion() const { return std::get_if<ConversionExpression>(&data); }

  const FunctionCall *asFunction() const { return std::get_if<FunctionCall>(&data); }
};

struct AST {
  std::unique_ptr<Expression> root;
};

class Parser {
public:
  Parser(std::string_view data, const UnitDatabase &unitDb);

  std::optional<OpaqueUnit> parseUnit();
  std::optional<NamedUnit> parseConversionTarget();

  bool isTimezoneToken(std::string_view name);
  std::optional<DateTimeLiteral> parseYYYYMMDD();
  std::optional<DateTimeLiteral> parseNaturalDateLiteral();
  std::optional<DateTimeLiteral> parseDate();
  // [<hour>]:[minute]:[second]
  std::optional<ParsedTime> parseTime(bool afterDate = false);
  std::optional<TimezoneLike> parseTimezone();
  std::optional<NamedNumberFormat> parseNumberFormat();
  std::optional<RelativeDateTimeLiteral> parseRelativeDateTimeLiteral();

  template <typename F> std::optional<std::string_view> greedyParse(int n, F fn) {
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

  AST parse();

protected:
  bool isPostfixPercent(const Lexer::Token &pct);
  std::optional<Scanned<abacus::Duration>> scanDuration();
  std::unique_ptr<Expression> parseTerm();
  std::unique_ptr<Expression> parseNumber();
  std::unique_ptr<Expression> pratParse(int minPrec = 0);
  std::unique_ptr<Expression> parseMul() { return pratParse(4); }

private:
  Lexer m_lexer;
  DateStringVocab m_dateStringVocab;
  const UnitDatabase &m_unitDb;
};
