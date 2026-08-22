#pragma once
#include <chrono>
#include <optional>
#include <string_view>

namespace numen {

struct ExchangeRate {
  double rate;

  /**
   * Ideally, this instant should refer to the last time the rate was updated by the upstream
   * provider, and should not be related to when the data was fetched using the provider.
   * Most providers don't provide live data and will cache rates.
   */
  std::optional<std::chrono::system_clock> lastUpdatedAt;
};

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
  virtual std::optional<ExchangeRate> getRate(std::string_view code) const = 0;

  /**
   * Request a rates refresh. May block or may return before the new rates
   * are in. If asynchronous, `getRate` needs to remain safely callable when rates are refreshing.
   */
  virtual void updateRates() = 0;
};
} // namespace numen
