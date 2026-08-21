#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

inline bool equalsIgnoreCase(const auto &a, const auto &b) {
  using V = std::string_view;
  return std::ranges::equal(V{a}, V{b}, [](auto a, auto b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  });
}

inline void lowerCase(std::string &s) {
  std::ranges::transform(s, s.begin(), [](char c) { return std::tolower(c); });
}
