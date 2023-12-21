#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

static int compar(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

static void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void mergeHigh(int *local_data, int *tmp_data, int local_n, FILE *fp) {
    for(int i = 0; i < local_n; i++) {
        for(int j = 0; j < 2 * local_n - 1; j++) {
            if(j < local_n - 1 && tmp_data[j] > tmp_data[j + 1]) {
                swap(&tmp_data[j], &tmp_data[j + 1]);
            } else if(j == local_n - 1 && tmp_data[j] > local_data[0]) {
                swap(&tmp_data[j], &local_data[0]);
                /* fprintf(fp, "swapped %d and %d, now: ", tmp_data[j], local_data[0]); */
                /* for(int k = 0; k < local_n; k++) { */
                    /* fprintf(fp, "%d,", local_data[k]); */
                /* } */
                /* fprintf(fp, "\n"); */
            } else if(j > local_n - 1 && local_data[j - local_n] > local_data[j - local_n + 1]) {
                swap(&local_data[j - local_n], &local_data[j - local_n + 1]);
                /* fprintf(fp, "swapped %dth and %dth element, now: ", j - local_n, j - local_n + 1); */
                /* for(int k = 0; k < local_n; k++) { */
                    /* fprintf(fp, "%d,", local_data[k]); */
                /* } */
                /* fprintf(fp, "\n"); */
            }
        }
    }
}

void mergeLow(int *local_data, int *tmp_data, int local_n, FILE *fp) {
    /* fprintf(fp, "checking tmp_data: "); */
    /* for(int k = 0; k < local_n; k++) { */
    /*     fprintf(fp, "%d,", tmp_data[k]); */
    /* } */
    /* fprintf(fp, "\n"); */

    for(int i = 0; i < local_n; i++) {
        for(int j = (2 * local_n) - 2; j >= 0; j--) {
            if(j < local_n - 1 && local_data[j] > local_data[j + 1]) {
                swap(&local_data[j], &local_data[j + 1]);
                /* fprintf(fp, "swapped %dth and %dth element, now: ", j, j + 1); */
                /* for(int k = 0; k < local_n; k++) { */
                /*     fprintf(fp, "%d,", local_data[k]); */
                /* } */
                /* fprintf(fp, "\n"); */
            } else if(j == local_n - 1 && local_data[j] > tmp_data[0]) {
                /* fprintf(fp, "swapped %d and %d, now: ", local_data[j], tmp_data[0]); */
                swap(&local_data[j], &tmp_data[0]);
                /* for(int k = 0; k < local_n; k++) { */
                /*     fprintf(fp, "%d,", local_data[k]); */
                /* } */
                /* fprintf(fp, "\n"); */
            } else if(j > local_n - 1 && tmp_data[j - local_n] > tmp_data[j - local_n + 1]) {
                swap(&tmp_data[j - local_n], &tmp_data[j - local_n + 1]);
                /* fprintf(fp, "swapped %dth and %dth element, now: ", j - local_n -1, j - local_n); */
                /* for(int k = 0; k < local_n; k++) { */
                /*     fprintf(fp, "%d,", tmp_data[k]); */
                /* } */
                /* fprintf(fp, "\n"); */
            }
        }
    }
}

void OddEven_iter(int *local_data, int *temp_B, int *temp_C, int local_n, int phase, \
        int even_partner, int odd_partner, int my_rank, MPI_Comm comm) {
    MPI_Status status;
    char filename [256];
    snprintf(filename, sizeof(filename), "output_%d.txt", my_rank);
    FILE *fp = fopen(filename, "a");
    /* for (int i = 0; i < local_n; i++) { */
    /*     fprintf(fp, "%d,", local_data[i]); */
    /* } */
    /* fprintf(fp, "\n"); */

    if(phase % 2 == 0) {
        if(even_partner >= 0) {
            MPI_Sendrecv(local_data, local_n, MPI_INT, even_partner, 0, \
                    temp_B, local_n, MPI_INT, even_partner, 0, comm, &status);
            /* fprintf(fp, "with even_partner: %d; ", even_partner); */
            /* for(int i = 0; i < local_n; i++) { */
            /*     fprintf(fp, "%d,", temp_B[i]); */
            /* } */
            /* fprintf(fp, "\n"); */
            if(my_rank % 2 != 0) {
                /* fprintf(fp, "merge with high\n"); */
                mergeHigh(local_data, temp_B, local_n, fp);
            } else {
                /* fprintf(fp, "merge with low\n"); */
                mergeLow(local_data, temp_B, local_n, fp);
            }
        }
    } else {
        if(odd_partner >= 0) {
            MPI_Sendrecv(local_data, local_n, MPI_INT, odd_partner, 0, \
                    temp_C, local_n, MPI_INT, odd_partner, 0, comm, &status);
            /* fprintf(fp, "with odd_partner: %d; ", odd_partner); */
            /* for(int i = 0; i < local_n; i++) { */
            /*     fprintf(fp, "%d,", temp_C[i]); */
            /* } */
            /* fprintf(fp, "\n"); */
            if(my_rank % 2 != 0) {
                /* fprintf(fp, "merge with low\n"); */
                mergeLow(local_data, temp_C, local_n, fp);
            } else {
                /* fprintf(fp, "merge with high\n"); */
                mergeHigh(local_data, temp_C, local_n, fp);
            }
        }
    }

    /* for(int i = 0; i < local_n; i++) { */
        /* fprintf(fp, "%d,", local_data[i]); */
    /* } */
    /* fprintf(fp, "\n"); */
    fclose(fp);
}

void Sort(int *local_data, int local_n, int my_rank, int comm_sz, MPI_Comm comm) {
    int phase;
    /* int temp_B [4]; */
    /* int temp_C [4]; */
    int even_partner, odd_partner;

    int *temp_B = (int *)malloc(local_n * sizeof(int));
    int *temp_C = (int *)malloc(local_n * sizeof(int));
    if(my_rank % 2 != 0) {
        even_partner = my_rank - 1;
        odd_partner = my_rank + 1;
        if (odd_partner == comm_sz) {
            odd_partner = MPI_PROC_NULL;
        }
    } else {
        even_partner = my_rank + 1;
        odd_partner = my_rank - 1;
        if(even_partner == comm_sz) {
            even_partner = MPI_PROC_NULL;
        }
    }

    qsort(local_data, local_n, sizeof(int), compar);

    for(phase = 0; phase < comm_sz; phase++) {
        OddEven_iter(local_data, temp_B, temp_C, local_n, phase, \
                even_partner, odd_partner, my_rank, comm);
    }

    free(temp_B);
    free(temp_C);
}

int main(int argc, char *argv[]) {
    int my_rank, comm_sz;
    /* int *local_data; */
    int global_n, local_n;
    MPI_Comm comm;
    /* double start, finish, loc_elapsed, elapsed; */

    MPI_Init(&argc, &argv);
    comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &comm_sz);
    MPI_Comm_rank(comm, &my_rank);

    global_n = atoi(argv[1]);
    local_n = global_n / comm_sz;

    // Assuming each process has the same number of elements
    int *local_data = malloc(local_n * sizeof(int));

    // Initialize local data here (randomly or otherwise)
    for(int i = local_n - 1; i >= 0; i--) {
        local_data[local_n - 1 - i] = i * local_n + my_rank;
    }
    
    MPI_Barrier(comm);

    Sort(local_data, local_n, my_rank, comm_sz, comm);

    // Output sorted data

    /* printf("rank %d: ", my_rank); */
    /* for(int i = 0; i < local_n; i++) { */
    /*     printf("%d,", local_data[i]); */
    /* } */
    /* printf("\n"); */
    free(local_data);
    MPI_Finalize();
    return 0;
}

