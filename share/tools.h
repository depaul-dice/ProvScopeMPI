
#ifndef TOOLS_H
#define TOOLS_H

#include <vector>
#include <string>
#include <set>
#include <mpi.h>
#include <deque>
#include <memory>
#include <signal.h>
#include <execinfo.h>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

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

void open_debugfile();
void close_debugfile();
/* void DEBUG(const char* format, ...); */
#define INSTALL_HANDLER() install_segfault_handler()

#else
#define DEBUG(...)
#define DEBUG0(...)
#define open_debugfile()
#define close_debugfile()
#define INSTALL_HANDLER() ((void)0)
#endif // DEBUG_MODE

std::vector<std::string> parse(std::string& line, char delimit);
// returns the rank where you got the message from
int lookahead(std::vector<std::string>& orders, unsigned start, std::string& request);

void printtails(std::vector<std::vector<std::string>>& traces, unsigned tail);

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::deque<T>& deq) {
    os << "[";
    if(!deq.empty()) {
        for(unsigned i = 0; i < deq.size(); ++i)
            os << deq[i] << std::endl;
    }
    os << "]";
    return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::deque<std::shared_ptr<T>>& deq) {
    os << "[";
    if(!deq.empty()) {
        for(unsigned i = 0; i < deq.size(); ++i)
            os << *(deq[i]) << std::endl;
    }
    os << "]";
    return os;
}

void install_segfault_handler();
#endif // TOOLS_H
       
