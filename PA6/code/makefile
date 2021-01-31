# makefile

all: server client

common.o: common.h common.cpp
	g++ -g -w -std=c++11 -c common.cpp

Histogram.o: Histogram.h Histogram.cpp
	g++ -g -w -std=c++11 -c Histogram.cpp

NetworkReqChannel.o: NetworkReqChannel.h NetworkReqChannel.cpp
	g++ -g -w -std=c++11 -c NetworkReqChannel.cpp

client: client.cpp Histogram.o NetworkReqChannel.o common.o
	g++ -g -w -std=c++11 -fsanitize=address -o client client.cpp Histogram.o NetworkReqChannel.o common.o -lpthread -lrt -fno-omit-frame-pointer

server: server.cpp NetworkReqChannel.o common.o
	g++ -g -w -std=c++11 -fsanitize=address -o server server.cpp NetworkReqChannel.o common.o -lpthread -lrt -fno-omit-frame-pointer

clean:
	rm -rf *.o fifo* server client 
