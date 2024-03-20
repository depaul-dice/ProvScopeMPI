EXEC = mpi-particle

.PHONY: clean
CC = mpicc


mpi-particle.bc: $(EXEC)
	extract-bc $(EXEC) -o $@

$(EXEC): mpi-particle.c
	$(CC) -o $@ $< -lm

clean:
	rm -f $(EXEC) mpi-particle.bc
