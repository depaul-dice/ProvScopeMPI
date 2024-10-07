
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

    double **sendBufs = new double*[numProcs];
    double **recvBufs = new double*[numProcs];
    int sendCounts [numProcs];
    int recvCounts [numProcs];
    vector<vector<double>> recvExpected(numProcs);

    /*
     * read the files from data directory to send
     */
    vector<double> sendData;
    for (int i = 0; i < numProcs; i++) {
        if(rank == i) {
            continue;
        }
        sendData.clear();
        string fileName = "data/data_" + to_string(rank) + "_" + to_string(i) + ".txt";
        ifstream infile(fileName);
        EXPECT_FALSE(!infile);
        double value;
        while (infile >> value) {
            sendData.push_back(value);
        }
        infile.close();
        sendBufs[i] = new double[sendData.size()];

        for (int j = 0; j < sendData.size(); j++) {
            sendBufs[i][j] = sendData[j];
        }
        sendCounts[i] = sendData.size();
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

    /*
     * read the files from data directory to check the correctness
     */
    vector<double> recvData;
    for(int i = 0; i < numProcs; i++) {
        if(rank == i) {
            recvCounts[i] = -1;
            recvBufs[i] = nullptr;
            continue;
        }
        recvData.clear();
        string fileName = "data/data_" + to_string(i) + "_" + to_string(rank) + ".txt";
        ifstream infile(fileName);
        EXPECT_FALSE(!infile);
        double value;
        while (infile >> value) {
            recvData.push_back(value);
        }
        infile.close();
        recvExpected[i] = recvData;
        recvCounts[i] = recvData.size();
        recvBufs[i] = new double[recvData.size()];
        memset(recvBufs[i], 0, recvData.size() * sizeof(double));
    } 

    MessagePool messagePool;
    MPI_Request sendReqs[numProcs - 1];
    MPI_Request recvReqs[numProcs - 1];
    int recvIerr, sendIerr;
    for (int src = 0; src < numProcs; src++) {
        int reqIndex;
        if(rank == src) {
            continue;
        }
        if(src < rank) {
           reqIndex = src; 
        } else {
            reqIndex = src - 1;
        }
        //cerr << "at rank " << rank << " reqIndex:" << reqIndex << " for recv " << endl;
        recvIerr = __MPI_Irecv(
                recvBufs[src], 
                recvCounts[src],
                MPI_DOUBLE, 
                src, 
                0, 
                MPI_COMM_WORLD, 
                &recvReqs[reqIndex],
                messagePool);
        EXPECT_EQ(recvIerr, MPI_SUCCESS);
    }
    //cerr << "Irecv done with rank " << rank << endl;
    for(int dest = 0; dest < numProcs; dest++) {
        int reqIndex;
        if(rank == dest) {
            continue;
        }
        if(dest < rank) {
            reqIndex = dest;
        } else {
            reqIndex = dest - 1;
        }
        //cerr << "at rank " << rank << " reqIndex:" << reqIndex << " for send " << endl;
        sendIerr = __MPI_Isend(
                sendBufs[dest], 
                sendCounts[dest], 
                MPI_DOUBLE, 
                dest, 
                0, 
                MPI_COMM_WORLD, 
                &sendReqs[reqIndex],
                messagePool);
        EXPECT_EQ(sendIerr, MPI_SUCCESS);
    }
    //cerr << "Isend done with rank " << rank << endl;
    MPI_Status recvStatuses [numProcs - 1];
    MPI_Status sendStatuses [numProcs - 1];
    int waitallIerr;
    waitallIerr = __MPI_Waitall(
            numProcs - 1, 
            sendReqs, 
            sendStatuses,
            messagePool);
    EXPECT_EQ(waitallIerr, MPI_SUCCESS);
    //cerr << "Waitall send done with rank " << rank << endl;
    waitallIerr = __MPI_Waitall(
            numProcs - 1, 
            recvReqs, 
            recvStatuses,
            messagePool);
    EXPECT_EQ(waitallIerr, MPI_SUCCESS);
    //cerr << "Waitall recv done with rank " << rank << endl;
    for(int i = 0; i < numProcs; i++) {
        if(rank == i) {
            continue;
        }
        for(int j = 0; j < recvCounts[i]; j++) {
            EXPECT_DOUBLE_EQ(recvExpected[i][j], recvBufs[i][j]);
        }
    }

    /*
     * let's check the status
     */
    for(int i = 0; i < numProcs - 1; i++) {
        EXPECT_EQ(sendStatuses[i].MPI_SOURCE, i < rank ? i : i + 1);
        EXPECT_EQ(sendStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(sendStatuses[i].MPI_ERROR, MPI_SUCCESS);
        EXPECT_EQ(recvStatuses[i].MPI_SOURCE, i < rank ? i : i + 1);
        EXPECT_EQ(recvStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(recvStatuses[i].MPI_ERROR, MPI_SUCCESS);

        int count;
        MPI_Get_count(&sendStatuses[i < rank ? i : i + 1], MPI_DOUBLE, &count);
        EXPECT_EQ(count, sendCounts[i < rank ? i : i + 1]);
        MPI_Get_count(&recvStatuses[i < rank ? i : i + 1], MPI_DOUBLE, &count);
        EXPECT_EQ(count, recvCounts[i < rank ? i : i + 1]);
    }

}
