#include "timezone.hpp"
#include "unicode.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#if NUMEN_USE_DATE_TZ
#include <filesystem>
#endif

using namespace std::literals;

namespace {

struct GeoEntry {
  uint32_t name;
  uint32_t population;
  uint16_t tz;
  uint16_t admin1;
};

struct GeoQualifier {
  uint32_t name;
  uint16_t id;
  bool isCountry;
};

#include "gen/geo-tz-tables.inc"

std::string_view geoName(uint32_t offset) {
  auto chunk = kGeoNameChunks[offset / kGeoNameChunkSize].substr(offset % kGeoNameChunkSize);
  return chunk.substr(0, chunk.find('\0'));
}

auto lookup(const auto &table, std::string_view name) {
  return std::ranges::equal_range(table, name, {}, [](const auto &e) { return geoName(e.name); });
}

bool inScope(const GeoEntry &entry, std::ranges::range auto &&scopes) {
  return std::ranges::any_of(scopes, [&](const GeoQualifier &q) {
    return q.isCountry ? kGeoAdmin1Country[entry.admin1] == q.id : entry.admin1 == q.id;
  });
}

const GeoEntry *findGeoEntry(std::string_view name, std::string_view qualifier) {
  auto entries = lookup(kGeoEntries, name);
  if (entries.empty()) return nullptr;

  const GeoEntry *best = nullptr;
  if (qualifier.empty()) {
    best = &*std::ranges::max_element(entries, {}, &GeoEntry::population);
  } else {
    auto scopes = lookup(kGeoQualifiers, qualifier);
    for (const auto &entry : entries) {
      if (inScope(entry, scopes) && (!best || entry.population > best->population)) best = &entry;
    }
  }
  return best;
}

std::optional<std::string_view> queryGeoDb(std::string_view query) {
  auto normalized = normalizeName(query);
  std::string_view q{normalized};

  for (auto split = q.size(); split != std::string_view::npos; split = q.rfind(' ', split - 1)) {
    if (auto entry = findGeoEntry(q.substr(0, split), split < q.size() ? q.substr(split + 1) : ""sv)) {
      return kGeoTzNames[entry->tz];
    }
    if (split == 0) break;
  }

  return std::nullopt;
}

bool sameZoneName(std::string_view query, std::string_view name);

#if NUMEN_USE_DATE_TZ
struct OsTzLink {
  std::string name;
  std::string target;
};

// mirrors date's discovery, which it does not expose
std::filesystem::path zoneinfoDir() {
#ifdef __APPLE__
  std::error_code ec;
  auto localtime = std::filesystem::read_symlink("/etc/localtime", ec).string();
  if (auto i = localtime.find("zoneinfo"); !ec && i != std::string::npos) {
    return localtime.substr(0, localtime.find('/', i));
  }
#endif
  return "/usr/share/zoneinfo";
}

// date's OS tzdb lists link files as zones; the symlinks tell them apart
const std::vector<OsTzLink> &osLinks() {
  static const auto links = [] {
    namespace fs = std::filesystem;
    std::vector<OsTzLink> out;
    std::error_code ec;
    const auto dir = fs::weakly_canonical(zoneinfoDir(), ec);
    for (const auto &zone : numen::tz::get_tzdb().zones) {
      const auto path = dir / zone.name();
      if (!fs::is_symlink(path, ec)) continue;
      auto target = fs::weakly_canonical(path, ec).lexically_relative(dir).generic_string();
      if (!ec && !target.empty()) out.push_back({zone.name(), std::move(target)});
    }
    return out;
  }();
  return links;
}

bool isLinkName(std::string_view name) {
  return std::ranges::any_of(osLinks(), [&](auto &&l) { return l.name == name; });
}
#else
bool isLinkName(std::string_view) { return false; }
#endif

std::optional<std::string_view> findLinkTarget(std::string_view query) {
#if NUMEN_USE_DATE_TZ
  for (const auto &link : osLinks()) {
    if (sameZoneName(query, link.name)) return link.target;
  }
#else
  for (const auto &link : numen::tz::get_tzdb().links) {
    if (sameZoneName(query, link.name())) return link.target();
  }
#endif
  return std::nullopt;
}

// links resolve to their canonical zone, as std::chrono::locate_zone does
const numen::tz::time_zone *locateZone(std::string_view name) {
  try {
    if (isLinkName(name)) {
      if (auto target = findLinkTarget(name)) name = *target;
    }
    return numen::tz::locate_zone(name);
  } catch (const std::runtime_error &) { return nullptr; }
}

// clang-format off
constexpr auto CUSTOM_LINKS = std::to_array<CustomTzLink>({
    {.name = "nyc", .target = "America/New_York"},
    {.name = "ny", .target = "America/New_York"},
    {.name = "dc", .target = "America/New_York"},
    {.name = "ldn", .target = "Europe/London"},
    // the state outweighs the district in the geo database
    {.name = "washington", .target = "America/New_York"},
    {.name = "sf", .target = "America/Los_Angeles"},

    {.name = "PST", .target = "America/Los_Angeles"},
    {.name = "PDT", .target = "America/Los_Angeles"},
    {.name = "PT", .target = "America/Los_Angeles"},
    {.name = "MST", .target = "America/Denver"},
    {.name = "MDT", .target = "America/Denver"},
    {.name = "CST", .target = "America/Chicago"},
    {.name = "CDT", .target = "America/Chicago"},
    {.name = "EST", .target = "America/New_York"},
    {.name = "EDT", .target = "America/New_York"},
    {.name = "BST", .target = "Europe/London"},
    {.name = "CET", .target = "Europe/Paris"},
    {.name = "CEST", .target = "Europe/Paris"},
    {.name = "EET", .target = "Europe/Athens"},
    {.name = "EEST", .target = "Europe/Athens"},
    {.name = "MSK", .target = "Europe/Moscow"},
    {.name = "IST", .target = "Asia/Kolkata"},
    {.name = "HKT", .target = "Asia/Hong_Kong"},
    {.name = "KST", .target = "Asia/Seoul"},
    {.name = "JST", .target = "Asia/Tokyo"},
    {.name = "AEST", .target = "Australia/Sydney"},
    {.name = "AEDT", .target = "Australia/Sydney"},
});
// clang-format on

bool sameZoneName(std::string_view query, std::string_view name) {
  return std::ranges::equal(query, name, [](unsigned char a, unsigned char b) {
    if (a == ' ') a = '_';
    if (b == ' ') b = '_';
    return std::tolower(a) == std::tolower(b);
  });
}

}; // namespace

std::span<const CustomTzLink> TimezoneDB::customLinks() { return CUSTOM_LINKS; }

const numen::tz::time_zone *TimezoneDB::userTz() const { return numen::tz::get_tzdb().current_zone(); }

const numen::tz::time_zone *TimezoneDB::query(std::string_view query) const {
  const auto &db = numen::tz::get_tzdb();

  if (auto it =
          std::ranges::find_if(CUSTOM_LINKS, [&](auto &&link) { return equalsIgnoreCase(link.name, query); });
      it != CUSTOM_LINKS.end()) {
    return locateZone(it->target);
  }

  if (auto it = std::ranges::find_if(
          db.zones, [&](auto &&tz) { return !isLinkName(tz.name()) && sameZoneName(query, tz.name()); });
      it != db.zones.end()) {
    return &*it;
  }

  if (auto name = queryGeoDb(query)) { return locateZone(*name); }

  // after the geo database: Greenwich and GB are places before being legacy links
  if (auto target = findLinkTarget(query)) { return locateZone(*target); }

  return nullptr;
}
