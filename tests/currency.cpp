#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include "region-currency.hpp"
#include "vicinae-currency-provider.hpp"

TEST_CASE("Locale to currency mapping") {
  using abacus::currencyForLocale;
  using abacus::currencyForRegion;

  REQUIRE(currencyForRegion("FR") == "EUR");
  REQUIRE(currencyForRegion("fr") == "EUR");
  REQUIRE(currencyForRegion("US") == "USD");
  REQUIRE_FALSE(currencyForRegion("AQ").has_value());
  REQUIRE_FALSE(currencyForRegion("FRA").has_value());

  REQUIRE(currencyForLocale("fr_FR") == "EUR");
  REQUIRE(currencyForLocale("en-US") == "USD");
  REQUIRE(currencyForLocale("fr_FR.UTF-8@euro") == "EUR");
  REQUIRE(currencyForLocale("en_US.utf8") == "USD");
  REQUIRE(currencyForLocale("uz-Cyrl-UZ") == "UZS");
  REQUIRE(currencyForLocale("ja_JP") == "JPY");
  REQUIRE_FALSE(currencyForLocale("fr").has_value());
  REQUIRE_FALSE(currencyForLocale("C").has_value());
  REQUIRE_FALSE(currencyForLocale("POSIX").has_value());
  REQUIRE_FALSE(currencyForLocale("").has_value());
}

TEST_CASE("Use currency provider") {
  abacus::Abacus calc{};
  auto currency = std::make_unique<VicinaeCurrencyProvider>();

  currency->updateRates();
  calc.setCurrencyProvider(std::move(currency));

  REQUIRE(calc.evaluate("100 usd to eur"));
}
