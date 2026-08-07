#pragma once
#include <optional>
#include <string_view>

namespace abacus {

std::optional<std::string_view> currencyForRegion(std::string_view region);

/**
 * Resolve the tender currency (ISO 4217) for a POSIX or BCP 47 locale
 * string, e.g "fr_FR", "en-US", "fr_FR.UTF-8@euro". Locales without a
 * region subtag ("fr", "C", "POSIX") resolve to nothing.
 */
std::optional<std::string_view> currencyForLocale(std::string_view locale);

/**
 * Number of minor unit digits for an ISO 4217 code, e.g. 0 for JPY, 2 for USD,
 * 3 for BHD. Unknown codes get the CLDR default.
 */
int currencyDigits(std::string_view currency);

}; // namespace abacus
