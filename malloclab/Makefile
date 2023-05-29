#
# Students' Makefile for the Malloc Lab
#
TEAM = bovik
VERSION = 1
HANDINDIR = /afs/cs.cmu.edu/academic/class/15213-f01/malloclab/handin

CC = gcc
CFLAGS = -Wall -g -O2 -m32

OBJS_IMPLICIT = mdriver.o mm-implicit_freelist.o memlib.o fsecs.o fcyc.o clock.o ftimer.o
OBJS_EXPLICIT = mdriver.o mm-explicit_freelist.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver-implicit: $(OBJS_IMPLICIT)
	$(CC) $(CFLAGS) -o mdriver $(OBJS_IMPLICIT)

mdriver-explicit: $(OBJS_EXPLICIT)
	$(CC) $(CFLAGS) -o mdriver $(OBJS_EXPLICIT)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm-implicit_freelist.o: mm-implicit_freelist.c mm.h memlib.h
mm-explicit_freelist.o: mm-explicit_freelist.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

handin:
	cp mm.c $(HANDINDIR)/$(TEAM)-$(VERSION)-mm.c

clean:
	rm -f *~ *.o mdriver-*


