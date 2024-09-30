/*
 * CAUTION: loop trees are created at the static analysis phase,
 * so if you wish to know the location of the node, you need to read it here too.
 */

#include "mpiRecordReplay.h"

using namespace std;

static map<MPI_Request *, string> __requests;

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
/* FILE *recordtraceFile = nullptr; */
/* #define RECORDTRACE(...) \ */
    /* fprintf(recordtraceFile, __VA_ARGS__); \ */
/* #else */
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
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG("MPI_Init rank:%d\n", rank);

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

    return ret;
}

int MPI_Finalize(void) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DEBUG("MPI_Finalize, rank:%d\n", rank);
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
    MPI_Status *status) {
    if(!original_MPI_Recv) {
        original_MPI_Recv = reinterpret_cast<
            int (*)(
                    void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Recv"));
    }

    return __MPI_Recv(
            buf, 
            count, 
            datatype, 
            source, 
            tag, 
            comm, 
            status,
            messagePool,
            recordFile,
            nodecnt);
}

int MPI_Send(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm
) {
    if(!original_MPI_Send) {
        original_MPI_Send = reinterpret_cast<
            int (*)(
                    const void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm)>(
                        dlsym(RTLD_NEXT, "MPI_Send"));
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG("MPI_Send:to %d, at %d\n", dest, rank);
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    int ret = 0;
    int size;
    MPI_Type_size(datatype, &size);
    //MPI_ASSERT(count > 0);
    stringstream ss = convertData2StringStream(
            buf, 
            datatype, 
            count, 
            __LINE__);
    ss << lastNodes << '|' << size;
    string str = ss.str();
    if(str.size() + 1 >= msgSize) {
        fprintf(stderr, "message size is too large, length: %lu\n%s\n", 
                str.length(), str.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    /*
    fprintf(stderr, "sending message at MPI_Send %s, length: %lu, at rank:%d to dest: %d\n", 
            str.c_str(), 
            str.size(),
            rank,
            dest);
    */
    ret = original_MPI_Send(
            str.c_str(), 
            str.size() + 1, 
            MPI_CHAR, 
            dest, 
            tag, 
            comm);
    fprintf(recordFile, "MPI_Send:%d:%d:%lu\n", 
            rank, 
            dest, 
            nodecnt);
    //DEBUG("MPI_send return rank: %d\n", rank);
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
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG("MPI_Irecv:%p:%d\n", request, rank);
    if(!original_MPI_Irecv) {
        original_MPI_Irecv = reinterpret_cast<
            int (*)(
                    void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Request *)>(
                        dlsym(RTLD_NEXT, "MPI_Irecv"));
    }

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
        original_MPI_Isend = reinterpret_cast<
            int (*)(
                    const void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Request *)>(
                        dlsym(RTLD_NEXT, "MPI_Isend"));
    }
    string lastNodes = updateAndGetLastNodes(
            loopTrees, TraceType::RECORD);
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int size;
    MPI_Type_size(datatype, &size);
    int ret = 0;
    stringstream ss = convertData2StringStream(
            buf, 
            datatype, 
            count, 
            __LINE__);
    ss << lastNodes << '|' << size;
    string str = ss.str();
    if(rank == 1 && dest == 3 && datatype == MPI_DOUBLE) {
        fprintf(stderr, "sending message at MPI_Isend %s, length: %lu, at rank:%d to dest: %d, request: %p\n", 
                str.c_str(), 
                str.size(),
                rank,
                dest,
                request);
    }

    if(str.size() + 1 >= msgSize) {
        fprintf(stderr, "message size is too large, length: %lu\n%s\n", 
                str.length(), str.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string typeName = convertDatatype(datatype);
    /*
    DEBUG("MPI_ISend:to %d, at %d, datatype: %s, count: %d, msg: %s\n", 
            dest, 
            rank, 
            typeName.c_str(), 
            count, 
            str.c_str());
    */

    ret = original_MPI_Isend(
            (void *)str.c_str(), 
            str.size() + 1, 
            MPI_CHAR, 
            dest, 
            tag, 
            comm, 
            request);

    messagePool.addMessage(
            request, 
            (void *)buf, 
            datatype, 
            count, 
            tag, 
            comm, 
            dest, 
            true /* isSend */);

    // I just need to keep track of the request
    fprintf(recordFile, "MPI_Isend:%d:%d:%p:%lu\n", 
            rank, 
            dest, 
            request, 
            nodecnt);
    __requests[request] = "MPI_Isend";
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
    fprintf(stderr, "MPI_Irsend is not supported yet\n");
    MPI_Abort(MPI_COMM_WORLD, 1);

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
    if(!original_MPI_Cancel) {
        original_MPI_Cancel = reinterpret_cast<
            int (*)(
                    MPI_Request *)>(
                        dlsym(RTLD_NEXT, "MPI_Cancel"));
    }
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = __MPI_Cancel(
            request, 
            messagePool, 
            recordFile, 
            nodecnt);
    __requests.erase(request);
    return ret;
}

int MPI_Test(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) {
    if(!original_MPI_Test) {
        original_MPI_Test = reinterpret_cast<
            int (*)(
                    MPI_Request *, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Test"));
    }
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
    if(!original_MPI_Testall) {
        original_MPI_Testall = reinterpret_cast<
            int (*)(
                    int, 
                    MPI_Request *, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Testall"));
    }
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
    //DEBUG("MPI_Testsome:%d:%d:%p\n", rank, incount, array_of_requests);
    /*
     * first check if we have any matching peeked message
     * if there are some, abort for now
     */
    int ret;
    MessageBuffer *msgBuf;
    for(int i = 0; i < incount; i++) {
        msgBuf = messagePool.peekMessage(&array_of_requests[i]);
        if(msgBuf != nullptr) {
            ret = messagePool.loadPeekedMessage(
                    msgBuf->buf_, 
                    msgBuf->dataType_,
                    msgBuf->count_,
                    msgBuf->tag_, 
                    msgBuf->comm_, 
                    msgBuf->src_);
            if(ret != -1) {
                fprintf(stderr, "we currently do not support the case\
                        where there's a peeked message for MPI_Testsome.\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }

    MPI_Status stats [incount];
    ret = original_MPI_Testsome(
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
    if(*outcount > 0) {
        string lastNodes;
        for(int i = 0; i < *outcount; i++) {
            int ind = array_of_indices[i];
            //DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank);
            lastNodes = messagePool.loadMessage(
                    &array_of_requests[ind], &array_of_statuses[ind]);
            if(lastNodes.length() > 0) {
                //fprintf(stderr, "received at %s\n", lastNodes.c_str());
            }
            MPI_ASSERT(
                    __requests.find(&array_of_requests[ind]) != __requests.end());
            fprintf(recordFile, ":%p:%d", 
                    &(array_of_requests[ind]), array_of_statuses[ind].MPI_SOURCE); 
            /* __requests.erase(&array_of_requests[ind]); */
        }
    }
    fprintf(recordFile, ":%lu\n", nodecnt);
    return ret;
}

int MPI_Wait(
    MPI_Request *request, 
    MPI_Status *status
) {
    if(!original_MPI_Wait) {
        original_MPI_Wait = reinterpret_cast<
            int (*)(
                    MPI_Request *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Wait"));
    }
    MPI_ASSERT(__requests.find(request) != __requests.end());
    int ret = __MPI_Wait(
            request, 
            status, 
            messagePool, 
            recordFile, 
            nodecnt);
    return ret;
}

int MPI_Waitany(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) {
    fprintf(stderr, "MPI_Waitany not supported yet\n");
    MPI_Abort(MPI_COMM_WORLD, 1);

    int rank;
    if(!original_MPI_Waitany) {
        original_MPI_Waitany = reinterpret_cast<
            int (*)(
                    int, 
                    MPI_Request *, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Waitany"));
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
    } else {
        MPI_ASSERT(__requests.find(&array_of_requests[*index]) != __requests.end());
        /* string req = __requests[&array_of_requests[*index]]; */
        fprintf(recordFile, "MPI_Waitany:%d:SUCCESS:%p:%d:%lu\n", 
                rank, 
                &array_of_requests[*index], 
                stat.MPI_SOURCE, 
                nodecnt);
    }

    return ret;
}

// if it is successful, then record all of the requests and the source
int MPI_Waitall(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) {
    if(!original_MPI_Waitall) {
        original_MPI_Waitall = reinterpret_cast<
            int (*)(
                    int, 
                    MPI_Request *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Waitall"));
    }
    int ret = 0;
    try {
        ret = __MPI_Waitall(
                count, 
                array_of_requests, 
                array_of_statuses, 
                messagePool, 
                recordFile, 
                nodecnt);
    } catch(const exception &e) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string localLastNodes = updateAndGetLastNodes(
                loopTrees, TraceType::RECORD);
        fprintf(stderr, "exception caught at %s\nrank: %d\nlastNodes:%s", 
                __func__,
                rank, 
                localLastNodes.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for(int i = 0; i < count; i++) {
        MPI_ASSERT(__requests.find(&array_of_requests[i]) != __requests.end());
    }
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
    FUNCGUARD();
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /*
    if(!original_MPI_Probe) {
        original_MPI_Probe = reinterpret_cast<
            int (*)(
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Probe"));
    }
    */
    if(!original_MPI_Recv) {
        original_MPI_Recv = reinterpret_cast<
            int (*)(
                    void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Recv"));
    }
    /*
     * first, let's check if we have any appropriate message at peeked
     * if so, set the status to be appropriate values
     * CAUTION: status cannot be nullptr or STATUS_IGNORE 
     * (otherwise why would you call probe?)
     */
    int ret = messagePool.peekPeekedMessage(source, tag, comm, status);

    /*
     * if successful, record it and return without actually calling the function
     */
    if(ret != -1) {
        recordMPIProbe(
                recordFile,
                rank, 
                source, 
                tag, 
                status,
                nodecnt);
        return MPI_SUCCESS;
    }

    /*
     * if it's NOT successful, you need to call MPI_Recv instead
     * then add it to the peeked message pool
     */
    char tmpBuf[msgSize]; 
    MPI_Status stat;
    ret = original_MPI_Recv(
            tmpBuf, 
            msgSize, 
            MPI_CHAR, 
            source, 
            tag, 
            comm, 
            &stat);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &stat, 
                sizeof(MPI_Status));
    }
    MPI_ASSERT(ret == MPI_SUCCESS);
    messagePool.addPeekedMessage(
            tmpBuf, 
            msgSize, 
            tag, 
            comm, 
            stat.MPI_SOURCE);

    /*
     * update status' ucount manually
     * other member values should be correct
     * when called MPI_Recv
     */
    string tmpStr(tmpBuf);
    auto tokens = parse(tmpStr, '|');
    MPI_ASSERT(tokens.size() >= 2);
    int size = stoi(tokens.back());
    status->_ucount = (tokens.size() - 2) * size;

    recordMPIProbe(
            recordFile,
            rank, 
            source, 
            tag, 
            status,
            nodecnt);
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
    /*
     * if the actual function call is still not set
     * set them
     */
    if(!original_MPI_Iprobe) {
        original_MPI_Iprobe = reinterpret_cast<
            int (*)(
                    int, 
                    int, 
                    MPI_Comm, 
                    int *, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Iprobe"));
    }
    if(!original_MPI_Recv) {
        original_MPI_Recv = reinterpret_cast<
            int (*)(
                    void *, 
                    int, 
                    MPI_Datatype, 
                    int, 
                    int, 
                    MPI_Comm, 
                    MPI_Status *)>(
                        dlsym(RTLD_NEXT, "MPI_Recv"));
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG("MPI_Iprobe:%d, at rank:%d\n", source, rank);

    /*
     * we first look at the peeked messages
     * if we have appropriate message, we don't even call the function
     * and return
     */
    MPI_Status statIprobe;
    int ret = messagePool.peekPeekedMessage(
            source, 
            tag, 
            comm, 
            &statIprobe);
    if(ret != -1) {
        *flag = 1;  
        recordMPIIprobeSuccess(
                recordFile,
                rank, 
                source, 
                tag, 
                &statIprobe,
                nodecnt);
        if(status != MPI_STATUS_IGNORE) {
            memcpy(
                    status, 
                    &statIprobe, 
                    sizeof(MPI_Status));
        }
        fprintf(stderr, "did it work here, let's stop here, rank: %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
        /*
        DEBUG("MPI_Iprobe at rank: %d, source: %d, tag: %d, ucount:%lu\n", 
                rank, source, tag, statIprobe._ucount);
        */
        return MPI_SUCCESS;
    }

    ret = original_MPI_Iprobe(
            source, 
            tag, 
            comm, 
            flag, 
            &statIprobe);
    MPI_ASSERT(ret == MPI_SUCCESS);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &statIprobe, 
                sizeof(MPI_Status));
    }
    if(*flag) {
        /*
         * if iprobe found any message, 
         * receive it and add it to peeked messages
         */
        MPI_Status statRecv;
        char tmpBuf[msgSize];
        ret = original_MPI_Recv(
                tmpBuf, 
                msgSize, 
                MPI_CHAR, 
                statIprobe.MPI_SOURCE, 
                tag, 
                comm, 
                &statRecv);
        /*
        fprintf(stderr, "we received a message at rank: %d, src: %d, tag: %d\n%s\n", 
                rank, statRecv.MPI_SOURCE, tag, tmpBuf);
        */
        MPI_ASSERT(statIprobe.MPI_SOURCE == statRecv.MPI_SOURCE);
        MPI_ASSERT(ret == MPI_SUCCESS);
        messagePool.addPeekedMessage(
                tmpBuf,
                msgSize,
                tag,
                comm,
                statRecv.MPI_SOURCE);
                
        if(status != MPI_STATUS_IGNORE) {
        /* update status' ucount manually */
            /*
             * DEBUG("result of the MPI_Recv, rank: %d, src: %d, tag: %d, ucount:%lu\n", 
                    rank, statRecv.MPI_SOURCE, tag, statRecv._ucount);
             */
            auto tokens = parse(string(tmpBuf), '|');
            MPI_ASSERT(tokens.size() >= 2);
            int size = stoi(tokens.back());
            status->_ucount = (tokens.size() - 2) * size;
            /*
             * DEBUG("MPI_Iprobe at rank: %d, source: %d, tag: %d, ucount:%lu\n", 
                    rank, status->MPI_SOURCE, tag, status->_ucount);
             */
        }
        /* record the result of Iprobe */
        // I think this is wrong, you have to think about the count
        recordMPIIprobeSuccess(
                recordFile,
                rank, 
                source, 
                tag, 
                status,
                nodecnt);

    } else {
        fprintf(recordFile, "MPI_Iprobe:%d:%d:%d:FAIL:%lu\n", 
                rank, 
                source, 
                tag, 
                nodecnt);
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
    int rank;
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
    FUNCGUARD();
    int rank;
    //DEBUG0("MPI_Recv_init:%d:%p\n", source, request);
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
    FUNCGUARD();
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
    fprintf(recordFile, "MPI_Startall:%d:%d", 
            rank, count);
    RECORDTRACE("MPI_Startall:%d:%d", 
            rank, count);
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
    FUNCGUARD();
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
    messagePool.deleteMessage(request);
    return ret;
}

// MPI_Gather does not really show the way to create non-determinism, so I would just let it be
// same for MPI_Barrier
