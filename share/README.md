
## Some Bugs I Faced during The Development of This Project ##
1. **Bug**: When I was working on amg2013, the send message was corrupt and could not figure out why because the message being sent was correct, but not on the other end. 
   **Solution**: For MPI\_Isend, I was using the data with value in stack and not in heap, so whenever the function returned, the values were overwritten. So I changed it to write the sending message to heap instead and that worked like a charm.

2. **Bug**: When I was working on amg2013, after getting the value from PMPI\_Test, MPI\_Stat.MPI\_SOURCE was returning MPI\_ANY\_SOURCE, and this is still a mystery.
   **Solution**: Because this only happened at 1 place in MPI\_Test with MPI\_Irecv, and in MPI\_Irecv, the source was not MPI\_ANY\_SOURCE, so I just used the source from MPI\_Irecv and that worked.
