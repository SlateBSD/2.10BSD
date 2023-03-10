/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)main.c	5.7 (Berkeley) 5/22/86";
#endif not lint

#include <sys/param.h>
#include <sys/vmmac.h>
#include <sys/socket.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <stdio.h>

#ifdef BSD2_10
#define	nsprotox	_nsprtx
#endif

struct nlist nl[] = {
#define	N_MBSTAT	0
	{ "_mbstat" },
#define	N_IPSTAT	1
	{ "_ipstat" },
#define	N_TCB		2
	{ "_tcb" },
#define	N_TCPSTAT	3
	{ "_tcpstat" },
#define	N_UDB		4
	{ "_udb" },
#define	N_UDPSTAT	5
	{ "_udpstat" },
#define	N_RAWCB		6
	{ "_rawcb" },
#define	N_IFNET		7
	{ "_ifnet" },
#define	N_HOSTS		8
	{ "_hosts" },
#define	N_RTHOST	9
	{ "_rthost" },
#define	N_RTNET		10
	{ "_rtnet" },
#define	N_ICMPSTAT	11
	{ "_icmpstat" },
#define	N_RTSTAT	12
	{ "_rtstat" },
#define	N_NFILE		13
	{ "_nfile" },
#define	N_FILE		14
	{ "_file" },
#define	N_UNIXSW	15
	{ "_unixsw" },
#define N_RTHASHSIZE	16
	{ "_rthashsize" },
#define N_IDP		17
	{ "_nspcb"},
#define N_IDPSTAT	18
	{ "_idpstat"},
#define N_SPPSTAT	19
	{ "_spp_istat"},
#define N_NSERR		20
	{ "_ns_errstat"},
#ifndef	BSD2_10
#define	N_SYSMAP	21
	{ "_Sysmap" },
#define	N_SYSSIZE	22
	{ "_Syssize" },
#endif
	"",
};

/* internet protocols */
extern	int protopr();
extern	int tcp_stats(), udp_stats(), ip_stats(), icmp_stats();
extern	int nsprotopr();
extern	int spp_stats(), idp_stats(), nserr_stats();

struct protox {
	u_char	pr_index;		/* index into nlist of cb head */
	u_char	pr_sindex;		/* index into nlist of stat block */
	u_char	pr_wanted;		/* 1 if wanted, 0 otherwise */
	int	(*pr_cblocks)();	/* control blocks printing routine */
	int	(*pr_stats)();		/* statistics printing routine */
	char	*pr_name;		/* well-known name */
} protox[] = {
	{ N_TCB,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	"tcp" },
	{ N_UDB,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	"udp" },
	{ -1,		N_IPSTAT,	1,	0,
	  ip_stats,	"ip" },
	{ -1,		N_ICMPSTAT,	1,	0,
	  icmp_stats,	"icmp" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

struct protox nsprotox[] = {
	{ N_IDP,	N_IDPSTAT,	1,	nsprotopr,
	  idp_stats,	"idp" },
	{ N_IDP,	N_SPPSTAT,	1,	nsprotopr,
	  spp_stats,	"spp" },
	{ -1,		N_NSERR,	1,	0,
	  nserr_stats,	"ns_err" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

#ifdef BSD2_10
char	*system = "/unix";
#else
struct	pte *Sysmap;

char	*system = "/vmunix";
#endif
char	*kmemf = "/dev/kmem";
int	kmem;
int	kflag;
int	Aflag;
int	aflag;
int	hflag;
int	iflag;
int	mflag;
int	nflag;
int	rflag;
int	sflag;
int	tflag;
int	fflag;
int	interval;
char	*interface;
int	unit;
char	usage[] = "[ -Aaihmnrst ] [-f address_family] [-I interface] [ interval ] [ system ] [ core ]";

int	af = AF_UNSPEC;

main(argc, argv)
	int argc;
	char *argv[];
{
	int i;
	char *cp, *name;
	register struct protoent *p;
	register struct protox *tp;

	name = argv[0];
	argc--, argv++;
  	while (argc > 0 && **argv == '-') {
		for (cp = &argv[0][1]; *cp; cp++)
		switch(*cp) {

		case 'A':
			Aflag++;
			break;

		case 'a':
			aflag++;
			break;

		case 'h':
			hflag++;
			break;

		case 'i':
			iflag++;
			break;

		case 'm':
			mflag++;
			break;

		case 'n':
			nflag++;
			break;

		case 'r':
			rflag++;
			break;

		case 's':
			sflag++;
			break;

		case 't':
			tflag++;
			break;

		case 'u':
			af = AF_UNIX;
			break;

		case 'f':
			argv++;
			argc--;
			if (strcmp(*argv, "ns") == 0)
				af = AF_NS;
			else if (strcmp(*argv, "inet") == 0)
				af = AF_INET;
			else if (strcmp(*argv, "unix") == 0)
				af = AF_UNIX;
			else {
				fprintf(stderr, "%s: unknown address family\n",
					*argv);
				exit(10);
			}
			break;
			
		case 'I':
			iflag++;
			if (*(interface = cp + 1) == 0) {
				if ((interface = argv[1]) == 0)
					break;
				argv++;
				argc--;
			}
			for (cp = interface; isalpha(*cp); cp++)
				;
			unit = atoi(cp);
			*cp-- = 0;
			break;

		default:
use:
			printf("usage: %s %s\n", name, usage);
			exit(1);
		}
		argv++, argc--;
	}
	if (argc > 0 && isdigit(argv[0][0])) {
		interval = atoi(argv[0]);
		if (interval <= 0)
			goto use;
		argv++, argc--;
		iflag++;
	}
	if (argc > 0) {
		system = *argv;
		argv++, argc--;
	}
	nlist(system, nl);
	if (nl[0].n_type == 0) {
		fprintf(stderr, "%s: no namelist\n", system);
		exit(1);
	}
	if (argc > 0) {
		kmemf = *argv;
		kflag++;
	}
	kmem = open(kmemf, 0);
	if (kmem < 0) {
		fprintf(stderr, "cannot open ");
		perror(kmemf);
		exit(1);
	}
#ifndef BSD2_10
	if (kflag) {
		off_t off;

		off = nl[N_SYSMAP].n_value & 0x7fffffff;
		lseek(kmem, off, 0);
		nl[N_SYSSIZE].n_value *= 4;
		Sysmap = (struct pte *)malloc(nl[N_SYSSIZE].n_value);
		if (Sysmap == 0) {
			perror("Sysmap");
			exit(1);
		}
		read(kmem, Sysmap, nl[N_SYSSIZE].n_value);
	}
#endif
	if (mflag) {
		mbpr( (off_t)nl[N_MBSTAT].n_value);
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	if (iflag) {
		intpr(interval, (off_t)nl[N_IFNET].n_value);
		exit(0);
	}
	if (hflag) {
#ifndef BSD2_10
		hostpr( (off_t)nl[N_HOSTS].n_value);
#endif
		exit(0);
	}
	if (rflag) {
		if (sflag)
			rt_stats( (off_t)nl[N_RTSTAT].n_value);
		else
			routepr((off_t)nl[N_RTHOST].n_value,
				(off_t)nl[N_RTNET].n_value,
				(off_t)nl[N_RTHASHSIZE].n_value);
		exit(0);
	}
    if (af == AF_INET || af == AF_UNSPEC) {
	setprotoent(1);
	setservent(1);
	while (p = getprotoent()) {

		for (tp = protox; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, p->p_name) == 0)
				break;
		if (tp->pr_name == 0 || tp->pr_wanted == 0)
			continue;
		if (sflag) {
			if (tp->pr_stats)
			    (*tp->pr_stats)((off_t)nl[tp->pr_sindex].n_value,
					p->p_name);
		} else
			if (tp->pr_cblocks)
			    (*tp->pr_cblocks)((off_t)nl[tp->pr_index].n_value,
					p->p_name);
	}
	endprotoent();
    }
    if (af == AF_NS || af == AF_UNSPEC) {
	for (tp = nsprotox; tp->pr_name; tp++) {
		if (sflag) {
			if (tp->pr_stats)
			    (*tp->pr_stats)((off_t)nl[tp->pr_sindex].n_value,
					tp->pr_name);
		} else
			if (tp->pr_cblocks)
			    (*tp->pr_cblocks)(nl[tp->pr_index].n_value,
					tp->pr_name);
	}
    }
    if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
	    unixpr((off_t)nl[N_NFILE].n_value, (off_t)nl[N_FILE].n_value,
		nl[N_UNIXSW].n_value);
    exit(0);
}

/*
 * Seek into the kernel for a value.
 */
klseek(fd, base, off)
	int fd, off;
	off_t	base;
{

#ifndef BSD2_10
	if (kflag) {
		/* get kernel pte */
#ifdef vax
		base &= 0x7fffffff;
#endif
		base = ctob(Sysmap[btop(base)].pg_pfnum) + (base & PGOFSET);
	}
#endif
	lseek(fd, (off_t)base, off);
}

char *
plural(n)
	long n;
{

	return (n != 1 ? "s" : "");
}
