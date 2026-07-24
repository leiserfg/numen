#include "abacus/abacus.hpp"
#include "abacus/unit.hpp"
#include "parser.hpp"
#include "rang/rang.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace abacus {

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};

struct FunctionCtx {
  template <typename... Ts> std::tuple<Ts...> unpack() {
    if (args.size() != sizeof...(Ts))
      throw std::runtime_error("expected " + std::to_string(sizeof...(Ts)) +
                               " argument(s), got " +
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
      if (ctx.args.empty())
        throw std::runtime_error("min: at least 1 argument is required.");
      auto nn = ctx.unpackAll<Number>();
      auto min = std::ranges::min(nn, std::less{},
                                  [](const Number *a) { return a->n; });

      return ComputedValue{.value = *min};
    });
    registerFunction("max", [&](FunctionCtx ctx) {
      if (ctx.args.empty())
        throw std::runtime_error("min: at least 1 argument is required.");
      auto nn = ctx.unpackAll<Number>();
      auto max = std::ranges::max(nn, std::less{},
                                  [](const Number *a) { return a->n; });

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
    m_fns.emplace_back(
        FunctionDefinition{.name = name, .fn = std::move(handler)});
  }

  void registerFunction(FunctionDefinition def) {
    m_fns.emplace_back(std::move(def));
  }

  FunctionHandler *findFunction(std::string_view name) {
    auto it =
        std::ranges::find_if(m_fns, [&](auto &&fn) { return fn.name == name; });
    return it == m_fns.end() ? nullptr : &it->fn;
  }

private:
  std::vector<FunctionDefinition> m_fns;
};

struct OperationHandler {};

class Interpreter {
public:
  Interpreter(const UnitDatabase &db) : m_db(db) {}

  ComputedValue computeExpr(const Expression &expr) const {
    auto visitor = overloads{
        [&](const UnaryExpression &ue) -> ComputedValue {
          auto c = computeExpr(*ue.lhs);

          if (auto n = c.asNumber()) {
            auto c = *n;
            if (ue.op == "-") {
              c.n *= -1;
            }
            return {c};
          }

          return c;
        },
        [&](const BinaryExpression &be) {
          if (be.op == "+") {
            return add(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "-") {
            return subtract(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "*") {
            return multiply(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "/") {
            return div(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "%") {
            return modulo(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "^") {
            return pow(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "<<") {
            return leftshift(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == ">>") {
            return rightshift(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "&") {
            return bitwiseAnd(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }
          if (be.op == "|") {
            return bitwiseor(computeExpr(*be.lhs), computeExpr(*be.rhs));
          }

          throw std::runtime_error(std::format("Unhandled operator {}", be.op));
        },
        [&](const ConversionExpression &conv) -> ComputedValue {
          auto v = computeExpr(*conv.b);

          if (auto n = v.asNumber()) {
            auto value = *n;

            if (conv.target == "hex" || conv.target == "hexadecimal") {
              value.format = NumberOutputFormat::Hexadecimal;
              return ComputedValue{value};
            }

            if (conv.target == "binary") {
              value.format = NumberOutputFormat::Binary;
              return ComputedValue{value};
            }

            if (conv.target == "octal") {
              value.format = NumberOutputFormat::Octal;
              return ComputedValue{value};
            }

            auto targetCandidates = m_db.findUnitCandidates(conv.target);

            // if converted expression has no unit there is nothing to do, just
            // tag it with the target unit... 1m to s 1m to in
            if (!v.unitRaw)
              return ComputedValue{.value = value, .unitRaw = conv.target};

            auto valueCandidates = m_db.findUnitCandidates(*v.unitRaw);

            auto convert = [&](double n, const UnitDef &lhs,
                               const UnitDef &rhs) -> ComputedValue {
              if (lhs.type != rhs.type) {
                throw std::runtime_error(std::format(
                    "Incompatible units ({} to {})", lhs.id, rhs.id));
              }

              auto res = m_db.convert(value.n, lhs, rhs);

              if (!res)
                throw std::runtime_error(res.error());

              return {.value = Number{res.value()}, .unitRaw = conv.target};
            };

            // only one choice on both sides, there is no ambiguity
            if (valueCandidates.size() == 1 && targetCandidates.size() == 1) {
              auto lhs = valueCandidates.front();
              auto rhs = targetCandidates.front();
              return convert(value.n, lhs, rhs);
            }

            // we are unable to infer what unit should be used, we need to wait
            // for more info...
            if (valueCandidates.size() > 1 && targetCandidates.size() > 1) {
              return ComputedValue{.value = value, .unitRaw = conv.target};
            }

            // 1s to 1m
            if (valueCandidates.size() > targetCandidates.size()) {
              auto rhs = targetCandidates.front();
              auto lhs = std::ranges::find_if(
                  valueCandidates,
                  [&](const UnitDef &unit) { return unit.type == rhs.type; });
              if (lhs == valueCandidates.end()) {
                throw std::runtime_error(
                    std::format("Incompatible units: no common family"));
              }
              return convert(value.n, *lhs, rhs);
            }

            if (targetCandidates.size() > valueCandidates.size()) {
              auto lhs = valueCandidates.front();
              auto rhs = std::ranges::find_if(
                  targetCandidates,
                  [&](const UnitDef &unit) { return unit.type == lhs.type; });
              if (rhs == targetCandidates.end()) {
                throw std::runtime_error(
                    std::format("Incompatible units: no common type"));
              }
              return convert(value.n, lhs, *rhs);
            }
          }
          throw std::runtime_error("unexpected conversion flow");
        },
        [](NumberString n) { return ComputedValue{.value = Number{n}}; },
        [&](const UnitExpression &ue) {
          auto n = computeExpr(*ue.expr).value;

          // since we unitify the expression, we discard any unit the expr might
          // have had
          return ComputedValue{.value = n, .unitRaw = ue.unit};
        },
        [&](const FunctionCall &fn) { return executeFunction(fn); },
        [&](const DateString &fn) {
          if (auto s = std::get_if<std::string_view>(&fn.value)) {
            if (*s == "time" || *s == "now" || *s == "date") {
              auto now = std::chrono::system_clock::now();
              return ComputedValue{.value = DateTime{.time = now}};
            }
          }

          return ComputedValue{};
        }};

    return std::visit(visitor, expr.data);
  }

private:
  template <typename T, typename U = T>
  static void assertBinary(const ComputedValue &lhs, const ComputedValue &rhs) {
    bool ok = std::holds_alternative<T>(lhs.value) &&
              std::holds_alternative<U>(rhs.value);
    if (!ok)
      throw std::runtime_error("Invalid operands");
  }

  ComputedValue add(const ComputedValue &lhs, const ComputedValue &rhs) const {
    if (lhs.isDateTime() && rhs.isNumber()) {
      auto d = lhs.asDateTime();
      auto n = rhs.asNumber();

      if (rhs.unitRaw) {
        auto candidates = m_db.findUnitCandidates(rhs.unitRaw.value());
        auto it = std::ranges::find_if(candidates, [](const UnitDef &u) {
          return u.type == UnitType::Duration;
        });
        auto second = m_db.findUnit("second");

        if (it != candidates.end()) {
          // convert everything to seconds, then add it to time
          auto diff = m_db.convert(n->n, *it, *second);
          auto time =
              d->time + std::chrono::seconds(static_cast<int>(diff.value()));

          DateTime dt = *d;
          dt.time = time;
          return ComputedValue{dt};
        }
      }
    }

    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n + rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue subtract(const ComputedValue &lhs,
                                const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n - rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue multiply(const ComputedValue &lhs,
                                const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n * rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue div(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(lhs.asNumber()->n / rhs.asNumber()->n, lhs, rhs);
  }

  static ComputedValue modulo(const ComputedValue &lhs,
                              const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) %
                      static_cast<int>(rhs.asNumber()->n),
                  lhs, rhs);
  }

  static ComputedValue pow(const ComputedValue &lhs, const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(std::pow(lhs.asNumber()->n, rhs.asNumber()->n), lhs, rhs);
  }

  static ComputedValue leftshift(const ComputedValue &lhs,
                                 const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n)
                      << static_cast<int>(rhs.asNumber()->n),
                  lhs, rhs);
  }

  static ComputedValue rightshift(const ComputedValue &lhs,
                                  const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) >>
                      static_cast<int>(rhs.asNumber()->n),
                  lhs, rhs);
  }

  static ComputedValue bitwiseor(const ComputedValue &lhs,
                                 const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) |
                      static_cast<int>(rhs.asNumber()->n),
                  lhs, rhs);
  }

  static ComputedValue bitwiseAnd(const ComputedValue &lhs,
                                  const ComputedValue &rhs) {
    assertBinary<Number, Number>(lhs, rhs);
    return output(static_cast<int>(lhs.asNumber()->n) &
                      static_cast<int>(rhs.asNumber()->n),
                  lhs, rhs);
  }

  static ComputedValue output(double n, const ComputedValue &lhs,
                              const ComputedValue &rhs) {
    return ComputedValue{
        .value = Number{n},
        .unitRaw = rhs.unitRaw.or_else([&]() { return lhs.unitRaw; })};
  }

  ComputedValue executeFunction(const FunctionCall &fn) const {
    FunctionDatabase db;

    auto computedArgs =
        fn.args |
        std::views::transform([&](auto &&expr) { return computeExpr(*expr); }) |
        std::ranges::to<std::vector>();

    if (auto handler = db.findFunction(fn.name)) {
      FunctionCtx ctx{computedArgs};
      return (*handler)(ctx);
    } else {
      throw std::runtime_error(std::format("Unknown function \"{}\"", fn.name));
    }
  }

  const UnitDatabase &m_db;
};

std::expected<ComputedValue, std::string>
Abacus::compute(std::string_view expr) {
  try {
    Parser parser{expr, m_unitDb};
    auto ast = parser.parse();
    Interpreter i{m_unitDb};

    return i.computeExpr(*ast.root);
  } catch (const std::exception &e) {
    return std::unexpected(e.what());
  }
}

std::expected<std::string, std::string>
Abacus::evaluate(const std::string_view expr) {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();
  Interpreter i{m_unitDb};
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

  auto visitor = overloads{
      [&](const Number &number) -> std::string { return formatNumber(number); },
      [](const DateTime &date) -> std::string {
        return std::format("{:%Y-%m-%d %H:%M}", date.time);
      },
      [](const bool &v) -> std::string { return v ? "true" : "false"; },
  };

  return std::format("{}{}", std::visit(visitor, result.value),
                     result.unitRaw.value_or(""));
}

void walkAST(std::ostream &os, const Expression &expr, int depth = 0) {
  auto ident = [&]() {
    std::string s;
    for (int i = 0; i != depth; ++i)
      s += "  ";
    return s;
  };

  if (auto be = expr.asBinaryExpression()) {
    os << ident() << "Binary " << rang::fg::green << be->op << rang::fg::reset
       << " {\n";
    walkAST(os, *be->lhs, depth + 1);
    walkAST(os, *be->rhs, depth + 1);
    os << ident() << "}\n";
  }

  else if (auto ue = expr.asUnaryExpression()) {
    os << ident() << "Unary " << rang::fg::green << ue->op << rang::fg::reset
       << " {\n";
    walkAST(os, *ue->lhs, depth + 1);
    os << ident() << "}\n";

  }

  else if (auto conv = expr.asConversion()) {
    os << ident() << "Convert " << rang::fg::green << conv->target
       << rang::fg::reset << " {\n";
    walkAST(os, *conv->b, depth + 1);
    os << ident() << "}\n";
  }

  else if (auto n = std::get_if<NumberString>(&expr.data)) {
    os << ident() << "Number " << rang::fg::yellow << *n << rang::fg::reset
       << "\n";
  }

  else if (auto ue = std::get_if<UnitExpression>(&expr.data)) {
    os << ident() << "Unit " << rang::fg::green << ue->unit << rang::fg::reset
       << " {\n";
    walkAST(os, *ue->expr, depth + 1);
    os << ident() << "}\n";
  }

  else if (auto ds = std::get_if<DateString>(&expr.data)) {
    os << ident() << "Date " << " {\n";

    if (auto str = std::get_if<std::string_view>(&ds->value)) {
      os << ident() << "\tvalue " << *str << "\n";
    }

    if (ds->timezone) {
      os << ident() << "\ttimezone " << ds->timezone.value() << "\n";
    }

    os << ident() << "}\n";
  }

  else if (auto fn = expr.asFunction()) {
    os << ident() << "Fn " << rang::fg::green << fn->name << rang::fg::reset
       << " {\n";
    for (const auto &arg : fn->args) {
      walkAST(os, *arg, depth + 1);
    }
    os << ident() << "}\n";
  }
}

void Abacus::printAST(const std::string &expr) const {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();
  walkAST(std::cout, *ast.root, 0);
}

}; // namespace abacus
