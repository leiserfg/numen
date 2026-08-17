#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "nlohmann/json.hpp"
#include "httplib/httplib.h"
#include "vicinae-currency-provider.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
constexpr auto CACHE_TTL = std::chrono::minutes{30};

void persistOnDisk(const std::filesystem::path &path, std::string_view body) {
  fs::create_directories(path.parent_path());
  std::ofstream{path} << body;
}

std::optional<std::string_view> genv(const char *id) {
  if (auto v = getenv(id)) { return v; }
  return std::nullopt;
}

fs::path persistPath() {
  auto cacheHome = genv("XDG_CACHE_HOME")
                       .transform([](auto &&s) { return fs::path{s}; })
                       .value_or(fs::path{*genv("HOME")} / ".cache");
  return cacheHome / "libnumen" / "vicinae-rates.json";
}

std::string normalizeCurrencyId(std::string_view id) {
  std::string s{id};
  std::ranges::transform(id, s.begin(), [](char c) { return std::tolower(c); });
  return s;
}

} // namespace

std::optional<double> VicinaeCurrencyProvider::getRate(const std::string &code) const {
  {
    std::lock_guard lock{m_mut};
    if (auto it = m_rates.find(code); it != m_rates.end()) { return it->second; }
  }
  return std::nullopt;
}

bool VicinaeCurrencyProvider::loadRates(std::string_view json) {
  try {
    auto obj = json::parse(json);

    {
      std::lock_guard lock{m_mut};
      for (const auto &[k, v] : obj["crypto"]["prices"].get<json::object_t>()) {
        m_rates[normalizeCurrencyId(k)] = 1 / v.get<double>();
      }
      // fiat always take priority over crypto tickers for nowk
      for (const auto &[k, v] : obj["fiat"]["rates"].get<json::object_t>()) {
        m_rates[normalizeCurrencyId(k)] = v.get<double>();
      }

      return true;
    }
  } catch (const std::exception &e) {
    std::cerr << "VicinaeCurrencyProvider: failed to fetch rates: " << e.what() << "\n" << json << "\n";
    return false;
  }
}

void VicinaeCurrencyProvider::fetchRates() {
  std::error_code ec;
  auto path = persistPath();

  if (!m_lastFetchedAt && fs::is_regular_file(path, ec)) {
    if (std::chrono::file_clock::now() - fs::last_write_time(path) < CACHE_TTL) {
      std::ifstream ifs{path};
      std::stringstream buffer;
      buffer << ifs.rdbuf();
      loadRates(buffer.str());
      return;
    }
  }

  if (m_lastFetchedAt && std::chrono::system_clock::now() - *m_lastFetchedAt < CACHE_TTL) return;

  std::thread{[this]() {
    httplib::Client client{"https://api.vicinae.com"};
    auto res = client.Get("/v1/currencies");

    if (!res) {
      std::cerr << "VicinaeCurrencyProvider: failed to fetch rates: " << res.error() << "\n";
      return;
    }

    if (loadRates(res->body)) {
      m_lastFetchedAt = std::chrono::system_clock::now();
      persistOnDisk(persistPath(), res->body);
    }
  }}.join();
}
