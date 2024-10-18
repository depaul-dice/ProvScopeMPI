# ProvScopeMPI
extension of ProvScope in MPI applications

## Finding The Sample Programs
It is difficult to find the program that shows the divergence in the control flow graphs because of the non-determinism in communication order. This paragraph is simply to record the process of finding them.

What works:
1. MCB <- used ReMPI small sample for it, the reason of non-determinism is because of MPI\_Testsome
2. AMG2013 <- this is the most legit one we have so far, the reason of non-determinism is because of MPI\_ANY\_SOURCE at MPI\_Irecv


What doesn't work:
1. LULESH <- no non-determinism because of message order in the control flow graph
2. Hypre\_Parasails <- no non-determinism because of message order in the control flow graph
