#pragma once
#include "abacus/unit.hpp"
#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace abacus {

using DateTime = std::chrono::time_point<std::chrono::system_clock>;
using ValueType = std::variant<double, bool, DateTime>;

enum class OutputFormat { Decimal, Hexadecimal, Binary, Octal };

struct ComputedValue {
  double value;
  OutputFormat format = OutputFormat::Decimal;

  // TODO: maybe we want the pointer in the db, idk...
  // optional is convenient, and no lifetime issues by copying...
  std::optional<UnitDef> unit;
  std::optional<std::string_view> unitRaw;
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
