#pragma once
#include <algorithm>
#include <chrono>
#include <ranges>
#include <string_view>

class TimezoneDB {
public:
  // search the timezone database in a relaxed way, e.g a query of "new york"
  // should yield "America/New_York"
  //
  // "new york" -> America/New_York
  // "paris" -> Europe/Paris
  // "new_york" -> America/New_York
  const std::chrono::time_zone *query(std::string_view query) const {
    const auto &db = std::chrono::get_tzdb();

    auto splitOnAny = [](auto s, std::string_view delims) {
      return s | std::views::chunk_by([delims](char a, char b) {
               return delims.contains(a) == delims.contains(b);
             }) |
             std::views::filter([delims](auto &&chunk) {
               return !delims.contains(chunk.front());
             });
    };

    auto it = std::ranges::find_if(
        db.zones, [&query, splitOnAny](const std::chrono::time_zone &tz) {
          if (tz.name() == query) {
            return true;
          }

          auto parts = tz.name() | std::views::split(std::string_view{"/"});
          auto last = std::ranges::fold_left(
              parts, std::string_view{}, [](auto &&, auto &&x) { return x; });

          if (!last.empty()) {
            std::string_view delims{"_/ "};
            auto queryWords = splitOnAny(query, delims);
            auto words = splitOnAny(last, delims);

            return std::ranges::equal(
                queryWords, words, [](auto &&w1, auto &&w2) {
                  return std::ranges::equal(w1, w2, [](auto a, auto b) {
                    return std::tolower(a) == std::tolower(b);
                  });
                });
          }

          return false;
        });

    if (it == db.zones.end())
      return nullptr;

    return &*it;
  }

  const std::chrono::time_zone *userTz() {
    return std::chrono::get_tzdb().current_zone();
  }
};
