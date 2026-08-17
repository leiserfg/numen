#include "builtin-units.hpp"
#include <array>
#include <chrono>

namespace {
constexpr double toSeconds(auto d) { return std::chrono::duration<double>(d).count(); }
}; // namespace

// Aliases must be unique within a dimension; collisions across dimensions are
// resolved from context at evaluation time. Lookup is case-insensitive and
// strips plurals / expands SI and binary prefixes on `prefixable` bases, so
// rows only need irregular forms (feet, micron) and colloquial shadows that
// override expansion (mb = megabyte, not millibit).
//
// Factors are relative to a per-dimension base unit.
// Sources: NIST SP 811 (exact defined conversions), ISO 4217 (currency codes).

// must stay function-local: at namespace scope a consumer's own static Numen
// can be constructed before this table is, and read it empty
std::span<const UnitDef> units::builtins() {
// clang-format off
  static const auto BUILTIN_UNITS = std::to_array<UnitDef>({
    // length
    {.id = "meter",      .aliases = {"m", "metre"}, .factor = 1,        .dimension = dimensions::LENGTH, .family = families::METRIC,   .prefixable = true},
    {.id = "micrometer", .aliases = {"micron"},     .factor = 1e-6,     .dimension = dimensions::LENGTH, .family = families::METRIC},
    {.id = "inch",       .aliases = {"in"},         .factor = 0.0254,   .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    {.id = "foot",       .aliases = {"ft", "feet"}, .factor = 0.3048,   .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    {.id = "yard",       .aliases = {"yd"},         .factor = 0.9144,   .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    {.id = "mile",       .aliases = {"mi"},         .factor = 1609.344, .dimension = dimensions::LENGTH, .family = families::IMPERIAL},

    // mass
    {.id = "gram",     .aliases = {"g", "gm", "gramme"}, .factor = 1,            .dimension = dimensions::MASS, .family = families::METRIC,   .prefixable = true},
    {.id = "kilogram", .aliases = {"kg", "kilo"},        .factor = 1e3,          .dimension = dimensions::MASS, .family = families::METRIC},
    {.id = "tonne",    .aliases = {"t", "ton"},          .factor = 1e6,          .dimension = dimensions::MASS, .family = families::METRIC},
    {.id = "pound",    .aliases = {"lb"},                .factor = 453.59237,    .dimension = dimensions::MASS, .family = families::IMPERIAL},
    {.id = "ounce",    .aliases = {"oz"},                .factor = 28.349523125, .dimension = dimensions::MASS, .family = families::IMPERIAL},
    {.id = "stone",    .aliases = {"st"},                .factor = 6350.29318,   .dimension = dimensions::MASS, .family = families::IMPERIAL},

    // duration
    {.id = "second", .aliases = {"s", "sec"}, .factor = toSeconds(std::chrono::seconds{1}), .dimension = dimensions::DURATION, .family = families::DURATION, .prefixable = true},
    {.id = "minute", .aliases = {"m", "min"}, .factor = toSeconds(std::chrono::minutes{1}), .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "hour",   .aliases = {"h", "hr"},  .factor = toSeconds(std::chrono::hours{1}),   .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "day",    .aliases = {"d"},        .factor = toSeconds(std::chrono::days{1}),    .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "week",   .aliases = {"wk"},       .factor = toSeconds(std::chrono::weeks{1}),   .dimension = dimensions::DURATION, .family = families::DURATION},
    // average month/year
    {.id = "month",  .aliases = {"mo"},       .factor = toSeconds(std::chrono::months{1}),  .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "year",   .aliases = {"yr"},       .factor = toSeconds(std::chrono::years{1}),   .dimension = dimensions::DURATION, .family = families::DURATION},

    // temperature
    {.id = "kelvin",     .aliases = {"k"},                      .factor = 1,       .dimension = dimensions::TEMPERATURE, .family = families::DEGREE},
    {.id = "celsius",    .aliases = {"c", "cel", "centigrade"}, .factor = 1,       .dimension = dimensions::TEMPERATURE, .family = families::DEGREE, .offset = 273.15},
    {.id = "fahrenheit", .aliases = {"f", "fahren"},            .factor = 5.0 / 9, .dimension = dimensions::TEMPERATURE, .family = families::DEGREE, .offset = 459.67 * 5 / 9},

    // data: decimal SI sizes; binary sizes come from the ki/mi/gi/ti prefixes.
    // kb..pb shadow prefix expansion so "mb" stays megabyte, never millibit.
    {.id = "byte",     .aliases = {"b"},         .factor = 1,     .dimension = dimensions::DATA, .family = families::DATA, .prefixable = true},
    {.id = "bit",      .aliases = {},            .factor = 0.125, .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "kilobyte", .aliases = {"kb"},        .factor = 1e3,   .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "megabyte", .aliases = {"mb", "meg"}, .factor = 1e6,   .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "gigabyte", .aliases = {"gb", "gig"}, .factor = 1e9,   .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "terabyte", .aliases = {"tb"},        .factor = 1e12,  .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "petabyte", .aliases = {"pb"},        .factor = 1e15,  .dimension = dimensions::DATA, .family = families::DATA},

    // volume
    {.id = "liter",      .aliases = {"l", "litre"}, .factor = 1e-3,             .dimension = dimensions::VOLUME, .family = families::METRIC,   .prefixable = true},
    // US customary
    {.id = "gallon",     .aliases = {"gal"},        .factor = 3.785411784e-3,   .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "quart",      .aliases = {"qt"},         .factor = 0.946352946e-3,   .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "pint",       .aliases = {"pt"},         .factor = 0.473176473e-3,   .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "cup",        .aliases = {},             .factor = 0.2365882365e-3,  .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "tablespoon", .aliases = {"tbsp"},       .factor = 0.01478676478125e-3, .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "teaspoon",   .aliases = {"tsp"},        .factor = 0.00492892159375e-3, .dimension = dimensions::VOLUME, .family = families::IMPERIAL},

    // area
    {.id = "hectare",          .aliases = {"ha"},          .factor = 1e4,          .dimension = dimensions::AREA, .family = families::METRIC},
    {.id = "acre",             .aliases = {},              .factor = 4046.8564224, .dimension = dimensions::AREA, .family = families::IMPERIAL},

    // speed
    {.id = "knot",               .aliases = {"kt"},         .factor = 1852.0 / 3600, .dimension = dimensions::SPEED, .family = families::IMPERIAL},

    // currency (rates come from the currency provider; identity only)
    {.id = "usd", .aliases = {"dollar", "buck"},            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "eur", .aliases = {"euro"},                      .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "gbp", .aliases = {"pound", "quid", "sterling"}, .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "jpy", .aliases = {"yen"},                       .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "cny", .aliases = {"yuan", "renminbi", "rmb"},   .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "chf", .aliases = {"franc"},                     .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "cad", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "aud", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "nzd", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "hkd", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "sgd", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "twd", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "sek", .aliases = {"krona", "kronor"},           .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "nok", .aliases = {"krone", "kroner"},           .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "dkk", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "pln", .aliases = {"zloty"},                     .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "czk", .aliases = {"koruna"},                    .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "huf", .aliases = {"forint"},                    .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "ron", .aliases = {"leu", "lei"},                .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "try", .aliases = {"lira"},                      .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "rub", .aliases = {"ruble", "rouble"},           .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "uah", .aliases = {"hryvnia"},                   .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "inr", .aliases = {"rupee"},                     .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "pkr", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "krw", .aliases = {"won"},                       .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "thb", .aliases = {"baht"},                      .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "myr", .aliases = {"ringgit"},                   .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "idr", .aliases = {"rupiah"},                    .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "php", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "vnd", .aliases = {"dong"},                      .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "aed", .aliases = {"dirham"},                    .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "sar", .aliases = {"riyal"},                     .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "ils", .aliases = {"shekel"},                    .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "zar", .aliases = {"rand"},                      .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "egp", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "ngn", .aliases = {"naira"},                     .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "brl", .aliases = {"real", "reais"},             .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "mxn", .aliases = {"peso"},                      .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "ars", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "clp", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "cop", .aliases = {},                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "pen", .aliases = {"sol"},                       .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "btc", .aliases = {"bitcoin"},                   .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
    {.id = "eth", .aliases = {"ether", "ethereum"},         .dimension = dimensions::CURRENCY, .family = families::CURRENCY},
  });
// clang-format on

  return BUILTIN_UNITS;
}

// spellings that stand for a composition. the composition doubles as the
// rendered form, so it is written with short aliases
// clang-format off
std::span<const CompoundAlias> units::compoundAliases() {
  static constexpr auto ALIASES = std::to_array<CompoundAlias>({
      {.name = "mps",  .composition = "m/s"},
      {.name = "kmh",  .composition = "km/h"},
      {.name = "kph",  .composition = "km/h"},
      {.name = "mph",  .composition = "mi/h"},
      {.name = "sqm",  .composition = "m^2"},
      {.name = "m2",   .composition = "m^2"},
      {.name = "sqkm", .composition = "km^2"},
      {.name = "km2",  .composition = "km^2"},
      {.name = "sqft", .composition = "ft^2"},
      {.name = "ft2",  .composition = "ft^2"},
  });

  return ALIASES;
}
// clang-format on
