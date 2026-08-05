#pragma once
#include "abacus/unit.hpp"
#include <chrono>
#include <compare>
#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include "abstract-currency-provider.hpp"

namespace abacus {

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

enum class DateTimeOutputFormat { DateTime, Time, Date };

struct DateTime {
  TimePoint time;
  const std::chrono::time_zone *tz = nullptr;
  std::chrono::seconds offset = std::chrono::seconds(0);

  DateTimeOutputFormat format = DateTimeOutputFormat::DateTime;

  auto operator<=>(const DateTime &rhs) const { return time <=> rhs.time; }
  bool operator==(const DateTime &rhs) const { return time == rhs.time; }
};

struct Time {
  std::chrono::seconds seconds;
};

enum class NumberOutputFormat { Decimal, Hexadecimal, Binary, Octal };

struct Number {
  // unit may remain ambigious until more information is known.
  // To deal with that, the attached unit can either be a fully
  // qualified unit or a "raw" unit, that is the unit-like string
  // as it was parsed.
  // For instance: in "1m to s" the "m" unit can refer to "meters" or "minutes".
  // In this case, the second operand is what will allow disambiguation since both
  // are durations. Until we are able to consider the conversion operation, "1m" is
  // ambiguous.

  double n;
  NumberOutputFormat format;

  struct Unit {
    std::string_view raw;
    std::optional<UnitDef> def;
  };

  std::optional<Unit> unit;

  bool explicitlyConverted = false;

  bool operator==(const Number &rhs) const { return n == rhs.n; }
  std::partial_ordering operator<=>(const Number &rhs) const { return n <=> rhs.n; }
};

struct Boolean {
  bool value;
  auto operator<=>(const Boolean &rhs) const = default;
};

using ValueType = std::variant<Number, DateTime, Boolean>;

struct ComputedValue {
  ValueType value;

  bool isNumber() const { return std::holds_alternative<Number>(value); }

  bool isDateTime() const { return std::holds_alternative<DateTime>(value); }

  const Number *asNumber() const { return std::get_if<Number>(&value); }
  Number *asNumber() { return std::get_if<Number>(&value); }
  const DateTime *asDateTime() const { return std::get_if<DateTime>(&value); }
};

struct EvalConfig {
  /**
   * Timepoint that should be used as the current time.
   * You most likely don't want to change this, the main use case is testing.
   */
  std::optional<TimePoint> now;

  const std::chrono::time_zone *timezone;

  /**
   * If the expression is only a currency, it will automatically be
   * converted to the locale currency.
   * e.g if locale is set to fr_FR, "100 usd" will automatically convert
   * to euro.
   */
  bool implicitCurrencyConversion = true;

  /**
   * Locale to use for implicit conversions. If not specified, the default locale
   * is used.
   */
  std::optional<std::string> locale;
};

class Abacus {
public:
  Abacus();

  std::expected<std::string, std::string> evaluate(std::string_view expr, const EvalConfig &opts = {});
  std::expected<ComputedValue, std::string> compute(std::string_view expr, const EvalConfig &opts = {});
  void printAST(const std::string &expr) const;

  void setCurrencyProvider(std::unique_ptr<AbstractCurrencyProvider> provider) {
    m_currencyProvider = std::move(provider);
    m_unitDb.setCurrencyProvider(*m_currencyProvider);
  }

private:
  std::unique_ptr<AbstractCurrencyProvider> m_currencyProvider;
  UnitDatabase m_unitDb;
};

}; // namespace abacus
