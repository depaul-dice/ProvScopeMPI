
/* 
 * Assumptions:
 * 1. The MPI program is written in C
 * 2. We don't mix the blocking calls with non-blocking calls <- subject to relaxation
 */

#include "mpirecordreplay.h"

using namespace std;

static vector<string> orders;
static unsigned __order_index = 0;

/* static vector<vector<string>> recordtraces; */
/* static unsigned __recordtrace_index = 0; */
extern vector<shared_ptr<element>> recordTraces;
extern vector<vector<string>> replayTracesRaw;
/* extern vector<element> replaytraces; */
/* static unsigned __replaytrace_index = 0; */
/* static unordered_map<string, vector<unsigned long>> __map; */

/* FILE *replaytracefile = nullptr; */

static unordered_map<string, MPI_Request *> __requests;
static unordered_set<MPI_Request *> __isends;


/* #undef MPI_ASSERT */

// let's do the alignment before here
#define MPI_ASSERTNALIGN(CONDITION) \
    do { \
        if(!(CONDITION)) { \
            int rank; \
            MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
            DEBUG("line: %d, rank: %d, assertion failed: %s\n", __LINE__, rank, #CONDITION); \
            appendReplayTrace(); \
            greedyalignmentWhole(); \
            MPI_Abort(MPI_COMM_WORLD, 1); \
        } \
    } while(0)


extern "C" void printBBname(const char *name) {
    int rank, flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    if(flag1 && !flag2) {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string str(name);
        /* fprintf(stderr, "%d:%s\n", rank, str.c_str()); */
        replayTracesRaw.push_back(parse(str, ':'));
        /* MPI_ASSERT(replayTracesRaw.size() > 0); */
    }
}

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

    string tracefile = ".record" + to_string(rank) + ".tr";
    vector<vector<string>> rawTraces;
    fp = fopen(tracefile.c_str(), "r");
    if(fp == nullptr) {
        fprintf(stderr, "Error: cannot open file %s\n", tracefile.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    while((read = getline(&line, &len, fp)) != -1) {
        if(read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        string str(line);
        rawTraces.push_back(parse(str, ':'));
    }
    fclose(fp);

    MPI_ASSERT(rawTraces.size() > 0);
    /* for(unsigned i = 0; i < rawTraces.size(); i++) { */
        /* DEBUG0("%s:%s:%s\n", rawTraces[i][0].c_str(), rawTraces[i][1].c_str(), rawTraces[i][2].c_str()); */
    /* } */

    unsigned long index = 0;
    /* DEBUG("%d:MPI_Init\n", rank); */
    recordTraces = makeHierarchyMain(rawTraces, index); // this function is buggy acccording to the AMG2013
    /* if(rank == 0) { */
    /*     print(recordTraces, 0); */
    /* } */
    
    return ret;
}

int MPI_Finalize(
) {
    if (original_MPI_Finalize == NULL) {
        original_MPI_Finalize = (int (*)()) dlsym(RTLD_NEXT, "MPI_Finalize");
    }
    appendReplayTrace();
    greedyalignmentWhole();
    int ret = original_MPI_Finalize();

    return ret;
}

// in this we will just manipulate the message source when calling MPI_Recv
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
    DEBUG0("MPI_Recv:%s:%d\n", orders[__order_index].c_str(), __order_index);
    vector<string> msgs = parse(orders[__order_index++], ':');
    int src = stoi(msgs[2]);
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Recv");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    // force the source to the right source
    if(source == MPI_ANY_SOURCE) source = src;
    if(source != src) {
        DEBUG0("MPI_Recv: source = %d, src = %d\n", source, src);
    }
    MPI_ASSERTNALIGN(source == src);

    if (!original_MPI_Recv) {
        original_MPI_Recv = (int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *)) dlsym(RTLD_NEXT, "MPI_Recv");
    }
    int rv = original_MPI_Recv(buf, count, datatype, source, tag, comm, status);

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
    // I need to look into the source first
    /* int ret = original_MPI_Irecv(buf, count, datatype, source, tag, comm, request); */
    // I just need to keep track of the request
    
    vector<string> msgs = parse(orders[__order_index++], ':');
    DEBUG0("MPI_Irecv: %s -> %p: %s\t", msgs[3].c_str(), request, orders[__order_index - 1].c_str());
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Irecv");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == source);
    // below is commented on purpose, do not uncomment
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */
    __requests[msgs[3]] = request;
    if(source == MPI_ANY_SOURCE) {
        source = lookahead(orders, __order_index, msgs[3]);
        DEBUG0(":ANY_SOURCE to %d\n", source);
        // -1 means was not able to find the right source, -2 means it was cancelled
        MPI_ASSERTNALIGN(source != -1);
    } else {
        DEBUG0(":%d\n", source);
    }
    return original_MPI_Irecv(buf, count, datatype, source, tag, comm, request);
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
    DEBUG0("MPI_Isend: %s -> %p: %s\n", msgs[3].c_str(), request, orders[__order_index - 1].c_str());
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Isend");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == dest);
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */
    __requests[msgs[3]] = request;
    __isends.insert(request);
    return ret;
}

int MPI_Irsend(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) {
    if(!original_MPI_Irsend) {
        original_MPI_Irsend = reinterpret_cast<int (*)(const void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Irsend"));
    }
    // check if this is correct
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Irsend(buf, count, datatype, dest, tag, comm, request);
    // I just need to keep track of the request
    vector<string> msgs = parse(orders[__order_index++], ':');
    DEBUG("MPI_Irsend: %s -> %p\n", msgs[3].c_str(), request);
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Irsend");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == dest);
    MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end());
    __requests[msgs[3]] = request;
    __isends.insert(request);
    return ret;
}

int MPI_Cancel(
    MPI_Request *request
) {
    DEBUG("MPI_Cancel:%p\n", request);
    if(!original_MPI_Cancel) {
        original_MPI_Cancel = reinterpret_cast<int (*)(MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Cancel"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = original_MPI_Cancel(request);
    // I just need to keep track that it is cancelled
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(ret == MPI_SUCCESS); // what I am just hoping for
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Cancel");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERTNALIGN(__requests[msgs[2]] == request);
    __requests.erase(msgs[2]);
    if(__isends.find(request) != __isends.end()) {
        __isends.erase(request);
    }

    return ret;
}

int MPI_Test(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) {
    DEBUG0("MPI_Test");
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0(":%d:%p", rank, request);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERT(msgs[0] == "MPI_Test");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERT(__requests[msgs[2]] == request);
    int ret = MPI_SUCCESS;
    if(msgs[3] == "SUCCESS") {
        // call wait and make sure it succeeds
        DEBUG0(":SUCCESS\n");
        int src = stoi(msgs[4]);
        ret = original_MPI_Wait(request, status);
        if(__isends.find(request) != __isends.end()) {
            __isends.erase(request);
        }
        MPI_ASSERTNALIGN(status->MPI_SOURCE == src);
        /* __requests.erase(msgs[2]); */
        *flag = 1;
    } else {
        // don't call anything
        DEBUG0(":FAIL\n");
        *flag = 0;
    }
    
    return ret;
}

int MPI_Testall (
    int count, 
    MPI_Request array_of_requests[], 
    int *flag, 
    MPI_Status array_of_statuses[]
) {
    /* DEBUG0("MPI_Testall\n"); */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    if(msgs[0] != "MPI_Testall") {
        DEBUG0("msgs[0]: %s\n", msgs[0].c_str());
    }
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Testall");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    if(stoi(msgs[2]) != count) {
        DEBUG0("MPI_Testall: count = %d, stoi(msgs[2]) = %d\n__order_index: %d\n"\
                , count, stoi(msgs[2]), __order_index - 1);
    }
    MPI_ASSERTNALIGN(stoi(msgs[2]) == count);
    DEBUG0("%s\n", orders[__order_index - 1].c_str());
    if(msgs[3] == "SUCCESS") {
        MPI_ASSERTNALIGN(msgs.size() == 4 + 2 * count);
        /* if(!original_MPI_Wait) { */
        /*     original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait")); */
        /* } */
        /* if(!original_MPI_Testall) { */
        /*     original_MPI_Testall = reinterpret_cast<int (*)(int, MPI_Request *, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Testall")); */
        /* } */
        if(!original_MPI_Waitall) {
            original_MPI_Waitall = reinterpret_cast<int (*)(int, MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Waitall"));
        }
        int ret;
        MPI_Status stats[count];
        /* while(!flag) { */
        ret = original_MPI_Waitall(count, array_of_requests, stats);
        MPI_ASSERTNALIGN(ret == MPI_SUCCESS);
        /* } */

        for(int i = 0; i < count; i++) {
            if(__isends.find(&array_of_requests[i]) != __isends.end()) {
                __isends.erase(&array_of_requests[i]);
            } else {
                if(stats[i].MPI_SOURCE != stoi(msgs[4 + 2 * i + 1])) {
                    DEBUG0("stats[%d].MPI_SOURCE = %d\nstoi(msgs[%d]) = %d\n",\
                            i, stats[i].MPI_SOURCE, 4 + 2 * i + 1, stoi(msgs[4 + 2 * i + 1]));
                }
                MPI_ASSERTNALIGN(stats[i].MPI_SOURCE == stoi(msgs[4 + 2 * i + 1]));
            }
            if(array_of_statuses != MPI_STATUSES_IGNORE) {
                memcpy(&array_of_statuses[i], &stats[i], sizeof(MPI_Status));
            }
        
            MPI_ASSERTNALIGN(__requests.find(msgs[4 + 2 * i]) != __requests.end());
            MPI_ASSERTNALIGN(__requests[msgs[4 + 2 * i]] == &array_of_requests[i]);
            /* __requests.erase(msgs[4 + 2 * i]); */
        }
        *flag = 1;
    } else {
        *flag = 0;
        MPI_ASSERTNALIGN(msgs[3] == "FAIL");
    }
    return MPI_SUCCESS;
}

int MPI_Testsome(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) {
    // record which of the requests were filled in this
    DEBUG("MPI_Testsome");
    int myrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    DEBUG(":%d", myrank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    /* fprintf(stderr, "msgs[0]: %s\n", msgs[0].c_str()); */
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Testsome");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == myrank);
    int oc = stoi(msgs[2]);
    MPI_ASSERTNALIGN(msgs.size() == oc * 2 + 3);
    if(oc == 0) {
        // don't do anything
        DEBUG(":0\n");
        *outcount = 0;
        return MPI_SUCCESS;
    } else {
#ifdef DEBUG_MODE
        DEBUG(":%d", oc);
        for(int i = 0; i < oc; i++) {
            DEBUG(":%s:%s", msgs[3 + 2 * i].c_str(), msgs[3 + 2 * i + 1].c_str());
        }
        DEBUG("\n");
#endif
        if(!original_MPI_Wait) {
            original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
        }
        MPI_Request *req;
        int ind, ret, src;
        MPI_Status stat;
        for(int i = 0; i < oc; i++) {
            req = (MPI_Request *)__requests[msgs[3 + 2 * i]];
            src = stoi(msgs[3 + 2 * i + 1]);
            ind = req - array_of_requests;
            MPI_ASSERTNALIGN(0 <= ind && ind < incount);
            ret = original_MPI_Wait(req, &stat);
            MPI_ASSERTNALIGN(ret == MPI_SUCCESS);
            __requests.erase(msgs[3 + 2 * i]);
            array_of_indices[i] = ind;
            if(__isends.find(req) != __isends.end()) {
                __isends.erase(req);
            } else {
                /* if(src != stat.MPI_SOURCE) { */
                /*     DEBUG0("MPI_Testsome: for request[%p], src[%d] != stat.MPI_SOURCE[%d]\n", \ */
                /*             req, src, stat.MPI_SOURCE); */
                /* } */
                MPI_ASSERTNALIGN(src == stat.MPI_SOURCE);
            }
            if(array_of_statuses != MPI_STATUSES_IGNORE) {
                memcpy(&array_of_statuses[i], &stat, sizeof(MPI_Status));
            }
        }
        *outcount = oc;
        return MPI_SUCCESS;
    }

}

int MPI_Wait(
    MPI_Request *request, 
    MPI_Status *status
) {
    DEBUG0("MPI_Wait\n");
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // I just need to keep track that it is cancelled
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Wait");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(__requests.find(msgs[2]) != __requests.end());
    int src = stoi(msgs[4]);
    // let's first call wait and see if the message is from the source 
    int ret = original_MPI_Wait(request, status);
    MPI_ASSERT(ret == MPI_SUCCESS);
    // if we have the message earlier, or if the message is not from the right source, let's alternate the source
    MPI_ASSERTNALIGN(status->MPI_SOURCE == src);
    MPI_ASSERTNALIGN(__requests[msgs[2]] == request);
    __requests.erase(msgs[2]);

    return ret;
}

int MPI_Waitany(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) {
    DEBUG0("MPI_Waitany");
    for(int i = 0; i < count; i++) {
        DEBUG(":%p", &array_of_requests[i]);
    }
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<int (*)(MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    int rank;
    int ret = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Waitany");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    if(msgs[2] == "SUCCESS") {
        MPI_ASSERTNALIGN(__requests.find(msgs[3]) != __requests.end());
        *index = __requests[msgs[3]] - array_of_requests;
        /* if(__requests[msgs[3]] != &array_of_requests[*index]) { */
            /* DEBUG0("MPI_Waitany: __requests[%s] = %p != &array_of_requests[%d] = %p\n", msgs[3].c_str(), __requests[msgs[3]], *index, &array_of_requests[*index]); */
        /* } */
        DEBUG(":SUCCESS:%d\n", *index);
        MPI_ASSERTNALIGN(0 <= *index && *index < count);
        MPI_ASSERTNALIGN(__requests[msgs[3]] == &array_of_requests[*index]);
        __requests.erase(msgs[3]);
        ret = original_MPI_Wait(&array_of_requests[*index], status);
        MPI_ASSERTNALIGN(ret == MPI_SUCCESS);
    } else {
        DEBUG(":FAIL\n");
        MPI_ASSERTNALIGN(msgs[2] == "FAIL");
    }

    return ret;
}

int MPI_Waitall(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) {
    /* for(int i = 0; i < count; i++) { */
    /*     DEBUG0(":%p", &array_of_requests[i]); */
    /* } */
    if(!original_MPI_Waitall) {
        original_MPI_Waitall = reinterpret_cast<int (*)(int, MPI_Request *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Waitall"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Waitall:%s\n", orders[__order_index].c_str());
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Waitall");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == count);
    MPI_Status stats[count];

    int ret = original_MPI_Waitall(count, array_of_requests, stats);
    MPI_ASSERT(ret == MPI_SUCCESS);
    for(int i = 0; i < count; i++) {
        // first taking care of statuses
        if(array_of_statuses != MPI_STATUSES_IGNORE) {
            memcpy(&array_of_statuses[i], &stats[i], sizeof(MPI_Status));
        }

        // then requests
        MPI_ASSERTNALIGN(__requests.find(msgs[3 + 2 * i]) != __requests.end());
        MPI_ASSERTNALIGN(__requests[msgs[3 + 2 * i]] == &array_of_requests[i]);
        if(__isends.find(&array_of_requests[i]) != __isends.end()) {
            __isends.erase(&array_of_requests[i]);
        } else {
            MPI_ASSERTNALIGN(array_of_statuses[i].MPI_SOURCE == stoi(msgs[3 + 2 * i + 1]));
        }
        __requests.erase(msgs[3 + 2 * i]);
    }
    return ret;
}

int MPI_Probe (
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) {
    DEBUG0("MPI_Probe\n");
    if(!original_MPI_Probe) {
        original_MPI_Probe = reinterpret_cast<int (*)(int, int, MPI_Comm, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Probe"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Probe");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == source);
    MPI_ASSERTNALIGN(stoi(msgs[3]) == tag);
    MPI_Status stat;
    if(source == MPI_ANY_SOURCE) {
        source = stoi(msgs[4]);
    }
    int ret = original_MPI_Probe(source, tag, comm, &stat);
    if(source != MPI_ANY_SOURCE) {
        MPI_ASSERTNALIGN(stat.MPI_SOURCE == source);
    }
    if(tag != MPI_ANY_TAG) {
        MPI_ASSERTNALIGN(stat.MPI_TAG == tag);
    }
    if(status != MPI_STATUS_IGNORE) {
        memcpy(status, &stat, sizeof(MPI_Status));
    }
    MPI_ASSERT(ret == MPI_SUCCESS);
    return ret;
}

int MPI_Iprobe (
    int source, 
    int tag, 
    MPI_Comm comm, 
    int *flag, 
    MPI_Status *status
) {
    /* DEBUG0("MPI_Iprobe\n"); */
    /* if(!original_MPI_Iprobe) { */
        /* original_MPI_Iprobe = reinterpret_cast<int (*)(int, int, MPI_Comm, int *, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Iprobe")); */
    /* } */
    if(!original_MPI_Probe) {
        original_MPI_Probe = reinterpret_cast<int (*)(int, int, MPI_Comm, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Probe"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Iprobe:%s\n", orders[__order_index].c_str());
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Iprobe");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == source);
    MPI_ASSERTNALIGN(stoi(msgs[3]) == tag);
    int ret;
    if(msgs[4] == "SUCCESS") {
        if(!original_MPI_Probe) {
                original_MPI_Probe = reinterpret_cast<int (*)(int, int, MPI_Comm, MPI_Status *)>(dlsym(RTLD_NEXT, "MPI_Probe"));
        }
        *flag = 1;
        MPI_Status stat;
        if(source == MPI_ANY_SOURCE) {
            source = stoi(msgs[5]);
        }
        ret = original_MPI_Probe(source, tag, comm, &stat);
        MPI_ASSERT(ret == MPI_SUCCESS);
        MPI_ASSERTNALIGN(stat.MPI_SOURCE == source);

        if(tag != MPI_ANY_TAG) {
            MPI_ASSERTNALIGN(stat.MPI_TAG == tag);
        }
        if(status != MPI_STATUS_IGNORE) {
            memcpy(status, &stat, sizeof(MPI_Status));
        }
    } else {
        MPI_ASSERTNALIGN(msgs[4] == "FAIL");
        *flag = 0;
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
    DEBUG0("MPI_Send_init\n");
    if(!original_MPI_Send_init) {
        original_MPI_Send_init = reinterpret_cast<int (*)(const void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Send_init"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Send_init");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == dest);
    // below is commented on purpose, do not uncomment
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */ 
    __requests[msgs[3]] = request;
    __isends.insert(request);
    int ret = original_MPI_Send_init(buf, count, datatype, dest, tag, comm, request);
    MPI_ASSERT(ret == MPI_SUCCESS);
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
    DEBUG0("MPI_Recv_init\n");
    if(!original_MPI_Recv_init) {
        original_MPI_Recv_init = reinterpret_cast<int (*)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Recv_init"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Recv_init");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == source);
    // below is commented on purpose, do not uncomment
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */
    __requests[msgs[3]] = request;
    if(source == MPI_ANY_SOURCE) {
        source = lookahead(orders, __order_index, msgs[3]);
        MPI_ASSERTNALIGN(source != -1);
    }
    int ret = original_MPI_Recv_init(buf, count, datatype, source, tag, comm, request);
    MPI_ASSERTNALIGN(ret == MPI_SUCCESS);
    return ret;
}

int MPI_Startall (
    int count, 
    MPI_Request array_of_requests[]
) {
    DEBUG0("MPI_Startall\n");
    if(!original_MPI_Startall) {
        original_MPI_Startall = reinterpret_cast<int (*)(int, MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Startall"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Startall");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == count);
    for(int i = 0; i < count; i++) {
        MPI_ASSERTNALIGN(__requests.find(msgs[3 + i]) != __requests.end());
        MPI_ASSERTNALIGN(__requests[msgs[3 + i]] == &array_of_requests[i]);
    }
    int ret = original_MPI_Startall(count, array_of_requests);
    MPI_ASSERT(ret == MPI_SUCCESS);
    return ret;
}

int MPI_Request_free (
    MPI_Request *request
) {
    DEBUG0("MPI_Request_free:%p\n", request);
    if(!original_MPI_Request_free) {
        original_MPI_Request_free = reinterpret_cast<int (*)(MPI_Request *)>(dlsym(RTLD_NEXT, "MPI_Request_free"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(orders[__order_index++], ':');
    MPI_ASSERT(msgs[0] == "MPI_Request_free");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERT(__requests[msgs[2]] == request);
    int ret = original_MPI_Request_free(request);
    MPI_ASSERT(ret == MPI_SUCCESS);
    __requests.erase(msgs[2]);
    if(__isends.find(request) != __isends.end()) {
        __isends.erase(request);
    }
    return ret;
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
    int ret = original_MPI_Barrier(comm);
    MPI_ASSERT(ret == MPI_SUCCESS);
    return ret;
}
#endif
