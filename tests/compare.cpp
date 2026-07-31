#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

constexpr auto TAG = "[compare]";

const abacus::EvalConfig evalOpts = []() {
  // freeze "now" so that tests stay valid
  std::chrono::year_month_day now{std::chrono::year{2026}, std::chrono::month{7}, std::chrono::day(26)};

  return abacus::EvalConfig{
      .now = std::chrono::sys_days(now),
      .timzone = std::chrono::locate_zone("UTC"),
  };
}();

TEST_CASE("same numbers should compare equal", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("42 == 42") == "true");
  REQUIRE(calc.evaluate("42 == 21 * 2") == "true");
  REQUIRE(calc.evaluate("2^8==256") == "true");
  REQUIRE(calc.evaluate("-5 == -5") == "true");
  REQUIRE(calc.evaluate("1.5 == 1.5") == "true");
}

TEST_CASE("different numbers should not compare equal", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("42 == 21") == "false");
  REQUIRE(calc.evaluate("2^8==255") == "false");
  REQUIRE(calc.evaluate("-5 == 5") == "false");
}

TEST_CASE("different numbers should compare not equal", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("42 != 21") == "true");
  REQUIRE(calc.evaluate("2^8 != 255") == "true");
  REQUIRE(calc.evaluate("-5 != 5") == "true");
}

TEST_CASE("same numbers should not compare not equal", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("42 != 42") == "false");
  REQUIRE(calc.evaluate("42 != 21 * 2") == "false");
}

TEST_CASE("greater than should only hold for strictly greater numbers", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("42 > 21") == "true");
  REQUIRE(calc.evaluate("21 > 42") == "false");
  REQUIRE(calc.evaluate("42 > 42") == "false");
  REQUIRE(calc.evaluate("0 > -1") == "true");
  REQUIRE(calc.evaluate("1.5 > 1.4") == "true");
}

TEST_CASE("greater than or equal should hold for greater and equal numbers", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("42 >= 21") == "true");
  REQUIRE(calc.evaluate("42 >= 42") == "true");
  REQUIRE(calc.evaluate("21 >= 42") == "false");
  REQUIRE(calc.evaluate("-1 >= 1") == "false");
  REQUIRE(calc.evaluate("-1 >= 0") == "false");
}

TEST_CASE("less than should only hold for strictly smaller numbers", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("21 < 42") == "true");
  REQUIRE(calc.evaluate("42 < 21") == "false");
  REQUIRE(calc.evaluate("42 < 42") == "false");
  REQUIRE(calc.evaluate("-2 < -1") == "true");
  REQUIRE(calc.evaluate("-1 < 0") == "true");
  REQUIRE(calc.evaluate("1.4 < 1.5") == "true");
}

TEST_CASE("less than or equal should hold for smaller and equal numbers", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("21 <= 42") == "true");
  REQUIRE(calc.evaluate("42 <= 42") == "true");
  REQUIRE(calc.evaluate("42 <= 21") == "false");
  REQUIRE(calc.evaluate("0 <= -1") == "false");
}

TEST_CASE("comparison should bind looser than arithmetic", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("1 + 1 == 2") == "true");
  REQUIRE(calc.evaluate("2 * 3 > 4 + 1") == "true");
  REQUIRE(calc.evaluate("2 * 3 > 5 + 0") == "true");
  REQUIRE(calc.evaluate("10 - 5 <= 2 + 3") == "true");
  REQUIRE(calc.evaluate("2^3 < 3^2") == "true");
  REQUIRE(calc.evaluate("10 % 3 != 10 / 5") == "true");
}

TEST_CASE("units should convert implicitly before comparing", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("1 km == 1000 m") == "true");
  REQUIRE(calc.evaluate("1 km != 999 m") == "true");
  REQUIRE(calc.evaluate("1 km > 999 m") == "true");
  REQUIRE(calc.evaluate("1 km >= 1000 m") == "true");
  REQUIRE(calc.evaluate("1 km < 1001 m") == "true");
  REQUIRE(calc.evaluate("1 km <= 1000 m") == "true");
  REQUIRE(calc.evaluate("30min < 1hour") == "true");
  REQUIRE(calc.evaluate("2 hours <= 60 min") == "false");
}

TEST_CASE("dates should compare chronologically", TAG) {
  abacus::Abacus calc;

  REQUIRE(calc.evaluate("18/01/2001 == 18/01/2001", evalOpts) == "true");
  REQUIRE(calc.evaluate("18/01/2001 != 19/01/2001", evalOpts) == "true");
  REQUIRE(calc.evaluate("18/01/2001 < 19/01/2001", evalOpts) == "true");
  REQUIRE(calc.evaluate("18/01/2001 <= 18/01/2001", evalOpts) == "true");
  REQUIRE(calc.evaluate("19/01/2001 > 18/01/2001", evalOpts) == "true");
  REQUIRE(calc.evaluate("19/01/2001 >= 18/01/2001", evalOpts) == "true");
  REQUIRE(calc.evaluate("19/01/2001 < 18/01/2001", evalOpts) == "false");
}
