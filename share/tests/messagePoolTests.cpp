
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
    MessagePool mp;
    int bufInt [5];
    MPI_Request req;
    MPI_Status stat;
    char *realBuf;
    realBuf = (char *)mp.addMessage(
            &req,
            (void *)bufInt, 
            MPI_INT, 
            5 /* count */,
            0 /* tag */,
            MPI_COMM_WORLD,
            -1);
    const char *tmp = "1|2|3|4|5|location1|4";
    strncpy(realBuf, tmp, strlen(tmp) + 1);
    mp.loadMessage(
            &req, &stat);
    int bufIntExpected [5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(bufInt[i], bufIntExpected[i]);
    }

    /*
    long long int bufLongLong1 [5] = {6, 7, 8, 9, 10};
    mp.addMessage(
            (void *)bufLongLong1, 
            MPI_LONG_LONG_INT, 
            5);
    EXPECT_EQ(mp.size(), 2);
    long long int bufLongLong2 [4];
    mp.getMessage(
            (void *)bufLongLong2, 
            MPI_LONG_LONG_INT, 
            5);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(bufLongLong1[i], bufLongLong2[i]);
    }
    */
}
