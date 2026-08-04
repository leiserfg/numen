#pragma once
#include "abacus/abstract-currency-provider.hpp"
#include <cassert>
#include <expected>
#include <string>
#include <vector>

namespace dimensions {
inline constexpr const char *LENGTH = "length";
inline constexpr const char *MASS = "mass";
inline constexpr const char *DURATION = "duration";
inline constexpr const char *TEMPERATURE = "temperature";
inline constexpr const char *DATA = "data";
inline constexpr const char *VOLUME = "volume";
inline constexpr const char *AREA = "area";
inline constexpr const char *SPEED = "speed";
inline constexpr const char *CURRENCY = "currency";
}; // namespace dimensions

namespace families {
inline constexpr const char *METRIC = "metric";
inline constexpr const char *IMPERIAL = "imperial";
inline constexpr const char *DURATION = "duration";
inline constexpr const char *DEGREE = "degree";
inline constexpr const char *DATA = "data";
inline constexpr const char *CURRENCY = "currency";
}; // namespace families

struct UnitDef {
  std::string id;
  std::vector<std::string> aliases;
  // meaningless for currency: exchange rates are resolved at runtime, keyed
  // by the currency dimension
  double factor = 1;
  std::string dimension;
  std::string family;
  double offset = 0;
  bool prefixable = false;
};

class UnitDatabase {
public:
  UnitDatabase() noexcept;

  void registerUnit(UnitDef unit);

  // units can share a same identifier, e.g 'm' can stand for 'meter' or
  // 'minute'.
  std::vector<UnitDef> findUnitCandidates(std::string_view q) const;

  const UnitDef *findUnit(const std::string &id) const;

  std::expected<double, std::string> convert(double n, const UnitDef &from, const UnitDef &to) const;

  void setCurrencyProvider(const AbstractCurrencyProvider &provider) { m_currencyProvider = &provider; }

private:
  std::vector<UnitDef> matchExact(std::string_view q) const;
  std::vector<UnitDef> expandPrefixed(std::string_view q) const;

  std::vector<UnitDef> m_units;
  const AbstractCurrencyProvider *m_currencyProvider;
};
