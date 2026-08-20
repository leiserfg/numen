#include "helpers.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

std::string_view trim(std::string_view s) {
  auto first = std::ranges::find_if_not(s, isSpace);
  auto last =
      std::ranges::find_if_not(std::ranges::subrange(first, s.end()) | std::views::reverse, isSpace).base();
  return {first, last};
}

std::size_t findSeparator(std::string_view entry) {
  auto pos = entry.rfind("=>");

  while (pos != std::string_view::npos && pos > 0) {
    if (isSpace(entry[pos - 1]) && pos + 2 < entry.size() && isSpace(entry[pos + 2])) { return pos; }
    pos = pos > 1 ? entry.rfind("=>", pos - 2) : std::string_view::npos;
  }

  return std::string_view::npos;
}

// Corpus format, one entry per line:
//   <expression> => <expected output>
//   <expression> => !error            (any error)
//   <expression> => !error <message>  (exact error message)
// The '=>' separator can be padded with spaces or tabs.
// '#' starts a comment, blank lines are ignored
//
void runCorpusFile(const fs::path &path) {
  std::ifstream file(path);
  REQUIRE(file.is_open());

  const std::string src(std::istreambuf_iterator<char>(file), {});
  numen::Numen calc;
  int lineno = 0;

  for (auto raw : src | std::views::split('\n')) {
    ++lineno;
    auto entry = trim(std::string_view(raw.begin(), raw.end()));
    if (entry.empty() || entry.starts_with('#')) { continue; }

    INFO(path.filename().string() << ":" << lineno << ": " << entry);

    auto sep = findSeparator(entry);
    if (sep == std::string_view::npos) {
      FAIL_CHECK("malformed corpus line, missing whitespace-delimited '=>'");
      continue;
    }

    auto expr = trim(entry.substr(0, sep));
    auto expected = trim(entry.substr(sep + 2));
    auto res = calc.evaluate(expr, test::frozenConfig());

    if (expected.starts_with("!error")) {
      if (res) {
        FAIL_CHECK("expected an error, got: " << res.value());
        continue;
      }

      auto msg = trim(expected.substr(6));
      if (msg.empty()) {
        SUCCEED();
      } else {
        CHECK(res.error() == msg);
      }
      continue;
    }

    if (!res) {
      FAIL_CHECK("evaluation failed: " << res.error());
      continue;
    }
    CHECK(res.value() == expected);
  }
}

std::vector<std::string> corpusExpressions(const fs::path &path) {
  std::ifstream file(path);
  REQUIRE(file.is_open());

  const std::string src(std::istreambuf_iterator<char>(file), {});
  std::vector<std::string> out;

  for (auto raw : src | std::views::split('\n')) {
    auto entry = trim(std::string_view(raw.begin(), raw.end()));
    if (entry.empty() || entry.starts_with('#')) { continue; }

    auto sep = findSeparator(entry);
    if (sep == std::string_view::npos) { continue; }
    out.emplace_back(trim(entry.substr(0, sep)));
  }

  return out;
}

std::vector<fs::path> corpusFiles() {
  std::vector<fs::path> files;
  std::ranges::copy(fs::directory_iterator(CORPUS_DIR) | std::views::transform([](const auto &e) {
                      return e.path();
                    }) | std::views::filter([](const auto &p) { return p.extension() == ".corpus"; }),
                    std::back_inserter(files));
  std::ranges::sort(files);
  REQUIRE(!files.empty());
  return files;
}

} // namespace

// a launcher evaluates on every keystroke, so every prefix is a real input
TEST_CASE("typing an expression never crashes", "[corpus][typing]") {
  numen::Numen calc;

  for (const auto &file : corpusFiles()) {
    for (const auto &expr : corpusExpressions(file)) {
      for (std::size_t n = 1; n <= expr.size(); ++n) {
        auto prefix = std::string_view{expr}.substr(0, n);
        INFO(file.filename().string() << ": typing \"" << prefix << "\"");
        auto res = calc.evaluate(prefix, test::frozenConfig());
        CHECK((res.has_value() || !res.error().empty()));
      }
    }
  }
}

TEST_CASE("corpus", "[corpus]") {
  for (const auto &f : corpusFiles()) {
    DYNAMIC_SECTION(f.filename().string()) { runCorpusFile(f); }
  }
}
