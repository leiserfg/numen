#include "abacus/abacus.hpp"
#include "abacus/unit.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

namespace {

UnitTerm term(const UnitDatabase &db, const std::string &id, std::int8_t exponent = 1) {
  auto def = db.findUnit(id);
  REQUIRE(def);
  return UnitTerm{.def = *def, .display = id, .exponent = exponent};
}

} // namespace

TEST_CASE("a compound's dimension is the sum of its terms", "[unit][compound]") {
  UnitDatabase db;

  CHECK(CompoundUnit{{term(db, "m")}}.dimension() == Dimension{.length = 1});
  CHECK(CompoundUnit{{term(db, "m", 2)}}.dimension() == Dimension{.length = 2});

  // a composed unit must land on the same signature as the named one
  CHECK(CompoundUnit{{term(db, "m", 2)}}.dimension() == dimensionOf(dimensions::AREA));
  CHECK(CompoundUnit{{term(db, "km"), term(db, "h", -1)}}.dimension() == dimensionOf(dimensions::SPEED));
  CHECK(CompoundUnit{{term(db, "l")}}.dimension() == dimensionOf(dimensions::VOLUME));

  // like over like cancels to a plain number
  CHECK(CompoundUnit{{term(db, "km"), term(db, "m", -1)}}.dimension() == Dimension{});
}

TEST_CASE("a compound's factor agrees with the equivalent named unit", "[unit][compound]") {
  UnitDatabase db;

  auto factorOf = [&](const std::string &id) {
    auto def = db.findUnit(id);
    REQUIRE(def);
    return def->factor;
  };

  CHECK(CompoundUnit{{term(db, "km", 2)}}.factor() == factorOf("sqkm"));
  CHECK(CompoundUnit{{term(db, "m", 2)}}.factor() == factorOf("sqm"));
  CHECK_THAT((CompoundUnit{{term(db, "km"), term(db, "h", -1)}}.factor()),
             Catch::Matchers::WithinRel(factorOf("kmh"), 1e-12));
  CHECK(CompoundUnit{{term(db, "m"), term(db, "s", -1)}}.factor() == factorOf("mps"));
}

TEST_CASE("a compound renders the way it was typed", "[unit][compound]") {
  UnitDatabase db;

  auto render = [](CompoundUnit u) { return u.render(); };

  CHECK(render({{term(db, "km")}}) == "km");
  CHECK(render({{term(db, "m", 2)}}) == "m²");
  CHECK(render({{term(db, "m", 3)}}) == "m³");
  CHECK(render({{term(db, "m", 4)}}) == "m^4");

  CHECK(render({{term(db, "km"), term(db, "h", -1)}}) == "km/h");
  CHECK(render({{term(db, "kg"), term(db, "m", 2), term(db, "s", -2)}}) == "kg·m²/s²");

  // no numerator: "2 / 1km" reads as "2/km"
  CHECK(render({{term(db, "km", -1)}}) == "/km");

  // several denominators need grouping, "usd/kg·m" would read as (usd/kg)·m
  CHECK(render({{term(db, "usd"), term(db, "kg", -1), term(db, "m", -1)}}) == "usd/(kg·m)");

  // the casing of the original token survives
  CHECK(render({{term(db, "KM"), term(db, "hr", -1)}}) == "KM/hr");
}

TEST_CASE("unit should tag any expression", "[unit]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("5 * 2 + 10 usd");
    REQUIRE(res.has_value());
    REQUIRE(res->asNumber()->n == 20);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "usd");
  }
}

TEST_CASE("unit can be converted using the 'to' operator", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1 km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "m");
  }
}

TEST_CASE("unit can be converted using the 'in' operator", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1 km in m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "m");
  }
}

TEST_CASE("unit can be converted many times in a row", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1 km in m to km");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "km");
  }
}

TEST_CASE("artithmetic can be used on a converted unit", "[unit]") {
  abacus::Abacus calc;
  {
    auto res = calc.compute("1km to m to km * 10 + 5");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 15);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "km");
  }
}

TEST_CASE("unit name should be inferred from context", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1m to s");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 60);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "s");
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
  REQUIRE(res->asNumber()->unit);
  REQUIRE(res->asNumber()->unit->raw == "in");
}

TEST_CASE("Unit should convert implicitly when in a binary expression", "[unit]") {
  abacus::Abacus calc;

  {
    auto res = calc.compute("1km + 100m");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    // the larger of the two units decides, and 1 + 100/1000 is not exactly 1.1
    CHECK_THAT(res->asNumber()->n, Catch::Matchers::WithinAbs(1.1, 1e-12));
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "km");
  }

  {
    auto res = calc.compute("30min + 1hour");
    REQUIRE(res);
  }

  {
    auto res = calc.compute("29min + 1hour + 120 seconds - 60seconds to hours");
    REQUIRE(res);
    REQUIRE(res->isNumber());
    REQUIRE(res->asNumber()->n == 1.5);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "hours");
  }
}
