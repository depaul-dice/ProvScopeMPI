#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

void f() {
}
void g() {
}
void h() {
}

int Routine(int source, int data) {
    if(source == 2) {
        printf("option 1\n");
        f();
    } else {
        if(data % 2 == 0) {
            printf("option 2\n");
            g();
        } else {
            printf("option 3\n");
            h();
        }
    }
    return 0;
}

// this is the main process
int rank0(int rank) {
    int data = 42;

    MPI_Status status;

    MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD,  &status);
    printf("Process %d received data %d from Process %d\n", rank, data, status.MPI_SOURCE);
    Routine(status.MPI_SOURCE, data);

    MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD,  &status);
    printf("Process %d received data %d from Process %d\n", rank, data, status.MPI_SOURCE);
    Routine(status.MPI_SOURCE, data);

    return 0;
}

// this is the process that reads different files
int rank1(char *filename, int rank) {
    int data = 42;
    char buf [10];

    FILE *fp = fopen(filename, "r");
    if(!fp) {
        perror("Error opening file");
        return 1;
    }
    size_t size = fread(buf, 1, 9, fp);
    buf[size - 1] = '\0';
    fclose(fp);
    data = data + atoi(buf);
    MPI_Send(&data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    return 0;
}

// this process just receives the message from rank0 and waits a little and pass it back 
int rank2(int rank) {
    int data = 42;
    MPI_Send(&data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    return 0;
}

int main(int argc, char** argv) {
  int rank, size, data = 0;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) {
      rank0(rank);
  } else if (rank == 1) {
      char *filename = argv[1];
      rank1(filename, rank);
  } else {
      rank2(rank);
  }

  MPI_Finalize();
  return 0;
}
