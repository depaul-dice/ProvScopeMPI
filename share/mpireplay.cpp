
/* 
 * Assumptions:
 * 1. The MPI program is written in C
 * 2. We don't mix the blocking calls with non-blocking calls <- subject to relaxation
 */

//#define _GNU_SOURCE
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>
#include <string>
#include <vector>
#include <queue>
#include <dlfcn.h>
#include <mpi.h>
#include <algorithm>

#include <map>
#include "parse.h"

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

#define DEBUG_MODE

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

using namespace std;

static vector<string> orders;
static unsigned __order_index = 0;
static map<int, queue<string>> __messages;
static map<string, void *> __requests;

static int (*original_MPI_Init)(
    int *argc, 
    char ***argv
) = NULL;

static int (*original_MPI_Finalize)(
    void
) = NULL;

/* #ifdef __RECORD__MPIFUNCS__ */

static int (*original_MPI_Recv)(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) = NULL;

static int (*original_MPI_Irecv)(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = NULL;

static int (*original_MPI_Isend)(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = NULL;

static int (*original_MPI_Test)(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) = NULL;

static int (*original_MPI_Testany)(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    int *flag, 
    MPI_Status *status
) = NULL;

static int (*original_MPI_Testall)(
    int count, 
    MPI_Request array_of_requests[], 
    int *flag, 
    MPI_Status array_of_statuses[]
) = NULL;

static int (*original_MPI_Testsome)(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) = NULL;

static int (*original_MPI_Test_cancelled)(
    const MPI_Status *status, 
    int *flag
) = NULL;

static int (*original_MPI_Wait)(
    MPI_Request *request, 
    MPI_Status *status
) = NULL;

static int (*original_MPI_Waitany)(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) = NULL;

static int (*original_MPI_Waitall)(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) = NULL;

static int (*original_MPI_Waitsome)(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) = NULL;

static int (*original_MPI_Cancel)(
    MPI_Request *request
) = NULL;

static int (*original_MPI_Barrier)(
    MPI_Comm comm
) = NULL;

int MPI_Init(
    int *argc, 
    char ***argv
) {
    if (original_MPI_Init == NULL) {
        original_MPI_Init = (int (*)(int *, char ***)) dlsym(RTLD_NEXT, "MPI_Init");
    }
    int ret = original_MPI_Init(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    string filename = ".record" + to_string(rank) + ".txt";
    FILE *fp = fopen(filename.c_str(), "r");
    if (fp == nullptr) {
        fprintf(stderr, "Error: cannot open file rank%d.txt\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    char* line = nullptr; 
    size_t len = 0;
    ssize_t read;

    while((read = getline(&line, &len, fp)) != -1) {
        if(read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        orders.push_back(line);
    }
    fclose(fp);    

    return ret;
}

int MPI_Recv(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // let's look at who I am supposed to get a message from
    vector<string> msg = parse(orders[__order_index++], ':');
    string func = msg[0];
    int myrank = stoi(msg[1]);
    int src = stoi(msg[2]);
    MPI_ASSERT(func == "MPI_Recv");
    MPI_ASSERT(myrank == rank);
    // if I already have a message from the source, then I can just copy it and give it back
    if(__messages.find(src) != __messages.end() && !__messages[src].empty()) {
        DEBUG0("message found from %d\n", src);
        string message = __messages[src].front();
        __messages[src].pop(); // this shouldn't be a vector but a queue
        strcpy((char*)buf, message.c_str());
        if(status != MPI_STATUS_IGNORE) {
            status->MPI_SOURCE = src;
            status->MPI_TAG = 0;
            status->MPI_ERROR = MPI_SUCCESS;
        }
        return MPI_SUCCESS;
    }

    // if I don't have a message from the source, I call MPI_Recv
    if (original_MPI_Recv == NULL) {
        original_MPI_Recv = (int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *)) dlsym(RTLD_NEXT, "MPI_Recv");
    }
    // while I don't have the message from the right source, I keep calling MPI_Recv
    int rv = original_MPI_Recv(buf, count, datatype, source, tag, comm, status);
    while(status->MPI_SOURCE != src) {
        DEBUG0("src not matching: %d != %d\n", status->MPI_SOURCE, src);
        string message = string((char*)buf);
        __messages[status->MPI_SOURCE].push(message);
        rv = original_MPI_Recv(buf, count, datatype, source, tag, comm, status);
    }
    return rv;
}

int MPI_Irecv(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) {
    if(!original_MPI_Recv) {
        original_MPI_Irecv = reinterpret_cast<int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Irecv"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Irecv(buf, count, datatype, source, tag, comm, request);
    // I just need to keep track of the request
    vector<string> msgs = parse(orders[__order_index++], ':');
    DEBUG0("MPI_Irecv: %s -> %p\n", msgs[3].c_str(), (void *)request);
    MPI_ASSERT(msgs[0] == "MPI_Irecv");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(stoi(msgs[2]) == source);
    MPI_ASSERT(__requests.find(msgs[3]) == __requests.end());
    __requests[msgs[3]] = (void *)request;
    
    return ret;
}

int MPI_Isend(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) {
    if(!original_MPI_Isend) {
        original_MPI_Isend = reinterpret_cast<int (*)(const void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Isend"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Isend(buf, count, datatype, dest, tag, comm, request);
    // I just need to keep track of the request
    vector<string> msgs = parse(orders[__order_index++], ':');
    DEBUG0("MPI_Isend: %s -> %p\n", msgs[3].c_str(), (void *)request);
    MPI_ASSERT(msgs[0] == "MPI_Isend");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(stoi(msgs[2]) == dest);
    MPI_ASSERT(__requests.find(msgs[3]) == __requests.end());
    __requests[msgs[3]] = (void *)request;
    return ret;
}

int MPI_Cancel(
    MPI_Request *request
) {
    DEBUG0("MPI_Cancel\n");
    if(!original_MPI_Cancel) {
        original_MPI_Cancel = reinterpret_cast<int (*)(MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Cancel"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Cancel(request);
    // I just need to keep track that it is cancelled
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERT(ret == MPI_SUCCESS); // what I am just hoping for
    MPI_ASSERT(msgs[0] == "MPI_Cancel");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERT(__requests[msgs[2]] == (void *)request);
    __requests.erase(msgs[2]);
    return ret;
}

int MPI_Test(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) {
    DEBUG0("MPI_Test");
    int rank;
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0(":%d", rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERT(msgs[0] == "MPI_Test");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERT(__requests[msgs[2]] == (void *)request);
    int ret = MPI_SUCCESS;
    if(msgs[3] == "SUCCESS") {
        // call wait and make sure it succeeds
        DEBUG0(":SUCCESS:%p\n", (void *)request);
        ret = original_MPI_Wait(request, status);         
        MPI_ASSERT(ret == MPI_SUCCESS);
        __requests.erase(msgs[2]);
        *flag = 1;
    } else {
        // don't call anything
        DEBUG0(":FAIL:%p\n", (void *)request);
        *flag = 0;
    }
    
    return ret;
}

int MPI_Testsome(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) {
    // record which of the requests were filled in this
    DEBUG0("MPI_Testsome");
    int myrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    DEBUG0(":%d", myrank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    /* fprintf(stderr, "msgs[0]: %s\n", msgs[0].c_str()); */
    MPI_ASSERT(msgs[0] == "MPI_Testsome");
    MPI_ASSERT(stoi(msgs[1]) == myrank);
    int oc = stoi(msgs[2]);
    MPI_ASSERT(msgs.size() == oc + 3);
    if(oc == 0) {
        // don't do anything
        DEBUG0(":0\n");
        return MPI_SUCCESS;
    } else {
#ifdef DEBUG_MODE
        DEBUG0(":%d", oc);
        for(int i = 0; i < oc; i++) {
            DEBUG0(":%s", msgs[3 + i].c_str());
        }
        DEBUG0("\n");
#endif

        if(!original_MPI_Wait) {
            original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
        }

        /* map<MPI_Request *, int> incompleteRequests; */
        MPI_Request *req;
        int ind, ret;
        MPI_Status stat;
        for(int i = 0; i < oc; i++) {
            req = (MPI_Request *)__requests[msgs[3 + i]];
            ind = req - array_of_requests;
            MPI_ASSERT(0 <= ind && ind < incount);
            /* incompleteRequests[req] = ind; */
            ret = original_MPI_Wait(req, &stat);
            MPI_ASSERT(ret == MPI_SUCCESS);
            __requests.erase(msgs[3 + i]);
            array_of_indices[i] = ind;
            if(array_of_statuses != MPI_STATUSES_IGNORE) {
                memcpy(&array_of_statuses[i], &stat, sizeof(MPI_Status));
            }
        }
    
        *outcount = oc;
        return MPI_SUCCESS;
    }

}

#ifdef DEBUG_MODE
int MPI_Barrier(
    MPI_Comm comm
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Barrier Called:%d\n", rank);
    if(!original_MPI_Barrier) {
        original_MPI_Barrier = reinterpret_cast<int (*)(MPI_Comm)>(dlsym(RTLD_NEXT, "MPI_Barrier"));
    }
    return original_MPI_Barrier(comm);
}
#endif
