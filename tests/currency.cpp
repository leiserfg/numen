#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include "vicinae-currency-provider.hpp"

TEST_CASE("Use currency provider") {
  abacus::Abacus calc{};
  auto currency = std::make_unique<VicinaeCurrencyProvider>();

  currency->updateRates();
  calc.setCurrencyProvider(std::move(currency));

  REQUIRE(calc.evaluate("100 usd to eur"));
}
