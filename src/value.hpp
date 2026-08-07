#pragma once

#include "abacus/abacus.hpp"
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <compare>
#include <concepts>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace abacus {

using Integer = boost::multiprecision::cpp_int;
using Exact = boost::multiprecision::cpp_rational;
using Inexact = boost::multiprecision::cpp_bin_float_quad;

// Exact while the number is a fraction. Irrationals and external rates are not,
// and inexactness never converts back.
class Value {
public:
  Value() = default;
  Value(std::int64_t n) : m_v(Exact{n}) {}

  // a float would bind to the integer constructor and lose its fraction
  template <std::floating_point T> Value(T) = delete;
  Value(Integer n) : m_v(Exact{std::move(n)}) {}
  Value(Exact n) : m_v(std::move(n)) {}
  Value(Inexact n) : m_v(std::move(n)) {}

  static Value fromDouble(double d) { return Value{Inexact{d}}; }

  static Value scaled(const Integer &mantissa, int scale) {
    // -> Integer is required: boost's pow returns an expression template holding a
    // reference to the dead temporary
    auto ten = [](int e) -> Integer {
      return boost::multiprecision::pow(Integer{10}, static_cast<unsigned>(e));
    };
    if (scale >= 0) return Value{Exact{mantissa * ten(scale)}};
    return Value{Exact{mantissa, ten(-scale)}};
  }

  bool isExact() const { return m_v.index() == 0; }
  const Exact *asExact() const { return std::get_if<Exact>(&m_v); }
  const Inexact *asInexact() const { return std::get_if<Inexact>(&m_v); }

  bool isInteger() const {
    auto e = asExact();
    return e && denominator(*e) == 1;
  }

  std::optional<Integer> asInteger() const {
    if (auto e = asExact(); e && denominator(*e) == 1) return numerator(*e);
    return std::nullopt;
  }

  Inexact toInexact() const {
    if (auto e = asExact()) return static_cast<Inexact>(*e);
    return *asInexact();
  }

  double toDouble() const { return static_cast<double>(toInexact()); }

  bool isZero() const {
    if (auto e = asExact()) return *e == 0;
    return *asInexact() == 0;
  }

  Value operator-() const {
    if (auto e = asExact()) return Value{Exact{-*e}};
    return Value{Inexact{-*asInexact()}};
  }

  Value operator+(const Value &rhs) const { return combine(rhs, std::plus{}); }
  Value operator-(const Value &rhs) const { return combine(rhs, std::minus{}); }
  Value operator*(const Value &rhs) const { return combine(rhs, std::multiplies{}); }

  Value operator/(const Value &rhs) const {
    if (rhs.isZero()) throw std::runtime_error("Division by zero");
    return combine(rhs, std::divides{});
  }


  bool isNaN() const {
    auto i = asInexact();
    return i && isnan(*i);
  }

  Value mod(const Value &rhs) const {
    if (rhs.isZero()) throw std::runtime_error("Modulo by zero");

    if (isExact() && rhs.isExact()) {
      const auto &a = *asExact();
      const auto &b = *rhs.asExact();
      Exact q = a / b;
      Integer whole = numerator(q) / denominator(q); // toward zero, as fmod does
      return Value{Exact{a - b * Exact{whole}}};
    }

    return Value{Inexact{fmod(toInexact(), rhs.toInexact())}};
  }

  Value pow(const Value &rhs) const {
    static constexpr int MAX_EXACT_EXPONENT = 10000;

    if (auto e = rhs.asInteger(); e && isExact()) {
      bool negative = *e < 0;
      Integer magnitude = negative ? -*e : *e;

      if (magnitude <= MAX_EXACT_EXPONENT) {
        auto k = static_cast<unsigned>(magnitude);
        Exact raised{boost::multiprecision::pow(numerator(*asExact()), k),
                     boost::multiprecision::pow(denominator(*asExact()), k)};

        if (!negative) return Value{std::move(raised)};
        if (raised == 0) throw std::runtime_error("Division by zero");
        return Value{Exact{1 / raised}};
      }
    }

    return Value{Inexact{boost::multiprecision::pow(toInexact(), rhs.toInexact())}};
  }

  Value operator<<(const Value &rhs) const;
  Value operator>>(const Value &rhs) const;
  Value operator&(const Value &rhs) const;
  Value operator|(const Value &rhs) const;

  // fixedDecimals caps the fraction where the unit dictates its own precision,
  // as money does
  std::string render(NumberOutputFormat format = NumberOutputFormat::Decimal,
                     std::optional<int> fixedDecimals = std::nullopt) const;

  bool operator==(const Value &rhs) const {
    if (isExact() && rhs.isExact()) return *asExact() == *rhs.asExact();
    return closeEnough(toInexact(), rhs.toInexact());
  }

  // boost::multiprecision only provides the classic relational operators
  std::partial_ordering operator<=>(const Value &rhs) const {
    if (isExact() && rhs.isExact()) {
      const auto &a = *asExact();
      const auto &b = *rhs.asExact();
      if (a < b) return std::partial_ordering::less;
      if (b < a) return std::partial_ordering::greater;
      return std::partial_ordering::equivalent;
    }

    auto a = toInexact();
    auto b = rhs.toInexact();

    if (closeEnough(a, b)) return std::partial_ordering::equivalent;
    return a < b ? std::partial_ordering::less : std::partial_ordering::greater;
  }

private:
  static constexpr int MAX_DECIMALS = 6;
  static constexpr std::size_t MAX_RENDERED_DIGITS = 40;
  static constexpr int MAX_SHIFT = 1024;

  // digits below the radix point are dropped, as the bitwise operators always have
  Integer truncated() const {
    if (auto e = asExact()) return numerator(*e) / denominator(*e);
    return Integer{static_cast<std::int64_t>(static_cast<double>(*asInexact()))};
  }

  Integer shiftCount(const Value &rhs) const;
  std::string renderDecimal() const;
  std::string renderFixed(int decimals) const;
  std::string renderBase(NumberOutputFormat format) const;
  std::string renderScientific(const std::string &plain) const;

  static std::string renderExact(const Exact &r, int decimals, bool guardTiny);
  static std::string renderInexact(const Inexact &n);

  // sized for what enters the inexact world, not for Inexact's width: doubles come
  // in carrying ~1e-16 whatever we store them in
  static bool closeEnough(const Inexact &a, const Inexact &b) {
    static const Inexact relative{"1e-15"};
    static const Inexact absolute{"1e-300"};
    Inexact diff = abs(a - b);
    if (diff <= absolute) return true;

    Inexact magA = abs(a);
    Inexact magB = abs(b);

    return diff <= relative * (magA < magB ? magB : magA);
  }

  template <typename Op> Value combine(const Value &rhs, Op op) const {
    if (isExact() && rhs.isExact()) return Value{Exact{op(*asExact(), *rhs.asExact())}};
    return Value{Inexact{op(toInexact(), rhs.toInexact())}};
  }

  std::variant<Exact, Inexact> m_v{Exact{0}};
};

} // namespace abacus
