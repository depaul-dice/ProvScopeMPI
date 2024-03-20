
.PHONY: all clean 
all: laplace_mpi 

laplace_mpi: laplace_mpi.c
	mpicc -o laplace_mpi laplace_mpi.c -lm

clean: 
	rm -f laplace_solution.txt a.out laplace_mpi .laplace_mpi.o .laplace_mpi.o.bc
