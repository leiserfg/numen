#pragma once

#include "numen/numen.hpp"
#include "value.hpp"
#include <compare>
#include <optional>
#include <type_traits>
#include <variant>

namespace numen::detail {

// The interpreter's number. numen::Number is derived from it at the compute()
// boundary, where it picks up its rendered text.
struct Num {
  Value n;
  NumberOutputFormat format = NumberOutputFormat::Decimal;
  std::optional<Number::Unit> unit;
  bool isPercentage = false;

  bool operator==(const Num &rhs) const { return n == rhs.n; }
  std::partial_ordering operator<=>(const Num &rhs) const { return n <=> rhs.n; }
};

using Val = std::variant<Num, DateTime, Boolean, Duration, std::string>;

struct Computed {
  Val value;
  std::optional<Conversion> conversion;
  // the user pinned the unit/zone: implicit output passes must not override it
  bool explicitlyConverted = false;

  bool isNumber() const { return std::holds_alternative<Num>(value); }
  bool isDateTime() const { return std::holds_alternative<DateTime>(value); }
  bool isStr() const { return std::holds_alternative<std::string>(value); }

  const Num *asNumber() const { return std::get_if<Num>(&value); }
  Num *asNumber() { return std::get_if<Num>(&value); }
  const DateTime *asDateTime() const { return std::get_if<DateTime>(&value); }

  const Duration *asDuration() { return std::get_if<Duration>(&value); }
  const Duration *asDuration() const { return std::get_if<Duration>(&value); }
  const std::string *asStr() const { return std::get_if<std::string>(&value); }

  std::string_view valueTypeName() const {
    return std::visit(
        [](const auto &v) -> std::string_view {
          using T = std::remove_cvref_t<decltype(v)>;
          if constexpr (std::is_same_v<T, Duration>) {
            return "Duration";
          } else if constexpr (std::is_same_v<T, Num>) {
            return "Number";
          } else if constexpr (std::is_same_v<T, DateTime>) {
            return "DateTime";
          } else if constexpr (std::is_same_v<T, std::string>) {
            return "Text";
          } else {
            static_assert(std::is_same_v<T, Boolean>);
            return "Boolean";
          }
        },
        value);
  }
};

} // namespace numen::detail
