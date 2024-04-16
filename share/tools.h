
#ifndef PARSE_H
#define PARSE_H

#include <vector>
#include <string>

#define MPI_ASSERT(CONDITION) \
    do { \
        if(!(CONDITION)) { \
            int rank; \
            MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
            if(rank == 0) \
                fprintf(stderr, "line: %d, rank: %d, assertion failed: %s\n", __LINE__, rank, #CONDITION); \
            MPI_Abort(MPI_COMM_WORLD, 1); \
        } \
    } while(0)

#ifdef DEBUG_MODE
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG0(...) \
    do { \
        int rank; \
        MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
        if(rank == 0)\
            fprintf(stderr, __VA_ARGS__); \
    } while(0)

#else
#define DEBUG(...)
#define DEBUG0(...)
#endif

std::vector<std::string> parse(std::string& line, char delimit);

#endif // PARSE_H
