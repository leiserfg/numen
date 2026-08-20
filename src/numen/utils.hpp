#pragma once
#include <algorithm>
#include <ranges>
#include <cctype>
#include <string>

inline bool equalsIgnoreCase(const auto &a, const auto &b) {
  return std::ranges::equal(a, b, [](auto a, auto b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  });
}

inline void lowerCase(std::string &s) {
  std::ranges::transform(s, s.begin(), [](char c) { return std::tolower(c); });
}
