
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if(size != 2) {
        if(rank == 0) {
            printf("This program requires exactly 2 MPI ranks\n");
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }
    if(rank == 0) {
        if(argc == 2) {
            char *buf = "hello\n";
            MPI_Send(buf, 7, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        } else {
            char *buf = "world\n";
            MPI_Send(buf, 7, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        }
    } else {
        char buf[7];
        MPI_Recv(buf, 7, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("%s", buf);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}
