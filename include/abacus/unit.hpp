#pragma once
#include <cassert>
#include <expected>
#include <string>
#include <vector>

struct UnitDerivative {};

enum class UnitType { Date, Currency, Distance, Duration, Temperature };

struct UnitDef {
  std::string id;
  std::vector<std::string> aliases;
  double factor;
  std::string family;
  UnitType type;
  double offset = 0;
};

struct UnitBaseRelation {
  std::string lhs;
  std::string rhs;
  double factor;
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

private:
  std::vector<UnitDef> m_units;
};
