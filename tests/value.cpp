#include "value.hpp"
#include <catch2/catch_test_macros.hpp>

using abacus::Exact;
using abacus::Integer;
using abacus::Value;

TEST_CASE("decimal literals are exact fractions", "[value]") {
  auto tenth = Value::scaled(1, -1);
  auto fifth = Value::scaled(2, -1);
  auto threeTenths = Value::scaled(3, -1);

  REQUIRE(tenth.isExact());
  CHECK(tenth + fifth == threeTenths);
  CHECK((tenth + fifth - threeTenths).isZero());
  CHECK((tenth + fifth).isExact());
}

TEST_CASE("division round trips exactly", "[value]") {
  CHECK(Value{1} / Value{3} * Value{3} == Value{1});
  CHECK(Value{1} / Value{3} + Value{1} / Value{3} + Value{1} / Value{3} == Value{1});
  CHECK(Value{22} / Value{7} * Value{7} == Value{22});
}

TEST_CASE("integers stay exact past the double mantissa", "[value]") {
  Value big{Integer{"123456789012345678"}};
  CHECK(numerator(*(big + Value{1}).asExact()) == Integer{"123456789012345679"});

  Value pow53{Integer{1} << 53};
  CHECK((pow53 + Value{1}).asInteger() == Integer{"9007199254740993"});

  Value pow64{Integer{1} << 64};
  CHECK((pow64 + Value{1}).asInteger() == Integer{"18446744073709551617"});
}

TEST_CASE("scaled builds the value the lexer describes", "[value]") {
  CHECK(Value::scaled(15, 2) == Value{1500});
  CHECK(Value::scaled(1, 0) == Value{1});
  CHECK(Value::scaled(1, 300).isInteger());

  // pinned against a known multiple rather than against itself
  CHECK(Value::scaled(Integer{"1609344"}, -6) * Value{1000000} == Value{1609344});
  CHECK(Value::scaled(25, -3) * Value{1000} == Value{25});
  CHECK(Value::scaled(1, -1) * Value{10} == Value{1});
  CHECK(Value::scaled(1, 300) / Value::scaled(1, 299) == Value{10});
}

TEST_CASE("fromDouble keeps the fraction", "[value]") {
  // an implicit conversion to the integer constructor used to swallow it
  CHECK(Value::fromDouble(1.5).toDouble() == 1.5);
  CHECK(Value::fromDouble(1.609344).toDouble() == 1.609344);
  CHECK(Value::fromDouble(0.5) != Value{0});
}

TEST_CASE("pow is exact only for an integer exponent", "[value]") {
  CHECK(Value{2}.pow(Value{10}) == Value{1024});
  CHECK(Value{2}.pow(Value{10}).isExact());
  CHECK(Value{2}.pow(Value{-1}) == Value::scaled(5, -1));
  CHECK(Value{2}.pow(Value{-2}) == Value::scaled(25, -2));
  CHECK(Value::scaled(5, -1).pow(Value{2}) == Value::scaled(25, -2));

  // beyond the exact ceiling it degrades rather than trying to build the integer
  CHECK(!Value{10}.pow(Value{100000}).isExact());
  CHECK(!Value{2}.pow(Value::scaled(5, -1)).isExact());

  CHECK_THROWS(Value{0}.pow(Value{-1}));
}

TEST_CASE("mod keeps the fraction and the sign of the left operand", "[value]") {
  CHECK(Value::scaled(75, -1).mod(Value{2}) == Value::scaled(15, -1));
  CHECK(Value::scaled(55, -1).mod(Value::scaled(25, -1)) == Value::scaled(5, -1));
  CHECK(Value{-7}.mod(Value{3}) == Value{-1});
  CHECK(Value{7}.mod(Value{-3}) == Value{1});
  CHECK_THROWS(Value{5}.mod(Value{0}));
}

TEST_CASE("inexactness is contagious", "[value]") {
  auto pi = Value::fromDouble(3.14159265358979323846);

  CHECK(!pi.isExact());
  CHECK(!(pi + Value{1}).isExact());
  CHECK(!(pi * Value{0}).isExact());
  CHECK(!(Value{1} + pi).isExact());
}

TEST_CASE("comparison is exact only when both sides are", "[value]") {
  CHECK(Value::scaled(3, -1) != Value::scaled(4, -1));
  CHECK(Value{1} < Value{2});
  CHECK(Value{2} > Value{1});
  CHECK(Value{2} >= Value{2});

  // a double carries ~1e-16 of error, so equality has to tolerate it
  auto root2 = Value::fromDouble(1.4142135623730951);
  CHECK(root2 * root2 == Value{2});

  // but the tolerance must not merge genuinely distinct values
  CHECK(Value::fromDouble(1.0) != Value::fromDouble(1.0000001));
}

TEST_CASE("integer interrogation", "[value]") {
  CHECK(Value{5}.isInteger());
  CHECK(!Value::scaled(3, -1).isInteger());
  CHECK(Value{7}.asInteger() == Integer{7});
  CHECK(!Value::scaled(3, -1).asInteger().has_value());
  CHECK(!Value::fromDouble(4.0).asInteger().has_value());
}

TEST_CASE("dividing by zero is rejected", "[value]") {
  CHECK_THROWS(Value{1} / Value{0});
  CHECK_THROWS(Value{0} / Value{0});
  CHECK_THROWS(Value{1} / Value::fromDouble(0.0));
}

TEST_CASE("negation preserves exactness", "[value]") {
  CHECK((-Value::scaled(3, -1)) + Value::scaled(3, -1) == Value{0});
  CHECK((-Value{5}).isExact());
  CHECK(!(-Value::fromDouble(5.0)).isExact());
}
