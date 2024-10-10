EXECS = messageToolTests messagePoolTests AMGimitateTests
OBJS = messageToolTests.o messagePoolTests.o AMGimitateTests.o testUtils.o

all: $(EXECS)

messageToolTests: messageToolTests.o testUtils.o ../messageTools.o ../messagePool.o ../tools.o
	mpic++ -o messageToolTests -std=c++11 -isystem /usr/include/gtest -pthread messageToolTests.o ../messageTools.o ../messagePool.o ../tools.o testUtils.o -lgtest -lgtest_main

messagePoolTests: messagePoolTests.o ../messagePool.o
	mpic++ -o messagePoolTests -std=c++11 -isystem /usr/include/gtest -pthread messagePoolTests.o ../messagePool.o ../messageTools.o ../tools.o -lgtest -lgtest_main

AMGimitateTests: AMGimitateTests.o testUtils.o ../messagePool.o ../messageTools.o ../tools.o 
	mpic++ -o AMGimitateTests -std=c++11 -isystem /usr/include/gtest -pthread AMGimitateTests.o testUtils.o ../messagePool.o ../messageTools.o ../tools.o -lgtest -lgtest_main

%.o: %.cpp
	mpic++ -c $<

../%.o: ../%.cpp
	$(MAKE) -C .. $(MAKECMDGOALS)

.PHONY: clean
clean:
	rm -f $(EXECS) $(OBJS)
