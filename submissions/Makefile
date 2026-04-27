CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system -lrt

TARGETS = bin/arbiter bin/hip bin/asp

all: clean bin $(TARGETS)
	@echo Build complete.

bin:
	mkdir -p bin

bin/arbiter: arbiter/arbiter.cpp
	$(CXX) $(CXXFLAGS) arbiter/*.cpp -o bin/arbiter $(LIBS)

bin/hip: hip/hip.cpp
	$(CXX) $(CXXFLAGS) hip/*.cpp -o bin/hip $(LIBS)

bin/asp: asp/asp.cpp
	$(CXX) $(CXXFLAGS) asp/*.cpp -o bin/asp $(LIBS)

clean:
	rm -f bin/arbiter bin/hip bin/asp

.PHONY: all clean bin