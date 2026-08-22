#pragma once
#include "numen/abstract-currency-provider.hpp"

class DummyCurrencyProvider : public AbstractCurrencyProvider {
  std::optional<double> getRate(const std::string &) const override { return std::nullopt; }
  void updateRates() override {}
};
