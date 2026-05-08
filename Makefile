CXX = g++
CXXFLAGS = -Iinclude -std=c++17 -Wall -O3 -march=native -mavx2 -fopenmp -ffast-math -pthread 
LDLIBS = -lws2_32 -lmswsock -liphlpapi

SOURCES = $(wildcard src/*.cpp)

build: $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o main $(LDLIBS)

run: build
	./main
	
clean:
	rm -f main