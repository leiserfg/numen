#include "abacus/unit.hpp"
#include "abacus/abstract-currency-provider.hpp"
#include "builtin-units.hpp"
#include "utils.hpp"
#include <array>
#include <format>
#include <algorithm>
#include <iostream>
#include <ranges>

namespace {

struct Prefix {
  std::string_view symbol;
  double multiplier;
  // 'm'/'M' (milli/mega) and 'p'/'P' (pico/peta) only differ by case
  bool caseSensitive = false;
  // binary prefixes only make sense for data units
  bool dataOnly = false;
};

// declaration order is irrelevant: expansion collects candidates from
// every matching prefix, first-match-wins semantics must not be assumed
constexpr auto PREFIXES = std::to_array<Prefix>({
    {.symbol = "micro", .multiplier = 1e-6},
    {.symbol = "milli", .multiplier = 1e-3},
    {.symbol = "centi", .multiplier = 1e-2},
    {.symbol = "nano", .multiplier = 1e-9},
    {.symbol = "pico", .multiplier = 1e-12},
    {.symbol = "deci", .multiplier = 1e-1},
    {.symbol = "kilo", .multiplier = 1e3},
    {.symbol = "mega", .multiplier = 1e6},
    {.symbol = "giga", .multiplier = 1e9},
    {.symbol = "tera", .multiplier = 1e12},
    {.symbol = "peta", .multiplier = 1e15},
    {.symbol = "kibi", .multiplier = 1024.0, .dataOnly = true},
    {.symbol = "mebi", .multiplier = 1048576.0, .dataOnly = true},
    {.symbol = "gibi", .multiplier = 1073741824.0, .dataOnly = true},
    {.symbol = "tebi", .multiplier = 1099511627776.0, .dataOnly = true},
    {.symbol = "ki", .multiplier = 1024.0, .dataOnly = true},
    {.symbol = "mi", .multiplier = 1048576.0, .dataOnly = true},
    {.symbol = "gi", .multiplier = 1073741824.0, .dataOnly = true},
    {.symbol = "ti", .multiplier = 1099511627776.0, .dataOnly = true},
    {.symbol = "n", .multiplier = 1e-9},
    {.symbol = "u", .multiplier = 1e-6},
    {.symbol = "m", .multiplier = 1e-3, .caseSensitive = true},
    {.symbol = "M", .multiplier = 1e6, .caseSensitive = true},
    {.symbol = "c", .multiplier = 1e-2},
    {.symbol = "d", .multiplier = 1e-1},
    {.symbol = "k", .multiplier = 1e3},
    {.symbol = "G", .multiplier = 1e9},
    {.symbol = "T", .multiplier = 1e12},
    {.symbol = "p", .multiplier = 1e-12, .caseSensitive = true},
    {.symbol = "P", .multiplier = 1e15, .caseSensitive = true},
});

// "meters" -> "meter", "inches" -> "inch". Only ever consulted for tokens
// with no direct reading: a token that is a unit name by itself ("ms",
// "min") must never be reinterpreted as a plural.
std::vector<std::string_view> pluralForms(std::string_view q) {
  std::vector<std::string_view> forms;
  if (q.size() > 1 && (q.back() == 's' || q.back() == 'S')) {
    forms.push_back(q.substr(0, q.size() - 1));
    if (q.size() > 2 && equalsIgnoreCase(q.substr(q.size() - 2), std::string_view{"es"})) {
      forms.push_back(q.substr(0, q.size() - 2));
    }
  }
  return forms;
}

} // namespace

UnitDatabase::UnitDatabase() noexcept {
  for (const auto &unit : units::builtins()) {
    registerUnit(unit);
  }
}

std::vector<UnitDef> UnitDatabase::matchExact(std::string_view q) const {
  return m_units | std::views::filter([&](const UnitDef &unit) {
           return equalsIgnoreCase(unit.id, q) ||
                  std::ranges::any_of(unit.aliases, [&](auto &&str) { return equalsIgnoreCase(str, q); });
         }) |
         std::ranges::to<std::vector>();
}

std::vector<UnitDef> UnitDatabase::expandPrefixed(std::string_view q) const {
  std::vector<UnitDef> out;

  for (const auto &prefix : PREFIXES) {
    if (q.size() <= prefix.symbol.size()) { continue; }

    auto head = q.substr(0, prefix.symbol.size());
    bool matches = prefix.caseSensitive ? head == prefix.symbol : equalsIgnoreCase(head, prefix.symbol);
    if (!matches) { continue; }

    // exact-only: a plural remainder ("mins" as milli + "ins" -> inch) must
    // lose to the plural pass on the whole token
    auto rest = q.substr(prefix.symbol.size());
    auto bases = matchExact(rest);

    for (const auto &base : bases) {
      if (!base.prefixable || base.offset != 0) { continue; }
      if (prefix.dataOnly && base.dimension != dimensions::DATA) { continue; }

      out.push_back(UnitDef{
          .id = std::string{q},
          .factor = prefix.multiplier * base.factor,
          .dimension = base.dimension,
          .family = base.family,
      });
    }
  }

  return out;
}

std::vector<UnitDef> UnitDatabase::findUnitCandidates(std::string_view q) const {
  if (auto units = matchExact(q); !units.empty()) { return units; }
  if (auto units = expandPrefixed(q); !units.empty()) { return units; }

  for (auto form : pluralForms(q)) {
    if (auto units = matchExact(form); !units.empty()) { return units; }
    if (auto units = expandPrefixed(form); !units.empty()) { return units; }
  }

  return {};
}

void UnitDatabase::registerUnit(UnitDef unit) { m_units.emplace_back(std::move(unit)); }

const UnitDef *UnitDatabase::findUnit(const std::string &id) const {
  auto it = std::ranges::find_if(m_units, [&](const UnitDef &u) {
    return equalsIgnoreCase(u.id, id) ||
           std::ranges::any_of(u.aliases, [&](auto &&str) { return equalsIgnoreCase(id, str); });
  });

  return it != m_units.end() ? &*it : nullptr;
}

std::expected<double, std::string> UnitDatabase::convert(double n, const UnitDef &from,
                                                         const UnitDef &to) const {
  if (from.id == to.id) { return n; }

  if (from.dimension != to.dimension) {
    return std::unexpected(
        std::format("No idea how to convert {} to {}, as they are not of the same type.", from.id, to.id));
  }

  double fromFactor = from.factor;
  double toFactor = to.factor;

  if (from.dimension == dimensions::CURRENCY) {
    auto lhsRate = m_currencyProvider->getRate(from.id);
    auto rhsRate = m_currencyProvider->getRate(to.id);

    if (!lhsRate || !rhsRate) {
      return std::unexpected(std::format("No conversion rate available between {} and {}.", from.id, to.id));
    }

    return n / lhsRate.value() * rhsRate.value();
  }

  auto base = n * fromFactor + from.offset;
  return (base - to.offset) / toFactor;
}
