/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef SYSLIBC_SCCS
_sccsid: <@(#)execl.s	2.5 (Berkeley) 1/29/87\0>
	.even
#endif SYSLIBC_SCCS

/*
 * XXX - this routine can't use SYSCALL!!!
 */
#include "SYS.h"

.globl	_environ

ENTRY(execl)
	mov	_environ,-(sp)	/ pass default environment
	mov	sp,r0		/ calculate and pass address of first argv
	add	$6.,r0		/   element (can't use "mov sp,-(sp)")
	mov	r0,-(sp)
	mov	6(sp),-(sp)	/ pass the name
	tst	-(sp)		/ simulate return address stack spacing
	SYS(execve)		/   and go for it ...
	add	$8.,sp		/ if we get back it's an error
	mov	r0,_errno
	mov	$-1,r0
	rts	pc
