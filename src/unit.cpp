#include "abacus/unit.hpp"
#include "abacus/abstract-currency-provider.hpp"
#include "builtin-units.hpp"
#include "utils.hpp"
#include <array>
#include <cmath>
#include <format>
#include <map>
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

DimensionTraits traitsOf(std::string_view name) {
  if (name == dimensions::LENGTH) return {.signature = {.length = 1}};
  if (name == dimensions::MASS) return {.signature = {.mass = 1}};
  if (name == dimensions::DURATION) return {.signature = {.time = 1}};
  if (name == dimensions::DATA) return {.signature = {.data = 1}};
  if (name == dimensions::TEMPERATURE) return {.signature = {.temperature = 1}};
  if (name == dimensions::AREA) return {.signature = {.length = 2}};
  if (name == dimensions::VOLUME) return {.signature = {.length = 3}};
  if (name == dimensions::SPEED) return {.signature = {.length = 1, .time = -1}};

  if (name == dimensions::CURRENCY) {
    return {.signature = {.currency = 1},
            .composition = Composition::RateOnly,
            .dynamicFactor = true};
  }

  return {};
}

Dimension CompoundUnit::dimension() const {
  Dimension d;

  for (const auto &term : terms) {
    auto base = dimensionOf(term.def.dimension);
    d.length += base.length * term.exponent;
    d.mass += base.mass * term.exponent;
    d.time += base.time * term.exponent;
    d.currency += base.currency * term.exponent;
    d.data += base.data * term.exponent;
    d.temperature += base.temperature * term.exponent;
  }

  return d;
}

double CompoundUnit::factor() const {
  double f = 1;
  for (const auto &term : terms) {
    f *= std::pow(term.def.factor, term.exponent);
  }
  return f;
}

bool CompoundUnit::hasStableFactor() const {
  return std::ranges::none_of(
      terms, [](const UnitTerm &term) { return traitsOf(term.def.dimension).dynamicFactor; });
}

std::string CompoundUnit::render() const {
  auto name = [](const UnitTerm &term) {
    auto exponent = std::abs(term.exponent);
    if (exponent == 1) return term.display;
    if (exponent == 2) return term.display + "²";
    if (exponent == 3) return term.display + "³";
    return std::format("{}^{}", term.display, exponent);
  };

  std::vector<std::string> over, under;

  for (const auto &term : terms) {
    if (term.exponent > 0) { over.push_back(name(term)); }
    if (term.exponent < 0) { under.push_back(name(term)); }
  }

  auto join = [](const std::vector<std::string> &parts) {
    std::string out;
    for (const auto &part : parts) {
      if (!out.empty()) out += "·";
      out += part;
    }
    return out;
  };

  if (under.empty()) return join(over);
  // "usd/person·day" would read as (usd/person)·day
  if (under.size() > 1) return std::format("{}/({})", join(over), join(under));

  return std::format("{}/{}", join(over), under.front());
}

const UnitDef *CompoundUnit::sole() const {
  if (terms.size() != 1 || terms.front().exponent != 1) return nullptr;
  return &terms.front().def;
}

CompoundUnit soleUnit(UnitDef def, std::string display) {
  return CompoundUnit{{UnitTerm{.def = std::move(def), .display = std::move(display)}}};
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

std::optional<UnitDef> UnitDatabase::findUnit(const std::string &id) const {
  if (auto units = findUnitCandidates(id); !units.empty()) { return units.front(); }
  return std::nullopt;
}

std::expected<double, std::string> UnitDatabase::factorOf(const UnitDef &unit) const {
  if (!traitsOf(unit.dimension).dynamicFactor) return unit.factor;

  if (!m_currencyProvider) return std::unexpected("No currency provider is configured");

  // a rate is how many of this unit one base unit buys: the reciprocal of a factor
  auto rate = m_currencyProvider->getRate(unit.id);
  if (!rate || *rate == 0) {
    return std::unexpected(std::format("No conversion rate available for {}.", unit.id));
  }

  return 1 / *rate;
}

std::expected<double, std::string> UnitDatabase::conversionRatio(const CompoundUnit &from,
                                                                 const CompoundUnit &to) const {
  std::map<std::string, std::pair<UnitDef, int>> net;

  auto accumulate = [&](const CompoundUnit &unit, int sign) {
    for (const auto &term : unit.terms) {
      auto &[def, exponent] = net[term.def.id];
      def = term.def;
      exponent += term.exponent * sign;
    }
  };

  accumulate(from, 1);
  accumulate(to, -1);

  double ratio = 1;

  for (const auto &[id, entry] : net) {
    const auto &[def, exponent] = entry;
    // the same unit on both sides cancels, so it never needs a rate looked up
    if (exponent == 0) continue;

    auto base = factorOf(def);
    if (!base) return std::unexpected(base.error());

    ratio *= std::pow(*base, exponent);
  }

  return ratio;
}

std::expected<double, std::string> UnitDatabase::convert(double n, const UnitDef &from,
                                                         const UnitDef &to) const {
  if (from.id == to.id) { return n; }

  if (from.dimension != to.dimension) {
    return std::unexpected(
        std::format("No idea how to convert {} to {}, as they are not of the same type.", from.id, to.id));
  }

  auto fromFactor = factorOf(from);
  if (!fromFactor) return std::unexpected(fromFactor.error());

  auto toFactor = factorOf(to);
  if (!toFactor) return std::unexpected(toFactor.error());

  auto base = n * *fromFactor + from.offset;
  return (base - to.offset) / *toFactor;
}
