CC=g++
CCFLAGS=-std=c++11 -Wall -I. -m64 -O3
LDFLAGS=

DEPS=$(wildcard *.hh)

SRCS=$(wildcard *.cc)
OBJS=$(SRCS:.cc=.o)

default: test_lib_timing

test_lib_timing: test_lib_timing.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cc $(DEPS)
	$(CC) $(CCFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS) test_lib_timing
