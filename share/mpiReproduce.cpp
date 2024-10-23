
/* 
 * Assumptions:
 * 1. The MPI program is written in C
 * 2. We don't mix the blocking calls with non-blocking calls <- subject to relaxation
 */

#include "mpiRecordReplay.h"

using namespace std;

static vector<string> orders;
static unsigned __order_index = 0;

/* static vector<vector<string>> recordtraces; */
/* static unsigned __recordtrace_index = 0; */
extern vector<shared_ptr<element>> recordTraces;
extern vector<vector<string>> replayTracesRaw;
/*
 * below is for the debugging purpose, delete when done
 */
extern vector<shared_ptr<element>> replayTraces;

/* static unsigned __replaytrace_index = 0; */
/* static unordered_map<string, vector<unsigned long>> __map; */

/* FILE *replaytracefile = nullptr; */

static unordered_map<string, MPI_Request *> __requests;
static unordered_set<MPI_Request *> __isends;
static unordered_set<MPI_Request *> __unalignedRequests;

// key is the function name
static unordered_map<string, loopNode *> __looptrees;

// TODO: better name for this variable because it's not self descriptive
deque<shared_ptr<lastaligned>> __q;
// TODO: delete this logger as it's just not really necessary
Logger logger;
MessagePool messagePool;

// let's do the alignment before here
// this is currently faulty because it could lead to a loop of fault
// when assertnalign is called, there could be another procedure that could call assertnalign
#define MPI_ASSERTNALIGN(CONDITION) \
    do { \
        if(!(CONDITION)) { \
            int rank; \
            MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
            DEBUG("line: %d, rank: %d, func: %s, assertion failed: %s\n\
                    going to abort\n", __LINE__, rank, __func__, #CONDITION); \
            MPI_Abort(MPI_COMM_WORLD, 1); \
        } \
    } while(0)

void segfaultHandler(int sig, siginfo_t *info, void *ucontext) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    void *array[10];
    size_t size;
    char **strings;

    if(rank == 0) {
        size = backtrace(array, 10);
        cerr << "Error " << rank << ": signal " << sig \
        << "at line " << info->si_code << endl;
        size = backtrace(array, 10);
        strings = backtrace_symbols(array, size);
        fprintf(stderr, "Obtained %zd stack frames in rank %d\n", size, rank);
        for (size_t i = 0; i < size; i++) {
            fprintf(stderr, "%s\n", strings[i]);
        }
    }

    MPI_Abort(MPI_COMM_WORLD, 1);
}

void setupSignalHandler() {
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segfaultHandler;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGSEGV, &sa, NULL) == -1) {
        fprintf(stderr, "Error: cannot set signal handler\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

/* 
 * The name passed down as an argument will be in the format 
 * FunctionName:Type:Count:(node count)
 * Type is a type of a node (e.g. entry node, exit node, neither)
 * Count is a unique identifier
 * node count is the counter for nodes to be passed
 */
extern "C" void printBBname(const char *name) {
    int rank, flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    vector<string> tokens;
    if(flag1 && !flag2) {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string str(name);
        tokens = parse(str, ':');
        replayTracesRaw.push_back(tokens);
        // if it is a replay trace the trace length will be 3, but for record 4
        /* MPI_ASSERT(replayTracesRaw.size() > 0); */
    }
}

int MPI_Init(
    int *argc, 
    char ***argv
) {
    int ret = PMPI_Init(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /*
     * opening the file that records the communications
     * getline obtains all the traces
     */
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

    /*
     * opening the file that records the traces
     */
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

    /*
     * opening the file that has loop information
     */
    string looptreefile = "loops.dot";
    __looptrees = parseDotFile(looptreefile);
    for(auto lt: __looptrees) {
        lt.second->fixExclusives();
    }

    /*
     * creating the hierarchy of traces for recorded traces
     */
    unsigned long index = 0;
    recordTraces = makeHierarchyMain(rawTraces, index, __looptrees); 

    return ret;
}

int MPI_Finalize(
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* DEBUG("MPI_Finalize:%d\n", rank); */
    /* appendReplayTrace(); */
    /* greedyalignmentWholeOffline(); */

    /*
     * last alignment before finalizing
     */
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);

    /*
     * deleting the looptrees
     */
    for(auto it = __looptrees.begin(); it != __looptrees.end(); it++) {
        delete it->second;
    }
    /*
     * should probably delete all the elements in the recordTraces
     */

    return PMPI_Finalize();
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
    /* DEBUG0("MPI_Recv:%s:%d\n", orders[__order_index].c_str(), __order_index); */
    bool isAligned = true;
    size_t lastInd = 0;
    __q = onlineAlignment(
            __q, 
            isAligned, 
            lastInd, 
            __looptrees);
    vector<string> msgs;
    int ret;
    if(!isAligned) {
        DEBUG("at rank %d, the alignment was not successful at MPI_Recv\n", rank);
        /*
         * you still need to convert the message into the way it should be
         */
        ret = __MPI_Recv(
                buf, 
                count, 
                datatype, 
                source, 
                tag, 
                comm, 
                status,
                messagePool);
        return ret;
    } 
    string recSendNodes = "";
    msgs = getmsgs(
            orders, 
            lastInd, 
            __order_index,
            &recSendNodes);
    int src = stoi(msgs[2]);
    MPI_EQUAL(msgs[0], "MPI_Recv");
    MPI_EQUAL(stoi(msgs[1]), rank);
    /*
     * force the source to the right source
     */
    if(source == MPI_ANY_SOURCE) {
        source = src;
    }
    MPI_EQUAL(source, src);
    
    string repSendNodes = "";
    ret = __MPI_Recv(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            status,
            messagePool,
            &repSendNodes);

    MPI_EQUAL(recSendNodes, repSendNodes);
    return ret;
}

/*
 * in this function, there is no need of alignment
 * you just need to reformat the message and send it in the suitable way
 * by piggybacking the current node
 */
int MPI_Send(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm
) {
    string currNodes = updateAndGetLastNodes(
            __looptrees, 
            TraceType::REPLAY);
    int ret = __MPI_Send(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm,
            messagePool,
            currNodes);
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
    /* DEBUG0("MPI_Irecv\n"); */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // I need to look into the source first
    /* int ret = original_MPI_Irecv(buf, count, datatype, source, tag, comm, request); */
    // I just need to keep track of the request
    bool isAligned = true;
    size_t lastInd = 0;
    __q = onlineAlignment(
            __q, 
            isAligned, 
            lastInd, 
            __looptrees);
    int ret;
    if(!isAligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Irecv\n", rank); */
        // don't control anything, but keep track of the request
        __unalignedRequests.insert(request);
        ret = __MPI_Irecv(
                buf, 
                count, 
                datatype, 
                source, 
                tag, 
                comm, 
                request,
                messagePool);
        return ret;
    }
    
    vector<string> msgs = getmsgs(
            orders, 
            lastInd, 
            __order_index);

    MPI_EQUAL(msgs[0], "MPI_Irecv");
    MPI_EQUAL(stoi(msgs[1]), rank);
    /* MPI_ASSERT(stoi(msgs[2]) == source); // commenting for the greedy alignment */
    // below is commented on purpose, do not uncomment
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */
    __requests[msgs[3]] = request;
    if(source == MPI_ANY_SOURCE) {
        source = lookahead(
                orders, 
                __order_index, 
                msgs[3]);
    } 
    /*
     * -1 means was not able to find the right source,
     * -2 means it was cancelled
     */
    /* DEBUG("request: %p, source: %d\n", */ 
    /*         request, source); */
    MPI_ASSERT(source != -1);
    ret = __MPI_Irecv(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            request,
            messagePool);
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    string currNodes = updateAndGetLastNodes(
            __looptrees, TraceType::REPLAY);
    int ret = __MPI_Isend(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request,
            messagePool,
            currNodes);
        
    bool isAligned = true;
    size_t lastInd = 0;
    __q = onlineAlignment(
            __q, 
            isAligned, 
            lastInd, 
            __looptrees);
    if(!isAligned) {
        DEBUG("at rank %d, the alignment was not successful at MPI_Isend\n", rank);
        // don't control anything, but keep track of the request
        __unalignedRequests.insert(request);
        __isends.insert(request);
        // Isend is already called above
        return ret;
    }
    // I just need to keep track of the request
    vector<string> msgs = getmsgs(
            orders, 
            lastInd, 
            __order_index);
    if(rank == 4 && dest == 6) {
        /* print(replayTraces, 0); */
        /* print(recordTraces, 0); */
        /* DEBUG("MPI_Isend: %s -> %p: %s\nrank: %d, currNodes:%s\n", */ 
        /*         msgs[3].c_str(), */ 
        /*         request, */ 
        /*         orders[__order_index - 1].c_str(), */ 
        /*         rank, */
        /*         currNodes.c_str()); */
    }
    MPI_EQUAL(msgs[0], "MPI_Isend");
    MPI_EQUAL(stoi(msgs[1]), rank);
    /* MPI_ASSERT(stoi(msgs[2]) == dest); */ // commenting for the greedy alignment
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
    FUNCGUARD();
    DEBUG("MPI_Irsend not supported yet\n");
    MPI_Abort(MPI_COMM_WORLD, 1);

    // check if this is correct
    int rank;
    MPI_Comm_rank(
            MPI_COMM_WORLD, &rank);
    int ret = PMPI_Irsend(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Irsend\n", rank); */
        // don't control anything, but keep track of the request
        // I don't know what to do with general requests
        __unalignedRequests.insert(request);
        __isends.insert(request);
        // Irsend is already called above
        return ret;
    }
    // I just need to keep track of the request
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    /* DEBUG("MPI_Irsend: %s -> %p\n", msgs[3].c_str(), request); */
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
    /* DEBUG("MPI_Cancel:%p\n", request); */
    int rank;
    MPI_Comm_rank(
            MPI_COMM_WORLD, &rank);
    int ret = __MPI_Cancel(
            request, messagePool);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Cancel\n", rank); */
        // don't control anything
        return ret;
    }
    // I just need to keep track that it is cancelled
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    MPI_EQUAL(ret, MPI_SUCCESS);
    MPI_EQUAL(msgs[0], "MPI_Cancel");
    MPI_EQUAL(stoi(msgs[1]), rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
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
    /* DEBUG0("MPI_Test"); */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG0(":%d:%p", rank, request);
    bool isAligned = true;
    size_t lastInd = 0;
    __q = onlineAlignment(
            __q, 
            isAligned, 
            lastInd, 
            __looptrees);
    if(!isAligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Test\n", rank); */
        // don't control anything
        return __MPI_Test(
                request, 
                flag, 
                status,
                messagePool);
    }
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    string recSendNodes = "";
    vector<string> msgs = getmsgs(
            orders, 
            lastInd, 
            __order_index,
            &recSendNodes);
    MPI_ASSERT(msgs[0] == "MPI_Test");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERT(__requests[msgs[2]] == request);
    int ret = MPI_SUCCESS;
    if(msgs[3] == "SUCCESS") {
        /*
         * call wait and make sure it succeeds
         */
        int src = stoi(msgs[4]);
        ret = __MPI_Wait(
                request,
                status,
                messagePool);
        if(__isends.find(request) != __isends.end()) {
            __isends.erase(request);
        } else {
            MPI_EQUAL(status->MPI_SOURCE, src);
        }
        /* __requests.erase(msgs[2]); */
        *flag = 1;
    } else {
        /* 
         * don't call anything
         */
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        // don't control anything
        return __MPI_Testall(
                count, 
                array_of_requests, 
                flag, 
                array_of_statuses,
                messagePool);
    }
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    MPI_EQUAL(msgs[0], "MPI_Testall");
    MPI_EQUAL(stoi(msgs[1]), rank);
    MPI_EQUAL(stoi(msgs[2]), count);
    if(msgs[3] == "SUCCESS") {
        MPI_ASSERT(msgs.size() == 5 + 3 * count);
        int ret;
        MPI_Status stats[count];
        string repSendNodes [count];
        ret = __MPI_Waitall(
                count, 
                array_of_requests, 
                stats,
                messagePool,
                repSendNodes);
        MPI_EQUAL(ret, MPI_SUCCESS);

        for(int i = 0; i < count; i++) {
            if(__isends.find(&array_of_requests[i]) != __isends.end()) {
                __isends.erase(&array_of_requests[i]);
            } else {
                MPI_EQUAL(stats[i].MPI_SOURCE, stoi(msgs[4 + 3 * i + 1]));
                string recSendNodes = msgs[4 + 3 * i + 2];
                MPI_EQUAL(recSendNodes, repSendNodes[i]);
            }
            if(array_of_statuses != MPI_STATUSES_IGNORE) {
                memcpy(
                        &array_of_statuses[i], 
                        &stats[i], 
                        sizeof(MPI_Status));
            }
        
            MPI_ASSERT(__requests.find(msgs[4 + 3 * i]) != __requests.end());
            MPI_EQUAL(__requests[msgs[4 + 3 * i]], &array_of_requests[i]);
        }
        *flag = 1;
    } else {
        MPI_EQUAL(msgs[3], "FAIL");
        *flag = 0;
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* DEBUG0("MPI_Testsome:%d", rank); */
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Testsome\n", myrank); */
        // don't control anything
        return __MPI_Testsome(
                incount, 
                array_of_requests, 
                outcount, 
                array_of_indices, 
                array_of_statuses,
                messagePool);
    }

    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    /* fprintf(stderr, "msgs[0]: %s\n", msgs[0].c_str()); */
    MPI_EQUAL(msgs[0], "MPI_Testsome");
    MPI_EQUAL(stoi(msgs[1]), rank);
    int oc = stoi(msgs[2]);
    MPI_ASSERT(msgs.size() == oc * 3 + 4);
    if(oc == 0) {
        // don't do anything
        /* DEBUG(":0\n"); */
        *outcount = 0;
        return MPI_SUCCESS;
    } else {
        MPI_Request *req;
        int ind, ret, src;
        MPI_Status stat;
        for(int i = 0; i < oc; i++) {
            req = (MPI_Request *)__requests[msgs[3 + 3 * i]];
            src = stoi(msgs[3 + 3 * i + 1]);
            ind = req - array_of_requests;
            MPI_ASSERT(0 <= ind 
                    && ind < incount);
            string repSendNodes = "";
            ret = __MPI_Wait(
                    req, 
                    &stat,
                    messagePool,
                    &repSendNodes);
            MPI_EQUAL(ret, MPI_SUCCESS);
            __requests.erase(msgs[3 + 2 * i]);
            array_of_indices[i] = ind;
            if(__isends.find(req) != __isends.end()) {
                __isends.erase(req);
            } else {
                MPI_EQUAL(stat.MPI_SOURCE, src);
                auto recSendNodes = msgs[3 + 3 * i + 2];
                /* fprintf(stderr, "at %s, sendNodes: %s\n", */ 
                /*         __func__, recSendNodes.c_str()); */
                if(recSendNodes != repSendNodes) {
                    if(rank == 6 && src == 4) {
                        auto recSendNodesVec = parse(recSendNodes, '/');
                        auto repSendNodesVec = parse(repSendNodes, '/');
                        cerr << "rank: " << rank << ", src: " << src << endl 
                            << "recSendNodes:\n" << recSendNodesVec << endl
                            << "repSendNodes:\n" << repSendNodesVec << endl;
                    }
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
            }
            if(array_of_statuses != MPI_STATUSES_IGNORE) {
                memcpy(
                        &array_of_statuses[i], 
                        &stat, 
                        sizeof(MPI_Status));
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Wait\n", rank); */
        // don't control anything
        return __MPI_Wait(
                request, 
                status,
                messagePool);
    }
    // I just need to keep track that it is cancelled
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    string recSendNodes = "";
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index,
            &recSendNodes);
    MPI_EQUAL(msgs[0], "MPI_Wait");
    MPI_EQUAL(stoi(msgs[1]), rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    int src = stoi(msgs[4]);
    // let's first call wait and see if the message is from the source 
    MPI_Status localStat;
    string repSendNodes = "";
    int ret = __MPI_Wait(
            request, 
            &localStat,
            messagePool,
            &repSendNodes);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &localStat, 
                sizeof(MPI_Status));
    }
    MPI_EQUAL(ret, MPI_SUCCESS);
    /*
     * if the request is for the receive,
     * let's check the sendnodes
     */
    if(__isends.find(request) == __isends.end()) {
        MPI_EQUAL(recSendNodes, repSendNodes);
    }

    /*
     * if we have the message earlier, 
     * or if the message is not from the right source, 
     * let's alternate the source
     */
    MPI_EQUAL(status->MPI_SOURCE, src);
    MPI_EQUAL(__requests[msgs[2]], request);
    __requests.erase(msgs[2]);

    return ret;
}

int MPI_Waitany(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) {
    FUNCGUARD();
    /* DEBUG0("MPI_Waitany"); */
    /* for(int i = 0; i < count; i++) { */
    /*     DEBUG(":%p", &array_of_requests[i]); */
    /* } */
    int rank;
    int ret = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Waitany\n", rank); */
        // don't control anything
        return PMPI_Waitany(
                count, 
                array_of_requests, 
                index, 
                status);
    }
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    MPI_ASSERT(msgs[0] == "MPI_Waitany");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    if(msgs[2] == "SUCCESS") {
        MPI_ASSERT(__requests.find(msgs[3]) != __requests.end());
        *index = __requests[msgs[3]] - array_of_requests;
        /* DEBUG(":SUCCESS:%d\n", *index); */
        MPI_ASSERT(0 <= *index && *index < count);
        MPI_ASSERT(__requests[msgs[3]] == &array_of_requests[*index]);
        __requests.erase(msgs[3]);
        ret = PMPI_Wait(&array_of_requests[*index], status);
        MPI_ASSERT(ret == MPI_SUCCESS);
    } else {
        /* DEBUG(":FAIL\n"); */
        MPI_ASSERT(msgs[2] == "FAIL");
    }

    return ret;
}

int MPI_Waitall(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* DEBUG0("MPI_Waitall:%s\n", orders[__order_index].c_str()); */
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Waitall\n", rank); */
        // don't control anything
        return __MPI_Waitall(
                count, 
                array_of_requests, 
                array_of_statuses,
                messagePool);
    } 
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    MPI_EQUAL(msgs[0], "MPI_Waitall");
    MPI_EQUAL(stoi(msgs[1]), rank);
    MPI_EQUAL(stoi(msgs[2]), count);
    MPI_Status stats[count];
    string repSendNodes[count];

    int ret = __MPI_Waitall(
            count, 
            array_of_requests, 
            stats,
            messagePool,
            repSendNodes);
    MPI_EQUAL(ret, MPI_SUCCESS);
    for(int i = 0; i < count; i++) {
        // first taking care of statuses
        if(array_of_statuses != MPI_STATUSES_IGNORE) {
            memcpy(
                    &array_of_statuses[i], 
                    &stats[i], 
                    sizeof(MPI_Status));
        }

        // then requests
        MPI_ASSERT(__requests.find(msgs[3 + 3 * i]) != __requests.end());
        MPI_EQUAL(__requests[msgs[3 + 3 * i]], &array_of_requests[i]);
        if(__isends.find(&array_of_requests[i]) != __isends.end()) {
            __isends.erase(&array_of_requests[i]);
        } else {
            MPI_EQUAL(array_of_statuses[i].MPI_SOURCE, stoi(msgs[3 + 3 * i + 1]));
            auto recSendNodes = msgs[3 + 3 * i + 2];
            /* fprintf(stderr, "at %s, sendNodes: %s\n", */ 
                    /* __func__, recSendNodes.c_str()); */
            MPI_EQUAL(recSendNodes, repSendNodes[i]);
        }
        __requests.erase(msgs[3 + 3 * i]);
    }
    return ret;
}

int MPI_Probe (
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) {
    FUNCGUARD();
    /* DEBUG0("MPI_Probe\n"); */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Probe\n", rank); */
        // don't control anything
        return __MPI_Probe(
                source, 
                tag, 
                comm, 
                status,
                messagePool);
    } 
 
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            lastind,
            __order_index);
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Probe");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == source);
    MPI_ASSERTNALIGN(stoi(msgs[3]) == tag);
    MPI_Status stat;
    if(source == MPI_ANY_SOURCE) {
        source = stoi(msgs[4]);
    }
    int ret = __MPI_Probe(
            source, 
            tag, 
            comm, 
            &stat,
            messagePool);
    if(source != MPI_ANY_SOURCE) {
        MPI_ASSERT(stat.MPI_SOURCE == source);
    }
    if(tag != MPI_ANY_TAG) {
        MPI_ASSERT(stat.MPI_TAG == tag);
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    bool isaligned = true;
    size_t lastind = 0;
    __q = onlineAlignment(
            __q, 
            isaligned, 
            lastind, 
            __looptrees);
    if(!isaligned) {
        /* DEBUG("at rank %d, the alignment was not successful at MPI_Iprobe\n", rank); */
        // don't control anything
        return __MPI_Iprobe(
                source, 
                tag, 
                comm, 
                flag, 
                status,
                messagePool);
    } 
    vector<string> msgs = getmsgs(
            orders, 
            lastind, 
            __order_index);
    /* DEBUG0("MPI_Iprobe:%s\n", orders[__order_index].c_str()); */
    if(msgs[0] != "MPI_Iprobe") {
        string lastNodes = updateAndGetLastNodes(
                __looptrees, 
                TraceType::REPLAY);
        DEBUG("rank: %d\nmsgs[0]: %s\n__order_index: %d\nlastNodes: %s\n", 
                rank, 
                msgs[0].c_str(), 
                __order_index, 
                lastNodes.c_str());
    }
    MPI_EQUAL(msgs[0], "MPI_Iprobe");
    MPI_EQUAL(stoi(msgs[1]), rank);
    MPI_EQUAL(stoi(msgs[2]), source);
    MPI_EQUAL(stoi(msgs[3]), tag);
    int ret;
    if(msgs[4] == "SUCCESS") {
        *flag = 1;
        MPI_Status stat;
        if(source == MPI_ANY_SOURCE) {
            source = stoi(msgs[5]);
        }
        ret = __MPI_Probe(
                source, 
                tag, 
                comm, 
                &stat,
                messagePool);
        MPI_ASSERT(ret == MPI_SUCCESS);
        MPI_ASSERT(stat.MPI_SOURCE == source);

        if(tag != MPI_ANY_TAG) {
            MPI_ASSERT(stat.MPI_TAG == tag);
        }
        if(status != MPI_STATUS_IGNORE) {
            memcpy(status, &stat, sizeof(MPI_Status));
        }
    } else {
        MPI_ASSERT(msgs[4] == "FAIL");
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
    FUNCGUARD();
    DEBUG0("MPI_Send_init\n");
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            __order_index, 
            __order_index);
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Send_init");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == dest);
    // below is commented on purpose, do not uncomment
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */ 
    __requests[msgs[3]] = request;
    __isends.insert(request);
    int ret = PMPI_Send_init(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request);
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
    FUNCGUARD();
    DEBUG0("MPI_Recv_init\n");
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* vector<string> msgs = parse(orders[__order_index++], ':'); */
    vector<string> msgs = getmsgs(
            orders, 
            __order_index,
            __order_index);
    MPI_ASSERTNALIGN(msgs[0] == "MPI_Recv_init");
    MPI_ASSERTNALIGN(stoi(msgs[1]) == rank);
    MPI_ASSERTNALIGN(stoi(msgs[2]) == source);
    // below is commented on purpose, do not uncomment
    /* MPI_ASSERTNALIGN(__requests.find(msgs[3]) == __requests.end()); */
    __requests[msgs[3]] = request;
    if(source == MPI_ANY_SOURCE) {
        source = lookahead(
                orders, 
                __order_index, 
                msgs[3]);
        MPI_ASSERTNALIGN(source != -1);
    }
    int ret = PMPI_Recv_init(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            request);
    MPI_ASSERTNALIGN(ret == MPI_SUCCESS);
    return ret;
}

int MPI_Startall (
    int count, 
    MPI_Request array_of_requests[]
) {
    FUNCGUARD();
    DEBUG0("MPI_Startall\n");
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
    int ret = PMPI_Startall(
            count, array_of_requests);
    MPI_ASSERT(ret == MPI_SUCCESS);
    return ret;
}

int MPI_Request_free (
    MPI_Request *request
) {
    FUNCGUARD();
    DEBUG0("MPI_Request_free:%p\n", request);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    vector<string> msgs = parse(
            orders[__order_index++], ':');
    MPI_ASSERT(msgs[0] == "MPI_Request_free");
    MPI_ASSERT(stoi(msgs[1]) == rank);
    MPI_ASSERT(__requests.find(msgs[2]) != __requests.end());
    MPI_ASSERT(__requests[msgs[2]] == request);
    int ret = PMPI_Request_free(request);
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
    /* DEBUG0("MPI_Barrier Called:%d\n", rank); */
    int ret = PMPI_Barrier(comm);
    MPI_ASSERT(ret == MPI_SUCCESS);
    return ret;
}
#endif
