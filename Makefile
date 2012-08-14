CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-Wall
LDFLAGS= 
LDLIBS=-lboost_system -lboost_filesystem

SRCS=config.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: config-parser

config-parser: $(OBJS)
	g++ $(LDFLAGS) -o config-parser $(OBJS) $(LDLIBS) 

config.o: config.cpp

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) config-parser
