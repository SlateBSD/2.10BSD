/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)in_cksum.c	1.1 (2.10BSD Berkeley) 12/1/86
 */

#include "param.h"
#include "../machine/seg.h"

#include "mbuf.h"
#include "domain.h"
#include "protosw.h"
#include <netinet/in.h>
#include <netinet/in_systm.h>

/*
 * Checksum routine for Internet Protocol family headers.
 * This routine is very heavily used in the network
 * code and should be rewritten for each CPU to be as fast as possible.
 */

#if vax
#include <sys/types.h>

in_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_short *w;		/* known to be r9 */
	register int sum = 0;		/* known to be r8 */
	register int mlen = 0;

	for (;;) {
		/*
		 * Each trip around loop adds in
		 * word from one mbuf segment.
		 */
		w = mtod(m, u_short *);
		if (mlen == -1) {
			/*
			 * There is a byte left from the last segment;
			 * add it into the checksum.  Don't have to worry
			 * about a carry-out here because we make sure
			 * that high part of (32 bit) sum is small below.
			 */
			sum += *(u_char *)w << 8;
			w = (u_short *)((char *)w + 1);
			mlen = m->m_len - 1;
			len--;
		} else
			mlen = m->m_len;
		m = m->m_next;
		if (len < mlen)
			mlen = len;
		len -= mlen;
		/*
		 * Force to long boundary so we do longword aligned
		 * memory operations.  It is too hard to do byte
		 * adjustment, do only word adjustment.
		 */
		if (((int)w&0x2) && mlen >= 2) {
			sum += *w++;
			mlen -= 2;
		}
		/*
		 * Do as much of the checksum as possible 32 bits at at time.
		 * In fact, this loop is unrolled to make overhead from
		 * branches &c small.
		 *
		 * We can do a 16 bit ones complement sum 32 bits at a time
		 * because the 32 bit register is acting as two 16 bit
		 * registers for adding, with carries from the low added
		 * into the high (by normal carry-chaining) and carries
		 * from the high carried into the low on the next word
		 * by use of the adwc instruction.  This lets us run
		 * this loop at almost memory speed.
		 *
		 * Here there is the danger of high order carry out, and
		 * we carefully use adwc.
		 */
		while ((mlen -= 32) >= 0) {
			asm("clrl r0");		/* clears carry */
#undef ADD
#define ADD		asm("adwc (r9)+,r8;");
			ADD; ADD; ADD; ADD; ADD; ADD; ADD; ADD;
			asm("adwc $0,r8");
		}
		mlen += 32;
		while ((mlen -= 8) >= 0) {
			asm("clrl r0");
			ADD; ADD;
			asm("adwc $0,r8");
		}
		mlen += 8;
		/*
		 * Now eliminate the possibility of carry-out's by
		 * folding back to a 16 bit number (adding high and
		 * low parts together.)  Then mop up trailing words
		 * and maybe an odd byte.
		 */
		{ asm("ashl $-16,r8,r0; addw2 r0,r8");
		  asm("adwc $0,r8; movzwl r8,r8"); }
		while ((mlen -= 2) >= 0) {
			asm("movzwl (r9)+,r0; addl2 r0,r8");
		}
		if (mlen == -1)
			sum += *(u_char *)w;
		if (len == 0)
			break;
		/*
		 * Locate the next block with some data.
		 * If there is a word split across a boundary we
		 * will wrap to the top with mlen == -1 and
		 * then add it in shifted appropriately.
		 */
		for (;;) {
			if (m == 0) {
				printf("cksum: out of data\n");
				goto done;
			}
			if (m->m_len)
				break;
			m = m->m_next;
		}
	}
done:
	/*
	 * Add together high and low parts of sum
	 * and carry to get cksum.
	 * Have to be careful to not drop the last
	 * carry here.
	 */
	{ asm("ashl $-16,r8,r0; addw2 r0,r8; adwc $0,r8");
	  asm("mcoml r8,r8; movzwl r8,r8"); }
	return (sum);
}
#endif

#if pdp11
#ifdef DIAGNOSTIC
int	in_ckprint = 0;			/* print sums */
#endif

in_cksum(m, len)
	struct mbuf *m;
	int len;
{
	register u_char *w;		/* known to be r4 */
	register u_int mlen = 0;	/* known to be r3 */
	register u_int sum = 0;		/* known to be r2 */
	u_int plen = 0;

#ifdef DIAGNOSTIC
	if (in_ckprint)
		printf("ck m%o l%o", m, len);
#endif
	MAPSAVE();
	for (;;) {
		/*
		 * Each trip around loop adds in
		 * words from one mbuf segment.
		 */
		w = mtod(m, u_char *);
		if (plen & 01) {
			/*
			 * The last segment was an odd length, add the high
			 * order byte into the checksum.
			 */
			sum = in_ckadd(sum,(*w++ << 8));
			mlen = m->m_len - 1;
			len--;
		} else
			mlen = m->m_len;
		m = m->m_next;
		if (len < mlen)
			mlen = len;
		len -= mlen;
		plen = mlen;
		if (mlen > 0)
			in_ckbuf();	/* arguments already in registers */
		if (len == 0)
			break;
		/*
		 * Locate the next block with some data.
		 */
		for (;;) {
			if (m == 0) {
				printf("cksum: out of data\n");
				goto done;
			}
			if (m->m_len)
				break;
			m = m->m_next;
		}
	}
done:
	MAPREST();
#ifdef DIAGNOSTIC
	if (in_ckprint)
		printf(" s%o\n", ~sum);
#endif
	return(~sum);
}
#endif
