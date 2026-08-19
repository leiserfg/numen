#pragma once
#include <optional>
#include <string>

class AbstractCurrencyProvider {
public:
  virtual ~AbstractCurrencyProvider() = default;

  /**
   * Current exchange rate (against USD) for a lowercase ISO 4217 code or
   * crypto ticker, e.g. "eur", "btc".
   *
   * This doubles as the existence check: a code with a rate is a currency
   * the calculator accepts, even with no builtin unit behind it. It is
   * consulted while parsing, so it must be cheap and must never block.
   */
  virtual std::optional<double> getRate(const std::string &code) const = 0;

  virtual void updateRates() = 0;
};
