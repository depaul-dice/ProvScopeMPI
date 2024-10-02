
#include "../messageTools.h"

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
    int buf [5] = {1, 2, 3, 4, 5};
    stringstream ss = convertData2StringStream((void *)buf, MPI_INT, 5, __LINE__);
    EXPECT_EQ(ss.str(), "1|2|3|4|5|");
    long long int buf2 [5] = {6, 7, 8, 9, 10};
    ss = convertData2StringStream((void *)buf2, MPI_LONG_LONG_INT, 5, __LINE__);
    EXPECT_EQ(ss.str(), "6|7|8|9|10|");
    char buf3 [5] = {0, 1, 2, 3, 4};
    ss = convertData2StringStream((void *)buf3, MPI_CHAR, 5, __LINE__);
    EXPECT_EQ(ss.str(), "0|1|2|3|4|");
}
