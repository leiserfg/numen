PY := /usr/bin/env python

all: static
.PHONY: all

static:
	cmake --preset default
	cmake --build --preset default
.PHONY: static

shared:
	cmake --preset default -DBUILD_SHARED_LIBS=ON
	cmake --build --preset default
.PHONY: shared

test: all
	./build/numen-tests

debug:
	cmake --preset debug
	cmake --build --preset debug

fuzzer:
	cmake --preset fuzzer
	cmake --build --preset fuzzer

clean:
	rm -rf build build-debug build-fuzzer

gen:
	$(PY) ./scripts/gen-currency-tables.py
	$(PY) ./scripts/gen-timezone-tables.py
.PHONY: gen

tidy: static
	run-clang-tidy -quiet -p build 'src/.*'
.PHONY: tidy

format:
	find ./src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -print0 | xargs -0 -n 10 clang-format -i
	find ./tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -print0 | xargs -0 -n 10 clang-format -i
.PHONY: format

install:
	cmake --install build
.PHONY: install

re: clean all

.PHONY: all test debug fuzzer clean re
