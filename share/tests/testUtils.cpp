
#include "testUtils.h"

using namespace std;

int __MPI_Send(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm,
    string lastNodes) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG("MPI_Send:to %d, at %d\n", dest, rank);
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
    ret = PMPI_Send(
            str.c_str(), 
            str.size() + 1, 
            MPI_CHAR, 
            dest, 
            tag, 
            comm);
    //DEBUG("MPI_send return rank: %d\n", rank);
    return ret;
}

int MPI_Isend(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request,
    MessagePool &messagePool,
    string lastNodes) {

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
    ret = PMPI_Isend(
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

    return ret;
}


