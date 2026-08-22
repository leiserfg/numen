#include "numen/numen.hpp"
#include "rang/rang.hpp"
#include <iostream>
#include <string>
#ifdef BUILD_CURRENCY_PROVIDER
#include "vicinae-currency-provider.hpp"
#endif

void process(numen::Numen &calc, const std::string &s) {
  std::string_view line{s};

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
      std::cout << line << " = " << rang::fg::green << res.value() << rang::fg::reset << "\n";
    }
  }
}

int main(int ac, char **av) try {
  numen::Numen calc{};

#ifdef BUILD_CURRENCY_PROVIDER
  calc.setCurrencyProvider(std::make_unique<VicinaeCurrencyProvider>());
  calc.updateRates();
#endif

  if (ac == 2) {
    process(calc, av[1]);
    return 0;
  }

  std::string line;

  std::cout << "$> ";

  while (std::getline(std::cin, line)) {
    process(calc, line);
    std::cout << "$> ";
  }
} catch (const std::exception &e) {
  std::cerr << "fatal: " << e.what() << "\n";
  return 1;
}
