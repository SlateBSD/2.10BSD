#
# Copyright (c) 1983 Regents of the University of California.
# All rights reserved.  The Berkeley software License Agreement
# specifies the terms and conditions for redistribution.
#
#	@(#)Makefile	5.1 (Berkeley) 6/6/85
#
RDIST = /usr/ucb/rdist
DESTDIR=
SEPFLAG= -i
SRCS =	docmd.c expand.c gram.y lookup.c main.c server.c
OBJS =	docmd.o expand.o gram.o lookup.o main.o server.o
LINT =	lint -ps
CFLAGS= -O -DRDIST=\"${RDIST}\"

rdist:	${OBJS}
	${CC} ${SEPFLAG} -o rdist ${OBJS}

${OBJS}:  defs.h

clean:
	rm -f *.o gram.c errs

install: rdist
	install -s -m 4751 rdist ${DESTDIR}${RDIST}

lint:	docmd.c expand.c gram.c lookup.c main.c server.c
	${LINT} docmd.c expand.c gram.c lookup.c main.c server.c

print:	${SRCS}
	lpr -p ${SRCS} defs.h
