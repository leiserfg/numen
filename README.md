`libnumen` is a library to evaluate mathematical expressions, with an emphasis on natural language as well as unit and date-time operations.

> [!WARNING]
> This library is primarily developed for the [Vicinae](https://github.com/vicinaehq/vicinae) launcher.
> However, it is licensed separately from the main Vicinae project, under the [BSD 3-Clause License](./LICENSE), allowing its use in non-GPL projects.
> Note that the API is currently not stable, so you should treat each update as potentially breaking.

## Key features

- Dependency-free. You only need a C++23 compiler. Compiles with GCC/clang/MSVC.
- Evaluate algebraic expressions as you would expect.
- Built-in math functions (see below).
- Support for computer science operators like bitwise operators and modulo.
- Full support for units.
- Date-time arithmetic and timezone conversion based on timezone name, city/region/state name. Expressions like `time in 2 hours in SF` work.
- Currency conversion support (requires implementing a currency provider; this repo contains an example of one).
- Natural language syntactic sugar: you can write expressions like `20% of 100` to calculate `0.20 * 100`.

## Comparison with other calculator libraries

- [SoulverCore](https://github.com/soulverteam/SoulverCore) is the main inspiration for this library, as it supports extensive timezone conversions and natural language constructs. It is, however, closed-source and requires a Swift toolchain, which is less than convenient on non-Apple systems.
- [numbat](https://github.com/sharkdp/numbat) is a statically typed programming language, and lacks natural language and date-time arithmetic capabilities.
- [libqalculate](https://github.com/qalculate/libqalculate) is a very capable scientific calculator, supporting a lot of advanced math that `libnumen` does not plan to support. Timezone conversion and date-time arithmetic support is poor.

## Known limitations

- `libnumen` stores every number and computation result in `double` data types, meaning the range and precision of representable values are inherently limited. We could solve this by using one of the various arbitrary-precision libraries. We probably will, eventually.
- Does not implement an equation solver
- No support for niche units, only the most popular ones.
- Parsing may be too permissive (in a bad way) at times, we will tighten this as we go
- Natural language syntax sugar targets English only, although number and dates are localized
- `.` is always used as a decimal separator, and `,` as a thousand separator, no matter the user's locale. Date time values are otherwise properly localized.
- Code is still ugly in some places :)

## Getting started

### Build

You can just run `make`, or:

```sh
cmake --preset default
cmake --build --preset default
```

You can pass `-DBUILD_SHARED_LIBS=ON` if you want to build as a shared library.

Run `make test` to build and run the test suite (`./build/numen-tests`).

### Usage

```cpp
#include "numen/numen.hpp"
#include <iostream>

int main() {
  numen::Numen calc;
  auto res = calc.evaluate("20% of 100");

  if (!res) {
    std::cerr << "Error: " << res.error() << "\n";
    return 1;
  }

  std::cout << res.value() << "\n"; // 20
}
```

`evaluate()` returns the default rendering. When you need the value itself, `compute()` returns a `numen::ComputedValue` (a variant of `Number`, `DateTime`, `Duration` and `Boolean`) and `parse<T>()` converts straight to a C++ type. Every value type has a `toString()` producing the same text `evaluate()` would, and a matching `std::formatter`, so you can format some kinds yourself and fall back to the default for the rest:

```cpp
auto res = calc.compute("tomorrow - today + 90min");

if (auto d = res->asDuration()) {
  std::println("{}", *d);            // 1 day 1 hr 30 min
  std::println("{}", d->toString()); // same thing
  std::println("{}", calc.parse<std::chrono::minutes>("tomorrow - today + 90min")->count()); // 1530
}
```

The REPL built by `make` (`./build/repl`) can be used to quickly try things out, either interactively or by passing an expression as its single argument:

```sh
./build/repl "time in 2 hours in SF"
```

### Installation

If you want to use `libnumen` in your project, we recommend vendoring it directly, which is easily done as it is dependency-free.

The easiest way is to clone this repo and add it as a CMake subdirectory. You will probably want to pass `-DBUILD_REPL=OFF -DBUILD_TESTS=OFF` in that scenario.

If you want a system-wide installation of the library, run `make install`.

## Development

### Data

- The region → currency table under `src/numen/gen/` is generated from Unicode CLDR supplemental data (`extra/supplementalData.xml`) by `scripts/gen-currency-tables.py`. Currency minor units live on the unit definitions in `src/numen/builtin-units.cpp`.
- Place name to timezone resolution (`now in paris tx`) uses data from [GeoNames](https://www.geonames.org), licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). `scripts/gen-timezone-tables.py` downloads the GeoNames dumps into `.cache/geonames/` (or reads them from an existing directory with `python3 scripts/gen-timezone-tables.py --dumps <dir>`) and turns them into `src/gen/geo-tz-tables.inc` and `src/gen/geo-charmap.inc`. Delete the cached dumps to refresh them.
- `make gen` runs both scripts.
