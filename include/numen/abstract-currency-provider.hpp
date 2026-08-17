#pragma once
#include <optional>
#include <string>
#include <vector>

struct CurrencyDescriptor {
  std::string code;
  std::vector<std::string> aliases;
};

class AbstractCurrencyProvider {
public:
  virtual ~AbstractCurrencyProvider() = default;

  /**
   * Current exchange rate (against USD) for the currency code (ISO 4217)
   * or crypto ticker.
   */
  virtual std::optional<double> getRate(const std::string &code) const = 0;

  virtual void updateRates() = 0;

  virtual std::vector<CurrencyDescriptor> additionalCurrencies() { return {}; }
};
