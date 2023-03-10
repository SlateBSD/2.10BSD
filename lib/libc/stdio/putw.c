#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)putw.c	5.2 (Berkeley) 3/9/86";
#endif LIBC_SCCS and not lint

#include <stdio.h>

putw(w, iop)
register FILE *iop;
{
	register char *p;
	register i;

	p = (char *)&w;
	for (i=sizeof(int); --i>=0;)
		putc(*p++, iop);
	return(ferror(iop));
}

#ifdef BSD2_10
putlw(w, iop)
long w;
register FILE *iop;
{
	register char *p;
	register i;

	p = (char *)&w;
	for (i=sizeof(long); --i>=0;)
		putc(*p++, iop);
	return(ferror(iop));
}
#endif BSD2_10
