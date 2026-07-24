#include "timezone.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("should find timezone using fully qualifed name", "timezone") {
  TimezoneDB db;
  auto tz = db.query("America/New_York");

  REQUIRE(tz);
}

TEST_CASE("should not find timezone using wrong qualifed name", "timezone") {
  TimezoneDB db;
  auto tz = db.query("Americo/New_Yorgue");

  REQUIRE(!tz);
}

TEST_CASE("city name alone should match unique timezone", "timezone") {
  TimezoneDB db;
  auto tz = db.query("New_York");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name alone should match unique timezone, case insensitively",
          "timezone") {
  TimezoneDB db;
  auto tz = db.query("new_york");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name alone should match unique timezone, case insentively and "
          "without underscores",
          "timezone") {
  TimezoneDB db;
  auto tz = db.query("new york");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name alone should match unique timezone, case insentively and "
          "without underscores, and with any amount of spaces",
          "timezone") {
  TimezoneDB db;
  auto tz = db.query("   new     york ");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name for a timezone with many /") {
  TimezoneDB db;
  auto tz = db.query("buenos aires");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/Argentina/Buenos_Aires");
}

TEST_CASE("should handle famous european timezones") {
  TimezoneDB db;

  {
    auto tz = db.query("berlin");
    REQUIRE(tz);
    REQUIRE(tz->name() == "Europe/Berlin");
  }
  {
    auto tz = db.query("paris");
    REQUIRE(tz);
    REQUIRE(tz->name() == "Europe/Paris");
  }

  {
    auto tz = db.query("rome");
    REQUIRE(tz);
    REQUIRE(tz->name() == "Europe/Rome");
  }
}
