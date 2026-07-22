#include "abacus/abacus.hpp"
#include "abacus/unit.hpp"
#include "parser.hpp"
#include "rang/rang.hpp"
#include <algorithm>
#include <cassert>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <ostream>
#include <ranges>
#include <stdexcept>

namespace abacus {

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

public:
  FunctionCtx(std::span<const ComputedValue> args) : args(args) {}

private:
  std::span<const ComputedValue> args;
};

using FunctionHandler = std::function<ComputedValue(FunctionCtx ctx)>;

struct FunctionDefinition {
  std::string_view name;
  int requiredArgs = 0;
  FunctionHandler fn;
};

namespace {
void assertArgSize(std::string_view name, int expected, int actual) {
  if (expected != actual) {
    throw std::runtime_error(std::format("{}() expected {} args but got {}",
                                         name, expected, actual));
  }
}
}; // namespace

class FunctionDatabase {
public:
  FunctionDatabase() {
    registerFunction("min", [&](FunctionCtx ctx) {
      auto [lhs, rhs] = ctx.unpack<double, double>();
      return ComputedValue{.value = std::min(lhs, rhs)};
    });
    registerFunction("max", [&](FunctionCtx ctx) {
      auto [lhs, rhs] = ctx.unpack<double, double>();
      return ComputedValue{.value = std::max(lhs, rhs)};
    });
    registerFunction("sin", [&](FunctionCtx ctx) {
      auto [lhs] = ctx.unpack<double>();
      return ComputedValue{.value = std::sin(lhs)};
    });
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

class Interpreter {
public:
  Interpreter(const UnitDatabase &db) : m_db(db) {}

  ComputedValue computeExpr(const Expression &expr) const {
    if (auto ue = expr.asUnaryExpression()) {
      auto c = computeExpr(*ue->lhs);
      if (ue->op == "-") {
        c.value *= -1;
      }
      return c;
    } else if (auto be = expr.asBinaryExpression()) {
      if (be->op == "+") {
        return computeExpr(*be->lhs) + computeExpr(*be->rhs);
      }
      if (be->op == "-") {
        return computeExpr(*be->lhs) - computeExpr(*be->rhs);
      }
      if (be->op == "*") {
        return computeExpr(*be->lhs) * computeExpr(*be->rhs);
      }
      if (be->op == "/") {
        return computeExpr(*be->lhs) / computeExpr(*be->rhs);
      }
      if (be->op == "%") {
        return computeExpr(*be->lhs) % computeExpr(*be->rhs);
      }
      if (be->op == "^") {
        return computeExpr(*be->lhs).pow(computeExpr(*be->rhs));
      }

      throw std::runtime_error(std::format("Unhandled operator {}", be->op));
    }

    else if (auto conv = expr.asConversion()) {
      auto value = computeExpr(*conv->b);
      auto targetCandidates = m_db.findUnitCandidates(conv->target);

      // if converted expression has no unit there is nothing to do, just tag it
      // with the target unit...
      // 1m to s
      // 1m to in
      if (!value.unitRaw)
        return {.value = value.value, .unitRaw = conv->target};

      auto valueCandidates = m_db.findUnitCandidates(*value.unitRaw);

      auto convert = [&](double n, const UnitDef &lhs,
                         const UnitDef &rhs) -> ComputedValue {
        if (lhs.type != rhs.type) {
          throw std::runtime_error(
              std::format("Incompatible units ({} to {})", lhs.id, rhs.id));
        }

        auto res = m_db.convert(value.value, lhs, rhs);

        if (!res)
          throw std::runtime_error(res.error());

        return {.value = res.value(), .unitRaw = conv->target};
      };

      // only one choice on both sides, there is no ambiguity
      if (valueCandidates.size() == 1 && targetCandidates.size() == 1) {
        auto lhs = valueCandidates.front();
        auto rhs = targetCandidates.front();
        return convert(value.value, lhs, rhs);
      }

      // we are unable to infer what unit should be used, we need to wait for
      // more info...
      if (valueCandidates.size() > 1 && targetCandidates.size() > 1) {
        return {.value = value.value, .unitRaw = conv->target};
      }

      // 1s to 1m
      if (valueCandidates.size() > targetCandidates.size()) {
        auto rhs = targetCandidates.front();
        auto lhs =
            std::ranges::find_if(valueCandidates, [&](const UnitDef &unit) {
              return unit.type == rhs.type;
            });
        if (lhs == valueCandidates.end()) {
          throw std::runtime_error(
              std::format("Incompatible units: no common family"));
        }
        return convert(value.value, *lhs, rhs);
      }

      if (targetCandidates.size() > valueCandidates.size()) {
        auto lhs = valueCandidates.front();
        auto rhs =
            std::ranges::find_if(targetCandidates, [&](const UnitDef &unit) {
              return unit.type == lhs.type;
            });
        if (rhs == targetCandidates.end()) {
          throw std::runtime_error(
              std::format("Incompatible units: no common type"));
        }
        return convert(value.value, lhs, *rhs);
      }

      throw std::runtime_error("unexpected conversion flow");
    }

    else if (auto n = std::get_if<NumberString>(&expr.data)) {
      return ComputedValue{.value = *n};
    }

    else if (auto ue = std::get_if<UnitExpression>(&expr.data)) {
      double n = computeExpr(*ue->expr).value;
      // since we unitify the expression, we discard any unit the expr might
      // have had
      return ComputedValue{.value = n, .unitRaw = ue->unit};
    }

    else if (auto fn = expr.asFunction()) {
      return executeFunction(*fn);
    }

    throw std::runtime_error("Unhandled expression type");
  }

private:
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
  return std::format("{:.6g}", result.value);
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
