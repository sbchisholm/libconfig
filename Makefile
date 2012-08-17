CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-Wall
LDFLAGS= 
LDLIBS=-lboost_system -lboost_filesystem -lboost_regex

SRCS=Main.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: test

test: $(OBJS)
	g++ $(LDFLAGS) -o test $(OBJS) $(LDLIBS)

Main.o: Main.cpp Libconfig.h Types.h Configuration.h Parse.h Printing.h

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) test
