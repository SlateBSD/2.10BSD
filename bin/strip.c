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
static char sccsid[] = "@(#)strip.c	5.1 (Berkeley) 4/30/85";
#endif not lint

#include <a.out.h>
#include <signal.h>
#include <stdio.h>
#include <sys/file.h>

struct	exec head;
#ifdef BSD2_10
struct	ovlhdr ovlhdr;
#endif BSD2_10
int	status;
int	pagesize;

main(argc, argv)
	char *argv[];
{
	register i;

	pagesize = getpagesize();
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	for (i = 1; i < argc; i++) {
		strip(argv[i]);
		if (status > 1)
			break;
	}
	exit(status);
}

strip(name)
	char *name;
{
	register f;
	long size;

	f = open(name, O_RDWR);
	if (f < 0) {
		fprintf(stderr, "strip: "); perror(name);
		status = 1;
		goto out;
	}
	if (read(f, (char *)&head, sizeof (head)) < 0 || N_BADMAG(head)) {
		printf("strip: %s not in a.out format\n", name);
		status = 1;
		goto out;
	}
#ifdef BSD2_10
	if ((head.a_syms == 0) && ((head.a_flag & 1) != 0)) {
#else !BSD2_10
	if ((head.a_syms == 0) && (head.a_trsize == 0) && (head.a_drsize ==0)) {
#endif BSD2_10
		printf("strip: %s already stripped\n", name);
		goto out;
	}
	size = (long)head.a_text + head.a_data;
#ifdef BSD2_10
	head.a_syms = 0;
	head.a_flag |= 1;
	if (head.a_magic == A_MAGIC5 || head.a_magic == A_MAGIC6) {
		int i;
		if (read(f, (char *)&ovlhdr, sizeof (ovlhdr)) < 0) {
			printf("strip: %s not in a.out format\n", name);
			status = 1;
			goto out;
		}	
		for (i = 0; i < NOVL; i++)
			size += ovlhdr.ov_siz[i];
		size += sizeof (ovlhdr);
	}
#else !BSD2_10
	head.a_syms = head.a_trsize = head.a_drsize = 0;
	if (head.a_magic == ZMAGIC)
		size += pagesize - sizeof (head);
#endif BSD2_10
	if (ftruncate(f, size + sizeof (head)) < 0) {
		fprintf("strip: "); perror(name);
		status = 1;
		goto out;
	}
	(void) lseek(f, (long)0, L_SET);
	(void) write(f, (char *)&head, sizeof (head));
out:
	close(f);
}
