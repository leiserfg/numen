#pragma once
#include "numen/abstract-currency-provider.hpp"

class DummyCurrencyProvider : public numen::AbstractCurrencyProvider {
public:
  std::optional<numen::ExchangeRate> getRate(std::string_view e) const override {
    std::ignore = e;
    return std::nullopt;
  }
  void updateRates() override {}
};
