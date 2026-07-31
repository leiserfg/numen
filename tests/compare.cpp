#include "helpers.hpp"
#include <catch2/catch_test_macros.hpp>

constexpr auto TAG = "[compare]";

using test::assertExpr;

TEST_CASE("same numbers should compare equal", TAG) {
  assertExpr("42 == 42", "true");
  assertExpr("42 == 21 * 2", "true");
  assertExpr("2^8==256", "true");
  assertExpr("-5 == -5", "true");
  assertExpr("1.5 == 1.5", "true");
}

TEST_CASE("different numbers should not compare equal", TAG) {
  assertExpr("42 == 21", "false");
  assertExpr("2^8==255", "false");
  assertExpr("-5 == 5", "false");
}

TEST_CASE("different numbers should compare not equal", TAG) {
  assertExpr("42 != 21", "true");
  assertExpr("2^8 != 255", "true");
  assertExpr("-5 != 5", "true");
}

TEST_CASE("same numbers should not compare not equal", TAG) {
  assertExpr("42 != 42", "false");
  assertExpr("42 != 21 * 2", "false");
}

TEST_CASE("greater than should only hold for strictly greater numbers", TAG) {
  assertExpr("42 > 21", "true");
  assertExpr("21 > 42", "false");
  assertExpr("42 > 42", "false");
  assertExpr("0 > -1", "true");
  assertExpr("1.5 > 1.4", "true");
}

TEST_CASE("greater than or equal should hold for greater and equal numbers", TAG) {
  assertExpr("42 >= 21", "true");
  assertExpr("42 >= 42", "true");
  assertExpr("21 >= 42", "false");
  assertExpr("-1 >= 1", "false");
  assertExpr("-1 >= 0", "false");
}

TEST_CASE("less than should only hold for strictly smaller numbers", TAG) {
  assertExpr("21 < 42", "true");
  assertExpr("42 < 21", "false");
  assertExpr("42 < 42", "false");
  assertExpr("-2 < -1", "true");
  assertExpr("-1 < 0", "true");
  assertExpr("1.4 < 1.5", "true");
}

TEST_CASE("less than or equal should hold for smaller and equal numbers", TAG) {
  assertExpr("21 <= 42", "true");
  assertExpr("42 <= 42", "true");
  assertExpr("42 <= 21", "false");
  assertExpr("0 <= -1", "false");
}

TEST_CASE("comparison should bind looser than arithmetic", TAG) {
  assertExpr("1 + 1 == 2", "true");
  assertExpr("2 * 3 > 4 + 1", "true");
  assertExpr("2 * 3 > 5 + 0", "true");
  assertExpr("10 - 5 <= 2 + 3", "true");
  assertExpr("2^3 < 3^2", "true");
  assertExpr("10 % 3 != 10 / 5", "true");
}

TEST_CASE("units should convert implicitly before comparing", TAG) {
  assertExpr("1 km == 1000 m", "true");
  assertExpr("1 km != 999 m", "true");
  assertExpr("1 km > 999 m", "true");
  assertExpr("1 km >= 1000 m", "true");
  assertExpr("1 km < 1001 m", "true");
  assertExpr("1 km <= 1000 m", "true");
  assertExpr("30min < 1hour", "true");
  assertExpr("2 hours <= 60 min", "false");
}

TEST_CASE("dates should compare chronologically", TAG) {
  const auto &opts = test::frozenConfig();

  assertExpr("18/01/2001 == 18/01/2001", "true", opts);
  assertExpr("18/01/2001 != 19/01/2001", "true", opts);
  assertExpr("18/01/2001 < 19/01/2001", "true", opts);
  assertExpr("18/01/2001 <= 18/01/2001", "true", opts);
  assertExpr("19/01/2001 > 18/01/2001", "true", opts);
  assertExpr("19/01/2001 >= 18/01/2001", "true", opts);
  assertExpr("19/01/2001 < 18/01/2001", "false", opts);
}
