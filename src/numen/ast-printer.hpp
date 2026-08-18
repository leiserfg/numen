#pragma once

#include "parser.hpp"
#include <ostream>

namespace numen::detail {

void printAST(std::ostream &os, const Expression &expr);

} // namespace numen::detail
