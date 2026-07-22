#include "abacus/abacus.hpp"
#include <iostream>
#include <string>

void process(const std::string &s) {
  std::string_view line{s};
  abacus::Abacus calc;

  bool ast = false;
  if (line.starts_with("/ast")) {
    line = line.substr(4);
    ast = true;
  }

  if (ast) {
    calc.printAST(std::string{line});
  } else {
    auto res = calc.evaluate(line);

    if (!res) {
      std::cout << "Error: " << res.error() << "\n";
    } else {
      std::cout << res.value() << "\n";
    }
  }
}

int main(int ac, char **av) {
  if (ac == 2) {
    process(av[1]);
    return 0;
  }

  std::string line;

  std::cout << "$> ";

  while (std::getline(std::cin, line)) {
    process(line);
    std::cout << "$> ";
  }
}
