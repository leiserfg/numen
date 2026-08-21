#pragma once
#include "numen/tz.hpp"
#include <chrono>
#include <span>
#include <string_view>

struct CustomTzLink {
  std::string_view name;
  std::string_view target;
};

class TimezoneDB {
public:
  // abbreviations (pst), IANA names (Europe/Paris), then GeoNames places, optionally
  // qualified ("paris tx"). A bare name yields the most populous place called that
  const numen::tz::time_zone *query(std::string_view query) const;
  const numen::tz::time_zone *userTz() const;

  // take precedence over tzdb, so EST is New York rather than the fixed offset link
  static std::span<const CustomTzLink> customLinks();

  // the IANA name, whatever the tzdb calls the zone
  static std::string_view canonicalName(const numen::tz::time_zone &tz);
};
