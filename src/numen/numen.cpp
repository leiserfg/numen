#include "numen/numen.hpp"
#include "numen/unit.hpp"
#include "dummy-currency-provider.hpp"
#include "computed.hpp"
#include "parser.hpp"
#include "rang/rang.hpp"
#include "region-currency.hpp"
#include "timezone.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
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
#include <vector>

namespace numen {
using namespace numen::detail;
bool isWholeNumber(double x) { return std::isfinite(x) && x == std::trunc(x); };

namespace {
using namespace std::chrono;
constexpr auto perMinute = seconds{minutes{1}}.count();
constexpr auto perHour = seconds{hours{1}}.count();
constexpr auto perDay = seconds{days{1}}.count();
constexpr auto perWeek = seconds{weeks{1}}.count();
constexpr auto perMonth = seconds{months{1}}.count();

constexpr auto nanosPerSecond = nanoseconds{seconds{1}}.count();
constexpr auto nanosPerMilli = nanoseconds{milliseconds{1}}.count();
constexpr auto nanosPerMicro = nanoseconds{microseconds{1}}.count();
} // namespace

std::optional<Duration> durationFrom(double value, const UnitDef &unit) {
  if (unit.dimension != dimensions::DURATION) return std::nullopt;

  Duration d;

  if (unit.id == "year" || unit.id == "month") {
    auto asMonths = unit.id == "year" ? value * 12 : value;
    auto whole = std::trunc(asMonths);

    using MonthRep = std::chrono::months::rep;
    if (std::abs(whole) > static_cast<double>(std::numeric_limits<MonthRep>::max())) return std::nullopt;

    d.months = std::chrono::months{static_cast<MonthRep>(whole)};

    if (auto rest = asMonths - whole; rest != 0) {
      d.seconds = std::chrono::seconds{std::llround(rest * perMonth)};
    }

    return d;
  }

  auto asNanos = value * unit.factor * nanosPerSecond;
  if (asNanos != std::trunc(asNanos)) return std::nullopt;
  if (std::abs(asNanos) > static_cast<double>(std::numeric_limits<long long>::max())) return std::nullopt;

  auto nanos = static_cast<long long>(asNanos);
  d.seconds = std::chrono::seconds{nanos / nanosPerSecond};
  d.subsecond = std::chrono::nanoseconds{nanos % nanosPerSecond};

  return d.normalised();
}

Duration scaleDuration(const Duration &d, double factor) {
  using namespace std::chrono;

  auto months =
      (d.years.value_or(years{0}).count() * 12 + d.months.value_or(std::chrono::months{0}).count()) * factor;
  auto wholeMonths = std::trunc(months);

  auto secs = d.seconds.value_or(seconds{0}).count() * factor +
              d.subsecond.value_or(nanoseconds{0}).count() * factor / nanosPerSecond +
              (months - wholeMonths) * perMonth;
  auto wholeSecs = std::trunc(secs);

  Duration out;
  out.months = std::chrono::months{static_cast<std::chrono::months::rep>(wholeMonths)};
  out.seconds = seconds{static_cast<long long>(wholeSecs)};
  out.subsecond = nanoseconds{std::llround((secs - wholeSecs) * nanosPerSecond)};

  return out.normalised();
}

std::string formatDuration(const Duration &d) {
  using namespace std::chrono;

  auto calendar = d.years.value_or(years{0}).count() * 12 + d.months.value_or(months{0}).count();
  auto clock = d.seconds.value_or(seconds{0}).count();
  auto fraction = d.subsecond.value_or(nanoseconds{0}).count();

  std::vector<std::string> parts;

  const auto push = [&](long long n, std::string_view unit, bool plural) {
    if (n) parts.push_back(std::format("{} {}{}", n, unit, plural && n > 1 ? "s" : ""));
  };

  const auto group = [&](long long total, auto &&emit) {
    auto before = parts.size();
    emit(std::abs(total));
    if (total < 0 && parts.size() > before) parts[before].insert(0, "-");
  };

  group(calendar, [&](long long m) {
    push(m / 12, "yr", true);
    push(m % 12, "month", true);
  });

  // the two share one sign, so they are emitted as a single group
  group(clock != 0 ? clock : fraction, [&](long long) {
    auto s = std::abs(clock);
    auto ns = std::abs(fraction);

    push(s / perWeek, "week", true);
    push(s % perWeek / perDay, "day", true);
    push(s % perDay / perHour, "hr", false);
    push(s % perHour / perMinute, "min", false);
    push(s % perMinute, "sec", false);
    push(ns / nanosPerMilli, "ms", false);
    push(ns % nanosPerMilli / nanosPerMicro, "us", false);
    push(ns % nanosPerMicro, "ns", false);
  });

  if (parts.empty()) return "0 sec";

  std::ostringstream oss;
  for (std::size_t i = 0; i != parts.size(); ++i) {
    if (i) oss << " ";
    oss << parts[i];
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
                  return TimezoneDB{}.query(t.name);
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
              // TODO: implement weekday delta
              return now;
            } else {
              static_assert(std::is_same_v<A, Duration>);
              if (value.precision == DateTimePrecision::Date) {
                std::chrono::year_month_day date{std::chrono::floor<std::chrono::days>(now)};
                now = tz->to_sys(std::chrono::local_seconds{std::chrono::local_days{date}});
              }

              if (auto &y = anchor.years) now = shift<std::chrono::years>(now, sign * *y);
              if (auto &m = anchor.months) now = shift<std::chrono::months>(now, sign * *m);
              if (auto &s = anchor.seconds) now = now + sign * *s;
              return now;
            }
          },
          value.delta);
    } else {
      return now;
    }
  };

  auto instant = std::visit(visitor, d.value);

  return DateTime{
      .time = instant, .tz = tz, .offset = d.timezone ? d.timezone->offset : std::chrono::seconds{}};
}

std::string formatDate(const DateTime &dt) {
  if (!dt.tz) { return std::format("{:%Y-%m-%d %H:%M:%S} (UTC)", dt.time); }

  const auto instant = dt.time + dt.offset;
  const auto userTz = std::chrono::current_zone();
  const auto zt = userTz == dt.tz ? std::chrono::zoned_time{dt.tz, userTz->to_local(instant)}
                                  : std::chrono::zoned_time{dt.tz, instant};

  if (dt.offset.count() != 0) {
    auto hours = dt.offset.count() / 3600;
    auto minutes = std::abs((dt.offset.count() - hours * 3600) / 60);

    if (minutes != 0) {
      return std::format("{:%Y-%m-%d %H:%M:%OS} ({}{:+}:{})", zt, dt.tz->name(), hours, minutes);
    } else {
      return std::format("{:%Y-%m-%d %H:%M:%OS} ({}{:+})", zt, dt.tz->name(), hours);
    }
  }

  return std::format("{:%Y-%m-%d %H:%M:%OS} ({})", zt, dt.tz->name());
}

// money is only ever shown on its minor units, e.g. none for jpy and three for bhd
std::optional<int> unitDecimals(const detail::Num &v) {
  if (v.unit && v.unit->def() && v.unit->def()->dimension == dimensions::CURRENCY) {
    return currencyDigits(v.unit->def()->id);
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

          // converting years into months would floor the fraction, and adding
          // durations does not need a common unit to begin with
          bool durationSum = (be.op == "+" || be.op == "-") && promoteDuration(lhs) && promoteDuration(rhs);

          if (!durationSum) { reconcileUnits(lhs, rhs, be.op); }
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
        auto v = computeExpr(*conv.lhs);

        if (v.isDateTime()) {
          if (auto tz = conv.target.tz) {
            auto d = *v.asDateTime();

            d.tz = TimezoneDB{}.query(tz->name);
            d.offset = tz->offset;

            return Computed{d};
          }

          if (auto unit = conv.target.unit; unit && unit->isSimple()) {
            if (std::ranges::contains(std::initializer_list<std::string_view>{"unix", "epoch"},
                                      unit->simpleName())) {
              auto epoch = v.asDateTime()->time.time_since_epoch();
              auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
              return Computed{Num{.n = Value{static_cast<double>(seconds)},
                                  .unit = Number::Unit{.raw = "second"},
                                  .explicitlyConverted = true}};
            }
          }
        }

        if (auto d = v.asDuration()) {
          if (auto unit = conv.target.unit; unit && unit->isSimple()) {
            return convertToUnit(d->total().count(), "second", unit->simpleName());
          }
        }

        if (auto n = v.asNumber()) {
          auto value = *n;

          if (auto fmt = conv.target.fmt) {
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

          if (auto unit = conv.target.unit) {
            if (!n->unit) return Computed{.value = value};

            // a single token can still name a composition, so ask what it
            // resolved to rather than how it was spelled
            auto target = buildTarget(*unit);
            bool plainTarget = unit->isSimple() && target.sole();
            bool plainSource = !n->unit->resolved || n->unit->resolved->sole();

            // the plain path is the only one that knows about offsets, and the
            // only one that can settle an ambiguous token against its target
            if (plainTarget && plainSource) {
              return convertToUnit(n->n.toDouble(), n->unit->raw, unit->simpleName());
            }

            return convertCompound(*n, std::move(target));
          }
        }
        if (auto unit = conv.target.unit) {
          throw std::runtime_error(
              std::format("Cannot convert a {} to {}", v.valueTypeName(),
                          unit->isSimple() ? std::string{unit->simpleName()} : buildTarget(*unit).render()));
        }

        throw std::runtime_error(std::format("Cannot convert a {} to that", v.valueTypeName()));
      } else if constexpr (std::is_same_v<T, NumberString>) {
        return Computed{.value = Num{value}};
      } else if constexpr (std::is_same_v<T, UnitExpression>) {
        const auto &ue = value;
        Computed c{.value = computeExpr(*ue.expr).value};

        // unit only makes sense for a number, ignore it otherwise
        if (auto n = c.asNumber()) {
          n->unit = Number::Unit{.raw = std::string{ue.unit}};
          if (auto candidates = m_db.findCompounds(ue.unit); candidates.size() == 1) {
            n->unit->resolved = std::move(candidates.front());
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

    if (auto n = result.asNumber(); n && n->unit && n->unit->def() &&
                                    n->unit->def()->dimension == dimensions::CURRENCY &&
                                    !n->explicitlyConverted) {
      auto target = numen::currencyForLocale(m_opts.locale.value_or(std::locale{""}.name()));
      if (target && !equalsIgnoreCase(*target, n->unit->def()->id)) {
        result = convertToUnit(result.asNumber()->n.toDouble(), n->unit->def()->id, *target);
      }
    }

    if (auto n = result.asNumber(); n && !n->explicitlyConverted) {
      if (auto d = foldToDuration(*n)) return Computed{.value = *d};
    }

    return result;
  }

private:
  std::optional<Duration> foldToDuration(const Num &n) const {
    if (!n.unit) return std::nullopt;

    // the settled reading beats re-deriving one, which may still be ambiguous
    if (auto def = n.unit->def()) return durationFrom(n.n.toDouble(), *def);

    auto candidates = m_db.findUnitCandidates(n.unit->raw);
    if (candidates.size() != 1) return std::nullopt;

    return durationFrom(n.n.toDouble(), candidates.front());
  }

  std::optional<Duration> promoteDuration(const Computed &v) const {
    if (auto dur = v.asDuration()) return *dur;
    if (auto n = v.asNumber()) return foldToDuration(*n);

    return std::nullopt;
  }

  // the unambiguous side decides the other: in "1m to s" the second operand is
  // what makes "m" a minute. nullopt when both are ambiguous
  std::optional<std::pair<UnitDef, UnitDef>> resolvePair(std::string_view fromUnit,
                                                         std::string_view toUnit) const {
    auto valueCandidates = m_db.findUnitCandidates(fromUnit);
    auto targetCandidates = m_db.findUnitCandidates(toUnit);

    if (valueCandidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", fromUnit)); }
    if (targetCandidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", toUnit)); }

    if (valueCandidates.size() == 1 && targetCandidates.size() == 1) {
      return std::pair{valueCandidates.front(), targetCandidates.front()};
    }

    if (valueCandidates.size() > 1 && targetCandidates.size() > 1) { return std::nullopt; }

    if (valueCandidates.size() > targetCandidates.size()) {
      auto rhs = targetCandidates.front();
      auto lhs = std::ranges::find_if(valueCandidates,
                                      [&](const UnitDef &unit) { return unit.dimension == rhs.dimension; });
      if (lhs == valueCandidates.end()) {
        throw std::runtime_error(std::format("Incompatible units: no common family"));
      }
      return std::pair{*lhs, rhs};
    }

    auto lhs = valueCandidates.front();
    auto rhs = std::ranges::find_if(targetCandidates,
                                    [&](const UnitDef &unit) { return unit.dimension == lhs.dimension; });
    if (rhs == targetCandidates.end()) {
      throw std::runtime_error(std::format("Incompatible units: no common type"));
    }
    return std::pair{lhs, *rhs};
  }

  // callers holding both readings must come here: convertToUnit rediscovers them
  // from the tokens and cannot when both are ambiguous
  Computed convertResolved(double v, const UnitDef &from, const UnitDef &to, std::string display) const {
    if (from.dimension != to.dimension) {
      throw std::runtime_error(std::format("Incompatible units: {} ({}) to {} ({})", from.id, from.dimension,
                                           to.id, to.dimension));
    }

    auto res = m_db.convert(v, from, to);

    if (!res) throw std::runtime_error(res.error());

    return {
        .value = Num{.n = Value{res.value()},
                     .unit = Number::Unit{.raw = display, .resolved = soleUnit(to, display)},
                     .explicitlyConverted = true},
    };
  }

  CompoundUnit buildTarget(const NamedUnit &named) const {
    std::vector<UnitTerm> terms;

    for (const auto &named_term : named.terms) {
      auto candidates = m_db.findCompounds(named_term.name);
      if (candidates.empty()) {
        throw std::runtime_error(std::format("Unknown unit \"{}\"", named_term.name));
      }

      // the named term may itself be a composition, as in "to kmh"
      for (const auto &part : candidates.front().terms) {
        auto exponent = static_cast<std::int8_t>(part.exponent * named_term.exponent);
        auto known = std::ranges::find_if(terms, [&](const UnitTerm &x) { return x.def.id == part.def.id; });

        if (known == terms.end()) {
          terms.push_back(UnitTerm{.def = part.def, .display = part.display, .exponent = exponent});
        } else {
          known->exponent = static_cast<std::int8_t>(known->exponent + exponent);
        }
      }
    }

    std::erase_if(terms, [](const UnitTerm &term) { return term.exponent == 0; });
    return CompoundUnit{std::move(terms)};
  }

  Computed convertCompound(const Num &n, CompoundUnit target) const {
    Num source = n;
    resolveToDefault(source);
    const auto &from = *source.unit->resolved;

    // Allow implict conversion such as "150 km/h to in", by promoting rhs to in/h
    if (!from.sole() && target.sole()) {
      auto convertible =
          dimensionOf(from.terms[0].def.dimension) == dimensionOf(target.terms[0].def.dimension);

      if (from.terms[0].def.dimension == target.terms[0].def.dimension) {
        target.terms.insert(target.terms.end(), from.terms.begin() + 1, from.terms.end());
      }
    }

    if (from.dimension() != target.dimension()) {
      throw std::runtime_error(std::format("Incompatible units: {} to {}", from.render(), target.render()));
    }

    validateTerms(from.terms);
    validateTerms(target.terms);

    auto ratio = m_db.conversionRatio(from, target);
    if (!ratio) throw std::runtime_error(ratio.error());

    auto display = target.render();

    return Computed{.value = Num{.n = Value{n.n.toDouble() * *ratio},
                                 .unit = Number::Unit{.raw = display, .resolved = std::move(target)},
                                 .explicitlyConverted = true}};
  }

  Computed convertToCompound(const Num &n, const NamedUnit &named) const {
    return convertCompound(n, buildTarget(named));
  }

  Computed convertToUnit(double v, std::string_view fromUnit, std::string_view toUnit) const {
    auto pair = resolvePair(fromUnit, toUnit);

    // we are unable to infer what unit should be used, we need to
    // wait for more info...
    if (!pair) {
      return Computed{.value = Num{.n = Value{v},
                                   .unit = Number::Unit{.raw = std::string{toUnit}},
                                   .explicitlyConverted = true}};
    }

    return convertResolved(v, pair->first, pair->second, std::string{toUnit});
  }

  static bool composes(std::string_view op) { return op == "*" || op == "/"; }

  void resolveToDefault(Num &n) const {
    if (n.unit->resolved) return;

    auto candidates = m_db.findCompounds(n.unit->raw);
    if (candidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", n.unit->raw)); }
    n.unit->resolved = std::move(candidates.front());
  }

  static bool isComposed(const Number::Unit &unit) { return unit.resolved && !unit.resolved->sole(); }

  void reconcileCompounds(Computed &lhs, Computed &rhs) const {
    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();

    resolveToDefault(*n1);
    resolveToDefault(*n2);

    const auto &ca = *n1->unit->resolved;
    const auto &cb = *n2->unit->resolved;

    bool keepLhs = ca.hasStableFactor() && cb.hasStableFactor() && ca.factor() > cb.factor();

    if (keepLhs) {
      rhs = convertCompound(*n2, ca);
    } else {
      lhs = convertCompound(*n1, cb);
    }
  }

  void reconcileUnits(Computed &lhs, Computed &rhs, std::string_view op) const {
    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();
    if (!n1 || !n2) return;

    // each side reads on its own terms: "2 m * 3 s" must not let the second
    // operand turn the metre into a minute
    if (composes(op) || op == "^") {
      if (n1->unit) resolveToDefault(*n1);
      if (n2->unit) resolveToDefault(*n2);
    }

    if (!n1->unit || !n2->unit) return;

    UnitDef a, b;

    if (composes(op)) {
      auto da = n1->unit->def();
      auto db = n2->unit->def();
      if (!da || !db || da->dimension != db->dimension) return;
      a = *da;
      b = *db;
    } else {
      // a composed unit renders its name, so "km/h" is not in the table
      if (isComposed(*n1->unit) || isComposed(*n2->unit)) {
        reconcileCompounds(lhs, rhs);
        return;
      }

      auto pair = resolvePair(n1->unit->raw, n2->unit->raw);
      if (!pair) return;
      a = pair->first;
      b = pair->second;
    }

    // larger wins, so "1 km + 100 m" reads as 1.1km. a factor that moves has no
    // size to rank by, so there the right-hand side decides
    bool keepLhs = !traitsOf(a.dimension).dynamicFactor && a.factor > b.factor;

    // the kept side holds the settled reading, so "1m + 30s" still knows its
    // "m" is a minute once the other side is gone
    if (keepLhs) {
      n1->unit->resolved = soleUnit(a, n1->unit->raw);
      rhs = convertResolved(n2->n.toDouble(), b, a, n1->unit->raw);
    } else {
      n2->unit->resolved = soleUnit(b, n2->unit->raw);
      lhs = convertResolved(n1->n.toDouble(), a, b, n2->unit->raw);
    }
  }

  static void validateTerms(const std::vector<UnitTerm> &terms) {
    for (const auto &term : terms) {
      // 0°C is a point on a scale, not a quantity that can be multiplied out
      if (term.def.offset != 0 && (terms.size() > 1 || term.exponent != 1)) {
        throw std::runtime_error(std::format("Cannot build a compound unit out of {}", term.def.id));
      }

      if (compositionOf(term.def.dimension) != Composition::RateOnly) continue;

      // "usd/kg" and "km/usd" mean something, "usd·kg" and "usd²" do not
      bool alone = std::ranges::none_of(terms, [&](const UnitTerm &other) {
        return other.def.id != term.def.id && (other.exponent > 0) == (term.exponent > 0);
      });

      if (std::abs(term.exponent) != 1 || !alone) {
        throw std::runtime_error(
            std::format("{} can only be combined with other units as a rate", term.def.id));
      }
    }
  }

  // nullopt once everything cancels, which is what makes "1 km / 100 m" plain
  static std::optional<Number::Unit> composeUnits(const Num &n1, const Num &n2, int sign) {
    std::vector<UnitTerm> terms;

    auto merge = [&](const Number::Unit &unit, int s) {
      if (!unit.resolved) { throw std::runtime_error(std::format("Unknown unit \"{}\"", unit.raw)); }

      for (const auto &term : unit.resolved->terms) {
        auto exponent = static_cast<std::int8_t>(term.exponent * s);
        auto known = std::ranges::find_if(terms, [&](const UnitTerm &x) { return x.def.id == term.def.id; });

        if (known == terms.end()) {
          terms.push_back(UnitTerm{.def = term.def, .display = term.display, .exponent = exponent});
        } else {
          known->exponent = static_cast<std::int8_t>(known->exponent + exponent);
        }
      }
    };

    if (n1.unit) merge(*n1.unit, 1);
    if (n2.unit) merge(*n2.unit, sign);

    std::erase_if(terms, [](const UnitTerm &term) { return term.exponent == 0; });
    if (terms.empty()) return std::nullopt;

    validateTerms(terms);

    CompoundUnit compound{std::move(terms)};
    return Number::Unit{.raw = compound.render(), .resolved = compound};
  }

  template <typename T, typename U = T> static void assertBinary(const Computed &lhs, const Computed &rhs) {
    bool ok = std::holds_alternative<T>(lhs.value) && std::holds_alternative<U>(rhs.value);

    if (!ok) {
      throw std::runtime_error(
          std::format("Invalid operands: {} and {}", lhs.valueTypeName(), rhs.valueTypeName()));
    }
  }

  // swappable should be set to true for commutative operators
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
      if (dur1 && dur2) { return {*dur1 + *dur2}; }
    }

    if (auto ops = getTypedOperands<DateTime, Duration>(lhs, rhs, true)) {
      auto [dt, dur] = *ops;
      auto result = *dt;

      if (auto y = dur->years) { result.time = shift(result.time, *y); }
      if (auto m = dur->months) { result.time = shift(result.time, *m); }
      if (auto s = dur->seconds) { result.time += *s; }
      if (auto ns = dur->subsecond) { result.time += *ns; }

      return Computed{result};
    }

    if (auto ops = getTypedOperands<DateTime, Num>(lhs, rhs, true)) {
      auto [d, n] = *ops;
      if (auto dur = foldToDuration(*n)) return add(Computed{.value = *d}, Computed{.value = *dur});
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

      if (auto y = dur->years) result.time = shift(result.time, -*y);
      if (auto m = dur->months) result.time = shift(result.time, -*m);
      if (auto s = dur->seconds) result.time += -*s;
      if (auto ns = dur->subsecond) result.time += -*ns;

      return Computed{result};
    }

    if (lhs.asDuration() && rhs.asDuration()) { return Computed{{*lhs.asDuration() - *rhs.asDuration()}}; }

    if (auto ops = getTypedOperands<DateTime, Num>(lhs, rhs)) {
      auto [d, n] = *ops;
      if (auto dur = foldToDuration(*n)) return subtract(Computed{.value = *d}, Computed{.value = *dur});
    }

    assertBinary<Num, Num>(lhs, rhs);

    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();

    if (n2->isPercentage && !n1->isPercentage) { return output(n1->n - n1->n * n2->n, *n1, *n2); }

    return output(n1->n - n2->n, *n1, *n2);
  }

  static Computed multiply(const Computed &lhs, const Computed &rhs) {
    if (auto ops = getTypedOperands<Duration, Num>(lhs, rhs, true)) {
      auto [dur, n] = *ops;
      if (!n->unit) return Computed{.value = scaleDuration(*dur, n->n.toDouble())};
    }

    assertBinary<Num, Num>(lhs, rhs);

    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();

    return output(n1->n * n2->n, *n1, *n2, composeUnits(*n1, *n2, 1));
  }

  static Computed div(const Computed &lhs, const Computed &rhs) {
    if (auto ops = getTypedOperands<Duration, Num>(lhs, rhs)) {
      auto [dur, n] = *ops;
      if (!n->unit) {
        if (n->n.isZero()) throw std::runtime_error("Division by zero");
        return Computed{.value = scaleDuration(*dur, 1.0 / n->n.toDouble())};
      }
    }

    assertBinary<Num, Num>(lhs, rhs);
    if (rhs.asNumber()->n.isZero()) throw std::runtime_error("Division by zero");

    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();

    return output(n1->n / n2->n, *n1, *n2, composeUnits(*n1, *n2, -1));
  }

  static Computed modulo(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n.mod(rhs.asNumber()->n), *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed pow(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    auto n1 = lhs.asNumber();
    auto n2 = rhs.asNumber();
    auto raised = n1->n.pow(n2->n);

    if (!n1->unit || n2->n == Value{1}) return output(raised, *n1, *n2);
    if (n2->n.isZero()) return output(raised, *n1, *n2, std::nullopt);

    auto exponent = n2->n.toDouble();
    // a fractional power would need a root of the dimension, and exponents are
    // stored in a byte
    if (exponent != std::trunc(exponent) || std::abs(exponent) > 9) {
      throw std::runtime_error("Cannot raise a unit to that power");
    }

    if (!n1->unit->resolved) { throw std::runtime_error(std::format("Unknown unit \"{}\"", n1->unit->raw)); }

    auto terms = n1->unit->resolved->terms;

    for (auto &term : terms) {
      term.exponent = static_cast<std::int8_t>(term.exponent * static_cast<int>(exponent));
    }

    validateTerms(terms);

    CompoundUnit compound{std::move(terms)};
    return output(raised, *n1, *n2, Number::Unit{.raw = compound.render(), .resolved = compound});
  }

  static Computed leftshift(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n << rhs.asNumber()->n, *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed rightshift(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n >> rhs.asNumber()->n, *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed bitwiseor(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n | rhs.asNumber()->n, *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed bitwiseAnd(const Computed &lhs, const Computed &rhs) {
    assertBinary<Num, Num>(lhs, rhs);
    return output(lhs.asNumber()->n & rhs.asNumber()->n, *lhs.asNumber(), *rhs.asNumber());
  }

  static Computed output(Value n, const Num &lhs, const Num &rhs) {
    return output(n, lhs, rhs, rhs.unit.or_else([&]() { return lhs.unit; }));
  }

  static Computed output(Value n, const Num &lhs, const Num &rhs, std::optional<Number::Unit> unit) {
    if (n.isNaN()) throw std::runtime_error("Result is undefined");

    auto result = Num{.n = n,
                      .format = lhs.format,
                      .unit = std::move(unit),
                      .explicitlyConverted = lhs.explicitlyConverted || rhs.explicitlyConverted};
    return Computed{.value = result};
  }

  // arguments are computed independently, so without this "min(1 km, 999 m)"
  // compares the bare numbers and answers 1 km
  void reconcileArguments(std::vector<Computed> &args) const {
    if (args.size() < 2) return;

    for (auto &arg : args) {
      auto n = arg.asNumber();
      // a plain number among them, as in "max(1 km to m, 100)"
      if (!n || !n->unit) return;
      resolveToDefault(*n);
    }

    const CompoundUnit *target = &*args.front().asNumber()->unit->resolved;

    bool rankable = true;

    for (auto &arg : args) {
      const auto &unit = *arg.asNumber()->unit->resolved;

      if (unit.dimension() != target->dimension()) {
        throw std::runtime_error(
            std::format("Incompatible units: {} to {}", unit.render(), target->render()));
      }

      rankable = rankable && unit.hasStableFactor();
    }

    if (rankable) {
      for (auto &arg : args) {
        const auto &unit = *arg.asNumber()->unit->resolved;
        if (unit.factor() > target->factor()) target = &unit;
      }
    }

    auto chosen = *target;

    for (auto &arg : args) {
      auto n = arg.asNumber();
      auto from = n->unit->resolved->sole();
      auto to = chosen.sole();

      // only the plain path knows about offsets, which affine units need
      if (from && to) {
        arg = convertResolved(n->n.toDouble(), *from, *to, chosen.render());
      } else {
        arg = convertCompound(*n, chosen);
      }
    }
  }

  Computed executeFunction(const FunctionCall &fn) const {
    FunctionDatabase db;

    auto computedArgs = fn.args | std::views::transform([&](auto &&expr) { return computeExpr(*expr); }) |
                        std::ranges::to<std::vector>();

    reconcileArguments(computedArgs);

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

std::expected<ComputedValue, std::string> Numen::compute(std::string_view expr, const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb, opts};

    return toPublic(i.computeExprBase(*ast.root));
  } catch (const std::exception &e) { return std::unexpected(e.what()); }
}

std::expected<std::string, std::string> Numen::evaluate(const std::string_view expr, const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb, opts};
    auto result = i.computeExprBase(*ast.root);

    const auto formatNumber = [](const Num &v) -> std::string {
      auto unitName =
          v.unit.transform([&](const Number::Unit &u) { return u.resolved ? u.resolved->render() : u.raw; })
              .value_or("");
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
            if constexpr (std::is_same_v<T, TimezoneOffset>) {
              return std::format("Timezone({})", value.name);
            } else if constexpr (std::is_same_v<T, NamedUnit>) {
              std::string name;
              for (const auto &term : value.terms) {
                if (!name.empty()) name += term.exponent < 0 ? "/" : "*";
                name += term.name;
              }
              return std::format("Unit({})", name);
            } else {
              static_assert(std::is_same_v<T, NamedNumberFormat>);
              return std::format("NumericFormat({})", value.name);
            }
          };

          /*
  os << ident() << "Convert " << rang::fg::green << std::visit(visitor, value.target)
     << rang::fg::reset << " {\n";
                 */

          printASTNode(os, *value.lhs, depth + 1);
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

          if (value.timezone) { os << ident() << "\ttimezone " << value.timezone->name << "\n"; }

          os << ident() << "}\n";
        } else if constexpr (std::is_same_v<T, NumberString>) {
          os << ident() << "Num " << rang::fg::yellow << value.render() << rang::fg::reset << "\n";
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

void Numen::printAST(const std::string &expr) const {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();

  printASTNode(std::cout, *ast.root, 0);
}

Numen::Numen() { setCurrencyProvider(std::make_unique<DummyCurrencyProvider>()); }

}; // namespace numen
