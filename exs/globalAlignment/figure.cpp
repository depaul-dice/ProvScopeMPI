
#include <mpi.h>
#include <cstdlib>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if(size != 2) {
        if(rank == 0) {
            printf("This program requires exactly 2 processes\n");
        }
        MPI_Finalize();
        exit(0);
    }

    if(argc == 2 && rank == 0) {
        int data = atoi(argv[1]);
        MPI_Send(&data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    }

    if(rank == 0) {
        int data [2] = {1, 2};
        MPI_Send(&data[0], 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Send(&data[1], 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    } else {
        int data [2];
        MPI_Recv(&data[0], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&data[1], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Process 1 received data: %d, %d\n", data[0], data[1]);
    }

    if(argc == 2 && rank == 1) {
        int data;
        MPI_Recv(&data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Process 1 received data: %d\n", data);
    }


    MPI_Finalize();
}
