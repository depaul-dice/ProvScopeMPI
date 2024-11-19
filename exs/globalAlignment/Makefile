
simplestExample: simplestExample.o
	mpicc -o simplestExample simplestExample.o 

simplestExample.o: simplestExample.c
	mpicc -c simplestExample.c

figure: figure.o
	mpicxx -o figure figure.o

figure.o: figure.cpp
	mpicxx -c $<

.PHONY: clean
clean:
	rm -f simplestExample figure *.o .*.o.bc
