#include "duration.hpp"
#include "numen/unit.hpp"
#include <chrono>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace numen {

namespace {
using namespace std::chrono;
constexpr auto perMinute = seconds{minutes{1}}.count();
constexpr auto perHour = seconds{hours{1}}.count();
constexpr auto perDay = seconds{days{1}}.count();
constexpr auto perWeek = seconds{weeks{1}}.count();
constexpr auto perMonth = seconds{months{1}}.count();

constexpr auto nanosPerSecond = nanoseconds{seconds{1}}.count();
constexpr auto nanosPerMilli = nanoseconds{milliseconds{1}}.count();
constexpr auto nanosPerMicro = nanoseconds{microseconds{1}}.count();
} // namespace

std::optional<Duration> durationFrom(double value, const UnitDef &unit) {
  if (unit.dimension != dimensions::DURATION) return std::nullopt;

  Duration d;

  if (unit.id == "year" || unit.id == "month") {
    auto asMonths = unit.id == "year" ? value * 12 : value;
    auto whole = std::trunc(asMonths);

    using MonthRep = std::chrono::months::rep;
    // the bound rounds up on its way to double, so landing on it is already past
    if (std::abs(whole) >= static_cast<double>(std::numeric_limits<MonthRep>::max())) return std::nullopt;

    d.months = std::chrono::months{static_cast<MonthRep>(whole)};

    if (auto rest = asMonths - whole; rest != 0) {
      d.seconds = std::chrono::seconds{std::llround(rest * perMonth)};
    }

    return d;
  }

  auto asNanos = value * unit.factor * nanosPerSecond;
  if (asNanos != std::trunc(asNanos)) return std::nullopt;
  if (std::abs(asNanos) >= static_cast<double>(std::numeric_limits<long long>::max())) return std::nullopt;

  auto nanos = static_cast<long long>(asNanos);
  d.seconds = std::chrono::seconds{nanos / nanosPerSecond};
  d.subsecond = std::chrono::nanoseconds{nanos % nanosPerSecond};

  return d.normalised();
}

namespace detail {

Duration scaleDuration(const Duration &d, double factor) {
  using namespace std::chrono;

  auto months = static_cast<double>(d.years.value_or(years{0}).count() * 12 +
                                    d.months.value_or(std::chrono::months{0}).count()) *
                factor;
  auto wholeMonths = std::trunc(months);

  auto secs = static_cast<double>(d.seconds.value_or(seconds{0}).count()) * factor +
              static_cast<double>(d.subsecond.value_or(nanoseconds{0}).count()) * factor / nanosPerSecond +
              (months - wholeMonths) * perMonth;
  auto wholeSecs = std::trunc(secs);

  Duration out;
  out.months = std::chrono::months{static_cast<std::chrono::months::rep>(wholeMonths)};
  out.seconds = seconds{static_cast<long long>(wholeSecs)};
  out.subsecond = nanoseconds{std::llround((secs - wholeSecs) * nanosPerSecond)};

  return out.normalised();
}

} // namespace detail

std::string Duration::toString() const {
  auto calendar =
      years.value_or(std::chrono::years{0}).count() * 12 + months.value_or(std::chrono::months{0}).count();
  auto clock = seconds.value_or(std::chrono::seconds{0}).count();
  auto fraction = subsecond.value_or(std::chrono::nanoseconds{0}).count();

  std::vector<std::string> parts;

  const auto push = [&](long long n, std::string_view unit, bool plural) {
    if (n) parts.push_back(std::format("{} {}{}", n, unit, plural && n > 1 ? "s" : ""));
  };

  // the most negative value has no positive counterpart, so it saturates
  const auto magnitude = [](long long n) -> long long {
    if (n == std::numeric_limits<long long>::min()) return std::numeric_limits<long long>::max();
    return n < 0 ? -n : n;
  };

  const auto group = [&](long long total, auto &&emit) {
    auto before = parts.size();
    emit(magnitude(total));
    if (total < 0 && parts.size() > before) parts[before].insert(0, "-");
  };

  group(calendar, [&](long long m) {
    push(m / 12, "yr", true);
    push(m % 12, "month", true);
  });

  // the two share one sign, so they are emitted as a single group
  group(clock != 0 ? clock : fraction, [&](long long) {
    auto s = magnitude(clock);
    auto ns = magnitude(fraction);

    push(s / perWeek, "week", true);
    push(s % perWeek / perDay, "day", true);
    push(s % perDay / perHour, "hr", false);
    push(s % perHour / perMinute, "min", false);
    push(s % perMinute, "sec", false);
    push(ns / nanosPerMilli, "ms", false);
    push(ns % nanosPerMilli / nanosPerMicro, "us", false);
    push(ns % nanosPerMicro, "ns", false);
  });

  if (parts.empty()) return "0 sec";

  std::ostringstream oss;
  for (std::size_t i = 0; i != parts.size(); ++i) {
    if (i) oss << " ";
    oss << parts[i];
  }

  return oss.str();
}

} // namespace numen
