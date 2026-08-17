PY := /usr/bin/env python

all:
	cmake --preset default
	cmake --build --preset default

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

format:
	find ./src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -print0 | xargs -0 -n 10 clang-format -i
	find ./tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -print0 | xargs -0 -n 10 clang-format -i
.PHONY: format

re: clean all

.PHONY: all test debug fuzzer clean re
