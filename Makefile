CXX = g++

CXXFLAGS = -Iinclude \
	-std=c++17 \
	-Wall \
	-O3 \
	-march=native \
	-mavx2 \
	-fopenmp \
	-ffast-math \
	-pthread \
	-DJSON_USE_LEGACY_STRTOLL \

LDFLAGS = \
	-fopenmp \
	 \
	lib/libuWS.a \
	lib/libuSockets.a \
	-lssl \
	-lcrypto \
	-lz \
	-lpthread

SOURCES = $(wildcard src/*.cpp)

build:
	$(CXX) $(CXXFLAGS) src/*.cpp -o main $(LDFLAGS)

build_index: build
	./main build_index

run: build
	./main

run_test: build
	./main run_test

clean:
	rm -f main *.o src/*.o