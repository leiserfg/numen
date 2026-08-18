#pragma once

#include "numen/numen.hpp"
#include "parser.hpp"
#include <chrono>

namespace numen::detail {

// calendar difference, always non-negative: callers order the operands
Duration subtractDates(const DateTime &lhs, const DateTime &rhs);

DateTime parseDateTime(const DateString &d, const std::chrono::time_zone &userTz, TimePoint now);

// moves by whole calendar units, clamping to the last day of the month when
// the day does not exist there (jan 31 + 1 month is feb 28)
template <typename T> TimePoint shift(TimePoint t, T duration) {
  if (!duration.count()) return t;

  auto time = std::chrono::floor<std::chrono::days>(t);
  std::chrono::year_month_day ymd{time};
  auto tod = t - time;
  ymd += duration;

  if (!ymd.ok()) ymd = ymd.year() / ymd.month() / std::chrono::last;

  auto point = std::chrono::sys_days{ymd} + tod;

  return point;
}

} // namespace numen::detail
