#include "value.hpp"
#include <format>
#include <ios>
#include <stdexcept>

namespace abacus {

namespace {

std::string toBinary(Integer i) {
  if (i == 0) return "0";

  std::string out;
  while (i > 0) {
    out.insert(out.begin(), (i % 2 == 0) ? '0' : '1');
    i /= 2;
  }

  return out;
}

void stripTrailingZeroes(std::string &s) {
  auto last = s.find_last_not_of('0');
  s.resize(s[last] == '.' ? last : last + 1);
}

} // namespace

Integer Value::shiftCount(const Value &rhs) const {
  auto by = rhs.truncated();
  if (by < 0 || by > MAX_SHIFT) throw std::runtime_error("Shift count out of range");
  return by;
}

Value Value::operator<<(const Value &rhs) const {
  return Value{Integer{truncated() << static_cast<unsigned>(shiftCount(rhs))}};
}

Value Value::operator>>(const Value &rhs) const {
  return Value{Integer{truncated() >> static_cast<unsigned>(shiftCount(rhs))}};
}

Value Value::operator&(const Value &rhs) const { return Value{Integer{truncated() & rhs.truncated()}}; }

Value Value::operator|(const Value &rhs) const { return Value{Integer{truncated() | rhs.truncated()}}; }

// rounds p/q with integer arithmetic only, so an exact value never meets a float
std::string Value::renderExact(const Exact &r) {
  Integer num = numerator(r);
  Integer den = denominator(r);
  bool negative = num < 0;

  if (negative) num = -num;

  Integer scale = boost::multiprecision::pow(Integer{10}, MAX_DECIMALS);
  Integer scaled = (num * scale * 2 + den) / (den * 2);

  if (scaled == 0 && num != 0) { return renderInexact(static_cast<Inexact>(r)); }

  auto digits = scaled.str();
  if (digits.size() <= MAX_DECIMALS) digits.insert(0, MAX_DECIMALS + 1 - digits.size(), '0');
  digits.insert(digits.size() - MAX_DECIMALS, ".");
  stripTrailingZeroes(digits);

  return (negative && digits != "0" ? "-" : "") + digits;
}

std::string Value::renderInexact(const Inexact &n) {
  if (boost::multiprecision::isinf(n)) return n < 0 ? "-inf" : "inf";

  auto s = n.str(MAX_DECIMALS, std::ios_base::fixed);

  // a non zero value rounding to all zeroes would read as an exact 0
  if (n != 0 && s.find_first_of("123456789") == std::string::npos) {
    return n.str(MAX_DECIMALS, std::ios_base::fmtflags{});
  }

  stripTrailingZeroes(s);

  return s;
}

// cpp_bin_float_quad tops out around 1e4932, past which the exponent has to come
// from the decimal expansion itself
std::string Value::renderScientific(const std::string &plain) const {
  auto narrowed = toInexact();
  if (!boost::multiprecision::isinf(narrowed)) {
    return narrowed.str(MAX_DECIMALS, std::ios_base::scientific);
  }

  std::string_view view{plain};
  std::string sign;

  if (view.starts_with('-')) {
    sign = "-";
    view.remove_prefix(1);
  }

  auto dot = view.find('.');
  auto intDigits = dot == std::string_view::npos ? view.size() : dot;

  std::string digits;
  for (char c : view) {
    if (c != '.') digits += c;
  }

  auto frac = digits.substr(1, MAX_DECIMALS);
  frac.resize(MAX_DECIMALS, '0');

  return std::format("{}{}.{}e+{}", sign, digits.substr(0, 1), frac, intDigits - 1);
}

std::string Value::renderBase(NumberOutputFormat format) const {
  Integer i = truncated();
  bool negative = i < 0;

  if (negative) i = -i;

  auto [prefix, body] = [&]() -> std::pair<const char *, std::string> {
    switch (format) {
    case NumberOutputFormat::Hexadecimal:
      return {"0x", i.str(0, std::ios_base::hex)};
    case NumberOutputFormat::Octal:
      return {"0o", i.str(0, std::ios_base::oct)};
    default:
      return {"0b", toBinary(i)};
    }
  }();

  return std::format("{}{}{}", negative ? "-" : "", prefix, body);
}

std::string Value::renderDecimal() const {
  std::string out;

  if (auto e = asExact()) {
    out = denominator(*e) == 1 ? numerator(*e).str() : renderExact(*e);
  } else {
    out = renderInexact(*asInexact());
  }

  if (out.size() > MAX_RENDERED_DIGITS) return renderScientific(out);

  return out;
}

std::string Value::render(NumberOutputFormat format) const {
  return format == NumberOutputFormat::Decimal ? renderDecimal() : renderBase(format);
}

} // namespace abacus
