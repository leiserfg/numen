#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

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

/*
TEST_CASE("should parse YYYY/MM/DD", TAG) { REQUIRE(true); }

TEST_CASE("should parse DD/MM/YYYY", TAG) { REQUIRE(true); }

TEST_CASE("should parse MM/DD/YYYY", TAG) { REQUIRE(true); }
*/
