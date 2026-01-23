CXX ?= g++
ROOT ?= ../../../..

CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -march=native -g -I$(ROOT)

SOURCES = \
  $(ROOT)/tests/profiling/projection/kl/run_fast_kl.cpp \
  $(ROOT)/algorithms/mdp.cpp \
  $(ROOT)/algorithms/fast/kl/projection/project.cpp

TARGET = kl_profile

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
