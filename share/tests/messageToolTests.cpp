
#include "../messageTools.h"
#include "testUtils.h"

#include <mpi.h>
#include <gtest/gtest.h>

using namespace std;

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    MPI_Finalize();
    return ret;
}

TEST(MessageToolTests, convertDatatypeTest) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank != 0) {
        return;
    }

    EXPECT_EQ(convertDatatype(MPI_INT), "MPI_INT");
    EXPECT_EQ(convertDatatype(MPI_CHAR), "MPI_CHAR");
    EXPECT_EQ(convertDatatype(MPI_DOUBLE), "MPI_DOUBLE");
    EXPECT_EQ(convertDatatype(MPI_FLOAT), "MPI_FLOAT");
    EXPECT_EQ(convertDatatype(MPI_LONG), "MPI_LONG");
    EXPECT_EQ(convertDatatype(MPI_UNSIGNED_LONG), "MPI_UNSIGNED_LONG");
    EXPECT_EQ(convertDatatype(MPI_UNSIGNED_LONG_LONG), "MPI_UNSIGNED_LONG_LONG");
    EXPECT_EQ(convertDatatype(MPI_UNSIGNED), "MPI_UNSIGNED");
    EXPECT_EQ(convertDatatype(MPI_UNSIGNED_CHAR), "MPI_UNSIGNED_CHAR");
    EXPECT_EQ(convertDatatype(MPI_UNSIGNED_SHORT), "MPI_UNSIGNED_SHORT");
    EXPECT_EQ(convertDatatype(MPI_LONG_DOUBLE), "MPI_LONG_DOUBLE");
    EXPECT_EQ(convertDatatype(MPI_LONG_LONG_INT), "MPI_LONG_LONG_INT");
}

TEST(MessageToolTests, convertData2StringStreamTest) {
    int rank; 
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank != 0) {
        return;
    }

    int bufInt1 [5] = {1, 2, 3, 4, 5};
    stringstream ss = convertData2StringStream(
            (void *)bufInt1, 
            MPI_INT, 
            5);
    EXPECT_EQ(ss.str(), "1|2|3|4|5|");
    auto msgs = parse(ss.str(), '|');
    EXPECT_EQ(msgs.size(), 6);
    int bufInt2 [4];
    convertMsgs2Buf(
            bufInt2, 
            MPI_INT, 
            5, 
            msgs);
    /*
     * this is only till 4 because it only converts till msgs.size() - 1
     */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(bufInt1[i], bufInt2[i]);
    }

    long long int bufLongLong1 [5] = {6, 7, 8, 9, 10};
    ss = convertData2StringStream(
            (void *)bufLongLong1, 
            MPI_LONG_LONG_INT, 
            5);
    EXPECT_EQ(ss.str(), "6|7|8|9|10|");
    msgs = parse(ss.str(), '|');
    EXPECT_EQ(msgs.size(), 6);
    long long int bufLongLong2 [5];
    convertMsgs2Buf(
            bufLongLong2, 
            MPI_LONG_LONG_INT, 
            5, 
            msgs);
    /*
     * this is only till 4 because it only converts till msgs.size() - 1
     */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(bufLongLong1[i], bufLongLong2[i]);
    }

    char bufChar1 [5] = {0, 1, 2, 3, 4};
    ss = convertData2StringStream(
            (void *)bufChar1, 
            MPI_CHAR, 
            5);
    EXPECT_EQ(ss.str(), "0|1|2|3|4|");
    msgs = parse(ss.str(), '|');
    char bufChar2 [5];
    convertMsgs2Buf(
            bufChar2, 
            MPI_CHAR, 
            5, 
            msgs);
    /*
     * this is only till 4 because it only converts till msgs.size() - 1
     */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(bufChar1[i], bufChar2[i]);
    }

    char bufByte1 [5] = {'a', 'b', 'c', 'd', 'e'};
    ss = convertData2StringStream(
            (void *)bufByte1, 
            MPI_BYTE, 
            5);
    EXPECT_EQ(ss.str(), "97|98|99|100|101|");
    msgs = parse(ss.str(), '|');
    char bufByte2 [5];
    convertMsgs2Buf(
            bufByte2, 
            MPI_BYTE, 
            5, 
            msgs);
    /*
     * this is only till 4 because it only converts till msgs.size() - 1
     */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(bufByte1[i], bufByte2[i]);
    }

    double buf5 [5] = {1.1, 2.2, 3.3, 4.4, 5.5};
    ss = convertData2StringStream(
            (void *)buf5, 
            MPI_DOUBLE, 
            5);
    EXPECT_EQ(ss.str(), "1.1|2.2|3.3|4.4|5.5|");
    msgs = parse(ss.str(), '|');
    EXPECT_EQ(msgs.size(), 6);
    double buf6 [5];
    convertMsgs2Buf(
            buf6, 
            MPI_DOUBLE, 
            5, 
            msgs);
    /*
     * this is only till 4 because it only converts till msgs.size() - 1
     */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(buf5[i], buf6[i]);
    }
}

TEST(MessageToolTests, MPI_RecvTest) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MessagePool messagePool;
    MPI_Status status;
    if (rank == 0) {
        int bufInt [5] = {1, 2, 3, 4, 5};
        __MPI_Send(
                bufInt, 
                5,
                MPI_INT, 
                1, 
                0, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        int bufInt [5];
        __MPI_Recv(
                bufInt, 
                5, 
                MPI_INT, 
                0, 
                0, 
                MPI_COMM_WORLD,
                &status,
                messagePool);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(bufInt[i], i + 1);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 0);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        int count;
        MPI_Get_count(&status, MPI_INT, &count);
        EXPECT_EQ(count, 5);
    }

    if(rank == 0) {
        char bufChar [5] = {0, 1, 2, 3, 4};
        __MPI_Send(
                bufChar, 
                5,
                MPI_CHAR, 
                1, 
                0, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        char bufChar [5];
        __MPI_Recv(
                bufChar, 
                5, 
                MPI_CHAR, 
                0, 
                0, 
                MPI_COMM_WORLD,
                &status,
                messagePool);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(bufChar[i], i);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 0);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        int count;
        MPI_Get_count(&status, MPI_CHAR, &count);
        EXPECT_EQ(count, 5);
    }

    if(rank == 0) {
        double bufDouble [6] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
        __MPI_Send(
                bufDouble, 
                6,
                MPI_DOUBLE, 
                1, 
                0, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        double bufDouble [6];
        __MPI_Recv(
                bufDouble, 
                6, 
                MPI_DOUBLE, 
                0, 
                0, 
                MPI_COMM_WORLD,
                &status,
                messagePool);
        const double epsilon = 1e-9;
        for (int i = 0; i < 6; i++) {
            EXPECT_NEAR(bufDouble[i], (i + 1) * 1.1, epsilon);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 0);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        int count;
        MPI_Get_count(&status, MPI_DOUBLE, &count);
        EXPECT_EQ(count, 6);
    }
}

TEST(MessageToolTests, MPI_IrecvTest) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MessagePool messagePool;
    MPI_Status status;
    MPI_Request request;
    if (rank == 0) {
        int bufInt [5] = {1, 2, 3, 4, 5};
        __MPI_Send(
                bufInt, 
                5 /* count */,
                MPI_INT, 
                1 /* dest */, 
                1000 /* tag */, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        int bufInt [5];
        __MPI_Irecv(
                bufInt, 
                5 /* count */, 
                MPI_INT, 
                0 /* source */, 
                1000 /* tag */, 
                MPI_COMM_WORLD,
                &request,
                messagePool);
        __MPI_Wait(
                &request, 
                &status,
                messagePool);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(bufInt[i], i + 1);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 1000);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        int count;
        MPI_Get_count(&status, MPI_INT, &count);
        EXPECT_EQ(count, 5);
    }

    if(rank == 0) {
        char bufChar [5] = {0, 1, 2, 3, 4};
        __MPI_Send(
                bufChar, 
                5 /* count */,
                MPI_CHAR, 
                1 /* dest */, 
                2000 /* tag */, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        char bufChar [5];
        __MPI_Irecv(
                bufChar, 
                5 /* count */, 
                MPI_CHAR, 
                0 /* source */, 
                2000 /* tag */, 
                MPI_COMM_WORLD,
                &request,
                messagePool);
        __MPI_Wait(
                &request, 
                &status,
                messagePool);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(bufChar[i], i);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 2000);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        int count;
        MPI_Get_count(&status, MPI_CHAR, &count);
        EXPECT_EQ(count, 5);
    }

    if(rank == 0) {
        double bufDouble [6] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
        __MPI_Send(
                bufDouble, 
                6 /* count */,
                MPI_DOUBLE, 
                1 /* dest */, 
                3000 /* tag */, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        double bufDouble [6];
        __MPI_Irecv(
                bufDouble, 
                6 /* count */, 
                MPI_DOUBLE, 
                0 /* source */, 
                3000 /* tag */, 
                MPI_COMM_WORLD,
                &request,
                messagePool);
        __MPI_Wait(
                &request, 
                &status,
                messagePool);
        const double epsilon = 1e-9;
        for (int i = 0; i < 6; i++) {
            EXPECT_NEAR(bufDouble[i], (i + 1) * 1.1, epsilon);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 3000);
        EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        int count;
        MPI_Get_count(&status, MPI_DOUBLE, &count);
        EXPECT_EQ(count, 6);
    }
}

TEST(MessageToolTests, MPI_TestTest) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MessagePool messagePool;
    MPI_Status status;
    MPI_Request request;
    if (rank == 0) {
        int bufInt [5] = {1, 2, 3, 4, 5};
        __MPI_Send(
                bufInt, 
                5 /* count */,
                MPI_INT, 
                1 /* dest */, 
                1000 /* tag */, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        int bufInt [5];
        __MPI_Irecv(
                bufInt, 
                5 /* count */, 
                MPI_INT, 
                0 /* source */, 
                1000 /* tag */, 
                MPI_COMM_WORLD,
                &request,
                messagePool);
        int flag;
        while (flag == 0) {
            __MPI_Test(
                    &request, 
                    &flag, 
                    &status,
                    messagePool);
            EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        }
        EXPECT_EQ(flag, 1);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(bufInt[i], i + 1);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 1000);
        int count;
        MPI_Get_count(&status, MPI_INT, &count);
        EXPECT_EQ(count, 5);
    }

    if(rank == 0) {
        char bufChar [5] = {0, 1, 2, 3, 4};
        __MPI_Send(
                bufChar, 
                5 /* count */,
                MPI_CHAR, 
                1 /* dest */, 
                2000 /* tag */, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        char bufChar [5];
        __MPI_Irecv(
                bufChar, 
                5 /* count */, 
                MPI_CHAR, 
                0 /* source */, 
                2000 /* tag */, 
                MPI_COMM_WORLD,
                &request,
                messagePool);
        int flag = 0;
        while (flag == 0) {
            __MPI_Test(
                    &request, 
                    &flag, 
                    &status,
                    messagePool);
            EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        }
        EXPECT_EQ(flag, 1);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(bufChar[i], i);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 2000);
        int count;
        MPI_Get_count(&status, MPI_CHAR, &count);
        EXPECT_EQ(count, 5);
    }

    if(rank == 0) {
        double bufDouble [6] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
        __MPI_Send(
                bufDouble, 
                6 /* count */,
                MPI_DOUBLE, 
                1 /* dest */, 
                3000 /* tag */, 
                MPI_COMM_WORLD);
    } else if (rank == 1) {
        double bufDouble [6];
        __MPI_Irecv(
                bufDouble, 
                6 /* count */, 
                MPI_DOUBLE, 
                0 /* source */, 
                3000 /* tag */, 
                MPI_COMM_WORLD,
                &request,
                messagePool);
        int flag = 0;
        while (flag == 0) {
            __MPI_Test(
                    &request, 
                    &flag, 
                    &status,
                    messagePool);
            EXPECT_EQ(status.MPI_ERROR, MPI_SUCCESS);
        }
        EXPECT_EQ(flag, 1);
        const double epsilon = 1e-9;
        for (int i = 0; i < 6; i++) {
            EXPECT_NEAR(bufDouble[i], (i + 1) * 1.1, epsilon);
        }
        EXPECT_EQ(status.MPI_SOURCE, 0);
        EXPECT_EQ(status.MPI_TAG, 3000);
        int count;
        MPI_Get_count(&status, MPI_DOUBLE, &count);
        EXPECT_EQ(count, 6);
    }
}
