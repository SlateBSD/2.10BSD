/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)xpauto.c	1.1 (2.10BSD Berkeley) 12/1/86
 */

/*
 * Several drives use this probe routine.
 *
 * hp -- rjp04/06, rwp04/06
 * rm -- rjm02/rwm03, rm02/03/05
 * xp -- rm02/03/05, rp04/05/06, Diva, Ampex, SI Eagle, Fuji 160
 */
#include "param.h"
#include "../machine/autoconfig.h"
#include "../machine/machparam.h"

#include "hpreg.h"

xpprobe(addr)
	struct hpdevice *addr;
{
	stuff(HP_IE | HP_RDY, &(addr->hpcs1.w));
	DELAY(10);
	stuff(0, &(addr->hpcs1.w));
	return(ACP_IFINTR);
}
