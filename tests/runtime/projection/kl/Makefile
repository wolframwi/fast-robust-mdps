CXX ?= g++
ROOT ?= ../../../..

CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -g -I$(ROOT)

MOSEK_ROOT ?= /rds/general/user/wwiesema/home/mosek/11.1/tools/platform/linux64x86
MOSEK_INC = $(MOSEK_ROOT)/h
MOSEK_LIB = $(MOSEK_ROOT)/bin
LDFLAGS = -L$(MOSEK_LIB) -Wl,-rpath,$(MOSEK_LIB)
LDLIBS = -lfusion64 -lmosek64 -lpthread -lstdc++fs

SOURCES = \
  $(ROOT)/tests/runtime/projection/kl/run_kl_runtime.cpp \
  $(ROOT)/algorithms/mdp.cpp \
  $(ROOT)/algorithms/fast/kl/projection/project.cpp \
  $(ROOT)/algorithms/mosek/kl/projection/project.cpp

TARGET = kl_runtime

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -I$(MOSEK_INC) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
