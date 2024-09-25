
#ifndef MESSAGETOOLS_H
#define MESSAGETOOLS_H

#include <mpi.h>

#include <cstdio>

#include <string>
#include <sstream>

#include "tools.h"
#include "messagePool.h"

class MessagePool;

/* 
 * below is a recording functions for each mpi functions 
 */
void recordMPIIprobeSuccess(
        FILE *recordFile,
        int rank,
        int source,
        int tag,
        MPI_Status *stat,
        unsigned long nodeCount);

/* 
 * helper function to record mpi probe
 */
void recordMPIProbe (
        FILE *recordFile,
        int rank,
        int source,
        int tag,
        MPI_Status *status,
        unsigned long nodeCount);

std::string convertDatatype(MPI_Datatype datatype);

std::stringstream convertData2StringStream(
        const void *data, 
        MPI_Datatype datatype, 
        int count);

/*
 * lineNum and rank is for unsupportedDatatype function
 * this function is to abstract the message receipt when 
 * the datatype varies
 */
void convertMsgs2Buf(
        void *buf, 
        MPI_Datatype datatype, 
        int count,
        std::vector<std::string>& msgs, 
        int lineNum,
        int rank);

void unsupportedDatatype(
        int rank, 
        int lineNum, 
        MPI_Datatype datatype);

/*
 * this should be defined at mpirecordreplay.h
 */
extern int (*original_MPI_Recv)(
        void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int source, 
        int tag, 
        MPI_Comm comm, 
        MPI_Status *status);
/*
 * the function below is to abstract the message conversion
 */
int __MPI_Recv(
        void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int source, 
        int tag, 
        MPI_Comm comm, 
        MPI_Status *status,
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);
 
#endif // MESSAGETOOLS_H
