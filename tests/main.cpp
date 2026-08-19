#include "numen/numen.hpp"
#include "helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <numbers>
#include <string_view>

using test::assertExpr;
using test::assertNumber;

TEST_CASE("Should parse floating point numbers correctly", "[float]") {
  assertNumber("2.345678", 2.345678, 1e-7);
}

TEST_CASE("Should add floating point numbers correctly", "[float]") {
  assertNumber("2.345678 + 1.42", 3.765678, 1e-7);
}

TEST_CASE("support common constants", "[constant]") {
  auto [name, value] = GENERATE(table<std::string_view, double>({
      {"pi", std::numbers::pi},
      {"e", std::numbers::e},
      {"phi", std::numbers::phi},
  }));

  assertNumber(name, value, 0);
}

TEST_CASE("support operations with constants", "[constant]") {
  assertNumber("pi", 3.14, 1e-2);
  assertNumber("pi*2", 6.28, 1e-2);
  assertNumber("2*pi", 6.28, 1e-2);
  assertNumber("2(pi)", 6.28, 1e-2);
}

TEST_CASE("implicit multiplication of constants", "[constant]") {
  constexpr auto pi2 = std::numbers::pi * 2;
  constexpr auto pi4 = std::numbers::pi * 4;

  auto [expr, expected] = GENERATE_COPY(table<std::string_view, double>({
      {"2pi", pi2},
      {"pi2", pi2},
      {"4 * 2pi + 1", 4 * pi2 + 1},
      {"pi(1+1)", pi2},
      {"(1+1)pi", pi2},
      {"pi2^2", pi4},
      {"2^2pi", pi4},
  }));

  assertNumber(expr, expected, 1e-2);
}

TEST_CASE("Handle thousand separators", "[number]") { assertNumber("1_000_000 + 1_000", 1001000); }

TEST_CASE("Any amount of separator should be ignored", "[number]") { assertNumber("1_0_0______0", 1000); }

TEST_CASE("planned NLP operator phrasings", "[.][future]") {
  assertExpr("2 to the power of 8", "256");
  assertExpr("100 + 2 to the power of 8", "356");
  assertExpr("2 exp 8", "256");
}

TEST_CASE("deeply nested and very long input does not overflow the stack", "[robustness]") {
  numen::Numen calc;

  SECTION("nested parentheses") {
    std::string expr = std::string(200, '(') + "1" + std::string(200, ')');
    auto res = calc.evaluate(expr);
    REQUIRE(res);
    CHECK(res.value() == "1");
  }

  SECTION("a long chain of the same operator") {
    std::string expr = "1";
    for (int i = 0; i < 2000; ++i) {
      expr += "+1";
    }
    auto res = calc.evaluate(expr);
    REQUIRE(res);
    CHECK(res.value() == "2,001");
  }

  SECTION("stacked unary operators") {
    auto even = calc.evaluate(std::string(500, '-') + "1");
    REQUIRE(even);
    CHECK(even.value() == "1");

    auto odd = calc.evaluate(std::string(501, '-') + "1");
    REQUIRE(odd);
    CHECK(odd.value() == "-1");
  }
}
