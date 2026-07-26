#include "timezone.hpp"
#include "utils.hpp"
#include <array>
#include <bits/ranges_algo.h>
#include <chrono>
#include <ranges>
#include <string_view>

namespace {
auto splitOnAny(auto s, std::string_view delims) {
  return s | std::views::chunk_by([delims](char a, char b) {
           return delims.contains(a) == delims.contains(b);
         }) |
         std::views::filter([delims](auto &&chunk) {
           return !delims.contains(chunk.front());
         });
};

bool matchesTz(std::string_view query, std::string_view name) {
  if (equalsIgnoreCase(name, query)) {
    return true;
  }

  auto parts = name | std::views::split(std::string_view{"/"});
  auto last = std::ranges::fold_left(parts, std::string_view{},
                                     [](auto &&, auto &&x) { return x; });

  if (!last.empty()) {
    std::string_view delims{"_/ "};
    auto queryWords = splitOnAny(query, delims);
    auto words = splitOnAny(last, delims);

    return std::ranges::equal(queryWords, words, [](auto &&w1, auto &&w2) {
      return equalsIgnoreCase(std::string_view{w1}, std::string_view{w2});
    });
  }

  return false;
}

}; // namespace

struct CustomTzLink {
  std::string_view name;
  std::string_view target;
};

constexpr auto NewYorkTz = "America/New_York";

// clang-format off
constexpr auto CUSTOM_LINKS = std::to_array<CustomTzLink>({
    {.name = "nyc", .target = NewYorkTz},
    {.name = "Europe", .target = "Europe/Paris"},
    {.name = "Russia", .target = "Europe/Moscow"},
    {.name = "Beijing", .target = "Asia/Shanghai"},
    {.name = "China", .target = "Asia/Shanghai"},
});
// clang-format on

const std::chrono::time_zone *TimezoneDB::userTz() const {
  return std::chrono::get_tzdb().current_zone();
}

const std::chrono::time_zone *TimezoneDB::query(std::string_view query) const {
  const auto &db = std::chrono::get_tzdb();

  {
    auto it = std::ranges::find_if(db.zones,
                                   [&query](const std::chrono::time_zone &tz) {
                                     return matchesTz(query, tz.name());
                                   });

    if (it != db.zones.end()) {
      return &*it;
    }
  }

  {
    auto linkIt = std::ranges::find_if(
        db.links, [&query](const std::chrono::time_zone_link &link) {
          return matchesTz(query, link.name());
        });

    if (linkIt != db.links.end()) {
      return std::chrono::locate_zone(linkIt->target());
    }
  }

  auto linkIt =
      std::ranges::find_if(CUSTOM_LINKS, [&query](const CustomTzLink &link) {
        return matchesTz(query, link.name);
      });

  if (linkIt != CUSTOM_LINKS.end()) {
    return std::chrono::locate_zone(linkIt->target);
  }

  return nullptr;
}
