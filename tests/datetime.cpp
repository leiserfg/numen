#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>

const auto UTC = std::chrono::locate_zone("UTC");
constexpr auto TAG = "[datetime]";

const abacus::EvalConfig evalOpts = []() {
  return abacus::EvalConfig{
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
