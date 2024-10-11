
#include <mpi.h>
#include <gtest/gtest.h>

#include <iostream>

#include "../messagePool.h"

using namespace std;

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    MPI_Finalize();
    return ret;
}

TEST(MessagePoolTest, addAndLoadMessageTests) {
    MessagePool messagePool;

    /*
     * let's work with message of int first
     */
    int bufInt [5];
    MPI_Request req;
    MPI_Status stat;
    char *realBuf;
    realBuf = (char *)messagePool.addMessage(
            &req,
            (void *)bufInt, 
            MPI_INT, 
            5 /* count */,
            0 /* tag */,
            MPI_COMM_WORLD,
            -1 /* src */);

    /* let's peek the message */
    MessageBuffer *msgBuf = messagePool.peekMessage(&req);
    EXPECT_EQ(msgBuf->request_, &req);
    EXPECT_EQ(msgBuf->buf_, (void *)bufInt);
    EXPECT_EQ(msgBuf->realBuf_, realBuf);
    EXPECT_EQ(msgBuf->dataType_, MPI_INT);
    EXPECT_EQ(msgBuf->count_, 5);
    EXPECT_EQ(msgBuf->tag_, 0);
    EXPECT_EQ(msgBuf->comm_, MPI_COMM_WORLD);
    EXPECT_EQ(msgBuf->src_, -1);
    EXPECT_EQ(msgBuf->isSend_, false);
    EXPECT_EQ(msgBuf->timestamp_, 0);

    const char *tmp = "1|2|3|4|5|location1|4";
    strncpy(realBuf, tmp, strlen(tmp) + 1);
    string location = messagePool.loadMessage(
            &req, &stat);
    int bufIntExpected [5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(bufInt[i], bufIntExpected[i]);
    }
    EXPECT_EQ(location, "location1");
    EXPECT_EQ(stat.MPI_SOURCE, -1);
    EXPECT_EQ(stat.MPI_TAG, 0);
    int count;
    MPI_Get_count(&stat, MPI_INT, &count);
    EXPECT_EQ(count, 5);

    /*
     * now let's try long long int message
     */
    long long int bufLongLong [4];
    realBuf = (char *)messagePool.addMessage(
            &req,
            (void *)bufLongLong, 
            MPI_LONG_LONG_INT, 
            4 /* count */,
            1 /* tag */,
            MPI_COMM_WORLD,
            3 /* src */);
    msgBuf = messagePool.peekMessage(&req);
    EXPECT_EQ(msgBuf->request_, &req);
    EXPECT_EQ(msgBuf->realBuf_, realBuf);
    EXPECT_EQ(msgBuf->dataType_, MPI_LONG_LONG_INT);
    EXPECT_EQ(msgBuf->count_, 4);
    EXPECT_EQ(msgBuf->tag_, 1);
    EXPECT_EQ(msgBuf->comm_, MPI_COMM_WORLD);
    EXPECT_EQ(msgBuf->src_, 3);
    EXPECT_EQ(msgBuf->isSend_, false);
    EXPECT_EQ(msgBuf->timestamp_, 1);

    const char *tmp2 = "6|7|8|9|location2|8";
    strncpy(realBuf, tmp2, strlen(tmp2) + 1);
    location = messagePool.loadMessage(
            &req, &stat);
    long long int bufLongLongExpected [4] = {6, 7, 8, 9};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(bufLongLong[i], bufLongLongExpected[i]);
    }
    EXPECT_EQ(location, "location2");
    EXPECT_EQ(stat.MPI_SOURCE, 3);
    EXPECT_EQ(stat.MPI_TAG, 1);
    MPI_Get_count(&stat, MPI_LONG_LONG_INT, &count);
    EXPECT_EQ(count, 4);

    /*
     * finally let's try double message
     */
    double bufDouble [6];
    realBuf = (char *)messagePool.addMessage(
            &req,
            (void *)bufDouble, 
            MPI_DOUBLE, 
            6 /* count */,
            2 /* tag */,
            MPI_COMM_WORLD,
            2 /* src */);
    msgBuf = messagePool.peekMessage(&req);
    EXPECT_EQ(msgBuf->request_, &req);
    EXPECT_EQ(msgBuf->realBuf_, realBuf);
    EXPECT_EQ(msgBuf->dataType_, MPI_DOUBLE);
    EXPECT_EQ(msgBuf->count_, 6);
    EXPECT_EQ(msgBuf->tag_, 2);
    EXPECT_EQ(msgBuf->comm_, MPI_COMM_WORLD);
    EXPECT_EQ(msgBuf->src_, 2);
    EXPECT_EQ(msgBuf->isSend_, false);
    EXPECT_EQ(msgBuf->timestamp_, 2);

    const char *tmp3 = "1.1|2.98|35|0.05|7.89|5|location3|8";
    strncpy(realBuf, tmp3, strlen(tmp3) + 1);
    location = messagePool.loadMessage(
            &req, &stat);
    double bufDoubleExpected [6] = {1.1, 2.98, 35, 0.05, 7.89, 5};
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(bufDouble[i], bufDoubleExpected[i]);
    }
    EXPECT_EQ(location, "location3");
    EXPECT_EQ(stat.MPI_SOURCE, 2);
    EXPECT_EQ(stat.MPI_TAG, 2);
    MPI_Get_count(&stat, MPI_DOUBLE, &count);
    EXPECT_EQ(count, 6);
}

TEST(MessagePoolTest, peekedMessageTest) {
    MessagePool messagePool;
    /* MPI_Request req; */
    int intBuf [5] = {1, 2, 3, 4, 5};
    stringstream ss;
    for (int i = 0; i < 5; i++) {
        ss << intBuf[i] << "|";
    }
    ss << "location1|4";
    string tmp = ss.str();
    EXPECT_EQ(tmp, "1|2|3|4|5|location1|4");
    EXPECT_THROW(
            messagePool.addPeekedMessage(
                (void *)(tmp.c_str()), 
                5 /* count */,
                0 /* tag */,
                MPI_COMM_WORLD,
                -1 /* src */),
            runtime_error);
    EXPECT_THROW(
            messagePool.addPeekedMessage(
                nullptr,
                5 /* count */,
                0 /* tag */,
                MPI_COMM_WORLD,
                0 /* src */),
            runtime_error);
    // put the count of the string length
    EXPECT_NO_THROW(
            messagePool.addPeekedMessage(
                (void *)(tmp.c_str()),
                tmp.length() + 1 /* count */,
                0 /* tag */,
                MPI_COMM_WORLD,
                0 /* src */));

    MPI_Status stat;
    int rv = messagePool.peekPeekedMessage(
            0,
            0,
            MPI_COMM_WORLD,
            &stat);
    EXPECT_EQ(rv, 0);
    rv = messagePool.peekPeekedMessage(
            0,
            1,
            MPI_COMM_WORLD,
            &stat);
    EXPECT_EQ(rv, -1);
    int recvBuf [5];
    int retSrc;
    rv = messagePool.loadPeekedMessage(
            (void *)recvBuf,
            MPI_INT,
            5 /* count */,
            0 /* tag */,
            MPI_COMM_WORLD,
            0 /* src */,
            &retSrc);
    EXPECT_EQ(rv, 0);
    EXPECT_EQ(retSrc, 0);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(recvBuf[i], intBuf[i]);
    }
    rv = messagePool.loadPeekedMessage(
            (void *)recvBuf,
            MPI_INT,
            5 /* count */,
            0 /* tag */,
            MPI_COMM_WORLD,
            0 /* src */,
            &retSrc);
    EXPECT_EQ(rv, -1);
}
