#pragma once

#include <mpi.h>
#include <string>
#include <sstream>
#include <iostream>

#include "../messagePool.h"
#include "../messageTools.h"

int __MPI_Send(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm,
    std::string lastNodes = "randomLocation");

int __MPI_Isend(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request,
    MessagePool &messagePool,
    std::string lastNodes = "randomLocation");
