CXX = g++
CXXFLAGS = -Iinclude -std=c++17 -Wall -O3 -march=native -mavx2 -fopenmp -ffast-math -pthread

SOURCES = $(wildcard src/*.cpp)

build: $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o main

build_index: build
	./main build_index

run: build
	./main

run_test: build
	./main run_test

clean:
	rm -f main