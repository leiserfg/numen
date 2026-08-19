#include "helpers.hpp"
#include "numen/numen.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <format>
#include <string_view>

TEST_CASE("Should parse value type using parse method") {
  numen::Numen calc{};

  {
    auto result = calc.parse<numen::DateTime>("18 jan 2001 + 3 days");
    REQUIRE(result);
  }

  {
    auto result = calc.parse<int>("150 * 3 * 2");
    REQUIRE(result);
    REQUIRE(result.value() == 900);
  }

  {
    auto result = calc.parse<double>("1/2");
    REQUIRE(result);
    REQUIRE(result.value() == 0.5);
  }

  {
    auto result = calc.parse<int>("1/2");
    REQUIRE(result);
    REQUIRE(result.value() == 0);
  }

  {
    auto result = calc.parse<int>("1mb to bytes");
    REQUIRE(result);
    REQUIRE(result.value() == 1000000);
  }

  {
    auto result = calc.parse<std::chrono::days>("tomorrow - yesterday");
    REQUIRE(result);
    REQUIRE(result.value().count() == 2);
  }

  {
    auto result = calc.parse<std::chrono::hours>("tomorrow - yesterday + 5h + 30min");
    REQUIRE(result);
    REQUIRE(result.value().count() == 53);
  }

  {
    auto result = calc.parse<unsigned>("18 Jan 2001 to unix", {.timezone = std::chrono::locate_zone("UTC")});
    REQUIRE(result);
    REQUIRE(result.value() == 979776000);
  }

  {
    using namespace std::chrono;
    auto result = calc.parse<system_clock::time_point>("18 Jan 2001", {.timezone = locate_zone("UTC")});
    REQUIRE(result);
    REQUIRE(duration_cast<seconds>(result.value().time_since_epoch()).count() == 979776000);
  }

  {
    auto result = calc.parse<numen::DateTime>("18 Jan 2001");
    REQUIRE(result);
    REQUIRE(result->isCurrentTimezone());
  }
};

TEST_CASE("should return an error if parsing wrong type") {
  numen::Numen calc{};
  auto res = calc.parse<numen::Boolean>("1 + 1");
  REQUIRE_FALSE(res);
}

TEST_CASE("toString on a computed value matches evaluate") {
  numen::Numen calc{};
  const auto &opts = test::frozenConfig();

  for (std::string_view expr : {"1.5 km + 200 m", "3 days + 4h + 250ms", "18 jan 2001 + 3 days", "1 == 1",
                                "0xff to binary", "10 usd + 5 usd", "-2 months - 1 day"}) {
    CAPTURE(expr);
    auto computed = calc.compute(expr, opts);
    auto evaluated = calc.evaluate(expr, opts);
    REQUIRE(computed);
    REQUIRE(evaluated);
    REQUIRE(computed->toString(opts.effectiveDateTimeFormat()) == *evaluated);
  }

  auto duration = calc.parse<numen::Duration>("1 yr 2 months 3 days 4h", opts);
  REQUIRE(duration);
  REQUIRE(duration->toString() == "1 yr 2 months 3 days 4 hr");
  REQUIRE(std::format("{}", *duration) == "1 yr 2 months 3 days 4 hr");
  REQUIRE(std::format("[{:>12}]", numen::Boolean{true}) == "[        true]");
  REQUIRE(std::format("{}", *calc.compute("1.5 km + 200 m", opts)) == "1.7km");
}
