#pragma once
#include <cstdint>
#include <string>
#include <string_view>

void appendUtf8(std::string &out, uint32_t codepoint);

// must match normalize() in scripts/gen-timezone-tables.py, which generates kCharMap
std::string normalizeName(std::string_view text);
