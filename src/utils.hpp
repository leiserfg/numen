#pragma once
#include <algorithm>
#include <cctype>

inline bool equalsIgnoreCase(const auto &a, const auto &b) {
  return std::ranges::equal(
      a, b, [](auto a, auto b) { return std::tolower(a) == std::tolower(b); });
}
