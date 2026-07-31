#include "helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <numbers>
#include <string_view>

using test::assertExpr;
using test::assertNumber;

TEST_CASE("Should compute basic expression", "[arithmetic]") { assertExpr("1 + 1", "2"); }

TEST_CASE("Should compute basic expression with many terms", "[arithmetic]") {
  assertExpr("1 + 1 + 1", "3");
}

TEST_CASE("proper priority without parentheses", "[priority]") { assertExpr("1 + 2 * 4", "9"); }

TEST_CASE("Should parse floating point numbers correctly", "[float]") {
  assertNumber("2.345678", 2.345678, 1e-7);
}

TEST_CASE("Should add floating point numbers correctly", "[float]") {
  assertNumber("2.345678 + 1.42", 3.765678, 1e-7);
}

TEST_CASE("priority with parentheses", "[priority]") { assertExpr("(1 + 2) * 4", "12"); }

TEST_CASE("modulo operator", "[arithmetic]") {
  assertExpr("5 % 2", "1");
  assertExpr("6 % 2", "0");
  assertExpr("6 mod 2", "0");
  assertExpr("6 modulo 2", "0");
}

TEST_CASE("addition operator", "[arithmetic]") { assertExpr("1 + 1", "2"); }

TEST_CASE("should support negative number", "[unary]") {
  assertExpr("-1", "-1");
  assertExpr("-1 + -1", "-2");
  assertExpr("-1 - 1", "-2");
  assertExpr("-1 + - 1", "-2");
}

TEST_CASE("unary should apply to parenthesized term", "[unary]") { assertExpr("-(150 * 2 / 2 + 1)", "-151"); }

TEST_CASE("two negative signs could yield positive", "[unary]") { assertExpr("--(2^8)", "256"); }

TEST_CASE("as many unary operators can be used", "[unary]") {
  assertExpr("--++ ++ ++++++ - -123 + 1", "124");
}

TEST_CASE("minus operator", "[arithmetic]") {
  assertExpr("1 - 1", "0");
  assertExpr("1 - 5", "-4");
}

TEST_CASE("multiplication operator", "[arithmetic]") {
  assertExpr("5 * 5", "25");
  assertExpr("5 mul 5", "25");
  assertExpr("5 * 5 mul 10", "250");
  assertExpr("5 * (5 mul 10 + 1)", "255");
}

TEST_CASE("division operator", "[arithmetic]") {
  assertExpr("5 / 5", "1");
  assertExpr("5 div 5", "1");
  assertExpr("100 div 10 / 2", "5");
}

TEST_CASE("whitespaces do no matter", "[lexer]") {
  assertExpr("1+1", "2");
  assertExpr("(1+1)/2", "1");
  assertExpr("((1+1)/2)*2", "2");
}

TEST_CASE("basic float support", "[float]") {
  assertExpr("1.2 + 2.5", "3.7");
  assertExpr("5 / 2", "2.5");
}

TEST_CASE("nested parentheses", "[priority]") {
  assertExpr("((1 + 2) * 4) / 2", "6");
  assertExpr("(1 + 2) * 4 / 2", "6");
}

TEST_CASE("exponent operator", "[arithmetic]") {
  // we support a few nlp variations
  assertExpr("2 ^ 8", "256");
  assertExpr("2 pow 8", "256");
  assertExpr("2 power 8", "256");
}

TEST_CASE("planned NLP operator phrasings", "[.][future]") {
  assertExpr("2 to the power of 8", "256");
  assertExpr("100 + 2 to the power of 8", "356");
  assertExpr("2 exp 8", "256");
}

TEST_CASE("function call should be recognized", "[function]") {
  assertExpr("min(1, 5)", "1");
  assertExpr("max(1, 5)", "5");
}

TEST_CASE("expressions can be used inside function parameters", "[function]") {
  assertExpr("max(1 km to m, 100)", "1000");
}

TEST_CASE("Function call in function argument", "[function]") { assertExpr("max(min(1, 10),max(1, 5))", "5"); }

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

TEST_CASE("Basic percentage", "[percentage]") {
  assertExpr("20% of 100", "20");
  assertExpr("20% of 100 * 2", "40");
  assertExpr("(20% of 100) * 2", "40");
  assertExpr("(10 + 10)% of 100 + 2", "22");
  assertExpr("40% of 10^2", "40");
}

TEST_CASE("malformed expressions should error", "[error]") {
  abacus::Abacus calc;

  CHECK(!calc.evaluate("1 +"));
  CHECK(!calc.evaluate("*5"));
}

TEST_CASE("unknown function should error with its name", "[error]") {
  abacus::Abacus calc;

  auto res = calc.evaluate("foo(1, 2)");
  REQUIRE(!res);
  CHECK(res.error() == "Unknown function \"foo\"");
}
