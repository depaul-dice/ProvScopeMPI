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
        char *data = (char *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << '|';
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
    MPI_ASSERT(original_MPI_Recv != nullptr);
    MPI_Status localStatus;
    int ind = messagePool.peekPeekedMessage(
            source,
            tag,
            comm,
            &localStatus);
    int ret;
    if(ind != -1) {
        ret = messagePool.loadPeekedMessage(
                buf,
                datatype,
                count,
                tag,
                comm,
                source);
        MPI_ASSERT(ret == -1);
        if(status != MPI_STATUS_IGNORE) {
            memcpy(
                    status, 
                    &localStatus, 
                    sizeof(MPI_Status));
        }
        if(recordFile != nullptr) {
            fprintf(recordFile, "MPI_Recv:%d:%d:%lu\n",
                    rank,
                    status->MPI_SOURCE,
                    nodeCnt);
        }
        return ret;
    }
    char tmpBuffer[msgSize];
    ret = original_MPI_Recv(
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
    fprintf(stderr, "received message from send at %s\n", msgs[msgs.size() - 2].c_str());
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
    // checking if the counts are the same
    MPI_ASSERT(size == sizeFromMsgs);
    // manipulating count manually

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

