/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef UCB_NET

#ifndef lint
static char sccsid[] = "@(#)inet.c	5.4 (Berkeley) 2/25/86";
#endif not lint

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netdb.h>
#include "net.h"	/* for CRASH only */

extern	int kmem;
extern	int Aflag;
extern	int aflag;
extern	int nflag;

static	int first = 1;
char	*inetname();

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
protopr(off, name)
	off_t off;
	char *name;
{
	register struct inpcb *prev, *next;
	struct inpcb inpcb, *inpcbp = &inpcb;
	struct socket sockb, *sockp = &sockb;
	struct tcpcb tcpcb, *tcpcbp = &tcpcb;
	int rcvct, sndct;	/* number of mbufs in receive/send queue */
	int istcp;

	if (off == 0)
		return;
	istcp = strcmp(name, "tcp") == 0;
	klseek(kmem, (off_t)off, 0);
	read(kmem, inpcbp, sizeof (struct inpcb));
	prev = (struct inpcb *)off;
	if (inpcbp->inp_next == (struct inpcb *)off)
		return;
	while (inpcbp->inp_next != (struct inpcb *)off) {
		char *cp;

		next = inpcbp->inp_next;
#ifdef	CRASH
		if (!VALADD(next, struct inpcb)) {
			printf("bad inpcb address (");
			DUMP((unsigned)next);
			puts(")");
			break;
		}
		(putars((unsigned)next, sizeof(struct inpcb),
			AS_INPCB))->as_ref++;
		inpcbp = XLATE(next, struct inpcb *);
#else
		klseek(kmem, (off_t)next, 0);
		read(kmem, inpcbp, sizeof (struct inpcb));
#endif	CRASH
		if (inpcbp->inp_prev != prev) {
			printf("???\n");
			break;
		}
		if (!aflag &&
		  inet_lnaof(inpcbp->inp_laddr.s_addr) == INADDR_ANY) {
			prev = next;
			continue;
		}
#ifdef	CRASH
		if (!VALADD(inpcbp->inp_socket, struct socket)) {
			printf("bad socket address (");
			DUMP((unsigned)inpcbp->inp_socket);
			puts(")");
			break;
		}
		(putars((unsigned)inpcbp->inp_socket,sizeof(struct socket),
			AS_SOCK))->as_ref++;
		sockp = XLATE(inpcbp->inp_socket, struct socket *);
#else
		klseek(kmem, (off_t)inpcbp->inp_socket, 0);
		read(kmem, sockp, sizeof (struct socket));
#endif	CRASH
		if (istcp) {
#ifdef	CRASH
			if (!VALADD(inpcbp->inp_ppcb, struct tcpcb)) {
				printf("bad tcpcb address (");
				DUMP((unsigned)inpcbp->inp_ppcb);
				puts(")");
				break;
			}
			(putars((unsigned)inpcbp->inp_ppcb,
				sizeof(struct tcpcb), AS_TCPCB))->as_ref++;
			tcpcbp = XLATE(inpcbp->inp_ppcb, struct tcpcb *);
#else
			klseek(kmem, (off_t)inpcbp->inp_ppcb, 0);
			read(kmem, &tcpcb, sizeof (tcpcb));
#endif	CRASH
		}
		if (first) {
			printf("Active Internet connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf(Aflag ?
				"%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n" :
				"%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
				"Proto", "Recv-Q", "Send-Q",
				"Local Address", "Foreign Address", "(state)");
			first = 0;
		}
#ifdef	CRASH		/* chase the chains of mbufs */
		rcvct = chasem(sockp->so_rcv.sb_mb,"recv",sockp->so_rcv.sb_cc);
		sndct = chasem(sockp->so_snd.sb_mb,"send",sockp->so_snd.sb_cc);
#endif	CRASH
		if (Aflag)
			if (istcp)
				printf("%8x ", inpcbp->inp_ppcb);
			else
				printf("%8x ", next);
		printf("%-5.5s %6d %6d ", name, sockp->so_rcv.sb_cc,
			sockp->so_snd.sb_cc);
		inetprint(&inpcbp->inp_laddr, inpcbp->inp_lport, name);
		inetprint(&inpcbp->inp_faddr, inpcbp->inp_fport, name);
		if (istcp) {
			if (tcpcbp->t_state < 0 ||
			   tcpcbp->t_state >= TCP_NSTATES)
				printf(" %d", tcpcbp->t_state);
			else
				printf(" %s", tcpstates[tcpcbp->t_state]);
		}
		putchar('\n');
#ifdef	CRASH
		if (istcp) {
			if (tcpcbp->t_tcpopt || tcpcbp->t_inpcb != next)
			printf("--tcp opts=%4x t_inpcb=%4x (actual inpcb=%4x)\n",
				tcpcbp->t_tcpopt, tcpcbp->t_inpcb, next);
		}
#endif	CRASH
		prev = next;
	}
}

/*
 * Dump TCP statistics structure.
 */
tcp_stats(off, name)
	off_t off;
	char *name;
{
	struct tcpstat tcpstat;

	if (off == 0)
		return;
	klseek(kmem, (off_t)off, 0);
	read(kmem, (char *)&tcpstat, sizeof (tcpstat));
	printf("%s:\n\t%D incomplete header%s\n", name,
		tcpstat.tcps_hdrops, plural(tcpstat.tcps_hdrops));
	printf("\t%D bad checksum%s\n",
		tcpstat.tcps_badsum, plural(tcpstat.tcps_badsum));
	printf("\t%D bad header offset field%s\n",
		tcpstat.tcps_badoff, plural(tcpstat.tcps_badoff));
	printf("\t%D bad segment%s\n",
		tcpstat.tcps_badsegs, plural(tcpstat.tcps_badsegs));
	printf("\t%D unacknowledged packet%s\n",
		tcpstat.tcps_unack, plural(tcpstat.tcps_unack));
}

/*
 * Dump UDP statistics structure.
 */
udp_stats(off, name)
	off_t off;
	char *name;
{
	struct udpstat udpstat;

	if (off == 0)
		return;
	klseek(kmem, (off_t)off, 0);
	read(kmem, (char *)&udpstat, sizeof (udpstat));
	printf("%s:\n\t%D incomplete header%s\n", name,
		udpstat.udps_hdrops, plural(udpstat.udps_hdrops));
	printf("\t%D bad data length field%s\n",
		udpstat.udps_badlen, plural(udpstat.udps_badlen));
	printf("\t%D bad checksum%s\n",
		udpstat.udps_badsum, plural(udpstat.udps_badsum));
}

/*
 * Dump IP statistics structure.
 */
ip_stats(off, name)
	off_t off;
	char *name;
{
	struct ipstat ipstat;

	if (off == 0)
		return;
	klseek(kmem, (off_t)off, 0);
	read(kmem, (char *)&ipstat, sizeof (ipstat));
	printf("%s:\n\t%D total packets received\n", name,
		ipstat.ips_total);
	printf("\t%D bad header checksum%s\n",
		ipstat.ips_badsum, plural(ipstat.ips_badsum));
	printf("\t%D with size smaller than minimum\n", ipstat.ips_tooshort);
	printf("\t%D with data size < data length\n", ipstat.ips_toosmall);
	printf("\t%D with header length < data size\n", ipstat.ips_badhlen);
	printf("\t%D with data length < header length\n", ipstat.ips_badlen);
	printf("\t%D fragment%s received\n",
		ipstat.ips_fragments, plural(ipstat.ips_fragments));
	printf("\t%D fragment%s dropped (dup or out of space)\n",
		ipstat.ips_fragdropped, plural(ipstat.ips_fragdropped));
	printf("\t%D fragment%s dropped after timeout\n",
		ipstat.ips_fragtimeout, plural(ipstat.ips_fragtimeout));
	printf("\t%D packet%s forwarded\n",
		ipstat.ips_forward, plural(ipstat.ips_forward));
	printf("\t%D packet%s not forwardable\n",
		ipstat.ips_cantforward, plural(ipstat.ips_cantforward));
	printf("\t%D redirect%s sent\n",
		ipstat.ips_redirectsent, plural(ipstat.ips_redirectsent));
}

static	char *icmpnames[] = {
	"echo reply",
	"#1",
	"#2",
	"destination unreachable",
	"source quench",
	"routing redirect",
	"#6",
	"#7",
	"echo",
	"#9",
	"#10",
	"time exceeded",
	"parameter problem",
	"time stamp",
	"time stamp reply",
	"information request",
	"information request reply",
	"address mask request",
	"address mask reply",
};

/*
 * Dump ICMP statistics.
 */
icmp_stats(off, name)
	off_t off;
	char *name;
{
	struct icmpstat icmpstat;
	register int i, first;

	if (off == 0)
		return;
	klseek(kmem, (off_t)off, 0);
	read(kmem, (char *)&icmpstat, sizeof (icmpstat));
	printf("%s:\n\t%D call%s to icmp_error\n", name,
		icmpstat.icps_error, plural(icmpstat.icps_error));
	printf("\t%D error%s not generated 'cuz old message was icmp\n",
		icmpstat.icps_oldicmp, plural(icmpstat.icps_oldicmp));
	for (first = 1, i = 0; i < ICMP_IREQREPLY + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %D\n", icmpnames[i],
				icmpstat.icps_outhist[i]);
		}
	printf("\t%D message%s with bad code fields\n",
		icmpstat.icps_badcode, plural(icmpstat.icps_badcode));
	printf("\t%D message%s < minimum length\n",
		icmpstat.icps_tooshort, plural(icmpstat.icps_tooshort));
	printf("\t%D bad checksum%s\n",
		icmpstat.icps_checksum, plural(icmpstat.icps_checksum));
	printf("\t%D message%s with bad length\n",
		icmpstat.icps_badlen, plural(icmpstat.icps_badlen));
	for (first = 1, i = 0; i < ICMP_IREQREPLY + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %D\n", icmpnames[i],
				icmpstat.icps_inhist[i]);
		}
	printf("\t%D message response%s generated\n",
		icmpstat.icps_reflect, plural(icmpstat.icps_reflect));
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
inetprint(in, port, proto)
	register struct in_addr *in;
	int port;
	char *proto;
{
	struct servent *sp = 0;
	char line[80], *cp, *index();
	int width;

	sprintf(line, "%.*s.", (Aflag && !nflag) ? 12 : 16, inetname(*in));
	cp = index(line, '\0');
	if (!nflag && port)
		sp = getservbyport(port, proto);
	if (sp || port == 0)
		sprintf(cp, "%.8s", sp ? sp->s_name : "*");
	else
		sprintf(cp, "%d", ntohs((u_short)port));
	width = Aflag ? 18 : 22;
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give 
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(in)
	struct in_addr in;
{
	register char *cp;
	char *index();
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && in.s_addr != INADDR_ANY) {
		u_long net = inet_netof(in);
		u_long lna = inet_lnaof(in);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr(&in, sizeof (in), AF_INET);
			if (hp) {
				if ((cp = index(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
		}
	}
	if (in.s_addr == INADDR_ANY)
		strcpy(line, "*");
	else if (cp)
		strcpy(line, cp);
	else {
		in.s_addr = ntohl(in.s_addr);
#define C(x)	((x) & 0xff)
		sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}

#endif UCB_NET
