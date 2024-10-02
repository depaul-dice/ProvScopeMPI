
all: messageToolTests messagePoolTests

messageToolTests: messageToolTests.o ../messageTools.o
	mpic++ -o messageToolTests -std=c++11 -isystem /usr/include/gtest -pthread messageToolTests.o ../messageTools.o ../messagePool.o ../tools.o -lgtest -lgtest_main

messagePoolTests: messagePoolTests.o ../messagePool.o
	mpic++ -o messagePoolTests -std=c++11 -isystem /usr/include/gtest -pthread messagePoolTests.o ../messagePool.o ../messageTools.o ../tools.o -lgtest -lgtest_main

messageToolTests.o: messageToolTests.cpp
	mpic++ -c messageToolTests.cpp

messagePoolTests.o: messagePoolTests.cpp
	mpic++ -c messagePoolTests.cpp

.PHONY: clean
clean:
	rm -f messageToolTests messagePoolTests messageToolTests.o messagePoolTests.o
