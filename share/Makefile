
.PHONY: clean all
SHARED_LIB = libmpirecord.so libmpireplay.so libmpireproduce.so
LD_FLAGS = -ldl
LD_FLAGS_LT = -ldl -lcgraph -lgvc
CC = mpicc
CXX = mpicxx
SRCS = mpirecord.cpp mpireplay.cpp mpireproduce.cpp tools.cpp alignment.cpp loops.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(SHARED_LIB)

libmpirecord.so: mpirecord.o tools.o 
	$(CXX) -shared -fPIC -o $@ $^ $(LD_FLAGS)

libmpireplay.so: mpireplay.o tools.o alignment.o
	$(CXX) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS)

libmpireproduce.so: mpireproduce.o tools.o alignment.o loops.o
	$(CXX) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS_LT)

# Explicit rule for loops.o
loops.o: loops.cpp loops.h
	$(CXX) -c -fPIC -g -DDEBUG_MODE -I/usr/include/graphviz -o $@ $<

%.o: %.cpp
	$(CXX) -c -fPIC -g -DDEBUG_MODE -o $@ $<

clean:
	rm -f *.o $(SHARED_LIB)
