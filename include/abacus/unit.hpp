#pragma once
#include <cstdint>
#include "abacus/abstract-currency-provider.hpp"
#include <cassert>
#include <expected>
#include <string>
#include <vector>

struct Dimension {
  std::int8_t length = 0;
  std::int8_t mass = 0;
  std::int8_t time = 0;
  std::int8_t currency = 0;
  std::int8_t data = 0;
  std::int8_t temperature = 0;

  auto operator<=>(const Dimension &) const = default;
};

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

// not about affine units, which are a property of the unit rather than its
// dimension: kelvin composes freely where celsius cannot compose at all
enum class Composition : std::uint8_t { Free, RateOnly };

struct DimensionTraits {
  // every unit factor must compose from these, which is why volume is based on
  // the cubic meter and not the litre
  Dimension signature;

  Composition composition = Composition::Free;

  // the table's factor is a placeholder, the real one is resolved per
  // evaluation. nothing may rank such a unit by size
  bool dynamicFactor = false;
};

DimensionTraits traitsOf(std::string_view name);

inline Dimension dimensionOf(std::string_view name) { return traitsOf(name).signature; }
inline Composition compositionOf(std::string_view name) { return traitsOf(name).composition; }

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

struct UnitTerm {
  UnitDef def;
  // the token as the user typed it, so "5KM/h" does not come back as "km/h"
  std::string display;
  std::int8_t exponent = 1;
};

struct CompoundUnit {
  std::vector<UnitTerm> terms;

  Dimension dimension() const;

  // meaningless unless hasStableFactor()
  double factor() const;
  bool hasStableFactor() const;

  std::string render() const;

  // null once composition made this something no table entry names
  const UnitDef *sole() const;
};

CompoundUnit soleUnit(UnitDef def, std::string display);

class UnitDatabase {
public:
  UnitDatabase() noexcept;

  void registerUnit(UnitDef unit);

  // units can share a same identifier, e.g 'm' can stand for 'meter' or
  // 'minute'.
  std::vector<UnitDef> findUnitCandidates(std::string_view q) const;

  std::optional<UnitDef> findUnit(const std::string &id) const;

  std::expected<double, std::string> factorOf(const UnitDef &unit) const;

  std::expected<double, std::string> convert(double n, const UnitDef &from, const UnitDef &to) const;

  std::expected<double, std::string> conversionRatio(const CompoundUnit &from, const CompoundUnit &to) const;

  void setCurrencyProvider(const AbstractCurrencyProvider &provider) { m_currencyProvider = &provider; }

private:
  std::vector<UnitDef> matchExact(std::string_view q) const;
  std::vector<UnitDef> expandPrefixed(std::string_view q) const;

  std::vector<UnitDef> m_units;
  const AbstractCurrencyProvider *m_currencyProvider = nullptr;
};
