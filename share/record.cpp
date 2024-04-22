
#include <cstdio>
/* #include <vector> */
/* #include <iostream> */
#include <mpi.h>

using namespace std;

static FILE *traceFile = nullptr;

extern "C" void printBBname(const char *name) {
    /* fprintf(stderr, "this printBBname should not be called\n"); */
    int flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    if(flag1 && !flag2) {
        char filename[100];
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(traceFile == nullptr) {
            sprintf(filename, ".record%d.tr", rank);
            traceFile = fopen(filename, "w");
            if(traceFile == nullptr) {
                fprintf(stderr, "Error: Cannot open file %s\n", filename);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        /* MPI_ASSERT(traceFile != nullptr); */        
        fprintf(traceFile, "%s\n", name);
    }
}



