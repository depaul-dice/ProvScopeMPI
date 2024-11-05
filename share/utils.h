
#ifndef TOOLS_H
#define TOOLS_H

#include <vector>
#include <string>
#include <sstream>
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
#include <unordered_map>
#include <stdexcept>

#define MPI_ASSERT(CONDITION) \
    do { \
        if(!(CONDITION)) { \
            int rank; \
            MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
            fprintf(stderr, "line: %d, rank: %d, func: %s assertion failed: %s\n", __LINE__, rank, __func__, #CONDITION); \
            printStackTrace(); \
            MPI_Abort(MPI_COMM_WORLD, 1); \
        } \
    } while(0)

#define FUNCGUARD() \
    do { \
        int rank; \
        MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
        fprintf(stderr, "rank: %d, %s is not supported now\n", rank, __func__); \
        MPI_Abort(MPI_COMM_WORLD, 1); \
    } while(0)

#define MPI_EQUAL(A, B) \
    do { \
        mpi_equal(A, B, __LINE__, __func__, #A, #B); \
    } while(0)

#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG_MODE

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
//#define DEBUG(...)
//#define DEBUG0(...)
#define open_debugfile()
#define close_debugfile()
#endif // DEBUG_MODE

const char msgDelimiter = '|';

std::vector<std::string> parse(
        std::string line, char delimit);
// returns the rank where you got the message from
int lookahead(
        std::vector<std::string>& orders, 
        unsigned start, 
        std::string& request);

void printtails(
        std::vector<std::vector<std::string>>& traces, unsigned tail);

template <typename T>
std::ostream& operator<<(
        std::ostream& os, const std::deque<T>& deq) {
    os << "[";
    if(!deq.empty()) {
        for(unsigned i = 0; i < deq.size(); ++i)
            os << deq[i] << std::endl;
    }
    os << "]";
    return os;
}

template <typename T>
std::ostream& operator<<(
        std::ostream& os, const std::deque<std::shared_ptr<T>>& deq) {
    os << "[";
    if(!deq.empty()) {
        for(unsigned i = 0; i < deq.size(); ++i)
            os << *(deq[i]) << std::endl;
    }
    os << "]";
    return os;
}

template <typename T>
std::ostream& operator<<(
        std::ostream& os, const std::vector<T>& vec) {
    os << "[";
    if(!vec.empty()) {
        for(unsigned i = 0; i < vec.size(); ++i)
            os << vec[i] << std::endl;
    }
    os << "]";
    return os;
}

template <typename T> 
std::ostream& operator<<(
        std::ostream& os, const std::vector<std::shared_ptr<T>>& vec) {
    os << "[";
    if(!vec.empty()) {
        for(unsigned i = 0; i < vec.size(); ++i)
            os << *(vec[i]) << std::endl;
    }
    os << "]";
    return os;
}

template <typename T>
std::unordered_set<T> operator+=(
        std::unordered_set<T>& lhs, const std::unordered_set<T>& rhs) {
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

std::string replaceall(
        std::string& str, 
        const std::string& from, 
        const std::string& to);

std::string splitNinsert(
        const std::string& str, 
        const std::string& delimit, 
        std::unordered_set<std::string>& container);

template <typename T>
void mpi_equal(
        T a, 
        T b, 
        int line, 
        const char* func,
        const char* A,
        const char* B) {
    if(a != b) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        std::cerr << "line: " << line 
            << ", function: " << func 
            << ", rank: " << rank 
            << ", " << A << ':' << a << " != " 
            << B << ':' << b << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

void mpi_equal(
        std::string a, 
        char* b, 
        int line, 
        const char* func,
        const char* A,
        const char* B);
void mpi_equal(
        char* a, 
        std::string b, 
        int line, 
        const char* func,
        const char* A,
        const char* B);
void mpi_equal(
        std::string a, 
        std::string b, 
        int line, 
        const char* func,
        const char* A,
        const char* B);

void exceptionAssert(
        bool condition, const std::string& message = "runtime error");

void printStackTrace();

void segfaultHandler(
        int sig, 
        siginfo_t *info, 
        void *ucontext);

void setupSignalHandler();

#endif // TOOLS_H
       
