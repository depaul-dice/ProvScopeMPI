
#include <cstdio>
#include <vector>
#include <iostream>
#include <mpi.h>

using namespace std;

vector<FILE *> recordFiles(16, nullptr);

/* extern "C" void preprocess(const char *dummy); */
extern "C" void printBBname(const char *name);
/* extern "C" void printFuncCall(const char *name); */
/* extern "C" void printReturn(const char *name); */
extern "C" void endMain(const char *dummy);

/* void preprocess(const char *dummy) { */
/*     // do nothing */
/* } */

void printBBname(const char *name) {
    int rank, flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    if(flag1 && !flag2) {
        char filename[100];
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(recordFiles[rank] == nullptr) {
            sprintf(filename, ".record%d.tr", rank);
            recordFiles[rank] = fopen(filename, "w");
            if(recordFiles[rank] == nullptr) {
                fprintf(stderr, "Error: Cannot open file %s\n", filename);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        fprintf(recordFiles[rank], "BB:%s:%d\n", name, rank);
    }
}

/* void printFuncCall(const char *name) { */
/*     /1* fprintf(stderr, "printFuncCall: %s\n", name); *1/ */
/*     int rank, flag1, flag2; */
/*     MPI_Initialized(&flag1); */
/*     MPI_Finalized(&flag2); */
/*     if(flag1 && !flag2) { */
/*         char filename[100]; */
/*         MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*         if(recordFiles[rank] == nullptr) { */
/*             sprintf(filename, ".record%d.tr", rank); */
/*             recordFiles[rank] = fopen(filename, "w"); */
/*             if(recordFiles[rank] == nullptr) { */
/*                 fprintf(stderr, "Error: Cannot open file %s\n", filename); */
/*                 MPI_Abort(MPI_COMM_WORLD, 1); */
/*             } */
/*         } */
/*         fprintf(recordFiles[rank], "Call:%s:%d\n", name, rank); */
/*         /1* cerr << "Function Call:" << name << ":" << rank << endl; *1/ */
/*     } */ 
/* } */

/* void printReturn(const char *name) { */
/*     /1* fprintf(stderr, "printReturn: %s\n", name); *1/ */
/*     int rank, flag1, flag2; */
/*     MPI_Initialized(&flag1); */
/*     MPI_Finalized(&flag2); */
/*     if(flag1 && !flag2) { */
/*         char filename[100]; */
/*         MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*         if(recordFiles[rank] == nullptr) { */
/*             sprintf(filename, ".record%d.tr", rank); */
/*             recordFiles[rank] = fopen(filename, "w"); */
/*             if(recordFiles[rank] == nullptr) { */
/*                 fprintf(stderr, "Error: Cannot open file %s\n", filename); */
/*                 MPI_Abort(MPI_COMM_WORLD, 1); */
/*             } */
/*         } */
/*         fprintf(recordFiles[rank], "Return:%s:%d\n", name, rank); */
/*     } */ 
/* } */

void endMain(const char *dummy) {
}
