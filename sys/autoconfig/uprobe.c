/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)uprobe.c	1.1 (2.10BSD Berkeley) 12/1/86
 */

/*
 * The uprobe table contains the pointers to the user-level probe routines
 * that usually attempt to make various devices interrupt.  The actual
 * probe routines are in the device driver sources.
 *
 * NOTES:
 *	Reads and writes to kmem (done by grab, stuff) are currently done a
 *	byte at a time in the kernel.  Also, many of these are untested and
 *	some of them assume that if something is at the address, it is the
 *	correct device.  Others assume that if something *isn't* at the
 *	address, the correct device *is*.  Why are you looking at me like
 *	that?
 */

#include "uprobe.h"

int	xpprobe(), hkprobe(), rlprobe(), rkprobe(), htprobe(), siprobe(),
	tmprobe(), tsprobe(), cnprobe(), dzprobe(), dhprobe(), dmprobe(),
	drprobe(), lpprobe(), dhuprobe(), raprobe(), rxprobe();

UPROBE uprobe[] = {
	"hk",	hkprobe,	/* hk -- rk611, rk06/07 */
	"hp",	xpprobe,	/* hp -- rjp04/06, rwp04/06 */
	"ra",	raprobe,	/* ra -- rqdx? (rx50,rd51/52/53), uda50 (ra60/80/81), klesi (ra25) */
	"rk",	rkprobe,	/* rk -- rk05 */
	"rl",	rlprobe,	/* rl -- rl01/02 */
	"si",	siprobe,	/* si -- SI 9500 for CDC 9766 */
	"xp",	xpprobe,	/* xp -- rm02/03/05, rp04/05/06, Diva, Ampex, SI Eagle */
	"ht",	htprobe,	/* ht -- tju77, twu77, tje16, twe16 */
	"tm",	tmprobe,	/* tm -- tm11 */
	"ts",	tsprobe,	/* ts -- ts11 */
	"dh",	dhprobe,	/* dh -- DH11 */
	"dm",	dmprobe,	/* dm -- DM11 */
	"dr",	drprobe,	/* dr -- DR11W */
	"du",	dhuprobe,	/* du -- dhu, dhv */
	"dz",	dzprobe,	/* dz -- dz11 */
	"cn",	cnprobe,	/* cn -- kl11, dl11 */
	"lp",	lpprobe,	/* lp -- line printer */
	"rx",	rxprobe,	/* rx -- RX01/02 */
	0,	0,
};
