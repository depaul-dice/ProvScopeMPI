#include "mpirecordreplay.h"

static map<MPI_Request *, string> __requests;

FILE *recordFile = nullptr;
FILE *traceFile = nullptr;
static unsigned long nodecnt = 0;
/* extern "C" void printBBname(const char *name); */

#ifdef DEBUG_MODE
/* FILE *recordtraceFile = nullptr; */
/* #define RECORDTRACE(...) \ */
    /* fprintf(recordtraceFile, __VA_ARGS__); \ */
/* #else */
#define RECORDTRACE(...)
#endif


extern "C" void printBBname(const char *name) {

    if(!original_printBBname) {
        original_printBBname = reinterpret_cast<void (*)(const char *)>(
                dlsym(RTLD_NEXT, "printBBname"));
    }

    int rank, flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    if(flag1 && !flag2) {
        char filename[100];
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_ASSERT(traceFile != nullptr);
        fprintf(traceFile, 
                "%s:%lu\n", 
                name, 
                nodecnt++);
        RECORDTRACE("%s\n", name);
    }
    return;
}

int MPI_Init(
    int *argc, 
    char ***argv
) {
    if(!original_MPI_Init) {
        original_MPI_Init = reinterpret_cast<
            int (*)(
                    int *, 
                    char ***)>(
                        dlsym(RTLD_NEXT, "MPI_Init"));
    }
    int ret = original_MPI_Init(argc, argv);
    int rank;
    DEBUG0("MPI_Init\n");
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    string filename = ".record" + to_string(rank) + ".txt";  
    recordFile = fopen(filename.c_str(), "w");
    if(recordFile == nullptr) {
        fprintf(stderr, "failed to open record file\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    filename = ".record" + to_string(rank) + ".tr";
    traceFile = fopen(filename.c_str(), "w");
    if(traceFile == nullptr) {
        fprintf(stderr, "failed to open trace file\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    return ret;
}

int MPI_Finalize(
    void
) {
    //DEBUG0("MPI_Finalize\n");
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(recordFile != nullptr);
    fclose(recordFile);
    MPI_ASSERT(traceFile != nullptr);
    if(!original_MPI_Finalize) {
        original_MPI_Finalize = reinterpret_cast<
            int (*)(void)>(
                    dlsym(RTLD_NEXT, "MPI_Finalize"));
    }
    fflush(traceFile);
    fclose(traceFile);
    int ret = original_MPI_Finalize();
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
    //DEBUG0("MPI_Recv:from %d\n", source); 
    if(!original_MPI_Recv) {
        original_MPI_Recv = reinterpret_cast<int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Recv"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Recv(buf, count, datatype, source, tag, comm, status);
    int source_rank = status->MPI_SOURCE;

    fprintf(recordFile, "MPI_Recv:%d:%d:%lu\n", 
            rank, 
            source_rank, 
            nodecnt);
    RECORDTRACE("MPI_Recv:%d:%d\n", 
            rank, 
            source_rank);
    
    return ret;
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
    DEBUG0("MPI_Irecv:%p\n", request);
    if(!original_MPI_Recv) {
        original_MPI_Irecv = reinterpret_cast<int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Irecv"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Irecv(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            request);
    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Irecv:%d:%d:%p:%lu\n",
            rank, 
            source, 
            request, 
            nodecnt);
    RECORDTRACE("MPI_Irecv:%d:%d:%p\n", 
            rank, 
            source, 
            request);
    __requests[request] = "MPI_Irecv";
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
    DEBUG0("MPI_Isend:%p\n", request);
    if(!original_MPI_Isend) {
        original_MPI_Isend = reinterpret_cast<int (*)(const void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Isend"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Isend(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request);
    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Isend:%d:%d:%p:%lu\n", 
            rank, 
            dest, 
            request, 
            nodecnt);
    RECORDTRACE("MPI_Isend:%d:%d:%p\n", 
            rank, 
            dest, 
            request);
    __requests[request] = "MPI_Isend";
    return ret;
}

// This is an MPI_Isend that needs the receiver to be ready by the time it is called.
// Otherwise, it creates an undefined behavior (e.g. lost messages, program crash, data corruption, etc.).
int MPI_Irsend(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) {
    DEBUG0("MPI_Irsend:%p\n", request);
    if(!original_MPI_Irsend) {
        original_MPI_Irsend = reinterpret_cast<
            int (*)(
                const void *, 
                int, MPI_Datatype, 
                int, 
                int, 
                MPI_Comm, 
                MPI_Request *)>(
                    dlsym(RTLD_NEXT, "MPI_Irsend"));
    }
    int rank;
    MPI_Comm_rank(
            MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Irsend(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request);
    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Irsend:%d:%d:%p:%lu\n", 
            rank, 
            dest, 
            request, 
            nodecnt);
    RECORDTRACE("MPI_Irsend:%d:%d:%p\n", 
            rank, 
            dest, 
            request);
    __requests[request] = "MPI_Irsend";
    return ret;
}

int MPI_Cancel(
    MPI_Request *request
) {
    DEBUG0("MPI_Cancel:%p\n", request);
    if(!original_MPI_Cancel) {
        original_MPI_Cancel = reinterpret_cast<
            int (*)(
                    MPI_Request *)>(
                        dlsym(RTLD_NEXT, "MPI_Cancel"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = original_MPI_Cancel(request);
    // I just need to keep track that it is cancelled
    fprintf(recordFile, "MPI_Cancel:%d:%p:%lu\n", 
            rank, 
            request, 
            nodecnt);
    RECORDTRACE("MPI_Cancel:%d:%p\n", 
            rank, 
            request);
    __requests.erase(request);
    return ret;
}

int MPI_Test(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) {
    DEBUG0("MPI_Test:%p\n", request);
    int rank;
    if(!original_MPI_Test) {
        original_MPI_Test = reinterpret_cast<
            int (*)(
                    MPI_Request *, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Test"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(
            __requests.find(request) != __requests.end());
    MPI_Status stat;
    int ret = original_MPI_Test(
            request, 
            flag, 
            &stat);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &stat, 
                sizeof(MPI_Status));
    }

    // record if the request was completed or not 
    if(*flag) {
        fprintf(recordFile, "MPI_Test:%d:%p:SUCCESS:%d:%lu\n", \
                rank, 
                request, 
                stat.MPI_SOURCE, 
                nodecnt);
        RECORDTRACE("MPI_Test:%d:%p:SUCCESS:%d\n", 
                rank, 
                request, 
                stat.MPI_SOURCE);
        /* __requests.erase(request); */
    } else {
        fprintf(recordFile, "MPI_Test:%d:%p:FAIL:%lu\n", \
                rank, 
                request, 
                nodecnt);
        RECORDTRACE("MPI_Test:%d:%p:FAIL\n", 
                rank, 
                request);
    }
    return ret;
}

// flag == true iff all requests are completed
int MPI_Testall(
    int count, 
    MPI_Request array_of_requests[], 
    int *flag, 
    MPI_Status array_of_statuses[]
) {
    int rank;
    if(!original_MPI_Testall) {
        original_MPI_Testall = reinterpret_cast<
            int (*)(
                    int, 
                    MPI_Request *, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Testall"));
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int outcount; 
    int indices [count];
    MPI_Status stats [count];

    int ret = original_MPI_Testall(
            count, 
            array_of_requests, 
            flag, 
            stats);
    MPI_ASSERT(ret == MPI_SUCCESS);

    DEBUG0("MPI_Testall:%d:%d", rank, count);
    fprintf(recordFile, "MPI_Testall:%d:%d", rank, count);
    RECORDTRACE("MPI_Testall:%d:%d", rank, count);
    if(*flag) {
        fprintf(recordFile, ":SUCCESS");
        RECORDTRACE(":SUCCESS");
        for(int i = 0; i < count; i++) {
            MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
            fprintf(recordFile, ":%p:%d", 
                    &array_of_requests[i], stats[i].MPI_SOURCE);
            RECORDTRACE(":%p:%d", 
                    &array_of_requests[i], stats[i].MPI_SOURCE);
            if(array_of_statuses != MPI_STATUSES_IGNORE) 
                memcpy(
                        &array_of_statuses[i], 
                        &stats[i], 
                        sizeof(MPI_Status));
            DEBUG0(":%p:%d", 
                    &array_of_requests[i], stats[i].MPI_SOURCE);
        }
    } else {
        fprintf(recordFile, ":FAIL");
        RECORDTRACE(":FAIL");
    }
    DEBUG0("\n");
    fprintf(recordFile, ":%lu\n", nodecnt);
    RECORDTRACE("\n");

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
    DEBUG0("MPI_Testsome\n");
    int rank;
    if(!original_MPI_Testsome) {
        original_MPI_Testsome = reinterpret_cast<
            int (*)(
                    int, 
                    MPI_Request *, 
                    int *, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Testsome"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stats [incount];
    int ret = original_MPI_Testsome(
            incount, 
            array_of_requests, 
            outcount, 
            array_of_indices, 
            stats);
    if(array_of_statuses != MPI_STATUSES_IGNORE) {
        for(int i = 0; i < incount; i++) {
            memcpy(
                    &array_of_statuses[i], 
                    &stats[i], 
                    sizeof(MPI_Status));
        }
    }

    fprintf(recordFile, "MPI_Testsome:%d:%d", rank, *outcount);
    RECORDTRACE("MPI_Testsome:%d:%d", rank, *outcount);
    if(*outcount > 0) {
        for(int i = 0; i < *outcount; i++) {
            int ind = array_of_indices[i];
            MPI_ASSERT(
                    __requests.find(&array_of_requests[ind]) != __requests.end());
            fprintf(recordFile, ":%p:%d", 
                    &(array_of_requests[ind]), array_of_statuses[i].MPI_SOURCE); 
            RECORDTRACE(":%p:%d", 
                    &(array_of_requests[ind]), array_of_statuses[i].MPI_SOURCE);
            /* __requests.erase(&array_of_requests[ind]); */
        }
    }
    fprintf(recordFile, ":%lu\n", nodecnt);
    RECORDTRACE("\n");
    return ret;
}

int MPI_Wait(
    MPI_Request *request, 
    MPI_Status *status
) {
    int rank;
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<
            int (*)(
                    MPI_Request *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Wait\n");
    MPI_Status stat;
    int ret = original_MPI_Wait(request, &stat);
    if(status != MPI_STATUS_IGNORE) {
        *status = stat;
    }
    MPI_ASSERT(__requests.find(request) != __requests.end());
    /* string req = __requests[request]; */
    /* __requests.erase(request); */

    if(ret == MPI_SUCCESS) {
        fprintf(recordFile, "MPI_Wait:%d:%p:SUCCESS:%d:%lu\n", \
                rank, 
                request, 
                stat.MPI_SOURCE, 
                nodecnt);
        RECORDTRACE("MPI_Wait:%d:%p:SUCCESS:%d\n", 
                rank, 
                request, 
                stat.MPI_SOURCE);
    } else {
        fprintf(recordFile, "MPI_Wait:%d:%p:FAIL:%lu\n", \
                rank, 
                request, 
                nodecnt);
        RECORDTRACE("MPI_Wait:%d:%p:FAIL\n", 
                rank, request);
    }
    return ret;
}

int MPI_Waitany(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) {
    int rank;
    if(!original_MPI_Waitany) {
        original_MPI_Waitany = reinterpret_cast<int (*)(int, MPI_Request *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Waitany"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    int ret = original_MPI_Waitany(
            count, 
            array_of_requests, 
            index, 
            &stat);
    
    if(status != MPI_STATUS_IGNORE) {
        *status = stat;
    }

    if(*index == MPI_UNDEFINED) {
        fprintf(recordFile, "MPI_Waitany:%d:FAIL:%lu\n", 
                rank, nodecnt);
        RECORDTRACE("MPI_Waitany:%d:FAIL\n", rank);
        DEBUG0("MPI_Waitany:%d:FAIL\n", rank);
    } else {
        MPI_ASSERT(__requests.find(&array_of_requests[*index]) != __requests.end());
        /* string req = __requests[&array_of_requests[*index]]; */
        fprintf(recordFile, "MPI_Waitany:%d:SUCCESS:%p:%d:%lu\n", 
                rank, 
                &array_of_requests[*index], 
                stat.MPI_SOURCE, 
                nodecnt);
        RECORDTRACE("MPI_Waitany:%d:SUCCESS:%p:%d\n", 
                rank, 
                &array_of_requests[*index], 
                stat.MPI_SOURCE);
        DEBUG0("MPI_Waitany:%d:%p\n", 
                *index, &array_of_requests[*index]);
    }

    return ret;
}

// if it is successful, then record all of the requests and the source
int MPI_Waitall(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) {
    int rank;
    if(!original_MPI_Waitall) {
        original_MPI_Waitall = reinterpret_cast<int (*)(int, MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Waitall"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Waitall:");
    MPI_Status stats [count];
    int ret = original_MPI_Waitall(count, array_of_requests, stats);
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Waitall:%d:%d", rank, count);
    RECORDTRACE("MPI_Waitall:%d:%d", rank, count);
    for(int i = 0; i < count; i++) {
        if(array_of_statuses != MPI_STATUSES_IGNORE) {
            memcpy(&array_of_statuses[i], &stats[i], sizeof(MPI_Status));
        }
        fprintf(recordFile, ":%p:%d", &array_of_requests[i], stats[i].MPI_SOURCE);
        RECORDTRACE(":%p:%d", &array_of_requests[i], stats[i].MPI_SOURCE);
        MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
        DEBUG0(":%p:%d", &array_of_requests[i], stats[i].MPI_SOURCE);
    }
    DEBUG0("\n");
    fprintf(recordFile, ":%lu\n", nodecnt);
    RECORDTRACE("\n");
    return ret;
}

// not sure if these are actually necessary
// we can check for MPI_Comm too later
int MPI_Probe (
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) {
    int rank;
    DEBUG0("MPI_Probe:%d\n", source);
    if(!original_MPI_Probe) {
        original_MPI_Probe = reinterpret_cast<int (*)(int, int, MPI_Comm, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Probe"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    int ret = original_MPI_Probe(source, tag, comm, &stat);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(status, &stat, sizeof(MPI_Status));
    }
    if(source == MPI_ANY_SOURCE) {
        fprintf(recordFile, "MPI_Probe:%d:%d:%d:%d:%lu\n", rank, source, tag, stat.MPI_SOURCE, nodecnt);
        RECORDTRACE("MPI_Probe:%d:%d:%d:%d\n", rank, source, tag, stat.MPI_SOURCE);
    } else {
        fprintf(recordFile, "MPI_Probe:%d:%d:%d:%lu\n", rank, source, tag, nodecnt);
        RECORDTRACE("MPI_Probe:%d:%d:%d\n", rank, source, tag);
    }
    return ret;
}

int MPI_Iprobe (
    int source, 
    int tag, 
    MPI_Comm comm, 
    int *flag, 
    MPI_Status *status
) {
    int rank;
    DEBUG0("MPI_Iprobe:%d\n", source);
    if(!original_MPI_Iprobe) {
        original_MPI_Iprobe = reinterpret_cast<int (*)(int, int, MPI_Comm, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Iprobe"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    int ret = original_MPI_Iprobe(source, tag, comm, flag, &stat);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(status, &stat, sizeof(MPI_Status));
    }
    if(*flag) {
        if(source == MPI_ANY_SOURCE) {
            fprintf(recordFile, "MPI_Iprobe:%d:%d:%d:SUCCESS:%d:%lu\n", rank, source, tag, stat.MPI_SOURCE, nodecnt);
            RECORDTRACE("MPI_Iprobe:%d:%d:%d:SUCCESS:%d\n", rank, source, tag, stat.MPI_SOURCE);
        } else {
            fprintf(recordFile, "MPI_Iprobe:%d:%d:%d:SUCCESS:%lu\n", rank, source, tag, nodecnt);
            RECORDTRACE("MPI_Iprobe:%d:%d:%d:SUCCESS\n", rank, source, tag);
        }
    } else {
        fprintf(recordFile, "MPI_Iprobe:%d:%d:%d:FAIL:%lu\n", rank, source, tag, nodecnt);
        /* RECORDTRACE("MPI_Iprobe:%d:%d:%d:FAIL\n", rank, source, tag); */
    }
    return ret;
}

int MPI_Send_init (
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) {
    int rank;
    DEBUG0("MPI_Send_init:%d:%p\n", dest, request);
    if(!original_MPI_Send_init) {
        original_MPI_Send_init = reinterpret_cast<
            int (*)(
                    const void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Request *)>(
                        dlsym(RTLD_NEXT, "MPI_Send_init"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Send_init(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request);
    MPI_ASSERT(__requests.find(request) == __requests.end());
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Send_init:%d:%d:%p\n", 
            rank, 
            dest, 
            request);
    RECORDTRACE("MPI_Send_init:%d:%d:%p\n", 
            rank, 
            dest, 
            request);
    __requests[request] = "MPI_Send_init";
    return ret;
}

int MPI_Recv_init (
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) {
    int rank;
    DEBUG0("MPI_Recv_init:%d:%p\n", source, request);
    if(!original_MPI_Recv_init) {
        original_MPI_Recv_init = reinterpret_cast<
            int (*)(
                    void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Request *)>(
                        dlsym(RTLD_NEXT, "MPI_Recv_init"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Recv_init(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            request);
    MPI_ASSERT(__requests.find(request) == __requests.end());
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Recv_init:%d:%d:%p\n", 
            rank, 
            source, 
            request);
    RECORDTRACE("MPI_Recv_init:%d:%d:%p\n", 
            rank, 
            source, 
            request);
    __requests[request] = "MPI_Recv_init";
    return ret;
}

int MPI_Startall (
    int count, 
    MPI_Request array_of_requests[]
) {
    int rank;
    if(!original_MPI_Startall) {
        original_MPI_Startall = reinterpret_cast<
            int (*)(int, MPI_Request *)>(
                    dlsym(RTLD_NEXT, "MPI_Startall"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Startall:%d\n", count);
    int ret = original_MPI_Startall(
            count, array_of_requests);
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Startall:%d:%d", rank, count);
    RECORDTRACE("MPI_Startall:%d:%d", rank, count);
    for(int i = 0; i < count; i++) {
        MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
        fprintf(recordFile, ":%p", &array_of_requests[i]);
        RECORDTRACE(":%p", &array_of_requests[i]);
        DEBUG0(":%p", &array_of_requests[i]);
    }
    fprintf(recordFile, "\n");
    RECORDTRACE("\n");
    DEBUG0("\n");
    return ret;
}

int MPI_Request_free (
    MPI_Request *request
) {
    int rank;
    if(!original_MPI_Request_free) {
        original_MPI_Request_free = reinterpret_cast<
            int (*)(MPI_Request *)>(
                    dlsym(RTLD_NEXT, "MPI_Request_free"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = original_MPI_Request_free(request);
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Request_free:%d:%p\n", 
            rank, request);
    RECORDTRACE("MPI_Request_free:%d:%p\n", 
            rank, request);
    __requests.erase(request);
    return ret;
}

// MPI_Gather does not really show the way to create non-determinism, so I would just let it be
// same for MPI_Barrier
