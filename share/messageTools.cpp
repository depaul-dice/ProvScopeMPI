#include "messageTools.h"

using namespace std;

void recordMPIIprobeSuccess(
       FILE *recordFile,
        int rank,
        int source,
        int tag,
        MPI_Status *stat,
        unsigned long nodeCount) {
    if(source == MPI_ANY_SOURCE) {
        fprintf(recordFile, "MPI_Iprobe:%d:%d:%d:SUCCESS:%d:%lu\n", 
                rank, 
                source, 
                tag, 
                stat->MPI_SOURCE, 
                nodeCount);
    } else {
        fprintf(recordFile, "MPI_Iprobe:%d:%d:%d:SUCCESS:%lu\n", 
                rank, 
                source, 
                tag, 
                nodeCount);
    }

}

void recordMPIProbe (
        FILE *recordFile,
        int rank,
        int source,
        int tag,
        MPI_Status *status,
        unsigned long nodeCount) {
    if(source == MPI_ANY_SOURCE) {
        fprintf(recordFile, "MPI_Probe:%d:%d:%d:%d:%lu\n", 
                rank, 
                source, 
                tag, 
                status->MPI_SOURCE, 
                nodeCount);
    } else {
        fprintf(recordFile, "MPI_Probe:%d:%d:%d:%lu\n", 
                rank, 
                source, 
                tag, 
                nodeCount);
    }
}

string convertDatatype(
        MPI_Datatype datatype) {
    char typeName[MPI_MAX_OBJECT_NAME];
    int nameLength;
    MPI_Type_get_name(
            datatype, 
            typeName, 
            &nameLength);
    return string(typeName);
}

stringstream convertData2StringStream(
        const void *buf, 
        MPI_Datatype datatype,
        int count,
        int lineNum) {
    stringstream ss;
    if(datatype == MPI_INT) {
        int *data = (int *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << '|';
        }
    } else if(datatype == MPI_LONG_LONG_INT) {
        long long int *data = (long long int *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << '|';
        }
    } else if(datatype == MPI_DOUBLE) {
        double *data = (double *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << '|';
        }
    } else if(datatype == MPI_CHAR
            || datatype == MPI_BYTE) {
        for(int i = 0; i < count; i++) {
            char c = ((char *)buf)[i];
            ss << (int)c << '|';
        }
    } else {
        unsupportedDatatype(
                -1, 
                lineNum, 
                datatype);
    }
    return ss;
}

void convertMsgs2Buf(
        void *buf, 
        MPI_Datatype datatype, 
        int count,
        std::vector<std::string>& msgs,
        int lineNum,
        int rank) {
    if(datatype == MPI_INT) {
        for(int i = 0; i < msgs.size() - 2; i++) {
            ((int *)buf)[i] = stoi(msgs[i]);
        } 
    } else if(datatype == MPI_CHAR 
            || datatype == MPI_BYTE) {
        for(int i = 0; i < msgs.size() - 2; i++) {
            ((char *)buf)[i] = (char)stoi(msgs[i]);
        }
    } else if(datatype == MPI_DOUBLE) {
        for(int i = 0; i < msgs.size() - 2; i++) {
            ((double *)buf)[i] = stod(msgs[i]);
        }
    } else if(datatype == MPI_LONG_LONG_INT) {
        for(int i = 0; i < msgs.size() - 2; i++) {
            ((long long int *)buf)[i] = stoll(msgs[i]);
        }
    } else {
        unsupportedDatatype(rank, lineNum, datatype);
    }

}

void unsupportedDatatype(
        int rank, 
        int lineNum, 
        MPI_Datatype datatype) {
    string typeName = convertDatatype(datatype);
    fprintf(stderr, "unsupported datatype: %s, at rank:%d, line # %d\n", 
            typeName.c_str(), 
            rank,
            lineNum);
    MPI_Abort(MPI_COMM_WORLD, 1);
}

int __MPI_Recv(
        void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int source, 
        int tag, 
        MPI_Comm comm, 
        MPI_Status *status,
        MessagePool& messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status localStatus;
    int ind = messagePool.peekPeekedMessage(
            source,
            tag,
            comm,
            &localStatus);
    /*
     * if it is successful, then load the message and return
     * don't call MPI_Recv
     */
    int ret;
    if(ind != -1) {
        if(status != MPI_STATUS_IGNORE) {
            memcpy(
                    status, 
                    &localStatus, 
                    sizeof(MPI_Status));
        }
        ret = messagePool.loadPeekedMessage(
                buf,
                datatype,
                count,
                tag,
                comm,
                source);
        MPI_ASSERT(ret != -1);
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Recv:%d:%d:%lu\n",
                    rank,
                    status->MPI_SOURCE,
                    nodeCnt);
        }
        return MPI_SUCCESS;
    }
    /*
     * if it is NOT successful, then call the actual MPI_Recv
     * update status and buf accordingly
     */
    char tmpBuffer[msgSize];
    ret = PMPI_Recv(
            (void *)tmpBuffer, 
            msgSize, 
            MPI_CHAR, 
            source, 
            tag, 
            comm, 
            &localStatus);
    MPI_ASSERT(ret == MPI_SUCCESS);
    string tmpStr(tmpBuffer);
    auto msgs = parse(tmpStr, '|');
    if(msgs.size() > count + 2) {
        DEBUG("message size is too large, length: %lu, count: %d\
                tag: %d, source: %d, rank: %d\n", 
                msgs.size(), 
                count,
                tag,
                source,
                rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    //fprintf(stderr, "received message from send at %s\n", msgs[msgs.size() - 2].c_str());
    convertMsgs2Buf(
            buf,
            datatype,
            count,
            msgs,
            __LINE__,
            rank);
    int size;
    MPI_Type_size(datatype, &size);
    int sizeFromMsgs = stoi(msgs.back());
    /*
     * checking if the counts are the same
     */
    MPI_ASSERT(size == sizeFromMsgs);
    /*
     * manipulating count manually
     */
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &localStatus, 
                sizeof(MPI_Status));
        status->_ucount = (msgs.size() - 2) * size;
    }
    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Recv:%d:%d:%lu|%s\n",
                rank,
                status->MPI_SOURCE,
                nodeCnt,
                msgs[msgs.size() - 2].c_str());
    }
    return ret;
}

int __MPI_Irecv(
        void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int source, 
        int tag, 
        MPI_Comm comm, 
        MPI_Request *request,
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret;
    /*
     * let's not support the case where there exists a peeked message
     * for now
     */
    MPI_Status stat;
    ret = messagePool.peekPeekedMessage(
            source, 
            tag, 
            comm, 
            &stat);
    if(ret != -1) {
        fprintf(stderr, "we don't support the case where there's \
                a peeked message for MPI_Irecv.\n\
                Aborting...\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    /*
     * in case the datatype is something we don't support,
     * we should abort
     */
    if(datatype != MPI_INT 
            && datatype != MPI_CHAR 
            && datatype != MPI_BYTE
            && datatype != MPI_DOUBLE
            && datatype != MPI_LONG_LONG_INT) {
        unsupportedDatatype(rank, __LINE__, datatype);
    }

    /*
     * 1. Create a new buffer for the request
     * 2. Call irecv with the new buffer
     * 3. Return irecv
     * 4. When the message is received (at either test or wait), 
     *  alternate the message in the original buffer
     * 5. Record the node timing
     */
    char *tmpBuf = messagePool.addMessage(
            request, 
            buf, 
            datatype,
            count, 
            tag,
            comm,
            source, /* this could be MPI_ANY_SOURCE */
            false /* isSend */);
    //cerr << "at addMessage in irecv, count is " << count << endl;
    //cerr << "Irecv: rank: " << rank << ", source: " << source << ", Request *: " << request << ", realBuf_: " << static_cast<void *>(tmpBuf) << endl;
    ret = PMPI_Irecv(
            static_cast<void *>(tmpBuf),
            msgSize, 
            MPI_CHAR /* datatype */,
            source, 
            tag, 
            comm, 
            request);
 
    // I just need to keep track of the request
    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Irecv:%d:%d:%p:%lu\n",
                rank, 
                source, 
                request, 
                nodeCnt);
    }
    return ret;
}

int __MPI_Isend(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request,
    MessagePool &messagePool,
    string& lastNodes,
    FILE *recordFile,
    unsigned long nodeCnt) {
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

    if(str.size() + 1 >= msgSize) {
        fprintf(stderr, "message size is too large, length: %lu\n%s\n", 
                str.length(), str.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string typeName = convertDatatype(datatype);

    char *realBuf_ = messagePool.addMessage(
            request, 
            (void *)buf, 
            datatype, 
            count, 
            tag, 
            comm, 
            dest, 
            true /* isSend */);

    memcpy(
            realBuf_, 
            str.c_str(), 
            str.size() + 1);

    /*
     * CAUTION: we need to send realBuf_ instead of str, 
     * because it could get corrupt after this function goes out of context
     */
    ret = PMPI_Isend(
            (void *)realBuf_, 
            str.size() + 1, 
            MPI_CHAR, 
            dest, 
            tag, 
            comm, 
            request);

    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Isend:%d:%d:%p:%lu\n", 
                rank, 
                dest, 
                request, 
                nodeCnt);
    }
    return ret;
}


int __MPI_Wait(
        MPI_Request *request, 
        MPI_Status *status,
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /*
     * Look for the message buffer information in MPI_Request
     */
    MessageBuffer *msgBuf = messagePool.peekMessage(request);
    MPI_ASSERT(msgBuf != nullptr);
    int src;
    int ret = messagePool.loadPeekedMessage(
            msgBuf->buf_, 
            msgBuf->dataType_,
            msgBuf->count_,
            msgBuf->tag_, 
            msgBuf->comm_, 
            msgBuf->src_,
            &src);
    /*
     * if the correct information is found,
     * record the information, update the status,
     * and return
     */
    if(ret != -1) {
        fprintf(stderr, "we currently do not support the case\
                where there's a peeked message for MPI_Wait.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);

        /* DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank); */
        string lastNodes = messagePool.loadMessage(
                request, status);
        //fprintf(stderr, "received at %s\n", lastNodes.c_str());
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Wait:%d:%p:SUCCESS:%d:%lu\n", 
                    rank, 
                    request, 
                    src, 
                    nodeCnt);
        }
        MPI_ASSERT(status->MPI_ERROR == MPI_SUCCESS);
        return MPI_SUCCESS;
    }

    /*
     * if the correct information is not found,
     * call the actual MPI_Wait and update the status
     * and record the information
     */
    MPI_Status stat;
    memset(&stat, 0, sizeof(MPI_Status));
    ret = PMPI_Wait(request, &stat);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &stat, 
                sizeof(MPI_Status));
    }

    MPI_ASSERT(ret == MPI_SUCCESS);
    /* DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank); */
    string lastNodes = messagePool.loadMessage(request, status);
    if(lastNodes.length() > 0) {
        //fprintf(stderr, "received at %s\n", lastNodes.c_str());
    }
    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Wait:%d:%p:SUCCESS:%d:%lu\n", 
                rank, 
                request, 
                stat.MPI_SOURCE, 
                nodeCnt);
    }
    return ret;
}

int __MPI_Test(
        MPI_Request *request, 
        int *flag, 
        MPI_Status *status,
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /*
     * first check if we have any matching peeked message
     */
    MessageBuffer *msgBuf = messagePool.peekMessage(request);
    int src;
    int ret = messagePool.loadPeekedMessage(
            msgBuf->buf_, 
            msgBuf->dataType_,
            msgBuf->count_,
            msgBuf->tag_, 
            msgBuf->comm_, 
            msgBuf->src_,
            &src);
    /*
     * if loading of the peeked message is successful
     * record it, update the status, and return
     */
    if(ret != -1) {
        fprintf(stderr, "we currently do not support the case\
                where there's a peeked message for MPI_Test.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);

        //DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank);
        string lastNodes = messagePool.loadMessage(
                request, 
                status);
        /*
         * this must be a receive request because ret == -1
         * so we should assert that lastNodes should not be an
         * empty string
         */
        MPI_ASSERT(lastNodes.length() > 0);

        //fprintf(stderr, "received at %s\n", lastNodes.c_str());
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Test:%d:%p:SUCCESS:%d:%lu\n", 
                    rank, 
                    request, 
                    src, 
                    nodeCnt);
        }
        return MPI_SUCCESS;
    }

    /*
     * if the peeked message is not found, then call the actual MPI_Test
     * and update the status and record the information
     */
    MPI_Status stat;
    memset(&stat, 0, sizeof(MPI_Status));
    ret = PMPI_Test(
            request, 
            flag, 
            &stat);
    MPI_ASSERT(ret == MPI_SUCCESS);
    if(status != MPI_STATUS_IGNORE) {
        memcpy(
                status, 
                &stat, 
                sizeof(MPI_Status));
    }

    /* 
     * use the loadMessage method only if MPI_Test was successful
     */
    if(*flag) {
        /* DEBUG("load message at line: %d, rank: %d, request: %p\n", */ 
        /*         __LINE__, rank, request); */
        string lastNodes = messagePool.loadMessage(request, status);
        if(lastNodes.length() > 0) {
            //fprintf(stderr, "received at %s\n", lastNodes.c_str());
        }
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Test:%d:%p:SUCCESS:%d:%lu\n", \
                    rank, 
                    request, 
                    stat.MPI_SOURCE, 
                    nodeCnt);
        }
    } else {
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Test:%d:%p:FAIL:%lu\n", \
                    rank, 
                    request, 
                    nodeCnt);
        }
    }
    return ret;
}

int __MPI_Waitall(
        int count, 
        MPI_Request array_of_requests[], 
        MPI_Status array_of_statuses[],
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret;
    /*
     * first check if we have any matching peeked message
     */
    for(int i = 0; i < count; i++) {
        MessageBuffer *msgBuf = messagePool.peekMessage(&array_of_requests[i]);
        MPI_ASSERT(msgBuf != nullptr);
        int src;
        ret = messagePool.loadPeekedMessage(
                msgBuf->buf_, 
                msgBuf->dataType_,
                msgBuf->count_,
                msgBuf->tag_, 
                msgBuf->comm_, 
                msgBuf->src_,
                &src);
        /*
         * if loading of the peeked message is successful
         * record it, update the status, and return
         */
        if(ret != -1) {
            fprintf(stderr, "we currently do not support the case\
                    where there's a peeked message for MPI_Waitall.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);

            /* DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank); */
            string lastNodes = messagePool.loadMessage(
                    &array_of_requests[i], 
                    &array_of_statuses[i]);
            /*
             * this must be a receive request because ret == -1
             * so we should assert that lastNodes should not be an
             * empty string
             */
            MPI_ASSERT(lastNodes.length() > 0);

            //fprintf(stderr, "received at %s\n", lastNodes.c_str());
            if(recordFile != nullptr) {
                fprintf(recordFile, "MPI_Waitall:%d:%p:SUCCESS:%d:%lu\n", 
                        rank, 
                        &array_of_requests[i], 
                        src, 
                        nodeCnt);
            }
        }
    }
    /*
     * if the peeked message is not found, then call the actual MPI_Waitall
     * and update the status and record the information
     */
    MPI_Status localStats[count];
    memset(localStats, 0, sizeof(MPI_Status) * count);
    ret = PMPI_Waitall(
            count, 
            array_of_requests, 
            localStats);

    MPI_ASSERT(ret == MPI_SUCCESS);
    string lastNodes [count];
    if(array_of_statuses != MPI_STATUSES_IGNORE) {
        memcpy(
                array_of_statuses, 
                localStats, 
                sizeof(MPI_Status) * count);
    }
    for(int i = 0; i < count; i++) {
        /*
        if(!messagePool.isSend(&array_of_requests[i])) {
            DEBUG("at %s, Request *: %p, realBuf_: %p\n", 
                    __func__, 
                    &array_of_requests[i], 
                    messagePool.getRealBuf(&array_of_requests[i]));
        }
        */
        /* DEBUG("loadMessage at line: %d, rank: %d\n", __LINE__, rank); */
        lastNodes[i] = messagePool.loadMessage(
                &array_of_requests[i], 
                &array_of_statuses[i]);
    }

    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Waitall:%d:%d\n", 
                rank, count);
        for(int i = 0; i < count; i++) {
            fprintf(recordFile, ":%p:%d", 
                    &array_of_requests[i], 
                    localStats[i].MPI_SOURCE);
        }
        fprintf(recordFile, ":%lu\n", nodeCnt);
    }
    return ret;
}

int __MPI_Testall(
        int count, 
        MPI_Request array_of_requests[], 
        int *flag, 
        MPI_Status array_of_statuses[],
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret;
    /*
     * first check if we have any matching peeked message
     */
    for(int i = 0; i < count; i++) {
        MessageBuffer *msgBuf = messagePool.peekMessage(&array_of_requests[i]);
        int src;
        ret = messagePool.loadPeekedMessage(
                msgBuf->buf_, 
                msgBuf->dataType_,
                msgBuf->count_,
                msgBuf->tag_, 
                msgBuf->comm_, 
                msgBuf->src_,
                &src);
        /*
         * if loading of the peeked message is successful
         * record it, update the status, and return
         */
        if(ret != -1) {
            fprintf(stderr, "we currently do not support the case\
                    where there's a peeked message for MPI_Testall.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);

            /* DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank); */
            string lastNodes = messagePool.loadMessage(
                    &array_of_requests[i], 
                    &array_of_statuses[i]);
            /*
             * this must be a receive request because ret == -1
             * so we should assert that lastNodes should not be an
             * empty string
             */
            MPI_ASSERT(lastNodes.length() > 0);

            //fprintf(stderr, "received at %s\n", lastNodes.c_str());
            if(recordFile != nullptr) {
                fprintf(recordFile, "MPI_Testall:%d:%p:SUCCESS:%d:%lu\n", 
                        rank, 
                        &array_of_requests[i], 
                        src, 
                        nodeCnt);
            }
        }
    }

    /*
     * if the peeked message is not found, then call the actual MPI_Testall
     * and update the status and record the information
     */
    MPI_Status localStats[count];
    memset(localStats, 0, sizeof(MPI_Status) * count);
    ret = PMPI_Testall(
            count, 
            array_of_requests, 
            flag, 
            localStats);
    MPI_ASSERT(ret == MPI_SUCCESS);

    if(array_of_statuses != MPI_STATUSES_IGNORE) {
        memcpy(
                array_of_statuses, 
                localStats, 
                sizeof(MPI_Status) * count);
    }
    if(*flag) {
        for(int i = 0; i < count; i++) {
            /* DEBUG("loadMessage at line: %d, rank: %d\n", __LINE__, rank); */
            string lastNodes = messagePool.loadMessage(
                    &array_of_requests[i], 
                    &array_of_statuses[i]);
            MPI_ASSERT(lastNodes.length() > 0 
                    || messagePool.isSend(&array_of_requests[i]));
        }
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Testall:%d:%d:SUCCESS", 
                    rank, 
                    count);
            for(int i = 0; i < count; i++) {
                fprintf(recordFile, ":%p:%d", 
                        &array_of_requests[i], 
                        localStats[i].MPI_SOURCE);
            }
            fprintf(recordFile, ":%lu\n", nodeCnt);
        }
    } else if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Testall:%d:%d:FAIL:%lu\n", 
                rank, 
                count,
                nodeCnt);
    }
    return ret;
}

int __MPI_Testsome(
        int incount, 
        MPI_Request array_of_requests[], 
        int *outcount, 
        int array_of_indices[], 
        MPI_Status array_of_statuses[],
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int ret;
    /*
     * first check if we have any matching peeked message
     */
    for(int i = 0; i < incount; i++) {
        MessageBuffer *msgBuf = messagePool.peekMessage(&array_of_requests[i]);
        int src;
        ret = messagePool.loadPeekedMessage(
                msgBuf->buf_, 
                msgBuf->dataType_,
                msgBuf->count_,
                msgBuf->tag_, 
                msgBuf->comm_, 
                msgBuf->src_,
                &src);
        /*
         * if loading of the peeked message is successful
         * record it, update the status, and return
         */
        if(ret != -1) {
            fprintf(stderr, "we currently do not support the case\
                    where there's a peeked message for MPI_Testsome.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);

            /* DEBUG("load message at line: %d, rank: %d\n", __LINE__, rank); */
            string lastNodes = messagePool.loadMessage(
                    &array_of_requests[i], 
                    &array_of_statuses[i]);
            /*
             * this must be a receive request because ret == -1
             * so we should assert that lastNodes should not be an
             * empty string
             */
            MPI_ASSERT(lastNodes.length() > 0);

            //fprintf(stderr, "received at %s\n", lastNodes.c_str());
            if(recordFile != nullptr) {
                fprintf(recordFile, "MPI_Testsome:%d:%p:SUCCESS:%d:%lu\n", 
                        rank, 
                        &array_of_requests[i], 
                        src, 
                        nodeCnt);
            }
        }
    }

    /*
     * if the peeked message is not found, then call the actual MPI_Testsome
     * and update the status and record the information
     */
    MPI_Status localStats[incount];
    memset(localStats, 0, sizeof(MPI_Status) * incount);
    ret = PMPI_Testsome(
            incount, 
            array_of_requests,
            outcount,
            array_of_indices,
            localStats);
    MPI_ASSERT(ret == MPI_SUCCESS);
    if(array_of_statuses != MPI_STATUSES_IGNORE) {
        memcpy(
                array_of_statuses, 
                localStats, 
                sizeof(MPI_Status) * incount);
    } 

    // below this part is not done yet, IMPLEMENT!!
    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Testsome:%d:%d", 
                rank, *outcount);
    }
    if(*outcount > 0) {
        string lastNodes;
        int ind;
        for(int i = 0; i < *outcount; i++) {
            ind = array_of_indices[i];
            /* DEBUG("loadMessage at line: %d, rank: %d, request: %p\n", */ 
            /*         __LINE__, rank, &array_of_requests[ind]); */
            lastNodes = messagePool.loadMessage(
                    &array_of_requests[ind], 
                    &array_of_statuses[ind]);
            if(recordFile != nullptr) {
                fprintf(recordFile, ":%p:%d", 
                        &array_of_requests[ind], 
                        array_of_statuses[ind].MPI_SOURCE);
            }
        }
    }
    if(recordFile != nullptr) {
        fprintf(recordFile, "%lu\n", 
                nodeCnt);
    }
    return ret;
}

int __MPI_Cancel(
        MPI_Request *request,
        MessagePool &messagePool,
        FILE *recordFile,
        unsigned long nodeCnt) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    messagePool.deleteMessage(request);
    int ret = PMPI_Cancel(request);
    if(recordFile != nullptr) {
        fprintf(recordFile, "MPI_Cancel:%d:%p:%lu\n", 
                rank, 
                request, 
                nodeCnt);
    }
    return ret;
}
