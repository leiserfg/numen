#pragma once
#include <chrono>
#include <string_view>

class TimezoneDB {
public:
  // search the timezone database in a relaxed way, e.g a query of "new york"
  // should yield "America/New_York"
  //
  // "new york" -> America/New_York
  // "paris" -> Europe/Paris
  // "new_york" -> America/New_York
  const std::chrono::time_zone *query(std::string_view query) const;
  const std::chrono::time_zone *userTz() const;
};
