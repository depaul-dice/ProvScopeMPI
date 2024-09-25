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
        int count, 
        MPI_Datatype datatype) {
    stringstream ss;
    if(datatype == MPI_INT) {
        int *data = (int *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << "|";
        }
    } else if(datatype == MPI_LONG_LONG_INT) {
        long long int *data = (long long int *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << "|";
        }
    } else if(datatype == MPI_DOUBLE) {
        double *data = (double *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << "|";
        }
    } else if(datatype == MPI_CHAR
            || datatype == MPI_BYTE) {
        char *data = (char *)buf;
        for(int i = 0; i < count; i++) {
            ss << data[i] << "|";
        }
    } else {
        unsupportedDatatype(-1, __LINE__, datatype);
    }
    return ss;
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


