#ifndef MPIRECORDREPLAY_H
#define MPIRECORDREPLAY_H

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>
#include <string>
#include <vector>
#include <queue>
#include <dlfcn.h>
#include <mpi.h>
#include <algorithm>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "tools.h"
#include "loops.h"
#include "alignment.h"

using namespace std;

static int (*original_MPI_Init)(
    int *argc, 
    char ***argv
) = nullptr;

static int (*original_MPI_Finalize)(
    void
) = nullptr;

static int (*original_MPI_Recv)(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Send)(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm
) = nullptr;

static int (*original_MPI_Irecv)(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Isend)(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Irsend)(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Test)(
    MPI_Request *request, 
    int *flag, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Testany)(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    int *flag, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Testall)(
    int count, 
    MPI_Request array_of_requests[], 
    int *flag, 
    MPI_Status array_of_statuses[]
) = nullptr;

static int (*original_MPI_Testsome)(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) = nullptr;

static int (*original_MPI_Test_cancelled)(
    const MPI_Status *status, 
    int *flag
) = nullptr;

static int (*original_MPI_Wait)(
    MPI_Request *request, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Waitany)(
    int count, 
    MPI_Request array_of_requests[], 
    int *index, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Waitall)(
    int count, 
    MPI_Request array_of_requests[], 
    MPI_Status array_of_statuses[]
) = nullptr;

static int (*original_MPI_Waitsome)(
    int incount, 
    MPI_Request array_of_requests[], 
    int *outcount, 
    int array_of_indices[], 
    MPI_Status array_of_statuses[]
) = nullptr;

static int (*original_MPI_Probe)(
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Iprobe)(
    int source, 
    int tag, 
    MPI_Comm comm, 
    int *flag, 
    MPI_Status *status
) = nullptr;

static int (*original_MPI_Send_init)(
    const void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int dest, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Recv_init)(
    void *buf, 
    int count, 
    MPI_Datatype datatype, 
    int source, 
    int tag, 
    MPI_Comm comm, 
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Start)(
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Startall)(
    int count, 
    MPI_Request array_of_requests[]
) = nullptr;

static int (*original_MPI_Cancel)(
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Request_free)(
    MPI_Request *request
) = nullptr;

static int (*original_MPI_Barrier)(
    MPI_Comm comm
) = nullptr;

static void (*original_printBBname) (
    const char *name
) = nullptr;

#endif // MPIRECORDREPLAY_H
