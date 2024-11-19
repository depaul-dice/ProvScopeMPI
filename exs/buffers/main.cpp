#include <mpi.h>
#include <cstdio>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Request requests[100];
    if (rank == 0) {
        /*
         * send a relatively large message to rank 1
         */
        int base = 0;
        double messages[1000];
        for (int i = 0; i < 100; i++) {
            for(int j = 0; j < 1000; j++) {
                messages[j] = base + j;
            }
            MPI_Isend(messages, 1000, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD, &requests[i]);
            base += 1000;
        }
    } else {
        double messages[100][1000];
        for (int i = 0; i < 100; i++) {
            MPI_Irecv(messages[i], 1000, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &requests[i]);
        }
        MPI_Waitall(100, requests, MPI_STATUSES_IGNORE);
        for (int i = 0; i < 100; i++) {
            for (int j = 0; j < 1000; j++) {
                printf("%d ", (int)(messages[i][j]));
            }
            printf("\n");
        }
    }
    MPI_Finalize();
    return 0;
}
