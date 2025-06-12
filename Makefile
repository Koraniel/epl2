build: CMakeLists.txt
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=RelWithDebugIngo

build/tests: build *.cpp *.hpp
	cd build && make tests -j `nproc`

test: build/tests
	build/tests > output.txt || true
	python3 checker.py output.txt

clean:
	rm -rf build
