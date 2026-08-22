#include "builtin-units.hpp"
#include <array>
#include <chrono>

namespace {
constexpr double toSeconds(auto d) { return std::chrono::duration<double>(d).count(); }
} // namespace

// Aliases must be unique within a dimension; collisions across dimensions are
// resolved from context at evaluation time. Lookup is case-insensitive and
// strips plurals / expands SI and binary prefixes on `prefixable` bases, so
// rows only need irregular forms (feet, micron) and colloquial shadows that
// override expansion (mb = megabyte, not millibit).
//
// Factors are relative to a per-dimension base unit.
// Sources: NIST SP 811 (exact defined conversions), ISO 4217 (currency codes).
std::span<const UnitDef> units::builtins() {
  // clang-format off
  static const auto BUILTIN_UNITS = std::to_array<UnitDef>({
    // length
    {.id = "meter",      .aliases = {"m", "metre"}, .symbol = "m",  .factor = 1,        .dimension = dimensions::LENGTH, .family = families::METRIC, .prefixable = true},
    {.id = "micrometer", .aliases = {"micron"},     .symbol = "µm", .factor = 1e-6,     .dimension = dimensions::LENGTH, .family = families::METRIC},
    {.id = "inch",       .aliases = {"in"},         .symbol = "in", .factor = 0.0254,   .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    {.id = "foot",       .aliases = {"ft", "feet"}, .symbol = "ft", .factor = 0.3048,   .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    {.id = "yard",       .aliases = {"yd"},         .symbol = "yd", .factor = 0.9144,   .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    {.id = "mile",       .aliases = {"mi"},         .symbol = "mi", .factor = 1609.344, .dimension = dimensions::LENGTH, .family = families::IMPERIAL},
    // css absolute units: 1in = 72pt = 6pc = 96 reference px, not device px
    {.id = "point", .aliases = {"pt"}, .symbol = "pt", .factor = 0.0254 / 72, .dimension = dimensions::LENGTH, .family = families::TYPOGRAPHIC},
    {.id = "pica",  .aliases = {"pc"}, .symbol = "pc", .factor = 0.0254 / 6,  .dimension = dimensions::LENGTH, .family = families::TYPOGRAPHIC},
    {.id = "pixel", .aliases = {"px"}, .symbol = "px", .factor = 0.0254 / 96, .dimension = dimensions::LENGTH, .family = families::TYPOGRAPHIC},

    // mass
    {.id = "gram",     .aliases = {"g", "gm", "gramme"}, .symbol = "g",  .factor = 1,            .dimension = dimensions::MASS, .family = families::METRIC, .prefixable = true},
    {.id = "kilogram", .aliases = {"kg", "kilo"},        .symbol = "kg", .factor = 1e3,          .dimension = dimensions::MASS, .family = families::METRIC},
    {.id = "tonne",    .aliases = {"t", "ton"},          .symbol = "t",  .factor = 1e6,          .dimension = dimensions::MASS, .family = families::METRIC},
    {.id = "pound",    .aliases = {"lb"},                .symbol = "lb", .factor = 453.59237,    .dimension = dimensions::MASS, .family = families::IMPERIAL},
    {.id = "ounce",    .aliases = {"oz"},                .symbol = "oz", .factor = 28.349523125, .dimension = dimensions::MASS, .family = families::IMPERIAL},
    {.id = "stone",    .aliases = {"st"},                .symbol = "st", .factor = 6350.29318,   .dimension = dimensions::MASS, .family = families::IMPERIAL},

    // duration
    {.id = "second", .aliases = {"s", "sec"}, .symbol = "s",   .factor = toSeconds(std::chrono::seconds{1}), .dimension = dimensions::DURATION, .family = families::DURATION, .prefixable = true},
    {.id = "minute", .aliases = {"m", "min"}, .symbol = "min", .factor = toSeconds(std::chrono::minutes{1}), .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "hour",   .aliases = {"h", "hr"},  .symbol = "h",   .factor = toSeconds(std::chrono::hours{1}),   .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "day",    .aliases = {"d"},        .symbol = "d",   .factor = toSeconds(std::chrono::days{1}),    .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "week",   .aliases = {"wk"},       .symbol = "wk",  .factor = toSeconds(std::chrono::weeks{1}),   .dimension = dimensions::DURATION, .family = families::DURATION},
    // average month/year
    {.id = "month", .aliases = {"mo"}, .symbol = "mo", .factor = toSeconds(std::chrono::months{1}), .dimension = dimensions::DURATION, .family = families::DURATION},
    {.id = "year",  .aliases = {"yr"}, .symbol = "yr", .factor = toSeconds(std::chrono::years{1}),  .dimension = dimensions::DURATION, .family = families::DURATION},

    // temperature
    {.id = "kelvin",     .aliases = {"k"},                      .symbol = "K",  .factor = 1,       .dimension = dimensions::TEMPERATURE, .family = families::DEGREE},
    {.id = "celsius",    .aliases = {"c", "cel", "centigrade"}, .symbol = "°C", .factor = 1,       .dimension = dimensions::TEMPERATURE, .family = families::DEGREE, .offset = 273.15},
    {.id = "fahrenheit", .aliases = {"f", "fahren"},            .symbol = "°F", .factor = 5.0 / 9, .dimension = dimensions::TEMPERATURE, .family = families::DEGREE, .offset = 459.67 * 5 / 9},

    // data: decimal SI sizes; binary sizes come from the ki/mi/gi/ti prefixes.
    // kb..pb shadow prefix expansion so "mb" stays megabyte, never millibit.
    {.id = "byte",     .aliases = {"b"},         .symbol = "B",   .factor = 1,     .dimension = dimensions::DATA, .family = families::DATA, .prefixable = true},
    {.id = "bit",      .aliases = {},            .symbol = "bit", .factor = 0.125, .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "kilobyte", .aliases = {"kb"},        .symbol = "kB",  .factor = 1e3,   .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "megabyte", .aliases = {"mb", "meg"}, .symbol = "MB",  .factor = 1e6,   .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "gigabyte", .aliases = {"gb", "gig"}, .symbol = "GB",  .factor = 1e9,   .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "terabyte", .aliases = {"tb"},        .symbol = "TB",  .factor = 1e12,  .dimension = dimensions::DATA, .family = families::DATA},
    {.id = "petabyte", .aliases = {"pb"},        .symbol = "PB",  .factor = 1e15,  .dimension = dimensions::DATA, .family = families::DATA},

    // volume
    {.id = "liter", .aliases = {"l", "litre"}, .symbol = "L", .factor = 1e-3, .dimension = dimensions::VOLUME, .family = families::METRIC, .prefixable = true},
    // US customary
    {.id = "gallon",     .aliases = {"gal"},  .symbol = "gal",  .factor = 3.785411784e-3,      .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "quart",      .aliases = {"qt"},   .symbol = "qt",   .factor = 0.946352946e-3,      .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "pint",       .aliases = {"pt"},   .symbol = "pt",   .factor = 0.473176473e-3,      .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "cup",        .aliases = {},       .symbol = "cup",  .factor = 0.2365882365e-3,     .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "tablespoon", .aliases = {"tbsp"}, .symbol = "tbsp", .factor = 0.01478676478125e-3, .dimension = dimensions::VOLUME, .family = families::IMPERIAL},
    {.id = "teaspoon",   .aliases = {"tsp"},  .symbol = "tsp",  .factor = 0.00492892159375e-3, .dimension = dimensions::VOLUME, .family = families::IMPERIAL},

    // area
    {.id = "hectare", .aliases = {"ha"}, .symbol = "ha", .factor = 1e4,          .dimension = dimensions::AREA, .family = families::METRIC},
    {.id = "acre",    .aliases = {},     .symbol = "ac", .factor = 4046.8564224, .dimension = dimensions::AREA, .family = families::IMPERIAL},

    // speed
    {.id = "knot", .aliases = {"kt"}, .symbol = "kn", .factor = 1852.0 / 3600, .dimension = dimensions::SPEED, .family = families::IMPERIAL},

    // currency (rates come from the currency provider; identity only). decimals
    // are the ISO 4217 / CLDR minor units; crypto gets satoshi precision.
    // a symbol is only stated where it reads back unambiguously: huf has none
    // because its "Ft" is the foot
    {.id = "usd", .aliases = {"dollar", "buck"},            .symbol = "$",   .name = "US Dollar",          .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "eur", .aliases = {"euro"},                      .symbol = "€",   .name = "Euro",               .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "gbp", .aliases = {"pound", "quid", "sterling"}, .symbol = "£",   .name = "Pound Sterling",     .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "jpy", .aliases = {"yen"},                       .symbol = "¥",   .name = "Japanese Yen",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},
    {.id = "cny", .aliases = {"yuan", "renminbi", "rmb"},   .symbol = "CN¥", .name = "Chinese Yuan",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "chf", .aliases = {"franc"},                                      .name = "Swiss Franc",                              .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "cad", .aliases = {},                            .symbol = "CA$", .name = "Canadian Dollar",    .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "aud", .aliases = {},                            .symbol = "A$",  .name = "Australian Dollar",  .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "nzd", .aliases = {},                            .symbol = "NZ$", .name = "New Zealand Dollar", .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "hkd", .aliases = {},                            .symbol = "HK$", .name = "Hong Kong Dollar",   .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "sgd", .aliases = {},                            .symbol = "S$",  .name = "Singapore Dollar",   .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "twd", .aliases = {},                            .symbol = "NT$", .name = "New Taiwan Dollar",  .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "sek", .aliases = {"krona", "kronor"},                            .name = "Swedish Krona",                            .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "nok", .aliases = {"krone", "kroner"},                            .name = "Norwegian Krone",                          .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "dkk", .aliases = {},                                             .name = "Danish Krone",                             .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "pln", .aliases = {"zloty"},                     .symbol = "zł",  .name = "Polish Zloty",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "czk", .aliases = {"koruna"},                    .symbol = "Kč",  .name = "Czech Koruna",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},

	// huf instead of Ft to not conflict with "foot". We may need a better way to solve this.
    {.id = "huf", .aliases = {"forint"},                                     .name = "Hungarian Forint",                         .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},

    {.id = "ron", .aliases = {"leu", "lei"},                                 .name = "Romanian Leu",                             .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "try", .aliases = {"lira"},                      .symbol = "₺",   .name = "Turkish Lira",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "rub", .aliases = {"ruble", "rouble"},           .symbol = "₽",   .name = "Russian Ruble",      .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "uah", .aliases = {"hryvnia"},                   .symbol = "₴",   .name = "Ukrainian Hryvnia",  .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "inr", .aliases = {"rupee"},                     .symbol = "₹",   .name = "Indian Rupee",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "pkr", .aliases = {},                                             .name = "Pakistani Rupee",                          .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "krw", .aliases = {"won"},                       .symbol = "₩",   .name = "South Korean Won",   .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},
    {.id = "thb", .aliases = {"baht"},                      .symbol = "฿",   .name = "Thai Baht",          .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "myr", .aliases = {"ringgit"},                   .symbol = "RM",  .name = "Malaysian Ringgit",  .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "idr", .aliases = {"rupiah"},                    .symbol = "Rp",  .name = "Indonesian Rupiah",  .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},
    {.id = "php", .aliases = {},                            .symbol = "₱",   .name = "Philippine Peso",    .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "vnd", .aliases = {"dong"},                      .symbol = "₫",   .name = "Vietnamese Dong",    .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},
    {.id = "aed", .aliases = {"dirham"},                                     .name = "United Arab Emirates Dirham",              .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "sar", .aliases = {"riyal"},                                      .name = "Saudi Riyal",                              .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "ils", .aliases = {"shekel"},                    .symbol = "₪",   .name = "Israeli New Shekel", .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "zar", .aliases = {"rand"},                      .symbol = "R",   .name = "South African Rand", .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "egp", .aliases = {},                            .symbol = "E£",  .name = "Egyptian Pound",     .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "ngn", .aliases = {"naira"},                     .symbol = "₦",   .name = "Nigerian Naira",     .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "brl", .aliases = {"real", "reais"},             .symbol = "R$",  .name = "Brazilian Real",     .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "mxn", .aliases = {"peso"},                      .symbol = "MX$", .name = "Mexican Peso",       .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "ars", .aliases = {},                                             .name = "Argentine Peso",                           .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "clp", .aliases = {},                                             .name = "Chilean Peso",                             .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},
    {.id = "cop", .aliases = {},                                             .name = "Colombian Peso",                           .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 0},
    {.id = "pen", .aliases = {"sol"},                                        .name = "Peruvian Sol",                             .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 2},
    {.id = "btc", .aliases = {"bitcoin"},                   .symbol = "₿",   .name = "Bitcoin",            .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 8},
    {.id = "eth", .aliases = {"ether", "ethereum"},         .symbol = "Ξ",   .name = "Ethereum",           .symbolPrefix = true, .dimension = dimensions::CURRENCY, .family = families::CURRENCY, .decimals = 8},
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
