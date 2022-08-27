CC=gcc
LD=$(CC)
CPPFLAGS=-g -std=c11 -Wpedantic -Wall -Wextra
CFLAGS=-I. -I/comp/111/assignments/aardvarks
LDFLAGS=
LDLIBS=-lrt -lpthread
PROGRAM=anthills

all: $(PROGRAM) 

$(PROGRAM): aardvarks.o anthills.o
	$(LD) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) -o $@ $^

aardvarks.o: aardvarks.c /comp/111/assignments/aardvarks/anthills.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

anthills.o: /comp/111/assignments/aardvarks/anthills.c /comp/111/assignments/aardvarks/anthills.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(PROGRAM)
