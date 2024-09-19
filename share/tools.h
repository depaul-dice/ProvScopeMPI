
#ifndef TOOLS_H
#define TOOLS_H

#include <vector>
#include <string>
#include <unordered_set>
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
            fprintf(stderr, "line: %d, rank: %d, assertion failed: %s\n", __LINE__, rank, #CONDITION); \
            MPI_Abort(MPI_COMM_WORLD, 1); \
        } \
    } while(0)

#define MPI_EQUAL(A, B) \
    do { \
        mpi_equal(A, B); \
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

#else
#define DEBUG(...)
#define DEBUG0(...)
#define open_debugfile()
#define close_debugfile()
#endif // DEBUG_MODE

std::vector<std::string> parse(std::string line, char delimit);
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

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    os << "[";
    if(!vec.empty()) {
        for(unsigned i = 0; i < vec.size(); ++i)
            os << vec[i] << std::endl;
    }
    os << "]";
    return os;
}

template <typename T> 
std::ostream& operator<<(std::ostream& os, const std::vector<std::shared_ptr<T>>& vec) {
    os << "[";
    if(!vec.empty()) {
        for(unsigned i = 0; i < vec.size(); ++i)
            os << *(vec[i]) << std::endl;
    }
    os << "]";
    return os;
}

template <typename T>
std::unordered_set<T> operator+=(std::unordered_set<T>& lhs, const std::unordered_set<T>& rhs) {
    lhs.insert(rhs.begin(), rhs.end());
    return lhs;
}

class Logger {
public:
    Logger();
    ~Logger();

    template <typename T>
    Logger& operator << (const T& message);
    Logger& operator << (std::ostream& (*manip)(std::ostream&));
private:
    int rank;
    int initialized;
};

template <typename T>
Logger& Logger::operator << (const T& message) {
    if(rank == -1 && !initialized) {
        MPI_Initialized(&initialized);
        if(initialized) {
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        } else {
            std::cerr << "called logger too early" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    if(rank == 0) {
        std::cerr << message;
    }
    return *this;
}
/* void segfault_handler(int sig, siginfo_t *info, void *ucontext); */

std::string replaceall(std::string& str, const std::string& from, const std::string& to);

void splitNinsert(const std::string& str, const std::string& delimit, std::unordered_set<std::string>& container);

template <typename T>
void mpi_equal(T a, T b) {
    if(a != b) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        std::cerr << "rank: " << rank << ", " << a << " != " << b << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

/* 
 * below is a recording functions for each mpi functions 
 */
void recordMPIIprobeSuccess(
        FILE *recordFile,
        int rank,
        int source,
        int tag,
        MPI_Status *stat,
        unsigned long nodeCount);

// helper function to record mpi probe
void recordMPIProbe (
        FILE *recordFile,
        int rank,
        int source,
        int tag,
        MPI_Status *status,
        unsigned long nodeCount);

#endif // TOOLS_H
       
