# @(#)Makefile	1.1 (Berkeley) 12/15/82

CFLAGS=-O
SEPFLAG= -i
m4:	m4.o m4y.o
	$(CC) ${SEPFLAG} -o m4 m4.o m4y.o

install:
	install -s m4 $(DESTDIR)/usr/bin

clean:
	rm -f *.o
