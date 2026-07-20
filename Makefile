all:
	cmake -GNinja -DBUILD_TESTS=ON -B build
	cmake --build build

test: all
	./build/abacus-tests

clean:
	rm -rf build

re: clean all
