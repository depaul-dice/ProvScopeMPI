
.PHONY: clean all
# SHARED_LIB = libmpirecord.so libmpireplay.so libmpireproduce.so
SHARED_LIB = libmpirecord.so libmpireproduce.so
LD_FLAGS = -ldl
LD_FLAGS_LT = -ldl -lcgraph -lgvc
CC = mpicc
CXX = mpicxx
SRCS = mpirecord.cpp mpireplay.cpp mpireproduce.cpp \
	   tools.cpp alignment.cpp loops.cpp messagePool.cpp messageTools.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(SHARED_LIB)

libmpirecord.so: mpirecord.o tools.o alignment.o loops.o messagePool.o messageTools.o
	$(CXX) -std=c++11 -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS_LT)

# libmpireplay.so: mpireplay.o tools.o alignment.o
	# $(CXX) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS)

libmpireproduce.so: mpireproduce.o tools.o alignment.o loops.o messagePool.o messageTools.o
	$(CXX) -std=c++11 -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS_LT)

# Explicit rule for loops.o
loops.o: loops.cpp loops.h
	$(CXX) -c -fPIC -g -DDEBUG_MODE -I/usr/include/graphviz -o $@ $<

%.o: %.cpp
	$(CXX) -c -fPIC -g -DDEBUG_MODE -o $@ $<

clean:
	rm -f *.o $(SHARED_LIB)
