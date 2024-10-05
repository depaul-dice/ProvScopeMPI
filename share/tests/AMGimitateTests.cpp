
#include <gtest/gtest.h>
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <string>

#include "testUtils.h"
#include "../tools.h"
#include "../messagePool.h"
#include "../messageTools.h"

using namespace std;

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    MPI_Finalize();
    return ret;
}

TEST(AMGimitateTest, IsendIrecvWaitallTests) {
    int rank, numProcs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
    EXPECT_GE(numProcs, 2);

    double **sendbufs = new double*[numProcs];
    double **recvbufs = new double*[numProcs];
    /*
     * read the files from data directory
     */
    for (int i = 0; i < numProcs; i++) {
        if(rank == i) {
            continue;
        }
        vector<double> data;
        string filename = "data/data_" + to_string(rank) + "_" + to_string(i) + ".txt";
        ifstream infile(filename);
        EXPECT_FALSE(!infile);
        double value;
        while (infile >> value) {
            data.push_back(value);
        }
        infile.close();
        sendbufs[i] = new double[data.size() + 1];
        recvbufs[i] = new double[data.size()];
        sendbufs[i][0] = data.size();
        for (int j = 1; j < data.size() + 1; j++) {
            sendbufs[i][j] = data[j];
        }
    }
    /*
    cerr << "rank " << rank << " read data done" << endl;
    for (int i = 0; i < size; i++) {
        if(rank == i) {
            continue;
        }
        cerr << "rank " << rank << " send to " << i << endl;
        for(int j = 1; j < sendbufs[i][0]; j++) {
            cerr << sendbufs[i][j] << " ";
        }
        cerr << endl;
    }
    */
    MessagePool messagePool;
    MPI_Request sendReqs[numProcs - 1];
    MPI_Request recvReqs[numProcs - 1];
    for (int i = 0; i < numProcs; i++) {
        int reqIndex;
        if(rank == i) {
            continue;
        }
        if(i < rank) {
           reqIndex = i; 
        } else {
            reqIndex = i - 1;
        }
        __MPI_Irecv(
                recvbufs[i], 
                sendbufs[i][0], 
                MPI_DOUBLE, 
                i, 
                0, 
                MPI_COMM_WORLD, 
                &recvReqs[reqIndex],
                messagePool);
    }
    for(int i = 0; i < numProcs; i++) {
        int reqIndex;
        if(rank == i) {
            continue;
        }
        if(i < rank) {
            reqIndex = i;
        } else {
            reqIndex = i - 1;
        }
        __MPI_Isend(
                &sendbufs[i][1], 
                sendbufs[i][0], 
                MPI_DOUBLE, 
                i, 
                0, 
                MPI_COMM_WORLD, 
                &sendReqs[reqIndex],
                messagePool);
    }
    MPI_Status recvStatuses [numProcs - 1];
    MPI_Status sendStatuses [numProcs - 1];
    __MPI_Waitall(
            numProcs - 1, 
            sendReqs, 
            sendStatuses,
            messagePool);
    __MPI_Waitall(
            numProcs - 1, 
            recvReqs, 
            recvStatuses,
            messagePool);
       
}
