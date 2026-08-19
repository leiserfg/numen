#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include "datetime.hpp"
#include "numen/numen.hpp"
#include "timezone.hpp"

namespace numen {

namespace detail {

Duration subtractDates(const DateTime &lhs, const DateTime &rhs) {
  using namespace std::chrono;

  auto lhsDays = floor<days>(lhs.time);
  auto rhsDays = floor<days>(rhs.time);

  // calendar difference a -> b, assuming a <= b
  auto amd = year_month_day{floor<days>(lhsDays)};
  auto bmd = year_month_day{floor<days>(rhsDays)};

  auto atod = lhs.time - lhsDays;
  auto btod = rhs.time - rhsDays;

  auto m = (bmd.year() / bmd.month()) - (amd.year() / amd.month()); // chrono::months, exact
  if (bmd.day() < amd.day()) --m;                                   // last month isn't complete yet

  auto anchor = amd + m; // same clamped month-shift you already have
  if (!anchor.ok()) anchor = anchor.year() / anchor.month() / last;

  auto d = sys_days{bmd} - sys_days{anchor}; // exact leftover days

  auto y = m / 12;
  auto mo = m % 12;

  seconds secs =
      duration_cast<seconds>(days{d}) - seconds{std::abs(duration_cast<seconds>(atod - btod).count())};

  return Duration{.years = std::chrono::years{std::abs(y.count())},
                  .months = std::chrono::months{std::abs(mo.count())},
                  .seconds = seconds{std::abs(secs.count())}};
}

static TimePoint parseDateTimeLiteral(const DateTimeLiteral &d, const std::chrono::time_zone &tz,
                                      TimePoint now) {
  std::chrono::year_month_day today{std::chrono::floor<std::chrono::days>(now)};

  auto process = [&](auto &&date) {
    std::chrono::local_seconds t{std::chrono::local_days{date}};

    if (auto time = d.time) {
      if (auto h = time->hours) t += *h;
      if (auto min = time->minutes) t += *min;
      if (auto secs = time->seconds) t += *secs;
    }
    return tz.to_sys(t);
  };

  return std::visit(
      [&](const auto &v) {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::chrono::weekday>) {
          std::chrono::year_month_weekday date{d.year.value_or(today.year()), d.month.value_or(today.month()),
                                               v[0]};
          return process(date);
        } else if constexpr (std::is_same_v<T, std::chrono::day>) {
          std::chrono::year_month_day date{d.year.value_or(today.year()), d.month.value_or(today.month()), v};
          return process(date);
        }
      },
      d.day.value_or(today.day()));
}

DateTime parseDateTime(const DateString &d, const std::chrono::time_zone &userTz, TimePoint now) {
  auto tz = d.timezone
                .and_then([](auto &&t) -> std::optional<const std::chrono::time_zone *> {
                  return TimezoneDB{}.query(t.name);
                })
                .value_or(&userTz);

  auto visitor = [&](const auto &value) -> TimePoint {
    using T = std::remove_cvref_t<decltype(value)>;
    if constexpr (std::is_same_v<T, DateTimeLiteral>) {
      return parseDateTimeLiteral(value, *tz, now);
    } else if constexpr (std::is_same_v<T, RelativeDateTimeLiteral>) {
      return std::visit(
          [&](auto &&anchor) {
            using A = std::remove_cvref_t<decltype(anchor)>;
            int sign = value.direction == RelativeDateTimeLiteral::Direction::Past ? -1 : 1;
            if constexpr (std::is_same_v<A, std::chrono::weekday>) {
              // TODO: implement weekday delta
              return now;
            } else {
              static_assert(std::is_same_v<A, Duration>);
              if (value.precision == DateTimePrecision::Date) {
                std::chrono::year_month_day date{std::chrono::floor<std::chrono::days>(now)};
                now = tz->to_sys(std::chrono::local_seconds{std::chrono::local_days{date}});
              }

              if (auto &y = anchor.years) now = shift<std::chrono::years>(now, sign * *y);
              if (auto &m = anchor.months) now = shift<std::chrono::months>(now, sign * *m);
              if (auto &s = anchor.seconds) now = now + sign * *s;
              return now;
            }
          },
          value.delta);
    } else {
      return now;
    }
  };

  auto instant = std::visit(visitor, d.value);

  return DateTime{
      .time = instant, .tz = tz, .offset = d.timezone ? d.timezone->offset : std::chrono::seconds{}};
}

} // namespace detail

bool DateTime::isCurrentTimezone() const { return tz == std::chrono::get_tzdb().current_zone(); }

std::string DateTime::toTimezoneString() const {
  std::string out{};

  out += tz->name();

  if (offset.count() != 0) {
    auto hours = offset.count() / 3600;
    auto minutes = std::abs((offset.count() - hours * 3600) / 60);

    if (minutes != 0) {
      out += std::format("{:+}:{}", hours, minutes);
    } else {
      out += std::format("{:+}", hours);
    }
  }

  return out;
}

std::string DateTime::toString(const DateTimeFormatOptions &opts) const {
  constexpr auto fl = [](auto &&time) { return std::chrono::floor<std::chrono::days>(time); };
  auto now = std::chrono::system_clock::now();
  const auto userTz = std::chrono::current_zone();
  std::chrono::zoned_time zt{tz, time + offset};
  const auto localTime = userTz->to_local(time);
  const auto localNow = userTz->to_local(now);
  const bool isSameLocalDay = fl(localNow) == fl(localTime);
  const bool hasTime = (localTime - fl(localTime)).count() != 0;
  std::string out{};

  // we still want to show the date for today if there is no time
  if (!opts.relative || !isSameLocalDay || !hasTime) {
    if (opts.format == DateTimeFormatOptions::TimeFormat::Local) {
      out += std::format("{:%x}", zt);
    } else {
      out += std::format("{:%F}", zt);
    }
  }

  if (!opts.relative || hasTime) {
    if (!out.empty()) out += " ";
    if (opts.format == DateTimeFormatOptions::TimeFormat::Local) {
      out += std::format("{:%r}", zt);
    } else {
      out += std::format("{:%H:%M:%OS}", zt);
    }
  }

  if (tz && opts.withTz) {
    if (!out.empty()) out += " ";
    out += std::format("({})", toTimezoneString());
  }

  return out;
}

} // namespace numen
