#include "timezone.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

constexpr auto TAG = "[timezone]";

TEST_CASE("should find timezone using fully qualifed name", TAG) {
  TimezoneDB db;
  auto tz = db.query("America/New_York");

  REQUIRE(tz);
}

TEST_CASE("should not find timezone using wrong qualifed name", TAG) {
  TimezoneDB db;
  auto tz = db.query("Americo/New_Yorgue");

  REQUIRE(!tz);
}

TEST_CASE("city name alone should match unique timezone", TAG) {
  TimezoneDB db;
  auto tz = db.query("New_York");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name alone should match unique timezone, case insensitively", TAG) {
  TimezoneDB db;
  auto tz = db.query("new_york");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name alone should match unique timezone, case insentively and "
          "without underscores",
          TAG) {
  TimezoneDB db;
  auto tz = db.query("new york");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name alone should match unique timezone, case insentively and "
          "without underscores, and with any amount of spaces",
          TAG) {
  TimezoneDB db;
  auto tz = db.query("   new     york ");

  REQUIRE(tz);
  REQUIRE(tz->name() == "America/New_York");
}

TEST_CASE("city name for a timezone with many /", TAG) {
  using SI = std::initializer_list<std::string_view>;

  TimezoneDB db;
  auto tz = db.query("buenos aires");

  REQUIRE(tz);
  REQUIRE(std::ranges::contains(SI{"America/Argentina/Buenos_Aires", "America/Buenos_Aires"}, tz->name()));
}

TEST_CASE("should handle famous european timezones", TAG) {
  TimezoneDB db;

  {
    auto tz = db.query("berlin");
    REQUIRE(tz);
    CHECK(tz->name() == "Europe/Berlin");
  }
  {
    auto tz = db.query("paris");
    REQUIRE(tz);
    CHECK(tz->name() == "Europe/Paris");
  }

  {
    auto tz = db.query("rome");
    REQUIRE(tz);
    CHECK(tz->name() == "Europe/Rome");
  }
}

TEST_CASE("Resolve timezone links", TAG) {
  TimezoneDB db;

  for (const auto &link : std::chrono::get_tzdb().links) {
    CAPTURE(link.name());
    auto tz = db.query(link.name());
    CHECK(tz);
    if (tz) { CHECK(tz->name() == link.target()); }
  }
}

TEST_CASE("resolve custom tz link", TAG) {
  TimezoneDB db;

  {
    auto tz = db.query("nyc");
    REQUIRE(tz);
    CHECK(tz->name() == "America/New_York");
  }

  {
    auto tz = db.query("europe");
    REQUIRE(tz);
    CHECK(tz->name() == "Europe/Paris");
  }
}
