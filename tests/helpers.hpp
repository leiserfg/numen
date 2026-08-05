#pragma once

#include "abacus/abacus.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <expected>
#include <string>
#include <string_view>

namespace Catch {
template <typename T, typename E> struct StringMaker<std::expected<T, E>> {
  static std::string convert(const std::expected<T, E> &res) {
    if (res) { return ::Catch::Detail::stringify(*res); }
    return "unexpected(" + ::Catch::Detail::stringify(res.error()) + ")";
  }
};
} // namespace Catch

namespace test {

// "now" is frozen so that tests depending on the current date stay valid
inline const abacus::EvalConfig &frozenConfig() {
  static const abacus::EvalConfig config = [] {
    std::chrono::year_month_day now{std::chrono::year{2026}, std::chrono::month{1}, std::chrono::day{18}};
    return abacus::EvalConfig{
        .now = std::chrono::sys_days(now),
        .timezone = std::chrono::locate_zone("UTC"),
    };
  }();
  return config;
}

inline void assertExpr(std::string_view expr, std::string_view expected,
                       const abacus::EvalConfig &opts = {}) {
  CAPTURE(expr);
  abacus::Abacus calc;
  auto res = calc.evaluate(expr, opts);
  if (!res) {
    FAIL_CHECK("evaluation failed: " << res.error());
    return;
  }
  CHECK(res.value() == expected);
}

inline void assertNumber(std::string_view expr, double expected, double margin = 1e-9,
                         const abacus::EvalConfig &opts = {}) {
  CAPTURE(expr);
  abacus::Abacus calc;
  auto res = calc.compute(expr, opts);
  if (!res) {
    FAIL_CHECK("evaluation failed: " << res.error());
    return;
  }
  if (!res->isNumber()) {
    FAIL_CHECK("result is not a number");
    return;
  }
  CHECK_THAT(res->asNumber()->n, Catch::Matchers::WithinAbs(expected, margin));
}

} // namespace test
