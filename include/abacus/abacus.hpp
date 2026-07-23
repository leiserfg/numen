#pragma once
#include "abacus/unit.hpp"
#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace abacus {

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

enum class DateTimeOutputFormat { DateTime, Time, Date };

struct DateTime {
  TimePoint time;
  DateTimeOutputFormat format = DateTimeOutputFormat::DateTime;
};

enum class NumberOutputFormat { Decimal, Hexadecimal, Binary, Octal };

struct Number {
  double n;
  NumberOutputFormat format;
};

using ValueType = std::variant<Number, DateTime, bool>;

struct ComputedValue {

  ValueType value;

  // TODO: maybe we want the pointer in the db, idk...
  // optional is convenient, and no lifetime issues by copying...
  std::optional<UnitDef> unit;
  std::optional<std::string_view> unitRaw;

  bool isNumber() const { return std::holds_alternative<Number>(value); }

  bool isDateTime() const { return std::holds_alternative<DateTime>(value); }
  bool isBool() const { return std::holds_alternative<bool>(value); }

  const Number *asNumber() const { return std::get_if<Number>(&value); }
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
