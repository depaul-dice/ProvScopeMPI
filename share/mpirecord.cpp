#include "mpirecordreplay.h"

static FILE *recordFile = nullptr;
static map<MPI_Request *, string> __requests;

int MPI_Init(
    int *argc, 
    char ***argv
) {
    DEBUG("MPI_Init\n");
    if(!original_MPI_Init) {
        original_MPI_Init = reinterpret_cast<int (*)(int *, char ***)>(dlsym(RTLD_NEXT, "MPI_Init"));
    }
    int ret = original_MPI_Init(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    string filename = ".record" + to_string(rank) + ".txt";  
    recordFile = fopen(filename.c_str(), "w");
    if(recordFile == nullptr) {
        fprintf(stderr, "failed to open record file\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    return ret;
}

int MPI_Finalize(
    void
) {
    DEBUG("MPI_Finalize\n");
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(recordFile != nullptr) {
        DEBUG0("end of record at %d\n", rank);
        fclose(recordFile);
    } 
    if(!original_MPI_Finalize) {
        original_MPI_Finalize = reinterpret_cast<int (*)(void)>(dlsym(RTLD_NEXT, "MPI_Finalize"));
    }
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
    DEBUG("MPI_Recv\n"); 
    if(!original_MPI_Recv) {
        original_MPI_Recv = reinterpret_cast<int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Recv"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Recv(buf, count, datatype, source, tag, comm, status);
    int source_rank = status->MPI_SOURCE;

    fprintf(recordFile, "MPI_Recv:%d:%d\n", rank, source_rank);
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
    DEBUG("MPI_Irecv\n");
    if(!original_MPI_Recv) {
        original_MPI_Irecv = reinterpret_cast<int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Irecv"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Irecv(buf, count, datatype, source, tag, comm, request);
    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Irecv:%d:%d:%p\n", rank, source, request);
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
    DEBUG("MPI_Isend\n");
    if(!original_MPI_Isend) {
        original_MPI_Isend = reinterpret_cast<int (*)(const void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Isend"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Isend(buf, count, datatype, dest, tag, comm, request);
    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Isend:%d:%d:%p\n", rank, dest, request);
    __requests[request] = "MPI_Isend";
    return ret;
}

int MPI_Cancel(
    MPI_Request *request
) {
    DEBUG("MPI_Cancel\n");
    if(!original_MPI_Cancel) {
        original_MPI_Cancel = reinterpret_cast<int (*)(MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Cancel"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = original_MPI_Cancel(request);
    // I just need to keep track that it is cancelled
    fprintf(recordFile, "MPI_Cancel:%d:%p\n", rank, request);
    __requests.erase(request);
    return ret;
}

int MPI_Test(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) {
    DEBUG("MPI_Test\n");
    int rank;
    if(!original_MPI_Test) {
        original_MPI_Test = reinterpret_cast<int (*)(MPI_Request *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Test"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(__requests.find(request) != __requests.end());
    MPI_Status stat;
    int ret = original_MPI_Test(request, flag, &stat);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(status, &stat, sizeof(MPI_Status));
    }

    // record if the request was completed or not 
    if(*flag) {
        fprintf(recordFile, "MPI_Test:%d:%p:SUCCESS:%d\n", rank, request, stat.MPI_SOURCE);
        __requests.erase(request);
    } else {
        fprintf(recordFile, "MPI_Test:%d:%p:FAIL\n", rank, request);
    }
    return ret;
}

/* // haven't tested this yet */
/* int MPI_Testany( */
/*     int count, */ 
/*     MPI_Request array_of_requests[], */ 
/*     int *index, */ 
/*     int *flag, */ 
/*     MPI_Status *status */
/* ) { */
/*     int rank; */
/*     if(!original_MPI_Testany) { */
/*         original_MPI_Testany = reinterpret_cast<int (*)(int, MPI_Request *, int *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Testany")); */
/*     } */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     MPI_Status stat; */
/*     int ret = original_MPI_Testany(count, array_of_requests, index, flag, &stat); */
/*     if(status != MPI_STATUS_IGNORE) { */
/*         *status = stat; */
/*     } */

/*     if(*flag && *index != MPI_UNDEFINED) { */
/*         string req = "void"; */
/*         if(requests.find(&(array_of_requests[*index])) == requests.end()) { */
/*             fprintf(stderr, "request not found on MPI_Testany\n"); */
/*         } else { */
/*             req = requests[&(array_of_requests[*index])].name; */
/*             requests.erase(&(array_of_requests[*index])); */
/*         } */
/*         fprintf(stderr, "MPI_Testany:%s:%d:%d\n", req.c_str(), rank, stat.MPI_SOURCE); */
/*     } */
/*     return ret; */
/* } */

/* // flag == true iff all requests are completed */
/* // haven't tested this yet */
/* int MPI_Testall( */
/*     int count, */ 
/*     MPI_Request array_of_requests[], */ 
/*     int *flag, */ 
/*     MPI_Status array_of_statuses[] */
/* ) { */
/*     int rank; */
/*     if(!original_MPI_Testall) { */
/*         original_MPI_Testall = reinterpret_cast<int (*)(int, MPI_Request *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Testall")); */
/*     } */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     MPI_Status stats [count]; */
/*     int ret = original_MPI_Testall(count, array_of_requests, flag, stats); */
/*     if(array_of_statuses != MPI_STATUSES_IGNORE) { */
/*         for(int i = 0; i < count; i++) { */
/*             array_of_statuses[i] = stats[i]; */
/*         } */
/*     } */

/*     if(*flag) { */
/*         for(int i = 0; i < count; i++) { */
/*             string req = "void"; */
/*             if(requests.find(&array_of_requests[i]) == requests.end()) { */
/*                 fprintf(stderr, "request not found on MPI_Testall\n"); */
/*             } else { */
/*                 req = requests[&array_of_requests[i]].name; */
/*                 requests.erase(&array_of_requests[i]); */
/*             } */
/*             fprintf(stderr, "MPI_Testall:%s:%d:%d\n", req.c_str(), rank, stats[i].MPI_SOURCE); */
/*         } */
/*     } */
/*     return ret; */
/* } */

int MPI_Testsome(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) {
    // record which of the requests were filled in this
    DEBUG("MPI_Testsome\n");
    int rank;
    if(!original_MPI_Testsome) {
        original_MPI_Testsome = reinterpret_cast<int (*)(int, MPI_Request *, int *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Testsome"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stats [incount];
    int ret = original_MPI_Testsome(incount, array_of_requests, outcount, array_of_indices, stats);
    if(array_of_statuses != MPI_STATUSES_IGNORE) {
        for(int i = 0; i < incount; i++) {
            memcpy(&array_of_statuses[i], &stats[i], sizeof(MPI_Status));
        }
    }

    fprintf(recordFile, "MPI_Testsome:%d:%d", rank, *outcount);
    if(*outcount > 0) {
        for(int i = 0; i < *outcount; i++) {
            int ind = array_of_indices[i];
            MPI_ASSERT(__requests.find(&array_of_requests[ind]) != __requests.end());
            fprintf(recordFile, ":%p:%d", &(array_of_requests[ind]), array_of_statuses[i].MPI_SOURCE); 
            __requests.erase(&array_of_requests[ind]);
        }
    }
    fprintf(recordFile, "\n");
    return ret;
}

/* // this requires MPI_Wait on the sending side */
/* // haven't tested this yet */
/* int MPI_Test_cancelled( */
/*     const MPI_Status *status, */ 
/*     int *flag */
/* ) { */
/*     int rank; */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     if(!original_MPI_Test_cancelled) { */
/*         original_MPI_Test_cancelled = reinterpret_cast<int (*)(const MPI_Status *, int *)>(dlsym(RTLD_NEXT, "MPI_Test_cancelled")); */
/*     } */
/*     int ret = original_MPI_Test_cancelled(status, flag); */
/*     if(flag) { */
/*         fprintf(stderr, "MPI_Test_cancelled:%d\n", rank); */
/*     } */
/*     return ret; */
/* } */

int MPI_Wait(
    MPI_Request *request, 
    MPI_Status *status
) {
    int rank;
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    int ret = original_MPI_Wait(request, &stat);
    if(status != MPI_STATUS_IGNORE) {
        *status = stat;
    }
    MPI_ASSERT(__requests.find(request) != __requests.end());
    /* string req = __requests[request]; */
    __requests.erase(request);

    if(ret == MPI_SUCCESS) {
        fprintf(recordFile, "MPI_Wait:%d:%p:SUCCESS:%d\n", rank, request, stat.MPI_SOURCE);
    } else {
        fprintf(recordFile, "MPI_Wait:%d:%p:FAIL\n", rank, request);
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
    int ret = original_MPI_Waitany(count, array_of_requests, index, &stat);
    if(status != MPI_STATUS_IGNORE) {
        *status = stat;
    }

    if(*index == MPI_UNDEFINED) {
        fprintf(recordFile, "MPI_Waitany:%d:FAIL\n", rank);
    } else {
        MPI_ASSERT(__requests.find(&array_of_requests[*index]) != __requests.end());
        /* string req = __requests[&array_of_requests[*index]]; */
        fprintf(recordFile, "MPI_Waitany:%d:SUCCESS:%p:%d\n", rank, &array_of_requests[*index], stat.MPI_SOURCE);
    }

    return ret;
}

/* int MPI_Waitall( */
/*     int count, */ 
/*     MPI_Request array_of_requests[], */ 
/*     MPI_Status array_of_statuses[] */
/* ) { */
/*     int rank; */
/*     if(!original_MPI_Waitall) { */
/*         original_MPI_Waitall = reinterpret_cast<int (*)(int, MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Waitall")); */
/*     } */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     MPI_Status stats [count]; */
/*     int ret = original_MPI_Waitall(count, array_of_requests, stats); */
/*     if(array_of_statuses != MPI_STATUSES_IGNORE) { */
/*         for(int i = 0; i < count; i++) { */
/*             array_of_statuses[i] = stats[i]; */
/*         } */
/*     } */
/*     /1* string req; *1/ */
/*     for(int i = 0; i < count; i++) { */
/*         /1* req = "void"; *1/ */
/*         if(requests.find(&array_of_requests[i]) == requests.end()) { */
/*             fprintf(stderr, "request not found on MPI_Waitall\n"); */
/*         } else { */
/*             /1* req = requests[&array_of_requests[i]].name; *1/ */
/*             requests.erase(&array_of_requests[i]); */
/*         } */
/*         if(i == 0) { */
/*             if(count == 1) { */
/*                 fprintf(stderr, "MPI_Waitall:%d:%d:%p\n", rank, count, array_of_requests[i]); */
/*             } else { */
/*                 fprintf(stderr, "MPI_Waitall:%d:%d:%p", rank, count, array_of_requests[i]); */
/*             } */
/*         } else { */
/*             if(i == count - 1) { */
/*                 fprintf(stderr, ":%p\n", array_of_requests[i]); */
/*             } else { */
/*                 fprintf(stderr, ":%p", array_of_requests[i]); */
/*             } */
/*         } */
/*     } */
/*     return ret; */
/* } */

/* // haven't tested this yet */
/* int MPI_Waitsome( */
/*     int incount, */ 
/*     MPI_Request array_of_requests[], */ 
/*     int *outcount, */ 
/*     int array_of_indices[], */ 
/*     MPI_Status array_of_statuses[] */
/* ) { */
/*     int rank; */
/*     if(!original_MPI_Waitsome) { */
/*         original_MPI_Waitsome = reinterpret_cast<int (*)(int, MPI_Request *, int *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Waitsome")); */
/*     } */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     MPI_Status stats [incount]; */
/*     int ret = original_MPI_Waitsome(incount, array_of_requests, outcount, array_of_indices, stats); */
/*     if(array_of_statuses != MPI_STATUSES_IGNORE) { */
/*         for(int i = 0; i < incount; i++) { */
/*             array_of_statuses[i] = stats[i]; */
/*         } */
/*     } */

/*     if(ret == MPI_UNDEFINED) { */
/*         fprintf(stderr, "MPI_Waitsome:%d\n", rank); */
/*     } else { */
/*         string req; */
/*         for(int i = 0; i < *outcount; i++) { */
/*             req = "void"; */
/*             if(requests.find(&array_of_requests[array_of_indices[i]]) == requests.end()) { */
/*                 fprintf(stderr, "request not found on MPI_Waitsome\n"); */
/*             } else { */
/*                 req = requests[&array_of_requests[array_of_indices[i]]].name; */
/*                 requests.erase(&array_of_requests[array_of_indices[i]]); */
/*             } */
/*             fprintf(stderr, "MPI_Waitsome:%s:%d:%d\n", req.c_str(), rank, stats[i].MPI_SOURCE); */
/*         } */
/*     } */
/*     return ret; */
/* } */


// MPI_Gather does not really show the way to create non-determinism, so I would just let it be
// same for MPI_Barrier
