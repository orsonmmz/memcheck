CXXFLAGS=-O0 -g -rdynamic -Wall
LDFLAGS=-ldl

memcheck_test: memcheck_test.cpp

all: memcheck_test

clean:
	rm memcheck_test || true
