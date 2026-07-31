#include "helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

constexpr auto TAG = "[datetime]";

static const auto &evalOpts = test::frozenConfig();

TEST_CASE("should parse time, delimited with :", TAG) {
  test::assertExpr("2:42:21", "2026-07-26 02:42:21 (Etc/UTC)", evalOpts);
}

TEST_CASE("should allow adding duration to time", TAG) {
  test::assertExpr("2:42:21 + 1hour", "2026-07-26 03:42:21 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse DD/MM/YYYY", TAG) {
  test::assertExpr("18/01/2001", "2001-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse DD/MM/YYYY hh:mm:ss", TAG) {
  test::assertExpr("18/01/2001 13:30:15", "2001-01-18 13:30:15 (Etc/UTC)", evalOpts);
}

TEST_CASE("should add duration to date time", TAG) {
  test::assertExpr("18/01/2001 13:30:15 + 1d + 5min", "2001-01-19 13:35:15 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse YYYY/MM/DD", TAG) {
  test::assertExpr("2001/01/18", "2001-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("separators should be adjacent to numbers", TAG) {
  // spaced out, this is a division: 2001 / 1 / 18 ~= 111
  test::assertNumber("2001 / 1 / 18", 111.1666, 1e-3, evalOpts);
}

TEST_CASE("should allow YYYY/MM/DD to have a timezone", TAG) {
  test::assertExpr("2001/01/18 New York", "2001-01-18 00:00:00 (America/New_York)", evalOpts);
}

TEST_CASE("ambiguous date format should be parsed as DD/MM/YYYY ", TAG) {
  test::assertExpr("11/08/2025", "2025-08-11 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse MM/DD/YYYY", TAG) {
  test::assertExpr("01/18/2001", "2001-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse month name", TAG) {
  auto expr = GENERATE("18 Jan 2001", "18 January 2001", "18 january 2001");

  test::assertExpr(expr, "2001-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse <month_name> <day_of_the_month>", TAG) {
  auto expr = GENERATE("Jan 18", "January 18");

  test::assertExpr(expr, "2026-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse <month_name> <day_of_the_month> <year>", TAG) {
  test::assertExpr("Jan 18 2025", "2025-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse <year> <month_name> <day_of_the_month>", TAG) {
  test::assertExpr("2025 Jan 18", "2025-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

// very cursed, but hey...
TEST_CASE("should parse <month_name> <year> <day_of_the_month>", TAG) {
  test::assertExpr("Jan 2025 18", "2025-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse <month_name> <year>", TAG) {
  test::assertExpr("Jan 2025", "2025-01-01 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should parse <year> <month_name>", TAG) {
  test::assertExpr("2025 Jan", "2025-01-01 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should be able to add months to date", TAG) {
  auto expr = GENERATE("01/18/2001 + 4 month", "01/18/2001 + 4 months", "01/18/2001 + 4mo");

  test::assertExpr(expr, "2001-05-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("should be able to add years to date", TAG) {
  test::assertExpr("01/18/2001 + 1 year", "2002-01-18 00:00:00 (Etc/UTC)", evalOpts);
  test::assertExpr("01/18/2001 + 3 years", "2004-01-18 00:00:00 (Etc/UTC)", evalOpts);
  test::assertExpr("01/18/2001 + 1yr", "2002-01-18 00:00:00 (Etc/UTC)", evalOpts);
}

TEST_CASE("subtracting two dates should produce the difference in seconds", TAG) {
  test::assertNumber("01/18/2001 1:00:00 - 01/18/2001 00:00:00", 3600, 1e-9, evalOpts);
  test::assertNumber("01/18/2001 1:00:00 - 01/18/2001 00:00:00 to hours", 1, 1e-9, evalOpts);
}
