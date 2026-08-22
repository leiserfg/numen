#pragma once

#include "computed.hpp"
#include "numen/numen.hpp"
#include "numen/unit.hpp"
#include "parser.hpp"

namespace numen::detail {

// evaluates the tree and applies the top-level defaults: implicit currency
// conversion to the locale and folding a lone duration-unit number into a Duration
Computed interpret(const Expression &expr, const UnitDatabase &db, const EvalOptions &opts);

} // namespace numen::detail
