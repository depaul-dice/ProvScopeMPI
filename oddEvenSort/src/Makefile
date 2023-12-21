EXEC = oddEvenSort 

EXEC: $(EXEC).c
	mpicc $(EXEC).c -o $(EXEC)

.PHONY: clean
clean:
	rm -f output_*.txt $(EXEC)
