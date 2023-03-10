/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)fatal.h	5.1 (Berkeley) 12/13/86
 */

extern int	Fflags;
extern char	*Ffile;
extern int	Fvalue;
extern int	(*Ffunc)();
extern int	(Fjmp[10]);

#define	FTLMSG		0100000
#define	FTLCLN		0040000
#define	FTLFUNC		0020000
#define	FTLACT		0000077
#define	FTLJMP		0000002
#define	FTLEXIT		0000001
#define	FTLRET		0000000

#define	FSAVE(val)	SAVE(Fflags,old_Fflags); Fflags = (val);
#define	FRSTR()		RSTR(Fflags,old_Fflags);
