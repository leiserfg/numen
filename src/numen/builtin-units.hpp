#pragma once
#include "numen/unit.hpp"
#include <span>

namespace units {

std::span<const UnitDef> builtins();
std::span<const CompoundAlias> compoundAliases();

} // namespace units
