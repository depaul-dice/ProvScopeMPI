
.PHONY: clean all
# SHARED_LIB = libmpirecord.so libmpireplay.so libmpireproduce.so
SHARED_LIB = libmpirecord.so libmpireproduce.so 
STD = -std=c++20
LD_FLAGS = -ldl
LD_FLAGS_LT = -ldl -lcgraph -lgvc
CC = mpicc
CXX = mpicxx
SRCS = mpiRecord.cpp mpiReplay.cpp mpiReproduce.cpp \
	   tools.cpp alignment.cpp loops.cpp messagePool.cpp messageTools.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(SHARED_LIB)

libmpirecord.so: mpiRecord.o tools.o alignment.o loops.o messagePool.o messageTools.o
	$(CXX) $(STD) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS_LT)

libmpireplay.so: mpiReplay.o tools.o alignment.o
	$(CXX) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS)

libmpireproduce.so: mpiReproduce.o tools.o alignment.o loops.o messagePool.o messageTools.o
	$(CXX) $(STD) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS_LT)

# Explicit rule for loops.o
loops.o: loops.cpp loops.h
	$(CXX) $(STD) -c -fPIC -g -DDEBUG_MODE -I/usr/include/graphviz -o $@ $<

%.o: %.cpp
	$(CXX) $(STD) -c -fPIC -g -DDEBUG_MODE -o $@ $<

clean:
	rm -f *.o $(SHARED_LIB)
