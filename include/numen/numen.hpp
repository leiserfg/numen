#pragma once
#include "numen/unit.hpp"
#include <bits/chrono.h>
#include <chrono>
#include <compare>
#include <expected>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include "abstract-currency-provider.hpp"

namespace numen {

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

struct Duration {
  std::optional<std::chrono::years> years;
  std::optional<std::chrono::months> months;

  // anything that is not a calendar unit can collapse as seconds
  std::optional<std::chrono::seconds> seconds;
  std::optional<std::chrono::nanoseconds> subsecond;

  // every producer returns this, so `subsecond` is always under a second and
  // always shares the sign of `seconds`. read the fields directly.
  Duration normalised() const {
    constexpr auto perSecond = std::chrono::nanoseconds{std::chrono::seconds{1}}.count();

    auto s = seconds.value_or(std::chrono::seconds{0}).count();
    auto ns = subsecond.value_or(std::chrono::nanoseconds{0}).count();

    s += ns / perSecond;
    ns %= perSecond;

    if (s > 0 && ns < 0) { --s, ns += perSecond; }
    if (s < 0 && ns > 0) { ++s, ns -= perSecond; }

    Duration n = *this;
    n.seconds = std::chrono::seconds{s};
    n.subsecond = std::chrono::nanoseconds{ns};

    return n;
  }

  std::chrono::seconds total() const {
    return years.value_or(std::chrono::years(0)) + months.value_or(std::chrono::months{0}) +
           seconds.value_or(std::chrono::seconds{0});
  }

  Duration operator+(const Duration &rhs) const {
    Duration n;
    n.years = years.value_or(std::chrono::years{0}) + rhs.years.value_or(std::chrono::years{0});
    n.months = months.value_or(std::chrono::months{0}) + rhs.months.value_or(std::chrono::months{0});
    n.seconds = seconds.value_or(std::chrono::seconds{0}) + rhs.seconds.value_or(std::chrono::seconds{0});
    n.subsecond =
        subsecond.value_or(std::chrono::nanoseconds{0}) + rhs.subsecond.value_or(std::chrono::nanoseconds{0});
    return n.normalised();
  }

  Duration operator-(const Duration &rhs) const {
    Duration n;
    n.years = years.value_or(std::chrono::years{0}) - rhs.years.value_or(std::chrono::years{0});
    n.months = months.value_or(std::chrono::months{0}) - rhs.months.value_or(std::chrono::months{0});
    n.seconds = seconds.value_or(std::chrono::seconds{0}) - rhs.seconds.value_or(std::chrono::seconds{0});
    n.subsecond =
        subsecond.value_or(std::chrono::nanoseconds{0}) - rhs.subsecond.value_or(std::chrono::nanoseconds{0});
    return n.normalised();
  }

  auto operator<=>(const Duration &rhs) const {
    if (auto order = total() <=> rhs.total(); order != 0) return order;
    return subsecond.value_or(std::chrono::nanoseconds{0}) <=>
           rhs.subsecond.value_or(std::chrono::nanoseconds{0});
  }

  bool operator==(const Duration &rhs) const {
    return total() == rhs.total() && subsecond.value_or(std::chrono::nanoseconds{0}) ==
                                         rhs.subsecond.value_or(std::chrono::nanoseconds{0});
  }
};

// the single place a value plus a duration unit becomes a Duration
std::optional<Duration> durationFrom(double value, const UnitDef &unit);

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

  // the rendered result, which is what should be displayed
  std::string text;

  NumberOutputFormat format;

  struct Unit {
    // owned: a view into the expression would dangle for callers of compute()
    std::string raw;
    std::optional<CompoundUnit> resolved;

    const UnitDef *def() const { return resolved ? resolved->sole() : nullptr; }
  };

  std::optional<Unit> unit;

  bool explicitlyConverted = false;

  // n holds the fraction, so "50%" is 0.5
  bool isPercentage = false;

  bool operator==(const Number &rhs) const { return n == rhs.n; }
  std::partial_ordering operator<=>(const Number &rhs) const { return n <=> rhs.n; }
};

struct Boolean {
  bool value;
  auto operator<=>(const Boolean &rhs) const = default;
};

using ValueType = std::variant<Number, DateTime, Boolean, Duration>;

struct ComputedValue {
  ValueType value;

  bool isNumber() const { return std::holds_alternative<Number>(value); }

  bool isDateTime() const { return std::holds_alternative<DateTime>(value); }

  const Number *asNumber() const { return std::get_if<Number>(&value); }
  Number *asNumber() { return std::get_if<Number>(&value); }
  const DateTime *asDateTime() const { return std::get_if<DateTime>(&value); }

  const Duration *asDuration() { return std::get_if<Duration>(&value); }
  const Duration *asDuration() const { return std::get_if<Duration>(&value); }

  std::string_view valueTypeName() const {
    return std::visit(
        [](const auto &v) {
          using T = std::remove_cvref_t<decltype(v)>;
          if constexpr (std::is_same_v<T, Duration>) {
            return "Duration";
          } else if constexpr (std::is_same_v<T, Number>) {
            return "Number";
          } else if constexpr (std::is_same_v<T, DateTime>) {
            return "DateTime";
          } else {
            static_assert(std::is_same_v<T, Boolean>);
            return "Boolean";
          }
        },
        value);
  }
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

class Numen {
public:
  Numen();

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

}; // namespace numen
