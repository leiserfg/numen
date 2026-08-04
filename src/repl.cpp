#include "abacus/abacus.hpp"
#include <iostream>
#include <string>
#ifdef BUILD_CURRENCY_PROVIDER
#include "vicinae-currency-provider.hpp"
#endif

void process(abacus::Abacus &calc, const std::string &s) {
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
      std::cout << res.value() << "\n";
    }
  }
}

int main(int ac, char **av) {
  abacus::Abacus calc{};

#ifdef BUILD_CURRENCY_PROVIDER
  auto provider = std::make_unique<VicinaeCurrencyProvider>();
  provider->updateRates();
  calc.setCurrencyProvider(std::move(provider));
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
}
