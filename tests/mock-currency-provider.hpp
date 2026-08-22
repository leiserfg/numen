#pragma once
#include "numen/numen.hpp"
#include "numen/abstract-currency-provider.hpp"
#include <map>
#include <memory>
#include <optional>

namespace test {

class MockCurrencyProvider : public numen::AbstractCurrencyProvider {
public:
  MockCurrencyProvider() = default;
  explicit MockCurrencyProvider(std::map<std::string_view, double> rates) : m_rates(std::move(rates)) {}

  std::optional<numen::ExchangeRate> getRate(const std::string_view code) const override {
    auto it = m_rates.find(code);
    if (it == m_rates.end()) return std::nullopt;
    return numen::ExchangeRate{.rate = it->second};
  }

  void updateRates() override { ++m_updates; }

  int updateCount() const { return m_updates; }

private:
  std::map<std::string_view, double> m_rates{
      {"usd", 1.0},
      {"eur", 0.9234567},
      {"gbp", 0.7891234},
      {"chf", 0.8712345},
      {"cad", 1.3698765},
      {"cny", 7.2345678},
      {"jpy", 157.891234},
      {"krw", 1234.5678},
      // crypto: btc and eth are builtins, xmr and shib only exist through the
      // provider. "m" checks a ticker never shadows a builtin unit
      {"btc", 0.0000117},
      {"xmr", 0.005},
      {"shib", 100000},
      {"m", 1},
      {"pi", 10}, // pi is a constant and will always overwrite currency in non conversion context.
      {"$ticker", 2}};

  int m_updates = 0;
};

inline numen::Numen mockCalc() {
  numen::Numen calc;
  calc.setCurrencyProvider(std::make_unique<MockCurrencyProvider>());
  return calc;
}

} // namespace test
