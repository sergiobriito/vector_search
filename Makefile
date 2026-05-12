CXX = g++
CXXFLAGS = -Iinclude -std=c++17 -Wall -O3 -march=native -mavx2 -fopenmp -ffast-math -pthread -DJSON_USE_LEGACY_STRTOLL
LDFLAGS = -fopenmp -lssl -lcrypto -lz -lpthread
SRCS = src/ivf.cpp src/utils.cpp src/vector_search.cpp src/main_local.cpp include/simdjson.cpp

build:
	$(CXX) $(CXXFLAGS) $(SRCS) -o main $(LDFLAGS)

build_index: build
	./main build_index

run: build
	./main

clean:
	rm -f main *.o src/*.o