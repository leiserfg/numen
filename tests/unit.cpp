#include "builtin-units.hpp"
#include "numen/numen.hpp"
#include "numen/unit.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

namespace {

UnitTerm term(const UnitDatabase &db, const std::string &id, std::int8_t exponent = 1) {
  auto def = db.findUnit(id);
  REQUIRE(def);
  return UnitTerm{.def = *def, .exponent = exponent};
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

TEST_CASE("an alias expands to the composition it names", "[unit][compound]") {
  UnitDatabase db;

  auto expand = [&](const std::string &name) {
    auto found = db.findCompounds(name);
    REQUIRE(found.size() == 1);
    return found.front();
  };

  CHECK(expand("sqm").render() == "m²");
  CHECK(expand("kmh").render() == "km/h");
  CHECK(expand("kph").render() == "km/h");
  CHECK(expand("mph").render() == "mi/h");

  CHECK(expand("km2").dimension() == dimensionOf(dimensions::AREA));
  CHECK(expand("mps").dimension() == dimensionOf(dimensions::SPEED));

  // the factor now follows from the parts rather than being stated
  CHECK(expand("sqkm").factor() == 1e6);
  CHECK(expand("sqft").factor() == 0.3048 * 0.3048);
  CHECK_THAT(expand("kmh").factor(), Catch::Matchers::WithinRel(1000.0 / 3600, 1e-12));
  CHECK_THAT(expand("mph").factor(), Catch::Matchers::WithinRel(0.44704, 1e-12));
}

// the units that are not products of anything we have stay atomic, so nothing
// enforces their factor except this
TEST_CASE("an atomic unit still agrees with its composed equivalent", "[unit][compound]") {
  numen::Numen calc;

  CHECK(calc.evaluate("1 hectare to m*m") == "10,000m²");
  CHECK(calc.evaluate("1 acre to m*m") == "4,046.856422m²");
  CHECK(calc.evaluate("1 knot to m/s") == "0.514444m/s");
}

TEST_CASE("a compound renders in conventional notation", "[unit][compound]") {
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
  CHECK(render({{term(db, "usd"), term(db, "kg", -1), term(db, "m", -1)}}) == "$/(kg·m)");

  // however the unit was spelled, it comes back the same way
  CHECK(render({{term(db, "KM"), term(db, "hr", -1)}}) == "km/h");
  CHECK(render({{term(db, "meters", 2)}}) == "m²");
  CHECK(render({{term(db, "celsius")}}) == "°C");

  CHECK(render({{term(db, "microseconds")}}) == "µs");
  CHECK(render({{term(db, "Mm")}}) == "Mm");
  CHECK(render({{term(db, "kib")}}) == "KiB");
}

TEST_CASE("a compound names itself", "[unit][compound]") {
  UnitDatabase db;

  auto name = [](CompoundUnit u) { return u.name(); };

  CHECK(name({{term(db, "km")}}) == "kilometer");
  CHECK(name({{term(db, "usd")}}) == "US Dollar");
  CHECK(name({{term(db, "sek")}}) == "Swedish Krona");
  CHECK(name({{term(db, "km", 2)}}) == "square kilometer");
  CHECK(name({{term(db, "m", 3)}}) == "cubic meter");
  CHECK(name({{term(db, "km"), term(db, "h", -1)}}) == "kilometer per hour");
  CHECK(name({{term(db, "usd"), term(db, "kg", -1), term(db, "m", -1)}}) ==
        "US Dollar per kilogram per meter");
  CHECK(name({{term(db, "microseconds")}}) == "microsecond");
  CHECK(name({{term(db, "kib")}}) == "kibibyte");
}

TEST_CASE("a unit resolves from its symbol", "[unit]") {
  UnitDatabase db;

  auto sole = [&](std::string_view q) {
    auto found = db.findUnit(std::string{q});
    REQUIRE(found);
    return found->id;
  };

  CHECK(sole("$") == "usd");
  CHECK(sole("€") == "eur");
  CHECK(sole("£") == "gbp");
  CHECK(sole("¥") == "jpy");
  CHECK(sole("₿") == "btc");
  CHECK(sole("°C") == "celsius");
  CHECK(sole("°F") == "fahrenheit");

  // µ resolves as a prefix, micro sign and greek mu alike
  CHECK(db.findUnit("µs")->dimension == dimensions::DURATION);
  CHECK(db.findUnit("μs")->dimension == dimensions::DURATION);

  CHECK_FALSE(db.findUnit("€ "));
}

TEST_CASE("a rendered unit parses back", "[unit]") {
  UnitDatabase db;

  auto roundtrip = [&](std::string_view q) {
    auto found = db.findCompounds(q);
    REQUIRE(!found.empty());
    auto once = found.front().render();

    auto again = db.findCompounds(once);
    REQUIRE(!again.empty());
    CHECK(again.front().render() == once);
    return once;
  };

  CHECK(roundtrip("km") == "km");
  CHECK(roundtrip("sqm") == "m²");
  CHECK(roundtrip("km2") == "km²");
  CHECK(roundtrip("m³") == "m³");
  CHECK(roundtrip("usd") == "$");
  CHECK(roundtrip("celsius") == "°C");
  CHECK(roundtrip("microsecond") == "µs");
}

TEST_CASE("every builtin renders under a spelling that resolves", "[unit]") {
  UnitDatabase db;

  for (const auto &def : units::builtins()) {
    CAPTURE(def.id);

    auto notation = soleUnit(def).render();
    CHECK_FALSE(notation.empty());

    // an ambiguous spelling may settle on another reading ("m" is a meter
    // before it is a minute), but must still render the same
    auto reread = db.findCompounds(notation);
    REQUIRE_FALSE(reread.empty());
    CHECK(reread.front().render() == notation);

    CHECK_FALSE(soleUnit(def).name().empty());
  }
}

TEST_CASE("a number renders its unit in conventional notation", "[unit]") {
  numen::Numen calc;

  auto render = [&](std::string_view expr) {
    auto res = calc.compute(expr, {.implicitCurrencyConversion = false});
    REQUIRE(res);
    return res->toString();
  };

  CHECK(render("1.5 KM") == "1.5km");
  CHECK(render("2 meters * 3 meters") == "6m²");

  CHECK(render("12.5 usd") == "$12.5");
  CHECK(render("0 - 5 euro") == "-€5");
  // a currency with no symbol of its own keeps its code
  CHECK(render("100 sek") == "100sek");
}

TEST_CASE("unit should tag any expression", "[unit]") {
  numen::Numen calc;
  {
    auto res = calc.compute("5 * 2 + 10 usd");
    REQUIRE(res.has_value());
    REQUIRE(res->asNumber()->n == 20);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "usd");
  }
}

TEST_CASE("unit can be converted using the 'to' operator", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("1 km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "m");
  }
}

TEST_CASE("unit can be converted using the 'in' operator", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("1 km in m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "m");
  }
}

TEST_CASE("unit can be converted many times in a row", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("1 km in m to km");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "km");
  }
}

TEST_CASE("artithmetic can be used on a converted unit", "[unit]") {
  numen::Numen calc;
  {
    auto res = calc.compute("1km to m to km * 10 + 5");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 15);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "km");
  }
}

TEST_CASE("unit name should be inferred from context", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("1m to s");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 60);
    REQUIRE(res->asNumber()->unit);
    REQUIRE(res->asNumber()->unit->raw == "s");
  }
}

TEST_CASE("inconvertible units should return an error", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("1meter to s");
    REQUIRE(!res);
  }
}

TEST_CASE("unit without number should assume a quantity of 1", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 1000);
  }
}

TEST_CASE("unit should always apply to everything to the left of it", "[unit]") {
  numen::Numen calc;

  {
    auto res = calc.compute("2^8 km to m");
    REQUIRE(res);
    REQUIRE(res->asNumber()->n == 256000);
  }
}

TEST_CASE("should convert between systems, with non base unit", "[unit]") {
  numen::Numen calc;

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
  numen::Numen calc;

  {
    auto res = calc.compute("1m to 150");
    REQUIRE(!res);
  }
}

TEST_CASE("in should work as a conversion operator in the right context, and a "
          "unit name in others",
          "[unit]") {
  numen::Numen calc;
  auto res = calc.compute("1m in in"); // 1 meter to inches, here the middle
                                       // in is an alias for the 'go' operator
  REQUIRE(res);
  REQUIRE(std::round(res->asNumber()->n) == 39);
  REQUIRE(res->asNumber()->unit);
  REQUIRE(res->asNumber()->unit->raw == "in");
}

TEST_CASE("Unit should convert implicitly when in a binary expression", "[unit]") {
  numen::Numen calc;

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
