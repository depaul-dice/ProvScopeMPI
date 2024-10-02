
#include <mpi.h>
#include <gtest/gtest.h>

#include "../messagePool.h"

using namespace std;

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    MPI_Finalize();
    return ret;
}

TEST(MessagePoolTests, addMessageTests) {
    MessagePool messagePool;
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
            -1);
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

    long long int bufLongLong [4];
    realBuf = (char *)messagePool.addMessage(
            &req,
            (void *)bufLongLong, 
            MPI_LONG_LONG_INT, 
            4,
            1,
            MPI_COMM_WORLD,
            3);
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

    double bufDouble [6];
    realBuf = (char *)messagePool.addMessage(
            &req,
            (void *)bufDouble, 
            MPI_DOUBLE, 
            6,
            2,
            MPI_COMM_WORLD,
            2);
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
