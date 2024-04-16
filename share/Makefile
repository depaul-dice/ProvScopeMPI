
.PHONY: clean all
SHARED_LIB = libmpirecord.so libmpireplay.so
LD_FLAGS = -ldl -lmpi
CC = mpicc
CXX = mpicxx
SRCS = mpirecord.cpp mpireplay.cpp parse.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(SHARED_LIB)

libmpirecord.so: mpirecord.o
	$(CXX) -shared -fPIC -o $@ $^ $(LD_FLAGS)

libmpireplay.so: mpireplay.o parse.o
	$(CXX) -shared -fPIC -o $@ $^ $(LD_FLAGS)

%.o: %.cpp
	$(CXX) -c -fPIC -o $@ $<

clean:
	rm -f *.o $(SHARED_LIB)
