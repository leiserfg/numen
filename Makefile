all:
	cmake -GNinja -DBUILD_TESTS=ON -B build
	cmake --build build

test: all
	./build/abacus-tests

clean:
	rm -rf build

fuzzer:
	cmake -GNinja -DBUILD_FUZZER=ON -DBUILD_REPL=OFF -DBUILD_TESTS=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build-fuzzer
	cmake --build build-fuzzer
.PHONY: fuzzer

fuzz: fuzzer
	./build-fuzzer/abacus-fuzz tests/fuzz-corpus -only_ascii=1 -max_total_time=60
.PHONY: fuzz

re: clean all

