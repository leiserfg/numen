#include "numen/numen.hpp"
#include <catch2/catch_test_macros.hpp>

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
};

TEST_CASE("should return an error if parsing wrong type") {
  numen::Numen calc{};
  auto res = calc.parse<numen::Boolean>("1 + 1");
  REQUIRE_FALSE(res);
}
