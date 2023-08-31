CXX = g++
CFLAGS = -g -O2 -Wall -Wno-deprecated

REACTOR=../../../reactor
REACTOR_H =$(REACTOR)/include
REACTOR_LIB=$(REACTOR)/lib  -llreactor

all:
	$(CXX) $(CFLAGS) -o example example.cpp -I$(REACTOR_H) -I../api ../lib/libclient.a -lprotobuf
	$(CXX) $(CFLAGS) -o simulator simulator.cpp -I$(REACTOR_H) -I../api ../lib/libclient.a -lprotobuf
	$(CXX) $(CFLAGS) -o qps qps.cpp -I$(REACTOR_H) -I../api ../lib/libclient.a -lprotobuf -lpthread
	$(CXX) $(CFLAGS) -o get_host get_host.cpp -I$(REACTOR_H) -I../api ../lib/libclient.a -lprotobuf
	$(CXX) $(CFLAGS) -o get_route get_route.cpp -I$(REACTOR_H) -I../api ../lib/libclient.a -lprotobuf
	$(CXX) $(CFLAGS) -o report report.cpp -I$(REACTOR_H) -I../api ../lib/libclient.a -lprotobuf

.PHONY: clean

clean:
	-rm ./example
