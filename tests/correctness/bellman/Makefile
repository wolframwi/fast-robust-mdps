CXX ?= g++
ROOT ?= ../../..

CXXFLAGS ?= -std=c++17 -O2 -I$(ROOT) -DIL_STD -DUSE_CPLEX

CPLEX_ROOT ?= /home/wwiesema/CPLEX_Studio2211
MOSEK_ROOT ?= /home/wwiesema/mosek/11.1/tools/platform/linux64x86

CPLEX_INC = $(CPLEX_ROOT)/cplex/include
CONCERT_INC = $(CPLEX_ROOT)/concert/include
CPLEX_LIB = $(CPLEX_ROOT)/cplex/lib/x86-64_linux/static_pic
CONCERT_LIB = $(CPLEX_ROOT)/concert/lib/x86-64_linux/static_pic

MOSEK_INC = $(MOSEK_ROOT)/h
MOSEK_LIB = $(MOSEK_ROOT)/bin
GUROBI_HOME ?= $(shell ls -d /home/wwiesema/gurobi*/linux64 /opt/gurobi*/linux64 2>/dev/null | sort -V | tail -n 1)
GUROBI_INC = $(GUROBI_HOME)/include
GUROBI_LIB = $(GUROBI_HOME)/lib

LDFLAGS = \
  -L$(CPLEX_LIB) -L$(CONCERT_LIB) \
  -L$(MOSEK_LIB) \
  -L$(GUROBI_LIB) \
  -Wl,-rpath,$(MOSEK_LIB) \
  -Wl,-rpath,$(GUROBI_LIB) \
  -lpthread

LDLIBS = \
  -lilocplex -lcplex -lconcert \
  -lfusion64 -lmosek64 \
  -lgurobi_c++ -lgurobi130

SOURCES = \
  $(ROOT)/tests/correctness/bellman/test_bellman.cpp \
  $(ROOT)/algorithms/mdp.cpp \
  $(ROOT)/algorithms/homotopy/bellman/bellman.cpp \
  $(ROOT)/algorithms/fast/bellman/bellman.cpp \
  $(ROOT)/algorithms/fast/kl/projection/project.cpp \
  $(ROOT)/algorithms/fast/burg/projection/project.cpp \
  $(ROOT)/algorithms/cplex/l1/bellman/bellman.cpp \
  $(ROOT)/algorithms/cplex/l2/bellman/bellman.cpp \
  $(ROOT)/algorithms/gurobi/l1/bellman/bellman.cpp \
  $(ROOT)/algorithms/gurobi/l2/bellman/bellman.cpp \
  $(ROOT)/algorithms/mosek/l1/bellman/bellman.cpp \
  $(ROOT)/algorithms/mosek/l2/bellman/bellman.cpp \
  $(ROOT)/algorithms/mosek/kl/bellman/bellman.cpp \
  $(ROOT)/algorithms/mosek/burg/bellman/bellman.cpp

TARGET = bellman_test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -I$(CPLEX_INC) -I$(CONCERT_INC) -I$(MOSEK_INC) -I$(GUROBI_INC) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
