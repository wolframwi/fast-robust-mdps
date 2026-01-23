CXX ?= g++
ROOT ?= ../../../..

CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -I$(ROOT) -DIL_STD -DUSE_CPLEX
HOMOTOPY_CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -I$(ROOT)

CPLEX_ROOT ?= /rds/general/user/wwiesema/home/CPLEX_Studio2211
CPLEX_INC = $(CPLEX_ROOT)/cplex/include
CONCERT_INC = $(CPLEX_ROOT)/concert/include
CPLEX_LIB = $(CPLEX_ROOT)/cplex/lib/x86-64_linux/static_pic
CONCERT_LIB = $(CPLEX_ROOT)/concert/lib/x86-64_linux/static_pic

MOSEK_ROOT ?= /rds/general/user/wwiesema/home/mosek/11.1/tools/platform/linux64x86
MOSEK_INC = $(MOSEK_ROOT)/h
MOSEK_LIB = $(MOSEK_ROOT)/bin
GUROBI_HOME ?= $(shell ls -d /rds/general/user/wwiesema/home/gurobi*/linux64 /home/wwiesema/gurobi*/linux64 /opt/gurobi*/linux64 2>/dev/null | sort -V | tail -n 1)
GUROBI_INC = $(GUROBI_HOME)/include
GUROBI_LIB = $(GUROBI_HOME)/lib

LDFLAGS = \
  -L$(CPLEX_LIB) -L$(CONCERT_LIB) \
  -L$(MOSEK_LIB) \
  -L$(GUROBI_LIB) \
  -Wl,-rpath,$(MOSEK_LIB) \
  -Wl,-rpath,$(GUROBI_LIB)
LDLIBS = \
  -lilocplex -lcplex -lconcert \
  -lfusion64 -lmosek64 \
  -lgurobi_c++ -lgurobi130 -lpthread -ldl -lstdc++fs

SOURCES = \
  $(ROOT)/tests/runtime/bellman/l1/run_l1_bellman_runtime.cpp \
  $(ROOT)/algorithms/mdp.cpp \
  $(ROOT)/algorithms/fast/bellman/bellman.cpp \
  $(ROOT)/algorithms/fast/l1/projection/project.cpp \
  $(ROOT)/algorithms/cplex/l1/bellman/bellman.cpp \
  $(ROOT)/algorithms/gurobi/l1/bellman/bellman.cpp \
  $(ROOT)/algorithms/mosek/l1/bellman/bellman.cpp

TARGET = l1_bellman_runtime
HOMOTOPY_TARGET = l1_homotopy_bellman_runtime

all: $(TARGET) $(HOMOTOPY_TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -I$(CPLEX_INC) -I$(CONCERT_INC) -I$(MOSEK_INC) -I$(GUROBI_INC) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET) $(HOMOTOPY_TARGET)

HOMOTOPY_SOURCES = \
  $(ROOT)/tests/runtime/bellman/l1/run_l1_homotopy_bellman_runtime.cpp \
  $(ROOT)/algorithms/mdp.cpp \
  $(ROOT)/algorithms/homotopy/bellman/bellman.cpp

$(HOMOTOPY_TARGET): $(HOMOTOPY_SOURCES)
	$(CXX) $(HOMOTOPY_CXXFLAGS) $(HOMOTOPY_SOURCES) -lstdc++fs -o $@

.PHONY: all clean
