#pragma once

#include "numen/numen.hpp"
#include <cmath>
#include <compare>
#include <optional>
#include <string>

namespace numen {

// Every number is a double, which is the trade numbat makes too: a utility
// calculator wants boring, predictable arithmetic, and the places a double falls
// short are well past anything anyone types. Overflow shows as inf.
class Value {
public:
  Value() = default;
  Value(double n) : m_n(n) {}

  // mantissa * 10^scale, scaled in one step so a literal is correctly rounded
  static Value scaled(double mantissa, int scale);

  double toDouble() const { return m_n; }

  template <typename T> T to() const { return static_cast<T>(m_n); }

  bool isZero() const { return m_n == 0; }
  bool isNaN() const { return std::isnan(m_n); }
  bool isInteger() const { return std::isfinite(m_n) && m_n == std::trunc(m_n); }

  Value operator-() const { return Value{-m_n}; }
  Value operator+(const Value &rhs) const { return Value{m_n + rhs.m_n}; }
  Value operator-(const Value &rhs) const { return Value{m_n - rhs.m_n}; }
  Value operator*(const Value &rhs) const { return Value{m_n * rhs.m_n}; }
  Value operator/(const Value &rhs) const;

  Value mod(const Value &rhs) const;
  Value pow(const Value &rhs) const { return Value{std::pow(m_n, rhs.m_n)}; }

  Value operator<<(const Value &rhs) const;
  Value operator>>(const Value &rhs) const;
  Value operator&(const Value &rhs) const { return Value{static_cast<double>(toInt() & rhs.toInt())}; }
  Value operator|(const Value &rhs) const { return Value{static_cast<double>(toInt() | rhs.toInt())}; }

  bool operator==(const Value &rhs) const { return m_n == rhs.m_n; }
  std::partial_ordering operator<=>(const Value &rhs) const { return m_n <=> rhs.m_n; }

  // fixedDecimals caps the fraction where the unit dictates its own precision,
  // as money does
  std::string render(NumberOutputFormat format = NumberOutputFormat::Decimal,
                     std::optional<int> fixedDecimals = std::nullopt) const;

private:
  // digits below the radix point are dropped, as the bitwise operators always have
  long long toInt() const { return static_cast<long long>(m_n); }

  long long shiftCount(const Value &rhs) const;
  std::string renderDecimal() const;
  std::string renderBase(NumberOutputFormat format) const;

  double m_n = 0;
};

} // namespace numen
