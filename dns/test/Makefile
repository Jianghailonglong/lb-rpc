TARGET= dns_test
CXX=g++
CFLAGS=-g -O2 -Wall -Wno-deprecated

BASE=../../base
BASE_H=$(BASE)/include

PROTO = $(BASE)/proto
PROTO_H = $(BASE)/proto

REACTOR=../../reactor
REACTOR_H =$(REACTOR)/include
REACTOR_LIB=$(REACTOR)/lib  -llreactor

OTHER_LIB = -lpthread -ldl -lprotobuf
SRC= ./src
INC= -I./include -I$(BASE_H) -I$(REACTOR_H) -I$(PROTO_H)

LIB= $(MYSQL_LIB) -L$(REACTOR_LIB) $(OTHER_LIB) 


OBJS = dns_test.o
OBJS += $(PROTO)/lbrpc.pb.o

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(INC) $(LIB)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC) 

.PHONY: clean

clean:
	-rm -f ./*.o $(TARGET)