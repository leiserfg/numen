#pragma once
#include "abacus/unit.hpp"
#include <span>

namespace units {

std::span<const UnitDef> builtins();
std::span<const CompoundAlias> compoundAliases();

}; // namespace units
