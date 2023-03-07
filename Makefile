all:
	rm -rf build
	cmake -Bbuild -H.
	cmake --build build -j4