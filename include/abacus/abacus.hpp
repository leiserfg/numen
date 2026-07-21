#pragma once
#include "abacus/unit.hpp"
#include <cmath>
#include <expected>
#include <string>
#include <string_view>

namespace abacus {
struct ComputedValue {
  double n;
  // TODO: maybe we want the pointer in the db, idk...
  // optional is convenient, and no lifetime issues by copying...
  std::optional<UnitDef> unit;
  std::optional<std::string_view> unitRaw;

  ComputedValue operator+(const ComputedValue &rhs) const {
    return output(n + rhs.n, rhs);
  }

  ComputedValue operator-(const ComputedValue &rhs) const {
    return output(n - rhs.n, rhs);
  }

  ComputedValue operator*(const ComputedValue &rhs) const {
    return output(n * rhs.n, rhs);
  }

  ComputedValue operator/(const ComputedValue &rhs) const {
    return output(n / rhs.n, rhs);
  }

  ComputedValue operator%(const ComputedValue &rhs) const {
    return output(static_cast<int>(n) % static_cast<int>(rhs.n), rhs);
  }

  ComputedValue pow(const ComputedValue &rhs) const {
    return output(std::pow(n, rhs.n), rhs);
  }

private:
  ComputedValue output(double n, const ComputedValue &rhs) const {
    return ComputedValue{.n = n,
                         .unit = foldUnit(rhs),
                         .unitRaw =
                             rhs.unitRaw.or_else([&]() { return unitRaw; })};
  }

  std::optional<UnitDef> foldUnit(const ComputedValue &rhs) const {
    return rhs.unit.or_else([&]() { return unit; });
  }
};

class Abacus {
public:
  Abacus() = default;

  std::expected<std::string, std::string> evaluate(std::string_view expr);
  std::expected<ComputedValue, std::string> compute(std::string_view expr);
  void printAST(const std::string &expr) const;

private:
  UnitDatabase m_unitDb;
};

}; // namespace abacus
