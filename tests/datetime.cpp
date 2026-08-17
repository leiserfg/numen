#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include "helpers.hpp"
#include "numen/numen.hpp"

using namespace std::chrono;

namespace {

numen::EvalConfig configAt(year_month_day now) {
  return numen::EvalConfig{
      .now = sys_days(now),
      .timezone = locate_zone("UTC"),
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
  auto config = numen::EvalConfig{.now = now, .timezone = locate_zone("UTC")};

  test::assertExpr("now", "2026-01-18 02:30:00 (Etc/UTC)", config);
  test::assertExpr("time", "2026-01-18 02:30:00 (Etc/UTC)", config);
  test::assertExpr("date", "2026-01-18 00:00:00 (Etc/UTC)", config);
  test::assertExpr("tomorrow", "2026-01-19 00:00:00 (Etc/UTC)", config);
  test::assertExpr("yesterday", "2026-01-17 00:00:00 (Etc/UTC)", config);
}
