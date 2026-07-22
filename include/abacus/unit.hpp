#pragma once
#include <algorithm>
#include <cassert>
#include <expected>
#include <format>
#include <ranges>
#include <string>
#include <vector>

struct UnitDerivative {};
enum class UnitType { Date, Currency, Distance, Duration, Temperature };

struct UnitDef {
  std::string id;
  std::vector<std::string> aliases;
  double factor;
  std::string family;
  UnitType type;
  double offset = 0;
};

struct UnitBaseRelation {
  std::string lhs;
  std::string rhs;
  double factor;
};

class UnitDatabase {
public:
  UnitDatabase() noexcept {
    registerUnit(UnitDef{.id = "inch",
                         .aliases = {"in"},
                         .factor = 0.0254,
                         .family = "imperial",
                         .type = UnitType::Distance});
    registerUnit(UnitDef{.id = "meter",
                         .aliases = {"m"},
                         .factor = 1,
                         .family = "metric",
                         .type = UnitType::Distance});
    registerUnit(UnitDef{.id = "kilometer",
                         .aliases = {"km"},
                         .factor = 1e3,
                         .family = "metric",
                         .type = UnitType::Distance});

    registerUnit(UnitDef{.id = "second",
                         .aliases = {"s", "sec", "secs", "seconds"},
                         .factor = 1,
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "minute",
                         .aliases = {"m", "min", "mins", "minutes"},
                         .factor = 60,
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "hour",
                         .aliases = {"h", "hr", "hrs", "hours"},
                         .factor = 60 * 60,
                         .family = "duration",
                         .type = UnitType::Duration});
    registerUnit(UnitDef{.id = "day",
                         .aliases = {"d", "days", "dys"},
                         .factor = 60 * 60 * 24,
                         .family = "duration",
                         .type = UnitType::Duration});

    registerUnit(UnitDef{.id = "kelvin",
                         .factor = 1,
                         .family = "degree",
                         .type = UnitType::Temperature});
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

  void registerUnit(UnitDef unit) { m_units.emplace_back(unit); }

  // units can share a same identifier, e.g 'm' can stand for 'meter' or
  // 'minute'.
  std::vector<UnitDef> findUnitCandidates(std::string_view q) const {
    auto units =
        m_units | std::views::filter([&](const UnitDef &unit) {
          return unit.id == q || std::ranges::contains(unit.aliases, q);
        }) |
        std::ranges::to<std::vector>();
    assert(!units.empty());
    return units;
  }

  const UnitDef *findUnit(const std::string &id) const {
    auto it = std::ranges::find_if(m_units, [&](const UnitDef &u) {
      return u.id == id || std::ranges::contains(u.aliases, id);
    });

    return it != m_units.end() ? &*it : nullptr;
  }

  std::expected<double, std::string> convert(double n, const UnitDef &from,
                                             const UnitDef &to) const {
    if (from.type == to.type) {
      auto base = n * from.factor + from.offset;
      return (base - to.offset) / to.factor;
    }

    return std::unexpected(std::format(
        "No idea how to convert {} to {}, as they are not of the same type.",
        from.family, to.family));
  }

private:
  std::vector<UnitDef> m_units;
};
