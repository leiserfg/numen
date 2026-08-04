#pragma once
#include <chrono>
#include <mutex>
#include <unordered_map>
#include "abacus/abstract-currency-provider.hpp"

class VicinaeCurrencyProvider : public AbstractCurrencyProvider {
public:
  std::optional<double> getRate(const std::string &code) const override;

  void updateRates() override { fetchRates(); }

private:
  void fetchRates();
  bool loadRates(std::string_view json);

  mutable std::mutex m_mut;
  std::unordered_map<std::string, double> m_rates;
  std::optional<std::chrono::time_point<std::chrono::system_clock>> m_lastFetchedAt;
};
