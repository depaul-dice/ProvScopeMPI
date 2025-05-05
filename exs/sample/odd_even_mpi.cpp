#include <mpi.h>
#include <iostream>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int msg_count;
    if (rank == 0) {
         // Skip program name (argv[0])
        int count = argc - 1;
        MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Convert all arguments at once
        int* values = new int[count];
        for(int i = 0; i < count; i++) {
            values[i] = std::stoi(argv[i + 1]);
            // Keep the special handling of number 3
            if (values[i] == 3) {
                values[i] += 1;
            }
        }

        MPI_Send(values, count, MPI_INT, 1, 0, MPI_COMM_WORLD);
        delete[] values;
    }
    else {
        MPI_Bcast(&msg_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Receive all numbers
        int* values = new int[msg_count];
        MPI_Recv(values, msg_count, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Process all numbers
        for(int i = 0; i < msg_count; i++) {
            if (values[i] % 2 == 0) {
                std::cout << values[i] << " is even\n";
            } else {
                std::cout << values[i] << " is odd\n";
            }
        }
        delete[] values;
    }

    MPI_Finalize();
    return 0;
}
