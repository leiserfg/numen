#include "abacus/abacus.hpp"
#include <iostream>
#include <string>

int main(int ac, char **av) {
  abacus::Abacus calc;

  std::string line;

  std::cout << "$> ";

  while (std::getline(std::cin, line)) {
    auto res = calc.compute(line);

    if (!res) {
      std::cout << "Error: " << res.error() << "\n";
    } else {
      std::cout << res->n << res->unitRaw.value_or("") << "\n";
    }

    std::cout << "$> ";
  }
}
