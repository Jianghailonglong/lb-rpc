TARGET= ../lib/libclient.a
CXX=g++
CFLAGS=-g -O2 -Wall -Wno-deprecated

BASE=../../../base
BASE_H=$(BASE)/include

PROTO = $(BASE)/proto
PROTO_H = $(BASE)/proto

REACTOR=../../../reactor
REACTOR_H =$(REACTOR)/include
REACTOR_LIB=$(REACTOR)/lib  -llreactor

OTHER_LIB = -lpthread -ldl -lprotobuf
SRC= ./
INC= -I./include -I$(BASE_H) -I$(REACTOR_H) -I$(PROTO_H)

LIB= -L$(REACTOR_LIB) $(OTHER_LIB) 


OBJS = $(addsuffix .o, $(basename $(wildcard $(SRC)/*.cpp)))
OBJS += $(PROTO)/lbrss.pb.o

$(TARGET): $(OBJS)
	mkdir -p ../lib
	ar cqs $@ $^ 

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC) 

.PHONY: clean

clean:
	-rm -f ./*.o  $(PROTO)/lbrss.pb.o $(TARGET)