
.PHONY: clean all
SHARED_LIB = libmpirecord.so libmpireplay.so
LD_FLAGS = -ldl 
CC = mpicc
CXX = mpicxx
SRCS = mpirecord.cpp mpireplay.cpp tools.cpp alignment.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(SHARED_LIB)

libmpirecord.so: mpirecord.o tools.o 
	$(CXX) -shared -fPIC -o $@ $^ $(LD_FLAGS)

libmpireplay.so: mpireplay.o tools.o alignment.o
	$(CXX) -shared -fPIC -g -DDEBUG_MODE -o $@ $^ $(LD_FLAGS)

%.o: %.cpp
	$(CXX) -c -fPIC -g -DDEBUG_MODE -o $@ $<

clean:
	rm -f *.o $(SHARED_LIB)
