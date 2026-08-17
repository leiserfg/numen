#include "numen/numen.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
  numen::Numen calc;

  auto res = calc.evaluate(std::string_view{reinterpret_cast<const char *>(data), size});

  if (res) {
    // touch the result so it cannot be optimised away
    static volatile std::size_t sink = 0;
    sink = res->size();
  }

  return 0;
}
