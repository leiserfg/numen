#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>

const auto UTC = std::chrono::locate_zone("UTC");
constexpr auto TAG = "[datetime]";

const abacus::EvalConfig evalOpts = []() {
  // freeze "now" so that tests stay valid
  std::chrono::year_month_day now{std::chrono::year{2026}, std::chrono::month{7}, std::chrono::day(26)};

  return abacus::EvalConfig{
      .now = std::chrono::sys_days(now),
      .timzone = UTC,
  };
}();

TEST_CASE("should parse time, delimited with :", TAG) {
  abacus::Abacus calc;

  auto res = calc.evaluate("2:42:21", evalOpts);

  REQUIRE(res);
  REQUIRE(res.value() == "2026-07-26 02:42:21 (Etc/UTC)");
}

TEST_CASE("should allow adding duration to time", TAG) {
  auto tz = std::chrono::locate_zone("UTC");
  abacus::Abacus calc;

  auto res = calc.evaluate("2:42:21 + 1hour", evalOpts);

  REQUIRE(res);
  REQUIRE(res.value() == "2026-07-26 03:42:21 (Etc/UTC)");
}

TEST_CASE("should parse DD/MM/YYYY", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("18/01/2001", evalOpts);

  REQUIRE(res.value() == "2001-01-18 00:00:00 (Etc/UTC)");
  REQUIRE(true);
}

TEST_CASE("should parse DD/MM/YYYY hh:mm:ss", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("18/01/2001 13:30:15", evalOpts);

  REQUIRE(res.value() == "2001-01-18 13:30:15 (Etc/UTC)");
  REQUIRE(true);
}

TEST_CASE("should add duration to date time", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("18/01/2001 13:30:15 + 1d + 5min", evalOpts);

  REQUIRE(res.value() == "2001-01-19 13:35:15 (Etc/UTC)");
  REQUIRE(true);
}

TEST_CASE("should parse YYYY/MM/DD", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("2001/01/18", evalOpts);

  REQUIRE(res.value() == "2001-01-18 00:00:00 (Etc/UTC)");
  REQUIRE(true);
}

TEST_CASE("separators should be adjacent to numbers", TAG) {
  abacus::Abacus calc;
  auto res = calc.compute("2001 / 1 / 18", evalOpts);

  REQUIRE(res->isNumber());
  REQUIRE(std::round(res->asNumber()->n) == 111);
  REQUIRE(true);
}

TEST_CASE("should allow YYYY/MM/DD to have a timezone", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("2001/01/18 New York", evalOpts);

  REQUIRE(res.value() == "2001-01-18 00:00:00 (America/New_York)");
  REQUIRE(true);
}

TEST_CASE("ambiguous date format should be parsed as DD/MM/YYYY ", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("11/08/2025", evalOpts);

  REQUIRE(res.value() == "2025-08-11 00:00:00 (Etc/UTC)");
  REQUIRE(true);
}

TEST_CASE("should parse MM/DD/YYYY", TAG) {
  abacus::Abacus calc;
  auto res = calc.evaluate("01/18/2001", evalOpts);

  REQUIRE(res.value() == "2001-01-18 00:00:00 (Etc/UTC)");
  REQUIRE(true);
}

TEST_CASE("should parse month name", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("18 Jan 2001", evalOpts);

    REQUIRE(res.value() == "2001-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
  {
    auto res = calc.evaluate("18 January 2001", evalOpts);

    REQUIRE(res.value() == "2001-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
  {
    auto res = calc.evaluate("18 january 2001", evalOpts);
    REQUIRE(res.value() == "2001-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

TEST_CASE("should parse <month_name> <day_of_the_month>", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("Jan 18", evalOpts);
    REQUIRE(res.value() == "2026-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }

  {
    auto res = calc.evaluate("January 18", evalOpts);
    REQUIRE(res.value() == "2026-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

TEST_CASE("should parse <month_name> <day_of_the_month> <year>", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("Jan 18 2025", evalOpts);
    REQUIRE(res.value() == "2025-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

TEST_CASE("should parse <year> <month_name> <day_of_the_month>", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("2025 Jan 18", evalOpts);
    REQUIRE(res.value() == "2025-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

// very cursed, but hey...
TEST_CASE("should parse <month_name> <year> <day_of_the_month>", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("Jan 2025 18", evalOpts);
    REQUIRE(res.value() == "2025-01-18 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

TEST_CASE("should parse <month_name> <year>", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("Jan 2025", evalOpts);
    REQUIRE(res.value() == "2025-01-01 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

TEST_CASE("should parse <year> <month_name>", TAG) {
  abacus::Abacus calc;

  {
    auto res = calc.evaluate("2025 Jan", evalOpts);
    REQUIRE(res.value() == "2025-01-01 00:00:00 (Etc/UTC)");
    REQUIRE(true);
  }
}

TEST_CASE("subtracting two dates should produce the difference in seconds", TAG) {
  {
    abacus::Abacus calc;
    auto res = calc.compute("01/18/2001 1:00:00 - 01/18/2001 00:00:00", evalOpts);

    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 3600); // 1 hour
  }

  {
    abacus::Abacus calc;
    auto res = calc.compute("01/18/2001 1:00:00 - 01/18/2001 00:00:00 to hours", evalOpts);

    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1); // 1 hour
  }
}
