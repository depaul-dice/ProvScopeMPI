
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


TEST(AMGImitateTest, IsendIrecvWaitallTests) {
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
        //cerr << "MPI_Irecv from " << src << " to " << rank << ", " << recvCounts[src] << " elements" << endl;
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
        //cerr << "MPI_Isend from " << rank << " to " << dest << ", " << sendCounts[dest] << " elements" << endl;
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
        EXPECT_EQ(sendStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(sendStatuses[i].MPI_ERROR, MPI_SUCCESS);
        EXPECT_EQ(recvStatuses[i].MPI_SOURCE, i < rank ? i : i + 1);
        EXPECT_EQ(recvStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(recvStatuses[i].MPI_ERROR, MPI_SUCCESS);

        int count;
        MPI_Get_count(&sendStatuses[i], MPI_DOUBLE, &count);
        EXPECT_EQ(count, sendCounts[i < rank ? i : i + 1]);
        MPI_Get_count(&recvStatuses[i], MPI_DOUBLE, &count);
        EXPECT_EQ(count, recvCounts[i < rank ? i : i + 1]);
    }

}

/*
 * I think by sending a message 2 -> 0 would do it, 
 * let's check if it works on MPI_Send instead
 */
TEST(AMGImitateTest, minimalErrorReproduceTest) {
    int rank, numProcs;
    MessagePool messagePool;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
    EXPECT_GE(numProcs, 2);

    // read the files to send from 2 -> 0
    ifstream infile("data/data_2_0.txt");
    EXPECT_FALSE(!infile);
    vector<double> sendVec;
    double val;
    while (infile >> val) {
        sendVec.push_back(val);
    }
    double sendData [sendVec.size()];
    for (int i = 0; i < sendVec.size(); i++) {
        sendData[i] = sendVec[i];
    }
    //cerr << sendData << endl; 
    MPI_Request req;
    if(rank == 0) {
        __MPI_Isend(
                sendData,
                sendVec.size(),
                MPI_DOUBLE,
                1 /* dest */,
                0 /* tag */,
                MPI_COMM_WORLD,
                &req,
                messagePool);
        MPI_Status status;
        __MPI_Wait(&req, &status, messagePool);
    } else if(rank == 1) {
        double recvData [sendVec.size()];
        memset(recvData, 0, sendVec.size() * sizeof(double));
        __MPI_Irecv(
                recvData,
                sendVec.size(),
                MPI_DOUBLE,
                0 /* src */,
                0 /* tag */,
                MPI_COMM_WORLD,
                &req,
                messagePool);
        MPI_Status status;
        __MPI_Waitall(1, &req, &status, messagePool);
        for(int i = 0; i < sendVec.size(); i++) {
            EXPECT_DOUBLE_EQ(recvData[i], sendVec[i]);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 0);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
    }
}

/*
 * instead of MPI_Waitall, we use testall to see if it works here
 */
TEST(AMGImitateTest, IsendIrecvTestallTests) {
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
        //cerr << "MPI_Irecv from " << src << " to " << rank << ", " << recvCounts[src] << " elements" << endl;
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
        //cerr << "MPI_Isend from " << rank << " to " << dest << ", " << sendCounts[dest] << " elements" << endl;
    }
    //cerr << "Isend done with rank " << rank << endl;
    MPI_Status recvStatuses [numProcs - 1];
    MPI_Status sendStatuses [numProcs - 1];
    int testallIerr, testallFlag = 0;
    while (!testallFlag) {
        testallIerr = __MPI_Testall(
                numProcs - 1, 
                sendReqs, 
                &testallFlag,
                sendStatuses,
                messagePool);
        EXPECT_EQ(testallIerr, MPI_SUCCESS);
    }
    //cerr << "Waitall send done with rank " << rank << endl;
    EXPECT_EQ(testallFlag, 1);
    testallFlag = 0;
    while (!testallFlag) {
        testallIerr = __MPI_Testall(
                numProcs - 1, 
                recvReqs, 
                &testallFlag,
                recvStatuses,
                messagePool);
        EXPECT_EQ(testallIerr, MPI_SUCCESS);
    }
    EXPECT_EQ(testallFlag, 1);
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
        EXPECT_EQ(sendStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(sendStatuses[i].MPI_ERROR, MPI_SUCCESS);
        EXPECT_EQ(recvStatuses[i].MPI_SOURCE, i < rank ? i : i + 1);
        EXPECT_EQ(recvStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(recvStatuses[i].MPI_ERROR, MPI_SUCCESS);

        int count;
        MPI_Get_count(&sendStatuses[i], MPI_DOUBLE, &count);
        EXPECT_EQ(count, sendCounts[i < rank ? i : i + 1]);
        MPI_Get_count(&recvStatuses[i], MPI_DOUBLE, &count);
        EXPECT_EQ(count, recvCounts[i < rank ? i : i + 1]);
    }

}

/*
 * instead of MPI_Irecv, let's try using MPI_Recv and see if 
 * we can still induce the error
 */
TEST(AMGImitateTest, IsendRecvWaitallTests) {
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
    int recvIerr, sendIerr;
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
        sendIerr = __MPI_Isend(
                sendBufs[dest], 
                sendCounts[dest], 
                MPI_DOUBLE, 
                dest, 
                0 /* tag */, 
                MPI_COMM_WORLD, 
                &sendReqs[reqIndex],
                messagePool);
        EXPECT_EQ(sendIerr, MPI_SUCCESS);
        //cerr << "MPI_Isend from " << rank << " to " << dest << ", " << sendCounts[dest] << " elements" << endl;
    }

    MPI_Status recvStatuses [numProcs - 1];
    for(int src = 0; src < numProcs; src++) {
        if(rank == src) {
            continue;
        }
        recvIerr = __MPI_Recv(
                recvBufs[src], 
                recvCounts[src],
                MPI_DOUBLE, 
                src, 
                0, 
                MPI_COMM_WORLD,
                &recvStatuses[src < rank ? src : src - 1],
                messagePool); 
    }
    //cerr << "Isend done with rank " << rank << endl;
    MPI_Status sendStatuses [numProcs - 1];
    int waitallIerr;
    waitallIerr = __MPI_Waitall(
            numProcs - 1, 
            sendReqs, 
            sendStatuses,
            messagePool);
    EXPECT_EQ(waitallIerr, MPI_SUCCESS);
    //cerr << "Waitall send done with rank " << rank << endl;
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
        EXPECT_EQ(sendStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(sendStatuses[i].MPI_ERROR, MPI_SUCCESS);
        EXPECT_EQ(recvStatuses[i].MPI_SOURCE, i < rank ? i : i + 1);
        EXPECT_EQ(recvStatuses[i].MPI_TAG, 0);
        EXPECT_EQ(recvStatuses[i].MPI_ERROR, MPI_SUCCESS);

        int count;
        MPI_Get_count(&sendStatuses[i], MPI_DOUBLE, &count);
        EXPECT_EQ(count, sendCounts[i < rank ? i : i + 1]);
        MPI_Get_count(&recvStatuses[i], MPI_DOUBLE, &count);
        EXPECT_EQ(count, recvCounts[i < rank ? i : i + 1]);
    }
}


