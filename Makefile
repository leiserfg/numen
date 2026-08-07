all:
	cmake --preset default
	cmake --build --preset default

test: all
	ctest --preset default

debug:
	cmake --preset debug
	cmake --build --preset debug

fuzzer:
	cmake --preset fuzzer
	cmake --build --preset fuzzer

clean:
	rm -rf build build-debug build-fuzzer

re: clean all

.PHONY: all test debug fuzzer clean re
