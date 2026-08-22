#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <locale>
#include <stdexcept>
#include "helpers.hpp"
#include "numen/numen.hpp"

using namespace std::chrono;

namespace {

numen::EvalOptions configAt(year_month_day now) {
  return numen::EvalOptions{
      .now = sys_days(now),
      .timezone = test::zone("UTC"),
      .dateTimeFormat = {.relative = false, .format = numen::DateTimeFormatOptions::TimeFormat::Neutral},
  };
}

void assertDate(std::string_view expr, year_month_day now, std::string_view expected) {
  auto config = configAt(now);
  test::assertExpr(expr, std::format("{} 00:00:00 ({})", expected, config.timezone->name()), config);
}

}; // namespace

TEST_CASE("Month/year shifts clamp to the end of the month") {
  assertDate("1 month ago", {2026y, March, 31d}, "2026-02-28");
  assertDate("1 month ago", {2026y, May, 31d}, "2026-04-30");
  assertDate("1 year ago", {2028y, February, 29d}, "2027-02-28");
  assertDate("1 month ago", {2026y, March, 15d}, "2026-02-15");
}

TEST_CASE("time anchors should parse as dates or instants") {
  numen::Numen calc{};
  auto now = sys_days{year_month_day{2026y, January, 18d}} + 2h + 30min; // 2026-01-18 02:30:00
  auto config = configAt({2026y, January, 18d});
  config.now = now;

  test::assertExpr("now", "2026-01-18 02:30:00 (Etc/UTC)", config);
  test::assertExpr("time", "2026-01-18 02:30:00 (Etc/UTC)", config);
  test::assertExpr("date", "2026-01-18 00:00:00 (Etc/UTC)", config);
  test::assertExpr("tomorrow", "2026-01-19 00:00:00 (Etc/UTC)", config);
  test::assertExpr("yesterday", "2026-01-17 00:00:00 (Etc/UTC)", config);
}

TEST_CASE("relative formatting drops the midnight time") {
  auto config = configAt({2026y, January, 18d});
  config.dateTimeFormat.relative = true;

  test::assertExpr("18 Jan 2001", "2001-01-18 (Etc/UTC)", config);
  test::assertExpr("18 Jan 2001 13:30", "2001-01-18 13:30:00 (Etc/UTC)", config);
}

TEST_CASE("localized date time formatting follows the configured locale") {
  auto localizedConfig = [](const char *locale) {
    auto config = configAt({2026y, January, 18d});
    config.dateTimeFormat = {.relative = false,
                             .withTz = false,
                             .format = numen::DateTimeFormatOptions::TimeFormat::Local,
                             .locale = locale};
    return config;
  };
  auto available = [](const char *name) {
    try {
      std::locale l{name};
      return true;
    } catch (const std::runtime_error &) { return false; }
  };

  // assertions stay loose on purpose: the exact strings depend on the host's locale data
  numen::Numen calc;

  if (available("en_US.UTF-8")) {
    auto res = calc.evaluate("18 Jan 2001 13:30:15", localizedConfig("en_US.UTF-8"));
    REQUIRE(res);
    CHECK((res->starts_with("01/18") || res->starts_with("1/18"))); // month first; MSVC does not pad
    // glibc's en_US %X is 12-hour, Apple's is 24-hour: only the hour digits are common ground
    CHECK((res->contains("1:30:15") || res->contains("13:30:15")));
  } else {
    WARN("en_US.UTF-8 locale not available, skipping");
  }

  if (available("fr_FR.UTF-8")) {
    auto res = calc.evaluate("18 Jan 2001 13:30:15", localizedConfig("fr_FR.UTF-8"));
    REQUIRE(res);
    CHECK(res->starts_with("18"));    // day first; glibc separates with '/', Apple with '.'
    CHECK(res->contains("13:30:15")); // 24-hour clock
  } else {
    WARN("fr_FR.UTF-8 locale not available, skipping");
  }
}

TEST_CASE("Trying to convert to non existent timezone fails") {
  numen::Numen calc{};
  REQUIRE_FALSE(calc.evaluate("now to WrongTimezone"));
}
