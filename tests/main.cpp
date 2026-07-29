#include "abacus/abacus.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <string_view>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

auto inline evaluate(std::string_view expr) {
  abacus::Abacus calc;
  return calc.evaluate(expr);
}

#define assertExpr(expr, expected)                                                                           \
  {                                                                                                          \
    abacus::Abacus calc;                                                                                     \
    auto res = calc.evaluate(std::string{expr});                                                             \
    REQUIRE(res);                                                                                            \
    REQUIRE(res.value() == expected);                                                                        \
  }

#define assertPrecision(a, b, precision)                                                                     \
  { REQUIRE_THAT(a, WithinAbs(b, std::pow(10, -precision))); }

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

TEST_CASE("Should parse floating point numbers correctly") {
  abacus::Abacus calc;
  auto res = calc.compute("2.345678");

  REQUIRE(res);
  REQUIRE(res->isNumber());
  assertPrecision(res->asNumber()->n, 2.345678, 7);
}

TEST_CASE("Should add floating point numbers correctly") {
  abacus::Abacus calc;
  auto res = calc.compute("2.345678 + 1.42");

  REQUIRE(res);
  REQUIRE(res->isNumber());
  assertPrecision(res->asNumber()->n, 3.765678, 7);
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

TEST_CASE("unary should apply to parenthesized term", "[unary]") { assertExpr("-(150 * 2 / 2 + 1)", "-151"); }

TEST_CASE("two negative signs could yield positive", "[unary]") { assertExpr("--(2^8)", "256"); }

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

TEST_CASE("Function call in function argument", "function") { assertExpr("max(min(1, 10),max(1, 5))", "5"); }

TEST_CASE("leftshit works", "[bitwise]") {
  assertExpr("1 << 8", "256");
  assertExpr("1 << 16", "65536");
}

TEST_CASE("righshift works", "[bitwise]") {
  assertExpr("256 >> 1", "128");
  assertExpr("256 >> 2", "64");
}

TEST_CASE("hex formatting", "[format]") { assertExpr("256 to hex", "0x100"); }

TEST_CASE("binary formatting", "[format]") { assertExpr("256 to binary", "0b100000000"); }

TEST_CASE("Numbers can be written in hex", "[base]") {
  assertExpr("0xFF", "255");
  assertExpr("0xFF + 10 + 0xA", "275");
}

TEST_CASE("Numbers can be written in octal", "[base]") { assertExpr("0o777", "511"); }

TEST_CASE("Numbers can be written in binary", "[base]") {
  assertExpr("0b10", "2");
  assertExpr("0b1111", "15");
}

TEST_CASE("Numbers from different bases can be combined", "[base]") {
  assertExpr("0b1111 * 0xF + 0o777 - 36", "700");
}

TEST_CASE("implicit multiplication for parenthesized terms", "[priority]") {
  // <expr> <parens>
  assertExpr("2(1+5)", "12");
  assertExpr("2 (1 + 5)", "12");
  assertExpr("2^2(1+5)", "24");
  assertExpr("2 ^ 2 (1 + 5)", "24");

  // <parens> <expr>
  assertExpr("(1+5)2", "12");
  assertExpr("( 1 + 5 ) 2", "12");
  assertExpr("( 1 + 5 ) 2^2", "24");
  assertExpr("( 1 + 5 ) (1+1+1+1)", "24");
}

TEST_CASE("support common constants", "[constant]") {
  std::initializer_list<std::tuple<std::string_view, double>> constants{
      {"pi", std::numbers::pi},
      {"e", std::numbers::e},
      {"phi", std::numbers::phi},
  };
  abacus::Abacus calc;

  for (auto [name, value] : constants) {
    auto res = calc.compute(name);
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == value);
  }
}

TEST_CASE("support operations with constants", "[constant]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("pi");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, 3.14, 2);
  }

  {
    auto res = calc.compute("pi*2");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, 6.28, 2);
  }

  {
    auto res = calc.compute("2*pi");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, 6.28, 2);
  }

  {
    auto res = calc.compute("2(pi)");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, 6.28, 2);
  }
}

TEST_CASE("implicit multiplication of constants") {
  abacus::Abacus calc;
  constexpr auto pi2 = std::numbers::pi * 2;
  constexpr auto pi4 = std::numbers::pi * 4;

  {
    auto res = calc.compute("2pi");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, pi2, 2);
  }

  {
    auto res = calc.compute("pi2");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, pi2, 2);
  }

  {
    auto res = calc.compute("4 * 2pi + 1");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, 4 * std::numbers::pi * 2 + 1, 2);
  }

  {
    auto res = calc.compute("pi(1+1)");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, pi2, 2);
  }

  {
    auto res = calc.compute("(1+1)pi");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    assertPrecision(res->asNumber()->n, pi2, 2);
  }

  {
    for (auto s : {"pi2^2", "2^2pi"}) {
      auto res = calc.compute(s);
      REQUIRE(res);
      REQUIRE(res->isNumber());
      assertPrecision(res->asNumber()->n, pi4, 2);
    }
  }
}

TEST_CASE("Handle thousand separators", "[number]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1_000_000 + 1_000");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1001000);
  };
}

TEST_CASE("Any amount of separator should be ignored", "[number]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("1_0_0______0");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1000);
  }
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

  { assertExpr("40% of 10^2", "40"); }
}
