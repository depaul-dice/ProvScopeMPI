
#ifndef MESSAGETOOLS_H
#define MESSAGETOOLS_H

#include <mpi.h>

#include <cstdio>

#include <string>
#include <sstream>

#include <vector>
#include <unordered_map>

#include "utils.h"
#include "loops.h"
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
        int count,
        int lineNum = -1);

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
        int lineNum = -1,
        int rank = -1);

void unsupportedDatatype(
        int rank, 
        int lineNum, 
        MPI_Datatype datatype);

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

int __MPI_Send(
        const void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int dest, 
        int tag, 
        MPI_Comm comm,
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Irecv(
        void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int source, 
        int tag, 
        MPI_Comm comm, 
        MPI_Request *request,
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Isend(
        const void *buf, 
        int count, 
        MPI_Datatype datatype, 
        int dest, 
        int tag, 
        MPI_Comm comm, 
        MPI_Request *request,
        MessagePool &messagePool,
        std::string& lastNodes,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);
 
int __MPI_Wait(
        MPI_Request *request, 
        MPI_Status *status,
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Test(
        MPI_Request *request, 
        int *flag, 
        MPI_Status *status,
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Waitall(
        int count, 
        MPI_Request array_of_requests[], 
        MPI_Status array_of_statuses[],
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Testall(
        int count, 
        MPI_Request array_of_requests[], 
        int *flag, 
        MPI_Status array_of_statuses[],
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Testsome(
        int incount, 
        MPI_Request array_of_requests[], 
        int *outcount, 
        int array_of_indices[], 
        MPI_Status array_of_statuses[],
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

int __MPI_Cancel(
        MPI_Request *request,
        MessagePool &messagePool,
        FILE *recordFile = nullptr,
        unsigned long nodeCnt = 0);

#endif // MESSAGETOOLS_H
