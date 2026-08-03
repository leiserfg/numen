#include "abacus/abacus.hpp"
#include "abacus/unit.hpp"
#include "parser.hpp"
#include "rang/rang.hpp"
#include "timezone.hpp"
#include <algorithm>
#include <bits/chrono.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>

namespace abacus {

TimePoint shiftMonth(TimePoint t, int delta) {
  auto time = std::chrono::floor<std::chrono::days>(t);
  std::chrono::year_month_day ymd{time};
  auto tod = t - time;
  ymd += std::chrono::months(static_cast<int>(delta));

  auto point = std::chrono::sys_days{ymd} + tod;

  return point;
}

TimePoint shiftYear(TimePoint t, int delta) {
  auto time = std::chrono::floor<std::chrono::days>(t);
  std::chrono::year_month_day ymd{time};
  auto tod = t - time;
  ymd += std::chrono::years(static_cast<int>(delta));

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
    } else {
      return std::chrono::system_clock::now();
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

  FunctionCtx(std::span<const ComputedValue> args) : args(args) {}
  std::span<const ComputedValue> args;
};

using FunctionHandler = std::function<ComputedValue(FunctionCtx ctx)>;

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
      auto nn = ctx.unpackAll<Number>();
      auto min = std::ranges::min(nn, std::less{}, [](const Number *a) { return a->n; });

      return ComputedValue{.value = *min};
    });
    registerFunction("max", [&](FunctionCtx ctx) {
      if (ctx.args.empty()) throw std::runtime_error("min: at least 1 argument is required.");
      auto nn = ctx.unpackAll<Number>();
      auto max = std::ranges::max(nn, std::less{}, [](const Number *a) { return a->n; });

      return ComputedValue{.value = *max};
    });
    /*
registerFunction("sin", [&](FunctionCtx ctx) {
  auto [lhs] = ctx.unpack<double>();
  return ComputedValue{.value = std::sin(lhs)};
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
  Interpreter(const UnitDatabase &db, const EvalConfig &opts) : m_db(db), m_opts(opts) {}

  ComputedValue computeExpr(const Expression &expr) const {
    auto visitor = [&](const auto &value) -> ComputedValue {
      using T = std::remove_cvref_t<decltype(value)>;
      if constexpr (std::is_same_v<T, UnaryExpression>) {
        const auto &ue = value;
        auto c = computeExpr(*ue.lhs);

        if (auto n = c.asNumber()) {
          auto c = *n;
          if (ue.op == "-") { c.n *= -1; }
          return {c};
        }

        return c;
      } else if constexpr (std::is_same_v<T, BinaryExpression>) {
        const auto &be = value;
        auto lhs = computeExpr(*be.lhs);
        auto rhs = computeExpr(*be.rhs);

        if (lhs.unitRaw && rhs.unitRaw && lhs.asNumber() && rhs.asNumber()) {
          lhs = convertToUnit(lhs.asNumber()->n, *lhs.unitRaw, *rhs.unitRaw);
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

        if (be.op == "==") { return ComputedValue{.value = Boolean{lhs.value == rhs.value}}; }
        if (be.op == "!=") { return ComputedValue{.value = Boolean{lhs.value != rhs.value}}; }
        if (be.op == ">") { return ComputedValue{.value = Boolean{lhs.value > rhs.value}}; }
        if (be.op == ">=") { return ComputedValue{.value = Boolean{lhs.value >= rhs.value}}; }
        if (be.op == "<") { return ComputedValue{.value = Boolean{lhs.value < rhs.value}}; }
        if (be.op == "<=") { return ComputedValue{.value = Boolean{lhs.value <= rhs.value}}; }

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

          return ComputedValue{d};
        }

        if (auto n = v.asNumber()) {
          auto value = *n;

          if (auto fmt = std::get_if<NamedNumberFormat>(&conv.target)) {
            if (fmt->name == "hex" || fmt->name == "hexadecimal") {
              value.format = NumberOutputFormat::Hexadecimal;
              return ComputedValue{value};
            }

            if (fmt->name == "binary") {
              value.format = NumberOutputFormat::Binary;
              return ComputedValue{value};
            }

            if (fmt->name == "octal") {
              value.format = NumberOutputFormat::Octal;
              return ComputedValue{value};
            }
          }

          if (auto unit = std::get_if<NamedUnit>(&conv.target)) {
            // if converted expression has no unit there is nothing to do,
            // just tag it with the target unit... 1m to s 1m to in
            if (!v.unitRaw) return ComputedValue{.value = value, .unitRaw = unit->name};
            return convertToUnit(n->n, *v.unitRaw, unit->name);
          }
        }
        throw std::runtime_error("unexpected conversion flow");
      } else if constexpr (std::is_same_v<T, NumberString>) {
        return ComputedValue{.value = Number{value}};
      } else if constexpr (std::is_same_v<T, UnitExpression>) {
        const auto &ue = value;
        auto n = computeExpr(*ue.expr).value;

        // since we unitify the expression, we discard any unit the expr might
        // have had
        return ComputedValue{.value = n, .unitRaw = ue.unit};
      } else if constexpr (std::is_same_v<T, FunctionCall>) {
        return executeFunction(value);
      } else {
        static_assert(std::is_same_v<T, DateString>);
        const auto &ds = value;
        auto &tz = m_opts.timzone ? *m_opts.timzone : *std::chrono::current_zone();
        auto dt = parseDateTime(ds, tz, m_opts.now.value_or(std::chrono::system_clock::now()));
        return ComputedValue{.value = dt};
      }
    };

    return std::visit(visitor, expr.data);
  }

private:
  ComputedValue convertToUnit(double v, std::string_view fromUnit, std::string_view toUnit) const {
    auto valueCandidates = m_db.findUnitCandidates(fromUnit);
    auto targetCandidates = m_db.findUnitCandidates(toUnit);

    if (valueCandidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", fromUnit)); }
    if (targetCandidates.empty()) { throw std::runtime_error(std::format("Unknown unit \"{}\"", toUnit)); }

    auto convert = [&](double n, const UnitDef &lhs, const UnitDef &rhs) -> ComputedValue {
      if (lhs.dimension != rhs.dimension) {
        throw std::runtime_error(std::format("Incompatible units ({} to {})", lhs.id, rhs.id));
      }

      auto res = m_db.convert(n, lhs, rhs);

      if (!res) throw std::runtime_error(res.error());

      return {.value = Number{res.value()}, .unitRaw = toUnit};
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
      return ComputedValue{.value = Number{v}, .unitRaw = toUnit};
    }

    // 1s to 1m
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

  template <typename T, typename U = T>
  static void assertBinary(const ComputedValue &lhs, const ComputedValue &rhs) {
    bool ok = std::holds_alternative<T>(lhs.value) && std::holds_alternative<U>(rhs.value);
    if (!ok) throw std::runtime_error("Invalid operands");
  }

  ComputedValue add(const ComputedValue &lhs, const ComputedValue &rhs) const {
    if (rhs.isDateTime() && lhs.isNumber()) { return add(rhs, lhs); }

    // TODO: handle date + time.
    // We need a way to discriminate datetime from time alone, because
    // adding two dates together obviously makes no sense

    if (lhs.isDateTime() && rhs.isNumber()) {
      auto d = lhs.asDateTime();
      auto n = rhs.asNumber();

      if (rhs.unitRaw) {
        auto candidates = m_db.findUnitCandidates(rhs.unitRaw.value());
        auto it =
            std::ranges::find_if(candidates, [](const UnitDef &u) { return u.dimension == dimensions::DURATION; });
        auto second = m_db.findUnit("second");

        if (it != candidates.end()) {
          if (it->id == "month") {
            DateTime dt = *d;
            dt.time = shiftMonth(dt.time, static_cast<int>(rhs.asNumber()->n));
            return ComputedValue{.value = dt};
          }

          if (it->id == "year") {
            DateTime dt = *d;
            dt.time = shiftYear(dt.time, static_cast<int>(rhs.asNumber()->n));
            return ComputedValue{.value = dt};
          }

          // convert everything to seconds, then add it to time
          auto diff = m_db.convert(n->n, *it, *second);
          auto time = d->time + std::chrono::seconds(static_cast<int>(diff.value()));

          DateTime dt = *d;
          dt.time = time;
          return ComputedValue{dt};
        }
      }
    }

    assertBinary<Number, Number>(lhs, rhs);

    return output(lhs.asNumber()->n + rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue subtract(const ComputedValue &lhs, const ComputedValue &rhs) {
    if (lhs.isDateTime() && rhs.isDateTime()) {
      auto diff =
          std::chrono::duration_cast<std::chrono::seconds>(lhs.asDateTime()->time - rhs.asDateTime()->time);

      return ComputedValue{
          .value = Number{static_cast<double>(diff.count())},
          .unitRaw = "second",
      };
    }

    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n - rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue multiply(const ComputedValue &lhs, const ComputedValue &rhs) {

    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n * rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue div(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n / rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue modulo(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) % static_cast<int>(rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue pow(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(std::pow(lhs.asNumber()->n, rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue leftshift(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) << static_cast<int>(rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue rightshift(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) >> static_cast<int>(rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue bitwiseor(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) | static_cast<int>(rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue bitwiseAnd(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) & static_cast<int>(rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue output(double n, const ComputedValue &lhs, const ComputedValue &rhs) {
    return ComputedValue{.value = Number{n}, .unitRaw = rhs.unitRaw.or_else([&]() { return lhs.unitRaw; })};
  }

  ComputedValue executeFunction(const FunctionCall &fn) const {
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
};

std::expected<ComputedValue, std::string> Abacus::compute(std::string_view expr, const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb, opts};

    return i.computeExpr(*ast.root);
  } catch (const std::exception &e) { return std::unexpected(e.what()); }
}

std::expected<std::string, std::string> Abacus::evaluate(const std::string_view expr,
                                                         const EvalConfig &opts) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb, opts};
    auto result = i.computeExpr(*ast.root);

    const auto formatNumber = [](const Number &v) -> std::string {
      switch (v.format) {
      case abacus::NumberOutputFormat::Hexadecimal:
        return std::format("{:#x}", static_cast<int>(std::round(v.n)));
      case abacus::NumberOutputFormat::Octal:
        return std::format("{:#o}", static_cast<int>(std::round(v.n)));
      case abacus::NumberOutputFormat::Binary:
        return std::format("{:#b}", static_cast<int>(std::round(v.n)));
      default:
        return std::format("{:.6g}", v.n);
      };
    };

    auto visitor = [&](const auto &value) -> std::string {
      using T = std::remove_cvref_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Number>) {
        return formatNumber(value);
      } else if constexpr (std::is_same_v<T, DateTime>) {
        return formatDate(value);
      } else {
        static_assert(std::is_same_v<T, Boolean>);
        return value.value ? "true" : "false";
      }
    };

    return std::format("{}{}", std::visit(visitor, result.value), result.unitRaw.value_or(""));
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
          os << ident() << "Number " << rang::fg::yellow << value << rang::fg::reset << "\n";
        } else if constexpr (std::is_same_v<T, FunctionCall>) {
          os << ident() << "Fn " << rang::fg::green << value.name << rang::fg::reset << " {\n";
          for (const auto &arg : value.args) {
            printASTNode(os, *arg, depth + 1);
          }
          os << ident() << "}\n";
        }
      },
      expr.data);
}

void Abacus::printAST(const std::string &expr) const {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();
  printASTNode(std::cout, *ast.root, 0);
}

}; // namespace abacus
