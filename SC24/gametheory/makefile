
TARGET = mpi-gametheory
OBJS = mpi-gametheory.o
SRCS = mpi-gametheory.c

.PHONY: clean

all: $(TARGET)

mpi-gametheory.mod.bc: mpi-gametheory.bc
	opt -load ~/yuta_experiment/bbprinter/skeleton/build/libSkeletonPass.so --hello <mpi-gametheory.bc> mpi-gametheory.mod.bc

mpi-gametheory.bc: $(TARGET)
	extract-bc $(TARGET) -o mpi-gametheory.bc

$(TARGET): 
	mpicc -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET) $(OBJS)
