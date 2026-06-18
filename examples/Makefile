# Builds every .cpp in this folder into a same-named binary.
# Usage:
#   make            # build all examples
#   make example1   # build a single example
#   make clean      # remove built binaries

CXX      ?= g++
CXXFLAGS ?= -std=c++23 -Wall -Wextra -pedantic -O2
INCLUDES := -I../include

SOURCES := $(wildcard *.cpp)
TARGETS := $(SOURCES:.cpp=)

.PHONY: all clean

all: $(TARGETS)

%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

clean:
	$(RM) $(TARGETS)
