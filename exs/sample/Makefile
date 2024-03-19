main: main.c
	mpicc -o $@ $<

original: main
	mpirun -n 3 ./main origInput.txt

reproduced: main
	mpirun -n 3 ./main reproducedInput.txt

.PHONY: clean
clean:
	rm -f main

