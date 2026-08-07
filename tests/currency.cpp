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

namespace {

// deliberately awkward rates, so the snapping is visible in the result
class FixedRateProvider : public AbstractCurrencyProvider {
public:
  std::optional<double> getRate(const std::string &code) const override {
    if (code == "usd") return 1.0;
    if (code == "eur") return 0.9234567;
    if (code == "jpy") return 157.891234;
    if (code == "krw") return 1234.5678;
    return std::nullopt;
  }

  void updateRates() override {}
};

abacus::Abacus fixedRateCalc() {
  abacus::Abacus calc{};
  calc.setCurrencyProvider(std::make_unique<FixedRateProvider>());
  return calc;
}

} // namespace

TEST_CASE("currency results snap to the minor units of the target", "[currency]") {
  auto calc = fixedRateCalc();

  // eur takes two decimals, jpy and krw none
  CHECK(calc.evaluate("100 usd to eur") == "92.35eur");
  CHECK(calc.evaluate("100 usd to jpy") == "15789jpy");
  CHECK(calc.evaluate("100 usd to krw") == "123457krw");
}

TEST_CASE("rounding money is a formatting concern, not a value one", "[currency]") {
  auto calc = fixedRateCalc();

  auto res = calc.compute("100 usd to eur");
  REQUIRE(res);

  // what gets shown sits on the minor units
  CHECK(res->asNumber()->text == "92.35");

  // and the full precision survives into further arithmetic
  CHECK(calc.evaluate("(100 usd to eur) * 100") == "9234.57eur");
}

TEST_CASE("a currency is not padded out to its minor units", "[currency]") {
  auto calc = fixedRateCalc();

  // the minor units cap the precision, they do not pad it: rendering money
  // properly is the caller's job, and half of the convention reads worse than none
  CHECK(calc.evaluate("1 usd") == "1usd");
  CHECK(calc.evaluate("0.9 usd") == "0.9usd");
  CHECK(calc.evaluate("1 usd / 2") == "0.5usd");
  CHECK(calc.evaluate("1 km") == "1km");
}

TEST_CASE("a currency amount lands on its minor units even without a conversion", "[currency]") {
  auto calc = fixedRateCalc();

  CHECK(calc.evaluate("0.22392323090 usd") == "0.22usd");
  CHECK(calc.evaluate("10.005 usd") == "10.01usd");
  CHECK(calc.evaluate("1 usd / 3") == "0.33usd");

  // jpy has no minor units at all
  abacus::EvalConfig jp{.locale = "ja_JP"};
  CHECK(calc.evaluate("1 jpy / 3", jp) == "0jpy");
  CHECK(calc.evaluate("1234.7 jpy", jp) == "1235jpy");

  // a plain unit is untouched by any of this
  CHECK(calc.evaluate("1 km / 3") == "0.333333km");
}

TEST_CASE("minor unit digits come from CLDR", "[currency]") {
  using abacus::currencyDigits;

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

TEST_CASE("Use currency provider") {
  abacus::Abacus calc{};
  auto currency = std::make_unique<VicinaeCurrencyProvider>();

  currency->updateRates();
  calc.setCurrencyProvider(std::move(currency));

  REQUIRE(calc.evaluate("100 usd to eur"));
}
