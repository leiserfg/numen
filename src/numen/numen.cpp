#include "numen/numen.hpp"
#include "ast-printer.hpp"
#include "computed.hpp"
#include "dummy-currency-provider.hpp"
#include "interpreter.hpp"
#include "parser.hpp"
#include "region-currency.hpp"
#include <exception>
#include <expected>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace numen {
using namespace numen::detail;

std::string Number::toString() const {
  auto unitName =
      unit.transform([](const Unit &u) { return u.resolved ? u.resolved->render() : u.raw; }).value_or("");
  return text + unitName;
}

std::string Boolean::toString() const { return value ? "true" : "false"; }

std::string ComputedValue::toString() const {
  return std::visit([](const auto &v) { return v.toString(); }, value);
}

// money is only ever shown on its minor units, e.g. none for jpy and three for bhd
static std::optional<int> unitDecimals(const detail::Num &v) {
  if (v.unit && v.unit->def() && v.unit->def()->dimension == dimensions::CURRENCY) {
    return currencyDigits(v.unit->def()->id);
  }
  return std::nullopt;
}

static ComputedValue toPublic(const detail::Computed &c) {
  if (auto n = c.asNumber()) {
    return ComputedValue{.value = Number{.n = n->n.toDouble(),
                                         .text = n->n.render(n->format, unitDecimals(*n)),
                                         .format = n->format,
                                         .unit = n->unit,
                                         .explicitlyConverted = n->explicitlyConverted,
                                         .isPercentage = n->isPercentage}};
  }
  if (auto d = c.asDateTime()) return ComputedValue{.value = *d};
  if (auto d = c.asDuration()) return ComputedValue{.value = *d};
  return ComputedValue{.value = std::get<Boolean>(c.value)};
}

std::expected<ComputedValue, std::string> Numen::compute(std::string_view expr, const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    return toPublic(interpret(*ast.root, m_unitDb, opts));
  } catch (const std::exception &e) { return std::unexpected(e.what()); }
}

std::expected<std::string, std::string> Numen::evaluate(const std::string_view expr, const EvalConfig &opts) {
  return compute(expr, opts).transform([](const ComputedValue &v) { return v.toString(); });
}

void Numen::printAST(const std::string &expr) const {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();

  detail::printAST(std::cout, *ast.root);
}

Numen::Numen() { setCurrencyProvider(std::make_unique<DummyCurrencyProvider>()); }

} // namespace numen
