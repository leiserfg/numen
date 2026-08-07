#include "abacus/abacus.hpp"
#include "abacus/unit.hpp"
#include "dummy-currency-provider.hpp"
#include "computed.hpp"
#include "parser.hpp"
#include "rang/rang.hpp"
#include "region-currency.hpp"
#include "timezone.hpp"
#include "utils.hpp"
#include <algorithm>
#include <bits/chrono.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <locale>
#include <memory>
#include <ostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace abacus {
using namespace abacus::detail;
bool isWholeNumber(double x) { return std::isfinite(x) && x == std::trunc(x); };


std::string formatDuration(const Duration &d) {
  using namespace std::chrono;
  auto y = d.years.value_or(years{0});
  auto m = d.months.value_or(months{0});
  auto s = d.seconds.value_or(seconds{0});

  std::ostringstream oss;

  auto years = y.count() + m.count() / 12;
  auto months = m % 12;
  auto days = s.count() / 86400;
  auto hours = s.count() % 86400 / 3600;
  auto minutes = s.count() % 3600 / 60;
  auto seconds = s.count() - days * 86400 - hours * 3600 - minutes * 60;

  if (years) {
    oss << years << " yr";
    if (years > 1) oss << "s";
  }
  if (months.count()) {
    if (!oss.str().empty()) oss << " ";
    oss << months.count() << " month";
    if (months.count() > 1) oss << "s";
  }
  if (days) {
    if (!oss.str().empty()) oss << " ";
    oss << days << " day";
    if (days > 1) oss << "s";
  }
  if (hours) {
    if (!oss.str().empty()) oss << " ";
    oss << hours << " hr";
  }
  if (minutes) {
    if (!oss.str().empty()) oss << " ";
    oss << minutes << " min";
  }
  if (seconds) {
    if (!oss.str().empty()) oss << " ";
    oss << seconds << " sec";
  }

  return oss.str();
}

Duration subtractDates(const DateTime &lhs, const DateTime &rhs) {
  using namespace std::chrono;

  auto lhsDays = floor<days>(lhs.time);
  auto rhsDays = floor<days>(rhs.time);

  // calendar difference a -> b, assuming a <= b
  auto amd = year_month_day{floor<days>(lhsDays)};
  auto bmd = year_month_day{floor<days>(rhsDays)};

  auto atod = lhs.time - lhsDays;
  auto btod = rhs.time - rhsDays;

  auto m = (bmd.year() / bmd.month()) - (amd.year() / amd.month()); // chrono::months, exact
  if (bmd.day() < amd.day()) --m;                                   // last month isn't complete yet

  auto anchor = amd + m; // same clamped month-shift you already have
  if (!anchor.ok()) anchor = anchor.year() / anchor.month() / last;

  auto d = sys_days{bmd} - sys_days{anchor}; // exact leftover days

  auto y = m / 12;
  auto mo = m % 12;

  seconds secs =
      duration_cast<seconds>(days{d}) - seconds{std::abs(duration_cast<seconds>(atod - btod).count())};

  return Duration{.years = std::chrono::years{std::abs(y.count())},
                  .months = std::chrono::months{std::abs(mo.count())},
                  .seconds = seconds{std::abs(secs.count())}};
}

template <typename T> TimePoint shift(TimePoint t, T duration) {
  if (!duration.count()) return t;

  auto time = std::chrono::floor<std::chrono::days>(t);
  std::chrono::year_month_day ymd{time};
  auto tod = t - time;
  ymd += duration;

  if (!ymd.ok()) ymd = ymd.year() / ymd.month() / std::chrono::last;

  auto point = std::chrono::sys_days{ymd} + tod;

  return point;
}

TimePoint parseDateTimeLiteral(const DateTimeLiteral &d, const std::chrono::time_zone &tz, TimePoint now) {
  std::chrono::year_month_day today{std::chrono::floor<std::chrono::days>(now)};

  auto process = [&](auto &&date) {
    std::chrono::local_seconds t{std::chrono::local_days{date}};

    if (auto time = d.time) {
      if (auto h = time->hours) t += *h;
      if (auto min = time->minutes) t += *min;
      if (auto secs = time->seconds) t += *secs;
    }
    return tz.to_sys(t);
  };

  return std::visit(
      [&](const auto &v) {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::chrono::weekday>) {
          std::chrono::year_month_weekday date{d.year.value_or(today.year()), d.month.value_or(today.month()),
                                               v[0]};
          return process(date);
        } else if constexpr (std::is_same_v<T, std::chrono::day>) {
          std::chrono::year_month_day date{d.year.value_or(today.year()), d.month.value_or(today.month()), v};
          return process(date);
        }
      },
      d.day.value_or(today.day()));
}

DateTime parseDateTime(const DateString &d, const std::chrono::time_zone &userTz, TimePoint now) {
  auto tz = d.timezone
                .and_then([](auto &&t) -> std::optional<const std::chrono::time_zone *> {
                  if (auto n = std::get_if<NamedTimezone>(&t)) { return TimezoneDB{}.query(n->name); }
                  return std::nullopt;
                })
                .value_or(&userTz);

  auto visitor = [&](const auto &value) -> TimePoint {
    using T = std::remove_cvref_t<decltype(value)>;
    if constexpr (std::is_same_v<T, DateTimeLiteral>) {
      return parseDateTimeLiteral(value, *tz, now);
    } else if constexpr (std::is_same_v<T, RelativeDateTimeLiteral>) {
      return std::visit(
          [&](auto &&anchor) {
            using A = std::remove_cvref_t<decltype(anchor)>;
            int sign = value.direction == RelativeDateTimeLiteral::Direction::Past ? -1 : 1;
            if constexpr (std::is_same_v<A, std::chrono::weekday>) {
              return now;
            } else {
              static_assert(std::is_same_v<A, Duration>);
              if (auto &y = anchor.years) now = shift<std::chrono::years>(now, sign * *y);
              if (auto &m = anchor.months) now = shift<std::chrono::months>(now, sign * *m);
              if (auto &s = anchor.seconds) now = now + sign * *s;
              return now;
            }
          },
          value.anchor);
    } else {
      return now;
    }
  };

  auto instant = std::visit(visitor, d.value);

  return DateTime{.time = instant, .tz = tz};
}

std::string formatDate(const DateTime &dt) {
  if (!dt.tz) { return std::format("{:%Y-%m-%d %H:%M:%S} (UTC)", dt.time); }

  const auto userTz = std::chrono::current_zone();
  const auto zt = userTz == dt.tz ? std::chrono::zoned_time{dt.tz, userTz->to_local(dt.time)}
                                  : std::chrono::zoned_time{dt.tz, dt.time};

  return std::format("{:%Y-%m-%d %H:%M:%OS} ({})", zt, dt.tz->name());
}

// money is only ever shown on its minor units, e.g. none for jpy and three for bhd
std::optional<int> unitDecimals(const detail::Num &v) {
  if (v.unit && v.unit->def && v.unit->def->dimension == dimensions::CURRENCY) {
    return currencyDigits(v.unit->def->id);
  }
  return std::nullopt;
}

struct FunctionCtx {
  template <typename... Ts> std::tuple<Ts...> unpack() {
    if (args.size() != sizeof...(Ts))
      throw std::runtime_error("expected " + std::to_string(sizeof...(Ts)) + " argument(s), got " +
                               std::to_string(args.size()));
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return std::tuple{(args[I].value)...};
    }(std::index_sequence_for<Ts...>{});
  }

  // unwraps all argument while asserting that they are indeed of type T
  template <typename T> std::vector<const T *> unpackAll() {
    std::vector<const T *> unpacked;
    for (const auto &arg : args) {
      if (auto n = std::get_if<T>(&arg.value)) {
        unpacked.emplace_back(n);
      } else {
        throw std::runtime_error("Found an argument with invalid type");
      }
    }

    return unpacked;
  }

  FunctionCtx(std::span<const Computed> args) : args(args) {}
  std::span<const Computed> args;
};

using FunctionHandler = std::function<Computed(FunctionCtx ctx)>;

struct FunctionDefinition {
  std::string_view name;
  int requiredArgs = 0;
  FunctionHandler fn;
};

class FunctionDatabase {
public:
  FunctionDatabase() {
    registerFunction("min", [&](FunctionCtx ctx) {
      if (ctx.args.empty()) throw std::runtime_error("min: at least 1 argument is required.");
      auto nn = ctx.unpackAll<Num>();
      auto min = std::ranges::min(nn, std::less{}, [](const Num *a) { return a->n; });

      return Computed{.value = *min};
    });

    registerFunction("max", [&](FunctionCtx ctx) {
      if (ctx.args.empty()) throw std::runtime_error("min: at least 1 argument is required.");
      auto nn = ctx.unpackAll<Num>();
      auto max = std::ranges::max(nn, std::less{}, [](const Num *a) { return a->n; });

      return Computed{.value = *max};
    });

    /*
registerFunction("sin", [&](FunctionCtx ctx) {
  auto [lhs] = ctx.unpack<double>();
  return Computed{.value = std::sin(lhs)};
});
    */
  }

  void registerFunction(std::string_view name, FunctionHandler handler) {
    m_fns.emplace_back(FunctionDefinition{.name = name, .fn = std::move(handler)});
  }

  void registerFunction(FunctionDefinition def) { m_fns.emplace_back(std::move(def)); }

  FunctionHandler *findFunction(std::string_view name) {
    auto it = std::ranges::find_if(m_fns, [&](auto &&fn) { return fn.name == name; });
    return it == m_fns.end() ? nullptr : &it->fn;
  }

private:
  std::vector<FunctionDefinition> m_fns;
};

struct OperationHandler {};

class Interpreter {
public:
  Interpreter(const UnitDatabase &db, const EvalConfig &opts)
      : m_db(db), m_opts(opts), m_now(opts.now.value_or(std::chrono::system_clock::now())) {}

  Computed computeExpr(const Expression &expr) const {
    auto visitor = [&](const auto &value) -> Computed {
      using T = std::remove_cvref_t<decltype(value)>;
      if constexpr (std::is_same_v<T, UnaryExpression>) {
        const auto &ue = value;
        auto c = computeExpr(*ue.lhs);

        if (auto n = c.asNumber()) {
          auto c = *n;
          if (ue.op == "-") { c.n = -c.n; }
          return {c};
        }

        return c;
      } else if constexpr (std::is_same_v<T, BinaryExpression>) {
        const auto &be = value;
        auto lhs = computeExpr(*be.lhs);
        auto rhs = computeExpr(*be.rhs);

        if (lhs.asNumber() && rhs.asNumber()) {
          auto nlhs = lhs.asNumber();
          auto nrhs = rhs.asNumber();
          if (nlhs->unit && nrhs->unit) {
            lhs = convertToUnit(lhs.asNumber()->n.toDouble(), nlhs->unit->raw, nrhs->unit->raw);
          }
        }

        if (be.op == "+") { return add(lhs, rhs); }
        if (be.op == "-") { return subtract(lhs, rhs); }
        if (be.op == "*") { return multiply(lhs, rhs); }
        if (be.op == "/") { return div(lhs, rhs); }
        if (be.op == "%") { return modulo(lhs, rhs); }
        if (be.op == "^") { return pow(lhs, rhs); }
        if (be.op == "<<") { return leftshift(lhs, rhs); }
        if (be.op == ">>") { return rightshift(lhs, rhs); }
        if (be.op == "&") { return bitwiseAnd(lhs, rhs); }
        if (be.op == "|") { return bitwiseor(lhs, rhs); }

        if (be.op == "==") { return Computed{.value = Boolean{lhs.value == rhs.value}}; }
        if (be.op == "!=") { return Computed{.value = Boolean{lhs.value != rhs.value}}; }
        if (be.op == ">") { return Computed{.value = Boolean{lhs.value > rhs.value}}; }
        if (be.op == ">=") { return Computed{.value = Boolean{lhs.value >= rhs.value}}; }
        if (be.op == "<") { return Computed{.value = Boolean{lhs.value < rhs.value}}; }
        if (be.op == "<=") { return Computed{.value = Boolean{lhs.value <= rhs.value}}; }

        throw std::runtime_error(std::format("Unhandled operator {}", be.op));
      } else if constexpr (std::is_same_v<T, ConversionExpression>) {
        const auto &conv = value;
        auto v = computeExpr(*conv.b);

        if (auto tzl = std::get_if<TimezoneLike>(&conv.target)) {
          if (!v.isDateTime())
            throw std::runtime_error("Only datetime expressions can be "
                                     "converted to another timezone");

          auto d = *v.asDateTime();

          if (auto ntz = std::get_if<NamedTimezone>(tzl)) {
            d.tz = TimezoneDB{}.query(ntz->name);
          } else if (auto otz = std::get_if<TimezoneOffset>(tzl)) {
            // d.tz = std::chrono::locate_zone(otz->name);
            d.offset = otz->offset;
          }

          return Computed{d};
        }

        if (v.isDateTime()) {
          if (auto unit = std::get_if<NamedUnit>(&conv.target)) {
            if (unit->name == "unix") {
              auto epoch = v.asDateTime()->time.time_since_epoch();
              auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
              return Computed{
                  Num{.n = Value{static_cast<double>(seconds)}, .unit = Number::Unit{.raw = "second"}}};
            }
          }
        }

        if (auto d = v.asDuration()) {
          if (auto unit = std::get_if<NamedUnit>(&conv.target)) {
            return convertToUnit(d->total().count(), "second", unit->name);
          }
        }

        if (auto n = v.asNumber()) {
          auto value = *n;

          if (auto fmt = std::get_if<NamedNumberFormat>(&conv.target)) {
            if (fmt->name == "hex" || fmt->name == "hexadecimal") {
              value.format = NumberOutputFormat::Hexadecimal;
              return Computed{value};
            }

            if (fmt->name == "binary") {
              value.format = NumberOutputFormat::Binary;
              return Computed{value};
            }

            if (fmt->name == "octal") {
              value.format = NumberOutputFormat::Octal;
              return Computed{value};
            }
          }

          if (auto unit = std::get_if<NamedUnit>(&conv.target)) {

            if (!n->unit) return Computed{.value = value};

            auto converted = convertToUnit(n->n.toDouble(), n->unit->raw, unit->name);
            return converted;
          }
        }
        throw std::runtime_error("unexpected conversion flow");
      } else if constexpr (std::is_same_v<T, NumberString>) {
        return Computed{.value = Num{value}};
      } else if constexpr (std::is_same_v<T, UnitExpression>) {
        const auto &ue = value;
        Computed c{.value = computeExpr(*ue.expr).value};

        // unit only makes sense for a number, ignore it otherwise
        if (auto n = c.asNumber()) {
          n->unit = Number::Unit{.raw = ue.unit};
          if (auto candidates = m_db.findUnitCandidates(ue.unit); candidates.size() == 1) {
            n->unit->def = candidates.front();
          }
        }

        return c;
      } else if constexpr (std::is_same_v<T, PercentExpression>) {
        auto c = computeExpr(*value.expr);

        if (auto n = c.asNumber()) {
          n->n = n->n / Value{100};
          n->isPercentage = true;
        }

        return c;
      } else if constexpr (std::is_same_v<T, FunctionCall>) {
        return executeFunction(value);
      } else if constexpr (std::is_same_v<T, Duration>) {
        return {value};
      } else {
        static_assert(std::is_same_v<T, DateString>);
        const auto &ds = value;
        auto &tz = m_opts.timezone ? *m_opts.timezone : *std::chrono::current_zone();
        auto dt = parseDateTime(ds, tz, m_now);
        return Computed{.value = dt};
      }
    };

    return std::visit(visitor, expr.data);
  }

  Computed computeExprBase(const Expression &expr) const {
    auto result = computeExpr(expr);

    if (auto n = result.asNumber(); n && n->unit && n->unit->def &&
                                    n->unit->def->dimension == dimensions::CURRENCY &&
                                    !n->explicitlyConverted) {
      auto target = abacus::currencyForLocale(m_opts.locale.value_or(std::locale{""}.name()));
      if (target && !equalsIgnoreCase(*target, n->unit->def->id)) {
        result = convertToUnit(result.asNumber()->n.toDouble(), n->unit->def->id, *target);
      }
    }

    return result;
  }

private:
  std::optional<Duration> promoteDuration(const Computed &v) const {
    if (auto dur = v.asDuration()) return *dur;
    auto nb = v.asNumber();
    if (!nb || !nb->unit) return std::nullopt;

    auto candidates = m_db.findUnitCandidates(nb->unit->raw);

    if (candidates.size() != 1) return std::nullopt;

    auto &unit = candidates[0];

    if (unit.dimension != dimensions::DURATION) return std::nullopt;

    Duration d;

    if (unit.id == "year")
      d.years = std::chrono::years{static_cast<int>(nb->n.toDouble())};
    else if (unit.id == "month")
      d.months = std::chrono::months{static_cast<int>(nb->n.toDouble())};
    else
      d.seconds = std::chrono::seconds{static_cast<int>(nb->n.toDouble() * unit.factor)};

    return d;
  }

  Computed convertToUnit(double v, std::string_view fromUnit, std::string_view toUnit) const {
    auto valueCandidates = m_db.findUnitCandidates(fromUnit);
    auto targetCandidates = m_db.findUnitCandidates(toUnit);

    if (valueCandidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", fromUnit)); }
    if (targetCandidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", toUnit)); }

    auto convert = [&](double n, const UnitDef &lhs, const UnitDef &rhs) -> Computed {
      if (lhs.dimension != rhs.dimension) {
        throw std::runtime_error(std::format("Incompatible units: {} ({}) to {} ({})", lhs.id, lhs.dimension,
                                             rhs.id, rhs.dimension));
      }

      auto res = m_db.convert(n, lhs, rhs);

      if (!res) throw std::runtime_error(res.error());

      return {
          .value = Num{.n = Value{res.value()},
                       .unit = Number::Unit{.raw = toUnit, .def = rhs},
                       .explicitlyConverted = true},
      };
    };

    // only one choice on both sides, there is no ambiguity
    if (valueCandidates.size() == 1 && targetCandidates.size() == 1) {
      auto lhs = valueCandidates.front();
      auto rhs = targetCandidates.front();
      return convert(v, lhs, rhs);
    }

    // we are unable to infer what unit should be used, we need to
    // wait for more info...
    if (valueCandidates.size() > 1 && targetCandidates.size() > 1) {
      return Computed{.value = Num{.n = Value{v},
                                   .unit = Number::Unit{.raw = toUnit},
                                   .explicitlyConverted = true}};
    }

    if (valueCandidates.size() > targetCandidates.size()) {
      auto rhs = targetCandidates.front();
      auto lhs = std::ranges::find_if(valueCandidates,
                                      [&](const UnitDef &unit) { return unit.dimension == rhs.dimension; });
      if (lhs == valueCandidates.end()) {
        throw std::runtime_error(std::format("Incompatible units: no common family"));
      }
      return convert(v, *lhs, rhs);
    }

    if (targetCandidates.size() > valueCandidates.size()) {
      auto lhs = valueCandidates.front();
      auto rhs = std::ranges::find_if(targetCandidates,
                                      [&](const UnitDef &unit) { return unit.dimension == lhs.dimension; });
      if (rhs == targetCandidates.end()) {
        throw std::runtime_error(std::format("Incompatible units: no common type"));
      }
      return convert(v, lhs, *rhs);
    }
    throw std::runtime_error("something bad happened");
  }

  template <typename T, typename U = T> static void assertBinary(const Computed &lhs, const Computed &rhs) {
    bool ok = std::holds_alternative<T>(lhs.value) && std::holds_alternative<U>(rhs.value);

    if (!ok) {
      throw std::runtime_error(
          std::format("Invalid operands: {} and {}", lhs.valueTypeName(), rhs.valueTypeName()));
    }
  }

  template <typename T, typename U>
  static std::optional<std::tuple<const T *, const U *>>
  getTypedOperands(const Computed &lhs, const Computed &rhs, bool swappable = false) {
    if (std::holds_alternative<T>(lhs.value) && std::holds_alternative<U>(rhs.value)) {
      return std::tuple<const T *, const U *>{std::get_if<T>(&lhs.value), std::get_if<U>(&rhs.value)};
    }

    if constexpr (std::is_same_v<T, U>) { return std::nullopt; }

    if (swappable) return getTypedOperands<T, U>(rhs, lhs, false);

    return std::nullopt;
  }

  Computed add(const Computed &lhs, const Computed &rhs) const {
    {
      auto dur1 = promoteDuration(lhs);
      auto dur2 = promoteDuration(rhs);
      if (dur1 && dur2) return Computed{*dur1 + *dur2};
    }

    if (rhs.isDateTime() && lhs.isNumber()) { return add(rhs, lhs); }
    if (lhs.asDuration() && rhs.asDateTime()) { return add(rhs, lhs); }

    if (auto ops = getTypedOperands<Duration, Duration>(lhs, rhs, true)) {
      auto [d1, d2] = *ops;
      return Computed{*d1 + *d2};
    }

    if (auto ops = getTypedOperands<DateTime, Duration>(lhs, rhs, true)) {
      auto [dt, dur] = *ops;
      auto result = *dt;

      if (auto y = dur->years) { result.time = shift(result.time, *y); }
      if (auto m = dur->months) { result.time = shift(result.time, *m); }
      if (auto s = dur->seconds) { result.time += *s; }

      return Computed{result};
    }

    if (auto ops = getTypedOperands<DateTime, Num>(lhs, rhs, true)) {
      auto [d, n] = *ops;

      if (n->unit) {
        auto candidates = m_db.findUnitCandidates(n->unit->raw);
        auto it = std::ranges::find_if(candidates,
                                       [](const UnitDef &u) { return u.dimension == dimensions::DURATION; });
        auto second = m_db.findUnit("second");

        if (it != candidates.end()) {
          if (it->id == "month") {
            DateTime dt = *d;
            dt.time = shift(dt.time, std::chrono::months{static_cast<int>(rhs.asNumber()->n.toDouble())});
            return Computed{.value = dt};
          }

          if (it->id == "year") {
            DateTime dt = *d;
            dt.time = shift(dt.time, std::chrono::years{static_cast<int>(rhs.asNumber()->n.toDouble())});
            return Computed{.value = dt};
          }

          // convert everything to seconds, then add it to time
          auto diff = m_db.convert(n->n.toDouble(), *it, *second);
          auto time = d->time + std::chrono::seconds(static_cast<int>(diff.value()));

          DateTime dt = *d;
          dt.time = time;
          return Computed{dt};
        }
      }
    }

    if (auto ops = getTypedOperands<Num, Num>(lhs, rhs, true)) {
      auto [n1, n2] = *ops;
      if (n2->isPercentage && !n1->isPercentage) { return output(n1->n + n1->n * n2->n, *n1, *n2); }
      return output(n1->n + n2->n, *n1, *n2);
    }

    throw std::runtime_error(std::format("Cannot add {} to {}", rhs.valueTypeName(), lhs.valueTypeName()));
  }

  Computed subtract(const Computed &lhs, const Computed &rhs) const {
    {
      auto dur1 = promoteDuration(lhs);
      auto dur2 = promoteDuration(rhs);
      if (dur1 && dur2) return Computed{*dur1 - *dur2};
    }

    if (lhs.isDateTime() && rhs.isDateTime()) {
      if (lhs.asDateTime()->time > rhs.asDateTime()->time) return subtract(rhs, lhs);
      return Computed{subtractDates(*lhs.asDateTime(), *rhs.asDateTime())};
    }

    if (lhs.isDateTime() && rhs.asDuration()) {
      auto dt = lhs.asDateTime();
      auto dur = rhs.asDuration();
      auto result = *dt;

      if (auto y = dur->years) result.time = shift(dt->time, -*y);
      if (auto m = dur->months) result.time = shift(dt->time, -*m);
      if (auto s = dur->seconds) result.time += -*s;

      return Computed{result};
    }

    if (lhs.asDuration() && rhs.asDuration()) { return Computed{{*lhs.asDuration() - *rhs.asDuration()}}; }

    assertBinary<Num, Num>(lhs, rhs);

    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();

    if (n2->isPercentage && !n1->isPercentage) { return output(n1->n - n1->n * n2->n, *n1, *n2); }

    return output(n1->n - n2->n, *n1, *n2);
  }

  static Computed multiply(const Computed &lhs, const Computed &rhs) {

    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n * rhs.asNumber()->n, *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed div(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    if (rhs.asNumber()->n.isZero()) throw std::runtime_error("Division by zero");
    return output(lhs.asNumber()->n / rhs.asNumber()->n, *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed modulo(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n.mod(rhs.asNumber()->n), *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed pow(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n.pow(rhs.asNumber()->n), *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed leftshift(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n << rhs.asNumber()->n, *lhs.asNumber(),
                  *rhs.asNumber());
  }

  static Computed rightshift(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n >> rhs.asNumber()->n, *lhs.asNumber(),
                  *rhs.asNumber());
  }

  static Computed bitwiseor(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n | rhs.asNumber()->n, *lhs.asNumber(),
                  *rhs.asNumber());
  }

  static Computed bitwiseAnd(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n & rhs.asNumber()->n, *lhs.asNumber(),
                  *rhs.asNumber());
  }

  static Computed output(Value n, const Num &lhs, const Num &rhs) {
    if (n.isNaN()) throw std::runtime_error("Result is undefined");

    auto result = Num{.n = n,
                      .format = lhs.format,
                      .unit = rhs.unit.or_else([&]() { return lhs.unit; }),
                      .explicitlyConverted = lhs.explicitlyConverted || rhs.explicitlyConverted};
    return Computed{.value = result};
  }

  Computed executeFunction(const FunctionCall &fn) const {
    FunctionDatabase db;

    auto computedArgs = fn.args | std::views::transform([&](auto &&expr) { return computeExpr(*expr); }) |
                        std::ranges::to<std::vector>();

    if (auto handler = db.findFunction(fn.name)) {
      FunctionCtx ctx{computedArgs};
      return (*handler)(ctx);
    } else {
      throw std::runtime_error(std::format("Unknown function \"{}\"", fn.name));
    }
  }

  const UnitDatabase &m_db;
  const EvalConfig &m_opts;
  TimePoint m_now;
};

static ComputedValue toPublic(const detail::Computed &c) {
  if (auto n = c.asNumber()) {
    return ComputedValue{.value = Number{.n = n->n.toDouble(),
                                         .text = n->n.render(n->format, unitDecimals(*n)),
                                         .format = n->format,
                                         .unit = n->unit,
                                         .explicitlyConverted = n->explicitlyConverted,
                                         .isPercentage = n->isPercentage}};
  }
  if (auto d = c.asDateTime()) return ComputedValue{.value = *d};
  if (auto d = c.asDuration()) return ComputedValue{.value = *d};
  return ComputedValue{.value = std::get<Boolean>(c.value)};
}

std::expected<ComputedValue, std::string> Abacus::compute(std::string_view expr, const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb, opts};

    return toPublic(i.computeExprBase(*ast.root));
  } catch (const std::exception &e) { return std::unexpected(e.what()); }
}

std::expected<std::string, std::string> Abacus::evaluate(const std::string_view expr,
                                                         const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb, opts};
    auto result = i.computeExprBase(*ast.root);

    const auto formatNumber = [](const Num &v) -> std::string {
      auto unitName = v.unit.transform([&](const Number::Unit &u) { return u.raw; }).value_or("");
      return std::format("{}{}", v.n.render(v.format, unitDecimals(v)), unitName);
    };

    auto visitor = [&](const auto &value) -> std::string {
      using T = std::remove_cvref_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Num>) {
        return formatNumber(value);
      } else if constexpr (std::is_same_v<T, DateTime>) {
        return formatDate(value);
      } else if constexpr (std::is_same_v<T, Duration>) {
        return formatDuration(value);
      } else {
        static_assert(std::is_same_v<T, Boolean>);
        return value.value ? "true" : "false";
      }
    };

    return std::format("{}", std::visit(visitor, result.value));
  } catch (const std::exception &e) { return std::unexpected(e.what()); }
}

static void printASTNode(std::ostream &os, const Expression &expr, int depth = 0) {
  auto ident = [&]() {
    std::string s;
    for (int i = 0; i != depth; ++i)
      s += "  ";
    return s;
  };

  std::visit(
      [&](const auto &value) {
        using T = std::remove_cvref_t<decltype(value)>;

        if constexpr (std::is_same_v<T, BinaryExpression>) {
          os << ident() << "Binary " << rang::fg::green << value.op << rang::fg::reset << " {\n";
          printASTNode(os, *value.lhs, depth + 1);
          printASTNode(os, *value.rhs, depth + 1);
          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, UnaryExpression>) {
          os << ident() << "Unary " << rang::fg::green << value.op << rang::fg::reset << " {\n";
          printASTNode(os, *value.lhs, depth + 1);
          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, ConversionExpression>) {
          auto visitor = [](const auto &value) -> std::string {
            using T = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<T, TimezoneLike>) {
              return std::visit([](const auto &tz) { return std::format("Timezone({})", tz.name); }, value);
            } else if constexpr (std::is_same_v<T, NamedUnit>) {
              return std::format("Unit({})", value.name);
            } else {
              static_assert(std::is_same_v<T, NamedNumberFormat>);
              return std::format("NumericFormat({})", value.name);
            }
          };

          os << ident() << "Convert " << rang::fg::green << std::visit(visitor, value.target)
             << rang::fg::reset << " {\n";

          printASTNode(os, *value.b, depth + 1);
          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, UnitExpression>) {
          os << ident() << "Unit " << rang::fg::green << value.unit << rang::fg::reset << " {\n";
          printASTNode(os, *value.expr, depth + 1);
          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, DateString>) {
          os << ident() << "Date " << " {\n";

          if (auto str = std::get_if<std::string_view>(&value.value)) {
            os << ident() << "\tvalue " << *str << "\n";
          }
          if (auto str = std::get_if<DateTimeLiteral>(&value.value)) {
            os << ident() << "\tvalue "
               << formatDate(parseDateTime({.value = *str, .timezone = value.timezone},
                                           *std::chrono::current_zone(), std::chrono::system_clock::now()))
               << "\n";
          }

          if (value.timezone) {
            auto v = [](const auto &tz) -> std::string {
              using T = std::remove_cvref_t<decltype(tz)>;
              if constexpr (std::is_same_v<T, TimezoneOffset>) {
                return std::format("Timezone({}+{})", tz.name, tz.offset.count());
              } else {
                static_assert(std::is_same_v<T, NamedTimezone>);
                return std::format("Timezone({})", tz.name);
              }
            };

            os << ident() << "\ttimezone " << std::visit(v, *value.timezone) << "\n";
          }

          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, NumberString>) {
          os << ident() << "Num " << rang::fg::yellow << value.render()
             << rang::fg::reset << "\n";
        } else if constexpr (std::is_same_v<T, FunctionCall>) {
          os << ident() << "Fn " << rang::fg::green << value.name << rang::fg::reset << " {\n";
          for (const auto &arg : value.args) {
            printASTNode(os, *arg, depth + 1);
          }
          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, Duration>) {
          os << ident() << "Duration " << rang::fg::green << value.total() << rang::fg::reset << "\n";
        }
      },
      expr.data);
}

void Abacus::printAST(const std::string &expr) const {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();
  printASTNode(std::cout, *ast.root, 0);
}

Abacus::Abacus() { setCurrencyProvider(std::make_unique<DummyCurrencyProvider>()); }

}; // namespace abacus
