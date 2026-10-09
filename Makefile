
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

all: server load_test

server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server

load_test: load_test.cpp
	$(CXX) $(CXXFLAGS) load_test.cpp -o load_test

run-server: server
	ulimit -n 20000 && ./server

run-test: load_test
	ulimit -n 20000 && ./load_test

clean:
	rm -f server load_test

.PHONY: all run-server run-test clean
