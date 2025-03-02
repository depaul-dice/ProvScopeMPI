
## TODOs if you face an issue ##
1. when dealing with the global alignment, you don't see the same send nodes, check if it's happening at Testsome, Testall, Testany, or equivalent of all the wait families, and see if they have the messages from the same source.

2. You should check if you are aligned at MPI\_Irecv if you are to check the same send nodes at MPI\_Test and Wait family. This is because it could be not aligned with one, but not the other, in that case, what would you do?

3. Inspect A LOT before you actually implement full-blown global alignment. Is it really necessary for the target program that you are looking at? Or, is that just because of the bug you introduced?

## Some Bugs I Faced during The Development of This Project ##
1. **Bug**: When I was working on amg2013, the send message was corrupt, and I could not figure out why because the message being sent was correct, but not on the other end.

   **Solution**: For MPI\_Isend, I used the data with value in the stack and not in a heap, so whenever the function returned, the values were overwritten. So I changed it to write the sending message to heap instead and that worked like a charm.

2. **Bug**: When I was working on amg2013, after getting the value from PMPI\_Test, MPI\_Stat.MPI\_SOURCE was returning MPI\_ANY\_SOURCE, and this is still a mystery.

   **Solution**: Because this only happened at 1 place in MPI\_Test with MPI\_Irecv, and in MPI\_Irecv, the source was not MPI\_ANY\_SOURCE, so I just used the source from MPI\_Irecv and that worked.

```bash
opt -load ~/ProvScopeMPI/skeleton/build/libSkeletonPass.so --bbprinter -o ex3.mod.bc < ex3.bc

mpicxx ex3.mod.bc /home/xulu/ProvScopeMPI/share/recorddummy.cpp -o ex3.mod.record -L/home/xulu/ProvScopeMPI/share/ -lmpirecord  -Wl,-rpath,/home/xulu/ProvScopeMPI/share/

time mpirun -np 4 ./ex3.mod.record -n 5 -solver 1 -v 1 1

mpicxx ex3.mod.bc /home/xulu/ProvScopeMPI/share/recorddummy.cpp -o ex3.mod.reproduce -L/home/xulu/ProvScopeMPI/share/ -lmpireproduce -Wl,-rpath,/home/xulu/ProvScopeMPI/share/

time mpirun -np 4 ./ex3.mod.reproduce -n 5 -solver 1 -v 2 2

cat pod*
```