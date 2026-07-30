#include "abacus/unit.hpp"
#include "utils.hpp"
#include <bits/chrono.h>
#include <format>
#include <algorithm>
#include <chrono>
#include <ranges>

UnitDatabase::UnitDatabase() noexcept {
  registerUnit(UnitDef{
      .id = "inch", .aliases = {"in"}, .factor = 0.0254, .family = "imperial", .type = UnitType::Distance});
  registerUnit(
      UnitDef{.id = "meter", .aliases = {"m"}, .factor = 1, .family = "metric", .type = UnitType::Distance});
  registerUnit(UnitDef{
      .id = "kilometer", .aliases = {"km"}, .factor = 1e3, .family = "metric", .type = UnitType::Distance});

  {
    using namespace std::chrono;

    const auto toSeconds = [](auto &&t) { return duration_cast<seconds>(t).count(); };

    registerUnit(UnitDef{.id = "second",
                         .aliases = {"s", "sec", "secs", "seconds"},
                         .factor = 1,
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "minute",
                         .aliases = {"m", "min", "mins", "minutes"},
                         .factor = toSeconds(minutes(1)),
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "hour",
                         .aliases = {"h", "hr", "hrs", "hours"},
                         .factor = toSeconds(hours(1)),
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "day",
                         .aliases = {"d", "days", "dys"},
                         .factor = toSeconds(days{1}),
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "month",
                         .aliases = {"mo", "months"},
                         // average number of seconds in a month
                         .factor = toSeconds(months{1}),
                         .family = "duration",
                         .type = UnitType::Duration});

    registerUnit(UnitDef{.id = "year",
                         .aliases = {"years", "yr"},
                         // average number of seconds in a year
                         .factor = toSeconds(years{1}),
                         .family = "duration",
                         .type = UnitType::Duration});
  }

  registerUnit(UnitDef{.id = "kelvin", .factor = 1, .family = "degree", .type = UnitType::Temperature});
  registerUnit(UnitDef{
      .id = "celsius",
      .aliases = {"cel", "c"},
      .factor = 1,
      .family = "degree",
      .type = UnitType::Temperature,
      .offset = 273.15,
  });
  registerUnit(UnitDef{
      .id = "fahrenheight",
      .aliases = {"fahren", "f"},
      .factor = 5.0 / 9,
      .family = "degree",
      .type = UnitType::Temperature,
      .offset = 459.67 * 5 / 9,
  });

  registerUnit(UnitDef{.id = "usd", .family = "currency"});
  registerUnit(UnitDef{.id = "gbp", .family = "currency"});
  registerUnit(UnitDef{.id = "eur", .family = "currency"});
}

std::vector<UnitDef> UnitDatabase::findUnitCandidates(std::string_view q) const {
  auto units = m_units | std::views::filter([&](const UnitDef &unit) {
                 return equalsIgnoreCase(unit.id, q) || std::ranges::any_of(unit.aliases, [&](auto &&str) {
                          return equalsIgnoreCase(str, q);
                        });
               }) |
               std::ranges::to<std::vector>();
  assert(!units.empty());
  return units;
}

void UnitDatabase::registerUnit(UnitDef unit) { m_units.emplace_back(unit); }

const UnitDef *UnitDatabase::findUnit(const std::string &id) const {
  auto it = std::ranges::find_if(
      m_units, [&](const UnitDef &u) { return u.id == id || std::ranges::contains(u.aliases, id); });

  return it != m_units.end() ? &*it : nullptr;
}

std::expected<double, std::string> UnitDatabase::convert(double n, const UnitDef &from,
                                                         const UnitDef &to) const {
  if (from.type == to.type) {
    auto base = n * from.factor + from.offset;
    return (base - to.offset) / to.factor;
  }

  return std::unexpected(std::format("No idea how to convert {} to {}, as they are not of the same type.",
                                     from.family, to.family));
}
