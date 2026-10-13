# CPU16 golden model
#
#   make        build
#   make test   build and run the self-tests
#   make clean

CXX      := g++
CXXFLAGS := -std=c++17 -O1 -g \
            -Wall -Wextra -Wpedantic \
            -Wconversion -Wsign-conversion -Wold-style-cast \
            -Wshadow -Wswitch-enum

# -Wconversion / -Wsign-conversion are the important ones here. In a CPU model
# every implicit narrowing is a potential architectural width bug, and these
# flags make the compiler point at each one. -Wswitch-enum names any enumerator
# a switch forgot, which is what you want in a decoder.

SRCS := main.cpp cpu16_isa.cpp cpu16_cpu.cpp
OBJS := $(SRCS:.cpp=.o)
BIN  := cpu16sim

.PHONY: all test clean
all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

main.o cpu16_isa.o cpu16_cpu.o: cpu16_isa.hpp
main.o cpu16_cpu.o: cpu16_cpu.hpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(BIN)
	./$(BIN) --test

clean:
	rm -f $(OBJS) $(BIN) $(BIN).exe *.trace
