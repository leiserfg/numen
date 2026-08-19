#include "value.hpp"
#include <algorithm>
#include <format>
#include <locale>
#include <ranges>
#include <stdexcept>

namespace numen {

namespace {

constexpr int MAX_DECIMALS = 6;

// past this a long long cannot hold the value, so it is printed as a plain decimal
constexpr double LLONG_RANGE = 9223372036854775808.0;

constexpr double SCIENTIFIC_THRESHOLD = 1e40;

// numbers are always rendered with US-style digit grouping (1,234,567.89)
// this is because the parser only handles `,` and `_` as thousand separators
// so we don't want to render something that can't be evaluated back.
struct GroupedNumpunct : std::numpunct<char> {
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};

const std::locale &groupedLocale() {
  static const std::locale loc(std::locale::classic(), new GroupedNumpunct);
  return loc;
}

void stripTrailingZeroes(std::string &s) {
  auto last = s.find_last_not_of('0');
  s.resize(s[last] == '.' ? last : last + 1);
}

// the one base std::format has no specifier for on a long long
std::string toBinary(unsigned long long i) {
  if (i == 0) return "0";

  std::string out;
  for (; i > 0; i /= 2) {
    out += (i % 2 == 0) ? '0' : '1';
  }
  std::ranges::reverse(out);

  return out;
}

} // namespace

Value Value::scaled(double mantissa, int scale) {
  if (scale == 0) return Value{mantissa};
  if (scale > 0) return Value{mantissa * std::pow(10.0, scale)};

  return Value{mantissa / std::pow(10.0, -scale)};
}

Value Value::operator/(const Value &rhs) const {
  if (rhs.isZero()) throw std::runtime_error("Division by zero");

  return Value{m_n / rhs.m_n};
}

Value Value::mod(const Value &rhs) const {
  if (rhs.isZero()) throw std::runtime_error("Modulo by zero");

  return Value{std::fmod(m_n, rhs.m_n)};
}

long long Value::shiftCount(const Value &rhs) const {
  auto by = rhs.toInt();
  if (by < 0 || by > 63) throw std::runtime_error("Shift count out of range");

  return by;
}

Value Value::operator<<(const Value &rhs) const {
  auto shifted = static_cast<unsigned long long>(toInt()) << shiftCount(rhs);
  return Value{static_cast<double>(static_cast<long long>(shifted))};
}

Value Value::operator>>(const Value &rhs) const {
  return Value{static_cast<double>(toInt() >> shiftCount(rhs))};
}

std::string Value::renderBase(NumberOutputFormat format) const {
  auto i = toInt();
  bool negative = i < 0;
  auto magnitude = static_cast<unsigned long long>(negative ? -i : i);

  auto [prefix, body] = [&]() -> std::pair<const char *, std::string> {
    switch (format) {
    case NumberOutputFormat::Hexadecimal:
      return {"0x", std::format("{:x}", magnitude)};
    case NumberOutputFormat::Octal:
      return {"0o", std::format("{:o}", magnitude)};
    default:
      return {"0b", toBinary(magnitude)};
    }
  }();

  return std::format("{}{}{}", negative ? "-" : "", prefix, body);
}

std::string Value::renderDecimal() const {
  if (std::isinf(m_n)) return m_n < 0 ? "-inf" : "inf";

  if (std::abs(m_n) >= SCIENTIFIC_THRESHOLD) return std::format("{:.{}e}", m_n, MAX_DECIMALS);

  const auto &loc = groupedLocale();

  if (isInteger()) {
    return std::abs(m_n) < LLONG_RANGE ? std::format(loc, "{:L}", static_cast<long long>(m_n))
                                       : std::format(loc, "{:.0Lf}", m_n);
  }

  auto out = std::format(loc, "{:.{}Lf}", m_n, MAX_DECIMALS);

  // a non zero value rounding to all zeroes would read as an exact 0
  if (out.find_first_of("123456789") == std::string::npos) return std::format("{:g}", m_n);

  stripTrailingZeroes(out);
  return out;
}

std::string Value::render(NumberOutputFormat format, std::optional<int> fixedDecimals) const {
  if (format != NumberOutputFormat::Decimal) return renderBase(format);
  if (!fixedDecimals) return renderDecimal();

  auto out = std::format(groupedLocale(), "{:.{}Lf}", m_n, *fixedDecimals);
  if (*fixedDecimals > 0) stripTrailingZeroes(out);

  return out;
}

} // namespace numen
