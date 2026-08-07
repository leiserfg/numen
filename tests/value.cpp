#include "value.hpp"
#include <catch2/catch_test_macros.hpp>

using abacus::Value;

TEST_CASE("scaled applies the exponent in one step", "[value]") {
  CHECK(Value::scaled(15, 2) == Value{1500});
  CHECK(Value::scaled(1, 0) == Value{1});
  CHECK(Value::scaled(1, -1) * Value{10} == Value{1});
  CHECK(Value::scaled(25, -3) * Value{1000} == Value{25});

  // scaling once is what keeps a literal correctly rounded
  CHECK(Value::scaled(1609344, -6).toDouble() == 1.609344);
  CHECK(Value::scaled(745645, -6).toDouble() == 0.745645);
  CHECK(Value::scaled(3141593, -6).toDouble() == 3.141593);
}

TEST_CASE("pow", "[value]") {
  CHECK(Value{2}.pow(Value{10}) == Value{1024});
  CHECK(Value{2}.pow(Value{-1}) == Value{0.5});
  CHECK(Value{16}.pow(Value{0.5}) == Value{4});
  CHECK(Value{10}.pow(Value{400}).render() == "inf");
}

TEST_CASE("mod keeps the fraction and the sign of the left operand", "[value]") {
  CHECK(Value{7.5}.mod(Value{2}) == Value{1.5});
  CHECK(Value{5.5}.mod(Value{2.5}) == Value{0.5});
  CHECK(Value{-7}.mod(Value{3}) == Value{-1});
  CHECK(Value{7}.mod(Value{-3}) == Value{1});
  CHECK_THROWS(Value{5}.mod(Value{0}));
}

TEST_CASE("dividing by zero is rejected", "[value]") {
  CHECK_THROWS(Value{1} / Value{0});
  CHECK_THROWS(Value{0} / Value{0});
}

TEST_CASE("shifts are bounded by the width they act on", "[value]") {
  CHECK((Value{1} << Value{8}) == Value{256});
  CHECK((Value{1} << Value{62}) == Value{4611686018427387904});
  CHECK((Value{256} >> Value{2}) == Value{64});
  CHECK_THROWS(Value{1} << Value{64});
  CHECK_THROWS(Value{1} << Value{-1});
}

TEST_CASE("bitwise operands are truncated toward zero", "[value]") {
  CHECK((Value{1.5} & Value{3}) == Value{1});
  CHECK((Value{7.9} | Value{8}) == Value{15});
  CHECK((Value{-1.5} & Value{3}) == Value{3});
}

TEST_CASE("rendering", "[value]") {
  CHECK(Value{0.1}.render() == "0.1");
  CHECK(Value{1.0 / 3}.render() == "0.333333");
  CHECK(Value{1234567.5}.render() == "1234567.5");
  CHECK(Value{1e-7}.render() == "1e-07");
  CHECK(Value{255}.render(abacus::NumberOutputFormat::Hexadecimal) == "0xff");
  CHECK(Value{511}.render(abacus::NumberOutputFormat::Octal) == "0o777");
  CHECK(Value{256}.render(abacus::NumberOutputFormat::Binary) == "0b100000000");

  // a unit that dictates its own precision caps the fraction
  CHECK(Value{0.223923}.render(abacus::NumberOutputFormat::Decimal, 2) == "0.22");
  CHECK(Value{1234.7}.render(abacus::NumberOutputFormat::Decimal, 0) == "1235");
}
