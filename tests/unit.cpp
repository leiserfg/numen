#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("unit should tag any expression", "[unit]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("5 * 2 + 10 usd");
    REQUIRE(res.has_value());
    REQUIRE(res->unitRaw == "usd");
    REQUIRE(res->asNumber()->n == 20);
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

TEST_CASE("unit should always apply to everything to the left of it", "[unit]") {
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

TEST_CASE("Unit should convert implicitly when in a binary expression", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1km + 100m");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1100);
    REQUIRE(res->unitRaw == "m");
  }

  {
    auto res = calc.compute("30min + 1hour");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1.5);
    REQUIRE(res->unitRaw == "hour");
  }

  {
    auto res = calc.compute("29min + 1hour + 120 seconds - 60seconds to hours");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1.5);
    REQUIRE(res->unitRaw == "hours");
  }
}
