#include "unicode.hpp"
#include <algorithm>
#include <cctype>
#include <iterator>
#include <ranges>

namespace {

struct CharMapping {
  uint32_t codepoint;
  std::string_view replacement;
};

#include "gen/geo-charmap.inc"

} // namespace

void appendUtf8(std::string &out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

std::string normalizeName(std::string_view query) {
  std::string out;
  auto separate = [&] {
    if (!out.empty() && out.back() != ' ') out.push_back(' ');
  };

  for (size_t i = 0; i < query.size(); ++i) {
    auto c = static_cast<unsigned char>(query[i]);

    if (c < 0x80) {
      if (c == '.' || c == '\'') continue;
      if (std::isalnum(c)) {
        out.push_back(static_cast<char>(std::tolower(c)));
      } else {
        separate();
      }
      continue;
    }

    const int extra = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
    if (extra < 0 || i + static_cast<size_t>(extra) >= query.size()) continue;

    uint32_t cp = c & (0x3F >> extra);
    for (int k = 0; k < extra; ++k)
      cp = (cp << 6) | (query[++i] & 0x3F);

    auto it = std::ranges::lower_bound(kCharMap, cp, {}, &CharMapping::codepoint);
    if (it == std::end(kCharMap) || it->codepoint != cp) {
      appendUtf8(out, cp);
    } else {
      for (const char r : it->replacement) {
        r == ' ' ? separate() : out.push_back(r);
      }
    }
  }
  if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}
