
#include <cstdio>
#include <mpi.h>
#include <string>

FILE *f = nullptr;

extern "C" void printBBname(char *name) {
    int rank, isInitialized, isFinalized;
    MPI_Initialized(&isInitialized);
    MPI_Finalized(&isFinalized);
    if (!isInitialized || isFinalized) {
        if(f == nullptr) {
            int rank;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            std::string filename 
                = "trace_" + std::to_string(rank) + ".txt"; 
            f = fopen(filename.c_str(), "w");
        }
        fprintf(f, "%s\n", name);
    }
}
