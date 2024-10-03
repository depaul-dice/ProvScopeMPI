
all: messageToolTests messagePoolTests

messageToolTests: messageToolTests.o testTools.o ../messageTools.o ../messagePool.o ../tools.o
	mpic++ -o messageToolTests -std=c++11 -isystem /usr/include/gtest -pthread messageToolTests.o ../messageTools.o ../messagePool.o ../tools.o testTools.o -lgtest -lgtest_main

messagePoolTests: messagePoolTests.o ../messagePool.o
	mpic++ -o messagePoolTests -std=c++11 -isystem /usr/include/gtest -pthread messagePoolTests.o ../messagePool.o ../messageTools.o ../tools.o -lgtest -lgtest_main

%.o: %.cpp
	mpic++ -c $<

.PHONY: clean
clean:
	rm -f messageToolTests messagePoolTests *.o
