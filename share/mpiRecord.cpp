/*
 * CAUTION: loop trees are created at the static analysis phase,
 * so if you wish to know the location of the node, you need to read it here too.
 */

#include "mpiRecordReplay.h"

using namespace std;
using json = nlohmann::json;

static map<MPI_Request *, string> __requests;

/*
 * this shows where the locations of calls are
 * the first string is the name of the function (e.g. MPI_Recv)
 * the second string is the location of the call
 * value is the nodecnt
 * dump it at the very end
 */
unordered_map<string, map<string, unsigned long>> __callLocations;

FILE *recordFile = nullptr;
FILE *traceFile = nullptr;

/*
 * nodecnt is incremented at printBBname
 */
static unsigned long nodecnt = 0;
static unordered_map<string, loopNode *> loopTrees;

Logger logger;
MessagePool messagePool;

#ifdef DEBUG_MODE
extern vector<vector<string>> recordTracesRaw;
extern vector<shared_ptr<element>> recordTraces;
#define RECORDTRACE(...)
#endif

extern "C" void printBBname(const char *name) {
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
        string str(name);
        appendRecordTracesRaw(parse(str, ':'));
    }
    return;
}

void checkCallLocations(
        string mpiCall, string& lastNodes) {
    static unordered_map<string, unsigned long> lastCallLocations;
    if(__callLocations[mpiCall].find(lastNodes) 
            != __callLocations[mpiCall].end()
            && __callLocations[mpiCall][lastNodes] != nodecnt - 1) {
        //if(lastCallLocations[mpiCall] + 1 != nodecnt - 1) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        fprintf(stderr, "rank: %d, mpiCall: %s, lastNodes: %s\nlastCallLocations:%lu, nodecnt:%lu\n", 
            rank,
            mpiCall.c_str(),
            lastNodes.c_str(),
            lastCallLocations[mpiCall], 
            nodecnt - 1);
        MPI_Abort(MPI_COMM_WORLD, 1);
        //}
    } else {
        __callLocations[mpiCall][lastNodes] = nodecnt - 1;
    }
    lastCallLocations[mpiCall] = nodecnt - 1;
}

int MPI_Init(
    int *argc, 
    char ***argv
) {
    int ret = PMPI_Init(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG("MPI_Init rank:%d\n", rank);

    /*
     * open the message exchange record file
     */
    string filename = ".record" + to_string(rank) + ".txt";  
    recordFile = fopen(filename.c_str(), "w");
    if(recordFile == nullptr) {
        fprintf(stderr, "failed to open record file\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /*
     * open the trace file
     */
    filename = ".record" + to_string(rank) + ".tr";
    traceFile = fopen(filename.c_str(), "w");
    if(traceFile == nullptr) {
        fprintf(stderr, "failed to open trace file\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /*
     * parse the loop tree file
     */ 
    string loopTreeFileName = "loops.dot";
    loopTrees = parseDotFile(loopTreeFileName);
    for(auto lt: loopTrees) {
        lt.second->fixExclusives();
    }
    
    __callLocations["MPI_Recv"] = map<string, unsigned long>();
    __callLocations["MPI_Irecv"] = map<string, unsigned long>();
    __callLocations["MPI_Isend"] = map<string, unsigned long>();
    __callLocations["MPI_Test"] = map<string, unsigned long>();
    __callLocations["MPI_Testall"] = map<string, unsigned long>();
    __callLocations["MPI_Testsome"] = map<string, unsigned long>();
    __callLocations["MPI_Wait"] = map<string, unsigned long>();
    __callLocations["MPI_Waitany"] = map<string, unsigned long>();
    __callLocations["MPI_Waitall"] = map<string, unsigned long>();
    __callLocations["MPI_Probe"] = map<string, unsigned long>();
    __callLocations["MPI_Iprobe"] = map<string, unsigned long>();
    __callLocations["MPI_Request_free"] = map<string, unsigned long>();
    __callLocations["MPI_Cancel"] = map<string, unsigned long>();

    return ret;
}

int MPI_Finalize(void) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(recordFile != nullptr);
    fclose(recordFile);
    MPI_ASSERT(traceFile != nullptr);
    fflush(traceFile);
    fclose(traceFile);
    for(auto lt: loopTrees) {
        delete lt.second;
    }
    json j;
    for(const auto &[key, inner_map]: __callLocations) {
        for(const auto &[subkey, value]: inner_map) {
            j[key][subkey] = value;
        }
    }
    string jsonFileName = "callLocations-" + to_string(rank) + ".json";
    std::ofstream jsonFile(jsonFileName);
    if(jsonFile.is_open()) {
        jsonFile << j.dump(4) << std::endl;
        jsonFile.close();
    } else {
        fprintf(stderr, "failed to open callLocations.json\n");
    }
    int ret = PMPI_Finalize();
    /* DEBUG("MPI_Finalize done, rank:%d\n", rank); */
    return ret;
}

int MPI_Recv(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status) {
    
    int ret =  __MPI_Recv(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            status,
            messagePool,
            nullptr,
            recordFile,
            nodecnt);
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Recv", lastNodes);
    return ret;
}

int MPI_Send(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm
) {
    //DEBUG("MPI_Send:to %d, at %d\n", dest, rank);
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    int ret = __MPI_Send(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm,
            messagePool,
            lastNodes);
    MPI_ASSERT(ret == MPI_SUCCESS);
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
    /* DEBUG("MPI_Irecv: %p, source: %d\n", */ 
    /*         request, source); */
    int ret = __MPI_Irecv(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            request,
            messagePool,
            recordFile,
            nodecnt);
    __requests[request] = "MPI_Irecv";
    /*
     * updating the call locations
     */
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Irecv", lastNodes);
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
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    /*
     * below is for debugging, delete afterwards
     */
    /* int rank; */
    /* MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
    /* if(rank == 4 && dest == 6) { */
    /*     auto lastNodesVec = parse(lastNodes, '/'); */
    /*     if(lastNodesVec.back() == "get_hash:exit:1") { */
    /*         print(recordTraces, 0); */
    /*     } */
    /* } */

    int ret = __MPI_Isend(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request,
            messagePool,
            lastNodes,
            recordFile,
            nodecnt);

    MPI_ASSERT(ret == MPI_SUCCESS);
    __requests[request] = "MPI_Isend";

    checkCallLocations("MPI_Isend", lastNodes);
    return ret;
}

/* 
 * This is an MPI_Isend that needs the receiver to be ready 
 * by the time it is called. Otherwise, it creates an undefined behavior 
 * (e.g. lost messages, program crash, data corruption, etc.).
 * We will simply abort when this function is called as of now 
 * (and implement as necessary).
 */
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
    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Irsend|%d|%d|%p|%lu\n", 
            rank, 
            dest, 
            request, 
            nodecnt);
    RECORDTRACE("MPI_Irsend|%d|%d|%p\n", 
            rank, 
            dest, 
            request);
    __requests[request] = "MPI_Irsend";
    return ret;
}

int MPI_Cancel(
    MPI_Request *request
) {
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = __MPI_Cancel(
            request, 
            messagePool, 
            recordFile, 
            nodecnt);
    __requests.erase(request);
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Cancel", lastNodes);
    return ret;
}

int MPI_Test(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) {
    MPI_ASSERT(
            __requests.find(request) != __requests.end());
    int ret = __MPI_Test(
            request, 
            flag, 
            status, 
            messagePool, 
            recordFile, 
            nodecnt);
    if(*flag) {
        __requests.erase(request);
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Test", lastNodes);
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
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = __MPI_Testall(
            count, 
            array_of_requests, 
            flag, 
            array_of_statuses, 
            messagePool, 
            recordFile, 
            nodecnt);

    if(*flag) {
        for(int i = 0; i < count; i++) {
            MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
        }
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Testall", lastNodes);

    return ret;
}

int MPI_Testsome(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) {
    int ret = __MPI_Testsome(
            incount, 
            array_of_requests, 
            outcount, 
            array_of_indices, 
            array_of_statuses, 
            messagePool, 
            recordFile, 
            nodecnt);

    if(*outcount > 0) {
        for(int i = 0; i < *outcount; i++) {
            int ind = array_of_indices[i];
            MPI_ASSERT(
                    __requests.find(&array_of_requests[ind]) != __requests.end());
        }
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Testsome", lastNodes);
    return ret;
}

int MPI_Wait(
    MPI_Request *request, 
    MPI_Status *status
) {
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = __MPI_Wait(
            request, 
            status, 
            messagePool, 
            nullptr,
            recordFile, 
            nodecnt);
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Wait", lastNodes);
    return ret;
}

int MPI_Waitany(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) {
    FUNCGUARD();
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    int ret = PMPI_Waitany(
            count, 
            array_of_requests, 
            index, 
            &stat);
    
    if(status != MPI_STATUS_IGNORE) {
        *status = stat;
    }

    if(*index == MPI_UNDEFINED) {
        fprintf(recordFile, "MPI_Waitany|%d|FAIL|%lu\n", 
                rank, nodecnt);
    } else {
        MPI_ASSERT(__requests.find(&array_of_requests[*index]) != __requests.end());
        /* string req = __requests[&array_of_requests[*index]]; */
        fprintf(recordFile, "MPI_Waitany|%d|SUCCESS|%p|%d|%lu\n", 
                rank, 
                &array_of_requests[*index], 
                stat.MPI_SOURCE, 
                nodecnt);
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Waitany", lastNodes);

    return ret;
}

// if it is successful, then record all of the requests and the source
int MPI_Waitall(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) {
    for(int i = 0; i < count; i++) {
        MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
    } 
    int ret = 0;
    try {
        ret = __MPI_Waitall(
                count, 
                array_of_requests, 
                array_of_statuses, 
                messagePool, 
                nullptr,
                recordFile, 
                nodecnt);
    } catch(const exception &e) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string localLastNodes = updateAndGetLastNodes(
                loopTrees, TraceType::RECORD);
        fprintf(stderr, "exception caught at %s\nrank: %d\n", 
                __func__,
                rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Waitall", lastNodes);
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
    int ret;
    try {
        ret = __MPI_Probe(
            source, 
            tag, 
            comm, 
            status, 
            messagePool, 
            recordFile, 
            nodecnt);
    } catch (const exception &e) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string localLastNodes = updateAndGetLastNodes(
                loopTrees, TraceType::RECORD);
        fprintf(stderr, "exception caught at %s\nrank: %d\n", 
                __func__,
                rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Probe", lastNodes);
    return ret;
}

int MPI_Iprobe (
    int source, 
    int tag, 
    MPI_Comm comm, 
    int *flag, 
    MPI_Status *status
) {
    int ret;
    try {
        ret = __MPI_Iprobe(
                source, 
                tag, 
                comm, 
                flag, 
                status, 
                messagePool, 
                recordFile, 
                nodecnt);
    } catch (const exception &e) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string localLastNodes = updateAndGetLastNodes(
                loopTrees, TraceType::RECORD);
        fprintf(stderr, "exception caught at %s\nrank: %d\n", 
                __func__,
                rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Iprobe", lastNodes);
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = PMPI_Send_init(
            buf, 
            count, 
            datatype, 
            dest, 
            tag, 
            comm, 
            request);
    MPI_ASSERT(__requests.find(request) == __requests.end());
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Send_init|%d|%d|%p\n", 
            rank, 
            dest, 
            request);
    RECORDTRACE("MPI_Send_init|%d|%d|%p\n", 
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
    FUNCGUARD();
    int rank;
    //DEBUG0("MPI_Recv_init:%d:%p\n", source, request);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret = PMPI_Recv_init(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            request);
    MPI_ASSERT(__requests.find(request) == __requests.end());
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Recv_init|%d|%d|%p\n", 
            rank, 
            source, 
            request);
    RECORDTRACE("MPI_Recv_init|%d|%d|%p\n", 
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
    FUNCGUARD();
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG0("MPI_Startall:%d\n", count);
    int ret = PMPI_Startall(
            count, array_of_requests);
    MPI_ASSERT(ret == MPI_SUCCESS);
    fprintf(recordFile, "MPI_Startall|%d|%d", 
            rank, count);
    for(int i = 0; i < count; i++) {
        MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
        fprintf(recordFile, "|%p", &array_of_requests[i]);
    }
    fprintf(recordFile, "\n");
    return ret;
}

int MPI_Request_free (
    MPI_Request *request
) {
    FUNCGUARD();
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = PMPI_Request_free(request);
    MPI_EQUAL(ret, MPI_SUCCESS);
    fprintf(recordFile, "MPI_Request_free|%d|%p\n", 
            rank, request);
    __requests.erase(request);
    messagePool.deleteMessage(request); 
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    checkCallLocations("MPI_Request_free", lastNodes);
    return ret;
}

// MPI_Gather does not really show the way to create non-determinism, so I would just let it be
// same for MPI_Barrier
