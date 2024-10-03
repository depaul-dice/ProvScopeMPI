
all: messageToolTests messagePoolTests

messageToolTests: messageToolTests.o testUtils.o ../messageTools.o ../messagePool.o ../tools.o
	mpic++ -o messageToolTests -std=c++11 -isystem /usr/include/gtest -pthread messageToolTests.o ../messageTools.o ../messagePool.o ../tools.o testUtils.o -lgtest -lgtest_main

messagePoolTests: messagePoolTests.o ../messagePool.o
	mpic++ -o messagePoolTests -std=c++11 -isystem /usr/include/gtest -pthread messagePoolTests.o ../messagePool.o ../messageTools.o ../tools.o -lgtest -lgtest_main

%.o: %.cpp
	mpic++ -c $<

.PHONY: clean
clean:
	rm -f messageToolTests messagePoolTests *.o
