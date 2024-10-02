
messageToolTests: messageToolTests.o ../messageTools.o
	mpic++ -o messageToolTests -std=c++11 -isystem /usr/include/gtest -pthread messageToolTests.o ../messageTools.o ../messagePool.o ../tools.o -lgtest -lgtest_main

messageToolTests.o: messageToolTests.cpp
	mpic++ -c messageToolTests.cpp

.PHONY: clean
clean:
	rm -f messageToolTests messageToolTests.o
