# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2018-19

SOURCES = CircuitRouter-AdvShell.c 
SOURCES+= ../lib/commandlinereader.c ../lib/vector.c
OBJS = $(SOURCES:%.c=%.o)
CC   = gcc
CFLAGS =-Wall -std=gnu99 -I../
LDFLAGS=-lm
TARGET = CircuitRouter-AdvShell

all: $(TARGET) CircuitRouter-SeqSolver CircuitRouter-Client

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LDFLAGS) 

CircuitRouter-AdvShell.o: CircuitRouter-AdvShell.c CircuitRouter-AdvShell.h ../lib/commandlinereader.h ../lib/vector.h
../lib/commandlinereader.o: ../lib/commandlinereader.c ../lib/commandlinereader.h
../lib/vector.o: ../lib/vector.c ../lib/vector.h ../lib/types.h ../lib/utility.h

CircuitRouter-Client: CircuitRouter-Client.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

CircuitRouter-Client.o: CircuitRouter-Client.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJS):
	$(CC) $(CFLAGS) -c -o $@ $<

CircuitRouter-SeqSolver:
	(cd ../CircuitRouter-SeqSolver/ && make)

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGET)
	rm -f *Client.o *Client 
	rm -f *.pipe
	(cd ../CircuitRouter-SeqSolver/ && make clean) 
