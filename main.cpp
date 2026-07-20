#include "include/abacus/abacus.hpp"
#include <iostream>

int main() { std::cout << abacus::Abacus().evaluate("1 + 152").value(); }
