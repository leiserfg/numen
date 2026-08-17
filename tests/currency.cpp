#include "numen/numen.hpp"
#include <catch2/catch_test_macros.hpp>
#include "region-currency.hpp"
#include "mock-currency-provider.hpp"

TEST_CASE("Locale to currency mapping") {
  using numen::currencyForLocale;
  using numen::currencyForRegion;

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

TEST_CASE("currency results snap to the minor units of the target", "[currency]") {
  auto calc = test::mockCalc();

  // eur takes two decimals, jpy and krw none
  CHECK(calc.evaluate("100 usd to eur") == "92.35eur");
  CHECK(calc.evaluate("100 usd to jpy") == "15789jpy");
  CHECK(calc.evaluate("100 usd to krw") == "123457krw");
}

TEST_CASE("rounding money is a formatting concern, not a value one", "[currency]") {
  auto calc = test::mockCalc();

  auto res = calc.compute("100 usd to eur");
  REQUIRE(res);

  // what gets shown sits on the minor units
  CHECK(res->asNumber()->text == "92.35");

  // and the full precision survives into further arithmetic
  CHECK(calc.evaluate("(100 usd to eur) * 100") == "9234.57eur");
}

TEST_CASE("a currency is not padded out to its minor units", "[currency]") {
  auto calc = test::mockCalc();

  // the minor units cap the precision, they do not pad it: rendering money
  // properly is the caller's job, and half of the convention reads worse than none
  CHECK(calc.evaluate("1 usd") == "1usd");
  CHECK(calc.evaluate("0.9 usd") == "0.9usd");
  CHECK(calc.evaluate("1 usd / 2") == "0.5usd");
  CHECK(calc.evaluate("1 km") == "1km");
}

TEST_CASE("a currency amount lands on its minor units even without a conversion", "[currency]") {
  auto calc = test::mockCalc();

  CHECK(calc.evaluate("0.22392323090 usd") == "0.22usd");
  CHECK(calc.evaluate("10.005 usd") == "10.01usd");
  CHECK(calc.evaluate("1 usd / 3") == "0.33usd");

  // jpy has no minor units at all
  numen::EvalConfig jp{.locale = "ja_JP"};
  CHECK(calc.evaluate("1 jpy / 3", jp) == "0jpy");
  CHECK(calc.evaluate("1234.7 jpy", jp) == "1235jpy");

  // a plain unit is untouched by any of this
  CHECK(calc.evaluate("1 km / 3") == "0.333333km");
}

TEST_CASE("minor unit digits come from CLDR", "[currency]") {
  using numen::currencyDigits;

  CHECK(currencyDigits("JPY") == 0);
  CHECK(currencyDigits("KRW") == 0);
  CHECK(currencyDigits("BHD") == 3);
  CHECK(currencyDigits("CLF") == 4);

  // not listed by CLDR, so it takes the default
  CHECK(currencyDigits("USD") == 2);
  CHECK(currencyDigits("EUR") == 2);
  CHECK(currencyDigits("ZZZ") == 2);

  CHECK(currencyDigits("jpy") == 0);
  CHECK(currencyDigits("") == 2);
}

TEST_CASE("the provider supplies the rates a conversion uses", "[currency]") {
  numen::Numen calc;
  auto provider = std::make_unique<test::MockCurrencyProvider>();
  auto *handle = provider.get();

  calc.setCurrencyProvider(std::move(provider));
  provider = nullptr;

  CHECK(calc.evaluate("100 usd to eur") == "92.35eur");
  CHECK(calc.evaluate("100 usd to gbp") == "78.91gbp");

  // an unknown code has no rate, so the conversion cannot be done
  CHECK_FALSE(calc.evaluate("100 usd to zzz"));
  CHECK(handle->updateCount() == 0);
}

TEST_CASE("a rate applies inside a composed unit too", "[currency][compound]") {
  auto calc = test::mockCalc();

  CHECK(calc.evaluate("100 usd / 2 hr to eur/hr") == "46.172835eur/hr");
  CHECK(calc.evaluate("1000 usd / 1 kg to eur/kg") == "923.4567eur/kg");

  // the hour is a factor on both sides, so only the rate is left to apply
  CHECK(calc.evaluate("60 usd / 1 hr to eur/min") == "0.923457eur/min");

  // money per something is a rate, money times money is nothing at all
  CHECK_FALSE(calc.evaluate("1 usd * 1 usd"));
  CHECK_FALSE(calc.evaluate("1 usd / 1 hr to eur*eur"));

  // the same currency cancels before any rate is consulted, which is why this
  // works even on a calculator with no provider at all
  CHECK(calc.evaluate("100 usd / 2 hr to usd/hr") == "50usd/hr");
  CHECK(numen::Numen{}.evaluate("100 usd / 2 hr to usd/hr") == "50usd/hr");

  CHECK_FALSE(calc.evaluate("100 usd / 2 hr to zzz/hr"));
}
