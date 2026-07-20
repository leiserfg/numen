#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string_view>

auto inline evaluate(std::string_view expr) {
  abacus::Abacus calc;
  return calc.evaluate(std::string{expr});
}

#define assertExpr(expr, expected)                                             \
  {                                                                            \
    abacus::Abacus calc;                                                       \
    auto res = calc.evaluate(std::string{expr});                               \
    REQUIRE(res);                                                              \
    REQUIRE(res.value() == expected);                                          \
  }

TEST_CASE("Should compute basic expression") {
  auto eval = evaluate("1 + 1");
  REQUIRE(eval);
  REQUIRE(eval.value() == "2");
}

TEST_CASE("Should compute basic expression with many terms") {
  auto eval = evaluate("1 + 1 + 1");
  REQUIRE(eval);
  REQUIRE(eval.value() == "3");
}

TEST_CASE("proper priority without parentheses") {
  auto eval = evaluate("1 + 2 * 4");
  REQUIRE(eval);
  REQUIRE(eval.value() == "9");
}

TEST_CASE("priority with parentheses") {
  auto eval = evaluate("(1 + 2) * 4");
  REQUIRE(eval);
  REQUIRE(eval.value() == "12");
}

TEST_CASE("modulo operator") {
  assertExpr("5 % 2", "1");
  assertExpr("6 % 2", "0");
  assertExpr("6 mod 2", "0");
  assertExpr("6 modulo 2", "0");
}

TEST_CASE("addition operator") { assertExpr("1 + 1", "2"); }

TEST_CASE("minus operator") {
  assertExpr("1 - 1", "0");
  assertExpr("1 - 5", "-4");
}

TEST_CASE("multiplication operator") {
  assertExpr("5 mul 5", "25");
  assertExpr("5 * 5", "25");
  assertExpr("5 * 5 mul 10", "250");
  assertExpr("5 * (5 mul 10 + 1)", "255");
}

TEST_CASE("division operator") {
  assertExpr("5 div 5", "1");
  assertExpr("5 / 5", "1");
  assertExpr("100 div 10 / 2", "5")
}

TEST_CASE("whitespaces do no matter") {
  assertExpr("1+1", "2");
  assertExpr("(1+1)/2", "1");
  assertExpr("((1+1)/2)*2", "2");
}

TEST_CASE("basic float support") {
  assertExpr("1.2 + 2.5", "3.7");
  assertExpr("5 / 2", "2.5");
}

TEST_CASE("nested parentheses") {
  {
    auto eval = evaluate("((1 + 2) * 4) / 2");
    REQUIRE(eval);
    REQUIRE(eval.value() == "6");
  }
  {
    auto eval = evaluate("(1 + 2) * 4 / 2");
    REQUIRE(eval);
    REQUIRE(eval.value() == "6");
  }
}

TEST_CASE("exponent operator") {
  // we support a few nlp variations
  assertExpr("2 ^ 8", "256");
  assertExpr("2 power 8", "256");
  assertExpr("2 to the power of 8", "256");
  assertExpr("100 + 2 to the power of 8", "356");
  assertExpr("2 exp 8", "256");
}

TEST_CASE("Basic percentage") {
  {
    auto eval = evaluate("20% of 100");
    REQUIRE(eval);
    REQUIRE(eval.value() == "20");
  }

  {
    auto eval = evaluate("20% of 100 * 2");
    REQUIRE(eval);
    REQUIRE(eval.value() == "40");
  }

  {
    auto eval = evaluate("(20% of 100) * 2");
    REQUIRE(eval);
    REQUIRE(eval.value() == "40");
  }

  {
    auto eval = evaluate("(10 + 10)% of 100 + 2");
    REQUIRE(eval);
    REQUIRE(eval.value() == "22");
  }

  {
    assertExpr("40% of 10^2", "40");
  }
}

TEST_CASE("time now unix") {}
