/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)cmds.c	2.2 (Berkeley) 4/21/86";
#endif not lint

#include "timedc.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#define TSPTYPES
#include <protocols/timed.h>
#include <sys/file.h>

#ifdef	BSD2_10
#define	measure_delta	meas_delta
#define	measure_status	meas_status
#endif

int id;
int sock;
int sock_raw;
char hostname[MAXHOSTNAMELEN];
struct hostent *hp, *gethostbyname();
struct sockaddr_in server;
extern long measure_delta;
int bytenetorder(), bytehostorder();
char *strcpy();

/*
 * Clockdiff computes the difference between the time of the machine on 
 * which it is called and the time of the machines given as argument.
 * The time differences measured by clockdiff are obtained using a sequence
 * of ICMP TSTAMP messages which are returned to the sender by the IP module
 * in the remote machine.
 * In order to compare clocks of machines in different time zones, the time 
 * is transmitted (as a 32-bit value) in milliseconds since midnight UT. 
 * If a hosts uses a different time format, it should set the high order
 * bit of the 32-bit quantity it transmits.
 * However, VMS apparently transmits the time in milliseconds since midnight
 * local time (rather than GMT) without setting the high order bit. 
 * Furthermore, it does not understand daylight-saving time.  This makes 
 * clockdiff behaving inconsistently with hosts running VMS.
 *
 * In order to reduce the sensitivity to the variance of message transmission 
 * time, clockdiff sends a sequence of messages.  Yet, measures between 
 * two `distant' hosts can be affected by a small error. The error can, however,
 * be reduced by increasing the number of messages sent in each measurement.
 */

clockdiff(argc, argv)
int argc;
char *argv[];
{
	int measure_status;
	struct timeval ack;
	int measure();

	if(argc < 2)  {
		printf("Usage: clockdiff host ... \n");
		return;
	}

	id = getpid();
	(void)gethostname(hostname,sizeof(hostname));

	while (argc > 1) {
		argc--; argv++;
		hp = gethostbyname(*argv);
		if (hp == NULL) {
			printf("%s: unknown host\n", *argv);
			continue;
		}
		server.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, &(server.sin_addr.s_addr), hp->h_length); 
		ack.tv_sec = 10;
		ack.tv_usec = 0;
		if ((measure_status = measure(&ack, &server)) < 0) {
			perror("measure");
			return;
		}
		switch (measure_status) {

		case HOSTDOWN:
			printf("%s is down\n", hp->h_name);
			continue;
			break;
		case NONSTDTIME:
			printf("%s time transmitted in a non-standard format\n",						 hp->h_name);
			continue;
			break;
		case UNREACHABLE:
			printf("%s is unreachable\n", hp->h_name);
			continue;
			break;
		default:
			break;
		}

		if (measure_delta > 0)
			printf("time on %s is %D ms. ahead of time on %s\n", 
						hp->h_name, measure_delta,
						hostname);
		else
			if (measure_delta == 0)
		      		printf("%s and %s have the same time\n", 
						hp->h_name, hostname);
			else
		      	     printf("time on %s is %D ms. behind time on %s\n",
					hp->h_name, -measure_delta, hostname);
	}
	return;
}
/*
 * finds location of master timedaemon
 */

msite(argc)
int argc;
{
	int length;
	int cc;
	fd_set ready;
	struct sockaddr_in dest;
	struct timeval tout;
	struct sockaddr_in from;
	struct tsp msg;
	struct servent *srvp;

	if (argc != 1) {
		printf("Usage: msite\n");
		return;
	}

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "udp/timed: unknown service\n");
		return;
	}	
	dest.sin_port = srvp->s_port;
	dest.sin_family = AF_INET;

	(void)gethostname(hostname,sizeof(hostname));
	hp = gethostbyname(hostname);
	if (hp == NULL) {
		perror("gethostbyname");
		return;
	}
	bcopy(hp->h_addr, &dest.sin_addr.s_addr, hp->h_length);

	(void)strcpy(msg.tsp_name, hostname);
	msg.tsp_type = TSP_MSITE;
	msg.tsp_vers = TSPVERSION;
	bytenetorder(&msg);
	length = sizeof(struct sockaddr_in);
	if (sendto(sock, (char *)&msg, sizeof(struct tsp), 0, 
						&dest, length) < 0) {
		perror("sendto");
		return;
	}

	tout.tv_sec = 15;
	tout.tv_usec = 0;
	FD_ZERO(&ready);
	FD_SET(sock, &ready);
	if (select(FD_SETSIZE, &ready, (fd_set *)0, (fd_set *)0, &tout)) {
		length = sizeof(struct sockaddr_in);
		cc = recvfrom(sock, (char *)&msg, sizeof(struct tsp), 0, 
							&from, &length);
		if (cc < 0) {
			perror("recvfrom");
			return;
		}
		bytehostorder(&msg);
		if (msg.tsp_type == TSP_ACK)
			printf("master timedaemon runs on %s\n", msg.tsp_name);
		else
			printf("received wrong ack: %s\n", 
						tsptype[msg.tsp_type]);
	} else
		printf("communication error\n");
}

/*
 * quits timedc
 */

quit()
{
	exit(0);
}

#define MAXH	4	/* max no. of hosts where election can occur */

/*
 * Causes the election timer to expire on the selected hosts
 * It sends just one udp message per machine, relying on
 * reliability of communication channel.
 */

testing(argc, argv)
int argc;
char *argv[];
{
	int length;
	int nhosts;
	struct servent *srvp;
	struct sockaddr_in sin[MAXH];
	struct tsp msg;

	if(argc < 2)  {
		printf("Usage: testing host ...\n");
		return;
	}

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "udp/timed: unknown service\n");
		return;
	}	

	nhosts = 0;
	while (argc > 1) {
		argc--; argv++;
		hp = gethostbyname(*argv);
		if (hp == NULL) {
			printf("%s: unknown host %s\n", *argv);
			argc--; argv++;
			continue;
		}
		sin[nhosts].sin_port = srvp->s_port;
		sin[nhosts].sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, &(sin[nhosts].sin_addr.s_addr), hp->h_length);
		if (++nhosts == MAXH)
			break;
	}

	msg.tsp_type = TSP_TEST;
	msg.tsp_vers = TSPVERSION;
	(void)gethostname(hostname, sizeof(hostname));
	(void)strcpy(msg.tsp_name, hostname);
	bytenetorder(&msg);	/* it is not really necessary here */
	while (nhosts-- > 0) {
		length = sizeof(struct sockaddr_in);
		if (sendto(sock, (char *)&msg, sizeof(struct tsp), 0, 
						&sin[nhosts], length) < 0) {
			perror("sendto");
			return;
		}
	}
}

/*
 * Enables or disables tracing on local timedaemon
 */

tracing(argc, argv)
int argc;
char *argv[];
{
	int onflag;
	int length;
	int cc;
	fd_set ready;
	struct sockaddr_in dest;
	struct timeval tout;
	struct sockaddr_in from;
	struct tsp msg;
	struct servent *srvp;

	if (argc != 2) {
		printf("Usage: tracing { on | off }\n");
		return;
	}

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "udp/timed: unknown service\n");
		return;
	}	
	dest.sin_port = srvp->s_port;
	dest.sin_family = AF_INET;

	(void)gethostname(hostname,sizeof(hostname));
	hp = gethostbyname(hostname);
	bcopy(hp->h_addr, &dest.sin_addr.s_addr, hp->h_length);

	if (strcmp(argv[1], "on") == 0) {
		msg.tsp_type = TSP_TRACEON;
		onflag = ON;
	} else {
		msg.tsp_type = TSP_TRACEOFF;
		onflag = OFF;
	}

	(void)strcpy(msg.tsp_name, hostname);
	msg.tsp_vers = TSPVERSION;
	bytenetorder(&msg);
	length = sizeof(struct sockaddr_in);
	if (sendto(sock, (char *)&msg, sizeof(struct tsp), 0, 
						&dest, length) < 0) {
		perror("sendto");
		return;
	}

	tout.tv_sec = 5;
	tout.tv_usec = 0;
	FD_ZERO(&ready);
	FD_SET(sock, &ready);
	if (select(FD_SETSIZE, &ready, (fd_set *)0, (fd_set *)0, &tout)) {
		length = sizeof(struct sockaddr_in);
		cc = recvfrom(sock, (char *)&msg, sizeof(struct tsp), 0, 
							&from, &length);
		if (cc < 0) {
			perror("recvfrom");
			return;
		}
		bytehostorder(&msg);
		if (msg.tsp_type == TSP_ACK)
			if (onflag)
				printf("timed tracing enabled\n");
			else
				printf("timed tracing disabled\n");
		else
			printf("wrong ack received: %s\n", 
						tsptype[msg.tsp_type]);
	} else
		printf("communication error\n");
}

priv_resources()
{
	int port;
	struct sockaddr_in sin;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("opening socket");
		return(-1);
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	for (port = IPPORT_RESERVED - 1; port > IPPORT_RESERVED / 2; port--) {
		sin.sin_port = htons((u_short)port);
		if (bind(sock, (struct sockaddr *)&sin, sizeof (sin)) >= 0)
			break;
		if (errno != EADDRINUSE && errno != EADDRNOTAVAIL) {
			perror("bind");
			(void) close(sock);
			return(-1);
		}
	}
	if (port == IPPORT_RESERVED / 2) {
		fprintf(stderr, "all reserved ports in use\n");
		(void) close(sock);
		return(-1);
	}

	sock_raw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); 
	if (sock_raw < 0)  {
		perror("opening raw socket");
		(void) close(sock_raw);
		return(-1);
	}
	return(1);
}
