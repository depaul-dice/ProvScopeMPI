
#ifndef MESSAGETOOLS_H
#define MESSAGETOOLS_H

#include <mpi.h>

#include <cstdio>

#include <string>
#include <sstream>

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

void unsupportedDatatype(
        int rank, 
        int lineNum, 
        MPI_Datatype datatype);

#endif // MESSAGETOOLS_H
