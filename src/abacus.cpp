#include "abacus/abacus.hpp"
#include "abacus/unit.hpp"
#include "parser.hpp"
#include <cassert>
#include <expected>
#include <functional>
#include <iostream>
#include <ostream>

namespace abacus {

class Interpreter {
public:
  Interpreter(const UnitDatabase &db) : m_db(db) {}

  ComputedValue computeExpr(const Expression &expr) const {
    if (auto be = expr.asBinaryExpression()) {
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
        return {.n = value.n, .unitRaw = conv->target};

      auto valueCandidates = m_db.findUnitCandidates(*value.unitRaw);

      auto convert = [&](double n, const UnitDef &lhs,
                         const UnitDef &rhs) -> ComputedValue {
        if (lhs.type != rhs.type) {
          throw std::runtime_error(
              std::format("Incompatible units ({} to {})", lhs.id, rhs.id));
        }

        auto res = m_db.convert(value.n, lhs, rhs);

        if (!res)
          throw std::runtime_error(res.error());

        return {.n = res.value(), .unitRaw = conv->target};
      };

      // only one choice on both sides, there is no ambiguity
      if (valueCandidates.size() == 1 && targetCandidates.size() == 1) {
        auto lhs = valueCandidates.front();
        auto rhs = targetCandidates.front();
        return convert(value.n, lhs, rhs);
      }

      // we are unable to infer what unit should be used, we need to wait for
      // more info...
      if (valueCandidates.size() > 1 && targetCandidates.size() > 1) {
        return {.n = value.n, .unitRaw = conv->target};
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
        return convert(value.n, *lhs, rhs);
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
        return convert(value.n, lhs, *rhs);
      }

      throw std::runtime_error("unexpected conversion flow");
    }

    else if (auto n = std::get_if<NumberString>(&expr.data)) {
      return ComputedValue{.n = *n};
    }

    else if (auto ue = std::get_if<UnitExpression>(&expr.data)) {
      double n = computeExpr(*ue->expr).n;
      // since we unitify the expression, we discard any unit the expr might
      // have had
      return ComputedValue{.n = n, .unitRaw = ue->unit};
    }

    throw std::runtime_error("Unhandled expression type");
  }

private:
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
  return std::format("{:.6g}", result.n);
}

void walkAST(std::ostream &os, const Expression &expr, int depth = 0) {
  auto ident = [&]() {
    std::string s;
    for (int i = 0; i != depth; ++i)
      s += '\t';
    return s;
  };

  if (auto be = expr.asBinaryExpression()) {
    os << "BinaryExpr {\n";
    os << ident() << "operator " << be->op << "\n";
    os << ident() << "lhs ";
    walkAST(os, *be->lhs, depth + 1);
    os << "\n";
    os << ident() << "rhs ";
    walkAST(os, *be->rhs, depth + 1);
    os << ident() << "}\n\n";
  }

  else if (auto conv = expr.asConversion()) {
    os << "Conversion {\n";
    os << ident() << "to " << conv->target << "\n";
    os << ident() << "lhs ";
    walkAST(os, *conv->b, depth + 1);
    os << ident() << "}\n\n";
  }

  else if (auto n = std::get_if<NumberString>(&expr.data)) {
    os << "Number " << *n << "\n";
  }

  else if (auto ue = std::get_if<UnitExpression>(&expr.data)) {
    os << "Unit {\n";
    os << ident() << "unit " << ue->unit << "\n";
    os << ident() << "lhs ";
    walkAST(os, *ue->expr, depth + 1);
    os << ident() << "\n}\n";
  }
}

void Abacus::printAST(const std::string &expr) const {
  Parser parser{expr, m_unitDb};
  auto ast = parser.parse();
  walkAST(std::cout, *ast.root, 0);
}

}; // namespace abacus
