#pragma once
#include "utils.hpp"
#include <string>
#include <chrono>

// Build the vocabulary of localized names for months and weekdays
class DateStringVocab {
public:
  struct MonthInfo {
    std::string name;
    std::chrono::month month;
  };

  struct WeekdayInfo {
    std::string name;
    std::chrono::weekday weekday;
  };

  using MonthMap = std::vector<MonthInfo>;
  using WeekdayMap = std::vector<WeekdayInfo>;

  DateStringVocab(const std::locale &locale = {}) {
    auto dflt = std::locale::classic();

    if (locale != dflt) {
      m_months.reserve(12 * 2);
      m_weekdays.reserve(12 * 2);
      for (const auto &l : {locale, dflt}) {
        generateMonthNames(m_months, l);
        generateWeekdays(m_weekdays, l);
      }
    } else {
      m_months.reserve(12);
      m_weekdays.reserve(12);
      generateMonthNames(m_months, dflt);
      generateWeekdays(m_weekdays, dflt);
    }
  }

  std::optional<std::chrono::month> asMonth(std::string_view str) const {
    auto it = std::ranges::find_if(m_months, [&](auto &&info) { return equalsIgnoreCase(info.name, str); });
    if (it == m_months.end()) return std::nullopt;
    return it->month;
  }

  std::optional<std::chrono::weekday> asWeekday(std::string_view str) const {
    auto it = std::ranges::find_if(m_weekdays, [&](auto &&info) { return equalsIgnoreCase(info.name, str); });
    if (it == m_weekdays.end()) return std::nullopt;
    return it->weekday;
  }

private:
  MonthMap generateMonthNames(MonthMap &map, std::locale locale = {}) {
    for (unsigned i = 0; i != 12; ++i) {
      std::chrono::month month{i + 1};
      auto add = [&](std::string str) { map.push_back({str, month}); };
      add(std::format(locale, "{:L%B}", month));
      add(std::format(locale, "{:L%b}", month));
    }

    return map;
  }

  WeekdayMap generateWeekdays(WeekdayMap &map, std::locale locale = {}) {
    for (unsigned i = 0; i != 6; ++i) {
      std::chrono::weekday weekday{i};
      auto add = [&](std::string str) { map.push_back({str, weekday}); };
      add(std::format(locale, "{:L%A}", weekday));
      add(std::format(locale, "{:L%a}", weekday));
    }

    return map;
  }

  WeekdayMap m_weekdays;
  MonthMap m_months;
};
