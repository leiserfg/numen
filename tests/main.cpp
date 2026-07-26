#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <numbers>
#include <string_view>

auto inline evaluate(std::string_view expr) {
  abacus::Abacus calc;
  return calc.evaluate(expr);
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
  // assertExpr("6 mod 2", "0");
  // assertExpr("6 modulo 2", "0");
}

TEST_CASE("addition operator") { assertExpr("1 + 1", "2"); }

TEST_CASE("should support negative number", "[unary]") {
  assertExpr("-1", "-1");
  assertExpr("-1 + -1", "-2");
  assertExpr("-1 - 1", "-2");
  assertExpr("-1 - 1", "-2");
  assertExpr("-1 + - 1", "-2");
}

TEST_CASE("unary should apply to parenthesized term", "[unary]") {
  assertExpr("-(150 * 2 / 2 + 1)", "-151");
}

TEST_CASE("two negative signs could yield positive", "[unary]") {
  assertExpr("--(2^8)", "256");
}

TEST_CASE("as many unary operators can be used", "[unary]") {
  assertExpr("--++ ++ ++++++ - -123 + 1", "124");
}

TEST_CASE("minus operator") {
  assertExpr("1 - 1", "0");
  assertExpr("1 - 5", "-4");
}

TEST_CASE("multiplication operator") {
  // assertExpr("5 mul 5", "25");
  assertExpr("5 * 5", "25");
  // assertExpr("5 * 5 mul 10", "250");
  // assertExpr("5 * (5 mul 10 + 1)", "255");
}

TEST_CASE("division operator") {
  // assertExpr("5 div 5", "1");
  assertExpr("5 / 5", "1");
  // assertExpr("100 div 10 / 2", "5")
}

TEST_CASE("expression support") {}

TEST_CASE("unit should tag any expression", "[unit]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("5 * 2 + 10 usd");
    REQUIRE(res.has_value());
    REQUIRE(res->unitRaw == "usd");
    REQUIRE(res->asNumber()->n == 20);
  }
}

TEST_CASE("right unit should take precedence over left unit", "[unit]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("5 * 2 + 10 usd * 100 gbp");
    REQUIRE(res.has_value());
    REQUIRE(res->asNumber()->n == 1010);
    REQUIRE(res->unitRaw == "gbp");
  }
}

TEST_CASE("unit should apply to parenthesized term", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("(5 * 2 + 10) usd * 100 gbp");
    REQUIRE(res.has_value());
    REQUIRE(res->unitRaw == "gbp");
    REQUIRE(res->asNumber()->n == 2000);
  }
}

TEST_CASE("unit can be converted using the 'to' operator", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1 km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
    REQUIRE(res->unitRaw == "m");
  }
}

TEST_CASE("unit can be converted using the 'in' operator", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1 km in m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
    REQUIRE(res->unitRaw == "m");
  }
}

TEST_CASE("unit can be converted many times in a row", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1 km in m to km");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1);
    REQUIRE(res->unitRaw == "km");
  }
}

TEST_CASE("artithmetic can be used on a converted unit", "[unit]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("1km to m to km * 10 + 5");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 15);
    REQUIRE(res->unitRaw == "km");
  }
}

TEST_CASE("unit name should be inferred from context", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1m to s");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 60);
    REQUIRE(res->unitRaw == "s");
  }
}

TEST_CASE("inconvertible units should return an error", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1meter to s");
    REQUIRE(!res);
  }
}

TEST_CASE("unit without number should assume a quantity of 1", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
  }
}

TEST_CASE("unit should always apply to everything to the left of it",
          "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("2^8 km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 256000);
  }
}

TEST_CASE("should convert between systems, with non base unit", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("km to in");
    REQUIRE(res);
    REQUIRE(std::round(res->asNumber()->n) == 39370);
  }

  {
    auto res = calc.compute("15000 in to km");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 0.381);
  }
}

TEST_CASE("no unit after conversion operator should error", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1m to 150");
    REQUIRE(!res);
  }
}

TEST_CASE("in should work as a conversion operator in the right context, and a "
          "unit name in others",
          "[unit]") {
  abacus::Abacus calc;
  auto res = calc.compute("1m in in"); // 1 meter to inches, here the middle
                                       // in is an alias for the 'go' operator
  REQUIRE(res);
  REQUIRE(std::round(res->asNumber()->n) == 39);
  REQUIRE(res->unitRaw == "in");
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
  // assertExpr("2 to the power of 8", "256");
  // assertExpr("100 + 2 to the power of 8", "356");
  // assertExpr("2 exp 8", "256");
}

TEST_CASE("function call should be recognized", "[function]") {
  assertExpr("min(1, 5)", "1");
  assertExpr("max(1, 5)", "5");
}

TEST_CASE("expressions can be used inside function parameters", "[function]") {
  assertExpr("max(1 km to m, 100)", "1000");
}

TEST_CASE("Function call in function argument", "function") {
  assertExpr("max(min(1, 10),max(1, 5))", "5");
}

TEST_CASE("leftshit works", "[bitwise]") {
  assertExpr("1 << 8", "256");
  assertExpr("1 << 16", "65536");
}

TEST_CASE("righshift works", "[bitwise]") {
  assertExpr("256 >> 1", "128");
  assertExpr("256 >> 2", "64");
}

TEST_CASE("hex formatting", "[format]") { assertExpr("256 to hex", "0x100"); }

TEST_CASE("binary formatting", "[format]") {
  assertExpr("256 to binary", "0b100000000");
}

/*
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
*/
