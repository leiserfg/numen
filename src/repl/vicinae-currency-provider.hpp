#pragma once
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "numen/abstract-currency-provider.hpp"

class VicinaeCurrencyProvider : public numen::AbstractCurrencyProvider {
public:
  std::optional<numen::ExchangeRate> getRate(std::string_view code) const override;

  // non-blocking: fetches on a worker thread and swaps rates in when done
  void updateRates() override { fetchRates(); }

private:
  void fetchRates();
  bool loadRates(std::string_view json);

  mutable std::mutex m_mut;
  std::unordered_map<std::string, double> m_rates;
  std::optional<std::chrono::time_point<std::chrono::system_clock>> m_lastFetchedAt;
  std::jthread m_worker; // joined on destruction, so the fetch never outlives this
};
