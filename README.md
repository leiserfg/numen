![libnumen usage in the Vicinae launcher](./extra/screenshot.png)

`libnumen` is a library to evaluate mathematical expressions, with an emphasis on natural language as well as unit and date-time operations.

> [!WARNING]
> This library is primarily developed for the [Vicinae](https://github.com/vicinaehq/vicinae) launcher.
> However, it is licensed separately from the main Vicinae project, under the [BSD 3-Clause License](./LICENSE), allowing its use in non-GPL projects.
> Note that the API is currently not stable, so you should treat each update as potentially breaking.

## Key features

- Dependency-free. You only need a C++23 compiler. Compiles with GCC/clang/MSVC. On Apple platforms, whose libc++ [has no `std::chrono` timezone database](https://github.com/llvm/llvm-project/pull/122010), the vendored [date](https://github.com/HowardHinnant/date) library is used instead (`-DNUMEN_USE_DATE_TZ=ON`, the default there).
- Evaluate algebraic expressions as you would expect.
- Built-in math functions
- Support for computer science operators like bitwise operators and modulo.
- Full support for units
- Date-time arithmetic and timezone conversion based on timezone name, city/region/state name.
- Currency conversion support (requires implementing a currency provider; this repo contains an example of one).
- Natural language syntactic sugar: you can write expressions like `20% of 100` to calculate `0.20 * 100`.

## Goals

- Zero dependency, compiles on every platform where modern C++ is supported
- Reasonably fast, should be able to run on every keystroke without slowing things down
- Intuitive defaults, respects localization rules where possible
- Doesn't throw (except internally)

## Comparison with other calculator libraries

- [SoulverCore](https://github.com/soulverteam/SoulverCore) is the main inspiration for this library, as it supports extensive timezone conversions and natural language constructs. It is, however, closed-source and requires a Swift toolchain, which is less than convenient on non-Apple systems.
- [numbat](https://github.com/sharkdp/numbat) is a statically typed programming language, and lacks natural language and date-time arithmetic capabilities.
- [libqalculate](https://github.com/qalculate/libqalculate) is a very capable scientific calculator, supporting a lot of advanced math that `libnumen` does not plan to support. Timezone conversion and date-time arithmetic support is poor.

## Known limitations

- `libnumen` stores every number and computation result in `double` data types, meaning the range and precision of representable values are inherently limited. We could solve this by using one of the various arbitrary-precision libraries. We probably will, eventually.
- Does not implement an equation solver
- Implicit unit conversion is only implemented for currencies at this time
- No support for niche units, only the most popular ones.
- Parsing may be too permissive (in a bad way) at times
- Natural language syntax sugar targets English only, although number and dates are localized
- `.` is always used as a decimal separator, and `,` as a thousand separator, no matter the user's locale. Numbers are rendered with US-style grouping (`1,234,567.89`) for the same reason, so any output can be fed back in as input. Date time values are otherwise properly localized.
- Code is still ugly in some places :)

## Roadmap

There is no strict roadmap properly speaking, but here are a few things I would like to implement in the future (in no particular order):

- first class color type with color conversion support
- more advanced maths stuff if I feel like it
- implict unit conversion (outside of curency)
- fully [localized](https://cldr.unicode.org/translation/date-time/date-time-patterns) date formatting (e.g `2010年12月5日` for Japanese)
- support for variables, function definitions, multi statements (basically turning this into an interpreted programming language of sort)
- better unicode support

## Getting started

### Installation

If you want to use `libnumen` in your project, we recommend vendoring it directly, which is easily done as it is dependency-free.

The easiest way is to pull it in with CMake's `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(numen
  GIT_REPOSITORY https://github.com/vicinaehq/libnumen.git
  GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(numen)

target_link_libraries(your-target PRIVATE numen::numen)
```

Cloning the repo and adding it with `add_subdirectory` works the same way.

If you want a system-wide installation of the library, run `make install`. It can then be consumed with `find_package(numen)`, which provides the same `numen::numen` target.

### Build

You can just run `make`, or:

```sh
cmake --preset default
cmake --build --preset default
```

You can pass `-DBUILD_SHARED_LIBS=ON` if you want to build as a shared library.

Run `make test` to build and run the test suite (`./build/numen-tests`).

### Usage

You can look at the `include/numen/numen.hpp` header and the tests, which there are many.

If built manually using `make` there is also a minimal REPL that you can use to try things out, without much configuration:

```bash
./build/numen
```
