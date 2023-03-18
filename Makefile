all:
	rm -rf build
	cmake -Bbuild -H. -D CMAKE_BUILD_TYPE=Debug
	cmake --build build -j4