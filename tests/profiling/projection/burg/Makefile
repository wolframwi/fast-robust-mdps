CXX ?= g++
ROOT ?= ../../../..

CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -march=native -g -I$(ROOT)

SOURCES = \
  $(ROOT)/tests/profiling/projection/burg/run_fast_burg.cpp \
  $(ROOT)/algorithms/mdp.cpp \
  $(ROOT)/algorithms/fast/burg/projection/project.cpp

TARGET = burg_profile

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
