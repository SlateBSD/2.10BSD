/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_fork.c	1.1 (2.10BSD Berkeley) 12/1/86
 */

#include "param.h"
#include "../machine/reg.h"
#include "../machine/seg.h"

#include "systm.h"
#include "map.h"
#include "user.h"
#include "proc.h"
#include "inode.h"
#include "acct.h"
#include "file.h"
#include "vm.h"
#include "text.h"
#include "kernel.h"

/*
 * fork --
 *	fork system call
 */
fork()
{
	fork1(0);
}

/*
 * vfork --
 *	vfork system call, fast version of fork
 */
vfork()
{
	fork1(1);
}

fork1(isvfork)
{
	register int a;
	register struct proc *p1, *p2;

#ifdef DIAGNOSTIC
	/*
	 * Make sure there's enough swap space for max
	 * core image, thus reducing chances of running out
	 */
	if ((a = malloc(swapmap, ctod(maxmem))) == 0) {
		printf("fork1: maxmem test failed.\n");
		u.u_error = ENOMEM;
		goto out;
	}
	mfree(swapmap, ctod(maxmem), a);
#endif

	a = 0;
	if (u.u_uid != 0) {
		for (p1 = allproc; p1; p1 = p1->p_nxt)
			if (p1->p_uid == u.u_uid)
				a++;
		for (p1 = zombproc; p1; p1 = p1->p_nxt)
			if (p1->p_uid == u.u_uid)
				a++;
	}
	/*
	 * Disallow if
	 *  No processes at all;
	 *  not su and too many procs owned; or
	 *  not su and would take last slot.
	 */
	p2 = freeproc;
	if (p2==NULL)
		tablefull("proc");
	if (p2==NULL || (u.u_uid!=0 && (p2->p_nxt == NULL || a>MAXUPRC))) {
		u.u_error = EAGAIN;
		goto out;
	}
	p1 = u.u_procp;
	if (newproc(isvfork)) {
		u.u_r.r_val1 = p1->p_pid;
#ifndef BSD2_10
		u.u_r.r_val2 = 1;  /* child */
#endif
		u.u_start = time.tv_sec;
		u.u_acflag = AFORK;
		bzero(&u.u_ru, sizeof(u.u_ru));
		bzero(&u.u_cru, sizeof(u.u_cru));
		return;
	}
	u.u_r.r_val1 = p2->p_pid;

out:
#ifdef BSD2_10			/* see libc/pdp/sys/fork.s */
	u.u_ar0[R7] += NBPW;
#else
	u.u_r.r_val2 = 0;
#endif
}

/*
 * newproc --
 *	Create a new process -- the internal version of system call fork.
 *	It returns 1 in the new process, 0 in the old.
 */
newproc(isvfork)
	int isvfork;
{
	register struct proc *rpp, *rip;
	register int n;
	static int pidchecked = 0;
	struct file *fp;
	int a1, s;
	memaddr a[3];

	/*
	 * First, just locate a slot for a process
	 * and copy the useful info from this process into it.
	 * The panic "cannot happen" because fork has already
	 * checked for the existence of a slot.
	 */
	mpid++;
retry:
	if (mpid >= 30000) {
		mpid = 100;
		pidchecked = 0;
	}
	if (mpid >= pidchecked) {
		int doingzomb = 0;

		pidchecked = 30000;
		/*
		 * Scan the proc table to check whether this pid
		 * is in use.  Remember the lowest pid that's greater
		 * than mpid, so we can avoid checking for a while.
		 */
		rpp = allproc;
again:
		for (; rpp != NULL; rpp = rpp->p_nxt) {
			if (rpp->p_pid == mpid || rpp->p_pgrp == mpid) {
				mpid++;
				if (mpid >= pidchecked)
					goto retry;
			}
			if (rpp->p_pid > mpid && pidchecked > rpp->p_pid)
				pidchecked = rpp->p_pid;
			if (rpp->p_pgrp > mpid && pidchecked > rpp->p_pgrp)
				pidchecked = rpp->p_pgrp;
		}
		if (!doingzomb) {
			doingzomb = 1;
			rpp = zombproc;
			goto again;
		}
	}
	if ((rpp = freeproc) == NULL)
		panic("no procs");

	freeproc = rpp->p_nxt;			/* off freeproc */
	rpp->p_nxt = allproc;			/* onto allproc */
	rpp->p_nxt->p_prev = &rpp->p_nxt;	/*   (allproc is never NULL) */
	rpp->p_prev = &allproc;
	allproc = rpp;

	/*
	 * Make a proc table entry for the new process.
	 */
	rip = u.u_procp;
	rpp->p_stat = SIDL;
	rpp->p_realtimer.it_value = 0;
	rpp->p_flag = SLOAD | (rip->p_flag & SDETACH);
	rpp->p_uid = rip->p_uid;
	rpp->p_pgrp = rip->p_pgrp;
	rpp->p_nice = rip->p_nice;
	rpp->p_textp = rip->p_textp;
	rpp->p_pid = mpid;
	rpp->p_ppid = rip->p_pid;
	rpp->p_pptr = rip;
	rpp->p_time = 0;
	rpp->p_cpu = 0;
	rpp->p_sigmask = rip->p_sigmask;
	rpp->p_sigcatch = rip->p_sigcatch;
	rpp->p_sigignore = rip->p_sigignore;
	/* take along any pending signals like stops? */
#ifdef UCB_METER
	if (isvfork) {
		forkstat.cntvfork++;
		forkstat.sizvfork += rip->p_dsize + rip->p_ssize;
	} else {
		forkstat.cntfork++;
		forkstat.sizfork += rip->p_dsize + rip->p_ssize;
	}
#endif
	rpp->p_wchan = 0;
	rpp->p_slptime = 0;
	n = PIDHASH(rpp->p_pid);
	rpp->p_idhash = pidhash[n];
	pidhash[n] = rpp - proc;
#ifdef UCB_METER
	multprog++;
#endif

	/*
	 * Increase reference counts on shared objects.
	 */
	for (n = 0; n <= u.u_lastfile; n++) {
		fp = u.u_ofile[n];
		if (fp == NULL)
			continue;
		fp->f_count++;
	}
	if ((rip->p_textp != NULL) && !isvfork) {
		rip->p_textp->x_count++;
		rip->p_textp->x_ccount++;
	}
	u.u_cdir->i_count++;
	if (u.u_rdir)
		u.u_rdir->i_count++;

	/*
	 * When the longjmp is executed for the new process,
	 * here's where it will resume.
	 */
	if (setjmp(&u.u_ssave)) {
		sureg();
		return(1);
	}

	rpp->p_dsize = rip->p_dsize;
	rpp->p_ssize = rip->p_ssize;
	rpp->p_daddr = rip->p_daddr;
	rpp->p_saddr = rip->p_saddr;
	a1 = rip->p_addr;
	if (isvfork)
		a[2] = malloc(coremap,USIZE);
	else {
#ifdef UCB_FRCSWAP
		a[2] = NULL;
		if (idleflg)
#endif
		a[2] = malloc3(coremap, rip->p_dsize, rip->p_ssize, USIZE, a);
	}

	/*
	 * Partially simulate the environment of the new process so that
	 * when it is actually created (by copying) it will look right.
	 */
	u.u_procp = rpp;

	/*
	 * If there is not enough core for the new process, swap out the
	 * current process to generate the copy.
	 */
	if (a[2] == NULL) {
		rip->p_stat = SIDL;
		rpp->p_addr = a1;
		rpp->p_stat = SRUN;
		swapout(rpp, X_DONTFREE, X_OLDSIZE, X_OLDSIZE);
		rip->p_stat = SRUN;
		u.u_procp = rip;
	}
	else {
		/*
		 * There is core, so just copy.
		 */
		rpp->p_addr = a[2];
#ifdef CGL_RTP
		/*
		 * Copy is now a preemptable kernel process.
		 * The u. area is non-reentrant so copy it first
		 * in non-preemptable mode.
		 */
		copyu(rpp->p_addr);
#else
		copy(a1, rpp->p_addr, USIZE);
#endif
		u.u_procp = rip;
		if (isvfork == 0) {
			rpp->p_daddr = a[0];
			copy(rip->p_daddr, rpp->p_daddr, rpp->p_dsize);
			rpp->p_saddr = a[1];
			copy(rip->p_saddr, rpp->p_saddr, rpp->p_ssize);
		}
		s = splhigh();
		rpp->p_stat = SRUN;
		setrq(rpp);
		splx(s);
	}
	rpp->p_flag |= SSWAP;
	if (isvfork) {
		/*
		 *  Set the parent's sizes to 0, since the child now
		 *  has the data and stack.
		 *  (If we had to swap, just free parent resources.)
		 *  Then wait for the child to finish with it.
		 */
		if (a[2] == NULL) {
			mfree(coremap,rip->p_dsize,rip->p_daddr);
			mfree(coremap,rip->p_ssize,rip->p_saddr);
		}
		rip->p_dsize = 0;
		rip->p_ssize = 0;
		rip->p_textp = NULL;
		rpp->p_flag |= SVFORK;
		rip->p_flag |= SVFPRNT;
		while (rpp->p_flag & SVFORK)
			sleep((caddr_t)rpp,PSWP+1);
		if ((rpp->p_flag & SLOAD) == 0)
			panic("newproc vfork");
		u.u_dsize = rip->p_dsize = rpp->p_dsize;
		rip->p_daddr = rpp->p_daddr;
		rpp->p_dsize = 0;
		u.u_ssize = rip->p_ssize = rpp->p_ssize;
		rip->p_saddr = rpp->p_saddr;
		rpp->p_ssize = 0;
		rip->p_textp = rpp->p_textp;
		rpp->p_textp = NULL;
		rpp->p_flag |= SVFDONE;
		wakeup((caddr_t)rip);
		/* must do estabur if dsize/ssize are different */
		estabur(u.u_tsize,u.u_dsize,u.u_ssize,u.u_sep,RO);
		rip->p_flag &= ~SVFPRNT;
	}
	return(0);
}
