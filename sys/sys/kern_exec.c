/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_exec.c	1.1 (2.10BSD Berkeley) 12/1/86
 */

#include "param.h"
#include "../machine/reg.h"
#include "../machine/seg.h"

#include "systm.h"
#include "map.h"
#include "user.h"
#include "proc.h"
#include "buf.h"
#include "inode.h"
#include "acct.h"
#include "namei.h"
#include "fs.h"
#include "file.h"
#include "text.h"

/*
 * exec system call, with and without environments.
 */
struct execa {
	char	*fname;
	char	**argp;
	char	**envp;
};

execv()
{
	((struct execa *)u.u_ap)->envp = NULL;
	execve();
}

execve()
{
	int nc;
	register char *cp;
	register struct buf *bp;
	struct execa *uap;
	int na, ne, ucp, ap;
	register int cc;
	unsigned len;
	int indir, uid, gid;
	char *sharg;
	struct inode *ip;
	memaddr bno;
	char cfname[MAXCOMLEN + 1];
#define	SHSIZE	32
	char cfarg[SHSIZE];
	union {
		char	ex_shell[SHSIZE];	/* #! and name of interpreter */
		struct	exec ex_exec;
	} exdata;
	int error;

	u.u_segflg = UIO_USERSPACE;
	u.u_dirp = ((struct execa *)u.u_ap)->fname;
	if ((ip = namei(LOOKUP | FOLLOW)) == NULL)
		return;
	bno = 0;
	bp = 0;
	indir = 0;
	uid = u.u_uid;
	gid = u.u_gid;
	if (ip->i_mode & ISUID)
		uid = ip->i_uid;
	if (ip->i_mode & ISGID)
		gid = ip->i_gid;

  again:
	if (access(ip, IEXEC))
		goto bad;
	if ((u.u_procp->p_flag&STRC) && access(ip, IREAD))
		goto bad;
	if ((ip->i_mode & IFMT) != IFREG ||
	    (ip->i_mode & (IEXEC|(IEXEC>>3)|(IEXEC>>6))) == 0) {
		u.u_error = EACCES;
		goto bad;
	}

	/*
	 * Read in first few bytes of file for segment sizes, magic number:
	 *	407 = plain executable
	 *	410 = RO text
	 *	411 = separated I/D
	 *	405 = text overlay
	 *	430 = auto-overlay (nonseparate)
	 *	431 = auto-overlay (separate)
	 * Also an ASCII line beginning with #! is
	 * the file name of a ``shell'' and arguments may be prepended
	 * to the argument list if given here.
	 *
	 * SHELL NAMES ARE LIMITED IN LENGTH.
	 *
	 * ONLY ONE ARGUMENT MAY BE PASSED TO THE SHELL FROM
	 * THE ASCII LINE.
	 */
	exdata.ex_shell[0] = '\0';	/* for zero length files */
	u.u_base = (caddr_t)&exdata;
	u.u_count = sizeof(exdata);
	u.u_offset = 0;
	u.u_segflg = UIO_SYSSPACE;
	readi(ip);
	u.u_segflg = UIO_USERSPACE;
	if (u.u_error)
		goto bad;
	if (u.u_count > sizeof(exdata) - sizeof(exdata.ex_exec) &&
	    exdata.ex_shell[0] != '#') {
		u.u_error = ENOEXEC;
		goto bad;
	}

	switch((int)exdata.ex_exec.a_magic) {

	case A_MAGIC1:
	case A_MAGIC2:
	case A_MAGIC3:
	case A_MAGIC4:
	case A_MAGIC5:
	case A_MAGIC6:
		break;

	default:
		if (exdata.ex_shell[0] != '#' ||
		    exdata.ex_shell[1] != '!' ||
		    indir) {
			u.u_error = ENOEXEC;
			goto bad;
		}
		cp = &exdata.ex_shell[2];		/* skip "#!" */
		while (cp < &exdata.ex_shell[SHSIZE]) {
			if (*cp == '\t')
				*cp = ' ';
			else if (*cp == '\n') {
				*cp = '\0';
				break;
			}
			cp++;
		}
		if (*cp != '\0') {
			u.u_error = ENOEXEC;
			goto bad;
		}
		cp = &exdata.ex_shell[2];
		while (*cp == ' ')
			cp++;
		u.u_dirp = cp;
		while (*cp && *cp != ' ')
			cp++;
		cfarg[0] = '\0';
		if (*cp) {
			*cp++ = '\0';
			while (*cp == ' ')
				cp++;
			if (*cp)
				bcopy((caddr_t)cp, (caddr_t)cfarg, SHSIZE);
		}
		indir = 1;
		iput(ip);
		u.u_segflg = UIO_SYSSPACE;
		ip = namei(LOOKUP | FOLLOW);
		u.u_segflg = UIO_USERSPACE;
		if (ip == NULL)
			return;
		bcopy((caddr_t)u.u_dent.d_name, (caddr_t)cfname, MAXCOMLEN);
		cfname[MAXCOMLEN] = '\0';
		goto again;
	}

	/*
	 * Collect arguments on "file" in swap space.
	 */
	na = 0;
	ne = 0;
	nc = 0;
	cc = 0;
	uap = (struct execa *)u.u_ap;
	bno = malloc(swapmap, ctod((int)btoc(NCARGS + MAXBSIZE)));
	if (bno == 0) {
		swkill(u.u_procp, "exec: no swap space");
		goto bad;
	}
	/*
	 * Copy arguments into file in argdev area.
	 */
	if (uap->argp) for (;;) {
		ap = NULL;
		sharg = NULL;
		if (indir && na == 0) {
			sharg = cfname;
			ap = (int)sharg;
			uap->argp++;		/* ignore argv[0] */
		} else if (indir && (na == 1 && cfarg[0])) {
			sharg = cfarg;
			ap = (int)sharg;
		} else if (indir && (na == 1 || na == 2 && cfarg[0]))
			ap = (int)uap->fname;
		else if (uap->argp) {
			ap = fuword((caddr_t)uap->argp);
			uap->argp++;
		}
		if (ap == NULL && uap->envp) {
			uap->argp = NULL;
			if ((ap = fuword((caddr_t)uap->envp)) != NULL)
				uap->envp++, ne++;
		}
		if (ap == NULL)
			break;
		na++;
		if (ap == -1) {
			u.u_error = EFAULT;
			break;
		}
		do {
			if (cc <= 0) {
				/*
				 * We depend on NCARGS being a multiple of
				 * CLSIZE*NBPG.  This way we need only check
				 * overflow before each buffer allocation.
				 */
				if (nc >= NCARGS-1) {
					error = E2BIG;
					break;
				}
				if (bp) {
					mapout(bp);
					bdwrite(bp);
				}
				cc = CLSIZE*NBPG;
				bp = getblk(swapdev, dbtofsb(clrnd(swplo + bno)) + lblkno(nc));
				cp = mapin(bp);
			}
			if (sharg) {
				error = copystr(sharg, cp, (unsigned)cc, &len);
				sharg += len;
			} else {
				error = copyinstr((caddr_t)ap, cp, (unsigned)cc,
				    &len);
				ap += len;
			}
			cp += len;
			nc += len;
			cc -= len;
		} while (error == ENOENT);
		if (error) {
			u.u_error = error;
			if (bp) {
				mapout(bp);
				bp->b_flags |= B_AGE;
				bp->b_flags &= ~B_DELWRI;
				brelse(bp);
			}
			bp = 0;
			goto badarg;
		}
	}
	if (bp) {
		mapout(bp);
		bdwrite(bp);
	}
	bp = 0;
	nc = (nc + NBPW-1) & ~(NBPW-1);
	getxfile(ip, &exdata.ex_exec, nc + (na+4)*NBPW, uid, gid);
	if (u.u_error) {
badarg:
		for (cc = 0;cc < nc; cc += CLSIZE * NBPG) {
			daddr_t blkno;

			blkno = dbtofsb(clrnd(swplo + bno)) + lblkno(cc);
			if (incore(swapdev,blkno)) {
				bp = bread(swapdev,blkno);
				bp->b_flags |= B_AGE;		/* throw away */
				bp->b_flags &= ~B_DELWRI;	/* cancel io */
				brelse(bp);
				bp = 0;
			}
		}
		goto bad;
	}
	iput(ip);
	ip = NULL;

	/*
	 * Copy back arglist.
	 */
	ucp = -nc - NBPW;
	ap = ucp - na*NBPW - 3*NBPW;
	u.u_ar0[R6] = ap;
	(void) suword((caddr_t)ap, na-ne);
	nc = 0;
	cc = 0;
	for (;;) {
		ap += NBPW;
		if (na == ne) {
			(void) suword((caddr_t)ap, 0);
			ap += NBPW;
		}
		if (--na < 0)
			break;
		(void) suword((caddr_t)ap, ucp);
		do {
			if (cc <= 0) {
				if (bp) {
					mapout(bp);
					brelse(bp);
				}
				cc = CLSIZE*NBPG;
				bp = bread(swapdev,
				    dbtofsb(clrnd(swplo + bno)) + lblkno(nc));
				bp->b_flags |= B_AGE;		/* throw away */
				bp->b_flags &= ~B_DELWRI;	/* cancel io */
				cp = mapin(bp);
			}
			error = copyoutstr(cp, (caddr_t)ucp, (unsigned)cc,
			    &len);
			ucp += len;
			cp += len;
			nc += len;
			cc -= len;
		} while (error == ENOENT);
		if (error == EFAULT)
			panic("exec: EFAULT");
	}
	(void) suword((caddr_t)ap, 0);
	(void) suword((caddr_t)(-NBPW), 0);
	if (bp) {
		mapout(bp);
		bp->b_flags |= B_AGE;
		brelse(bp);
		bp = NULL;
	}
	execve1();
	while (u.u_lastfile >= 0 && u.u_ofile[u.u_lastfile] == NULL)
		u.u_lastfile--;

	/*
	 * inline expansion of setregs(), found
	 * in ../pdp/machdep.c
	 *
	 * setregs(exdata.ex_exec.a_entry);
	 */
	u.u_ar0[PC] = exdata.ex_exec.a_entry & ~01;
#ifndef NONFP
	u.u_fps.u_fpsr = 0;
#endif

	/*
	 * Remember file name for accounting.
	 */
	u.u_acflag &= ~AFORK;
	if (indir)
		bcopy((caddr_t)cfname, (caddr_t)u.u_comm, MAXCOMLEN);
	else
		bcopy((caddr_t)u.u_dent.d_name, (caddr_t)u.u_comm, MAXCOMLEN);
bad:
	if (bp) {
		mapout(bp);
		bp->b_flags |= B_AGE;
		brelse(bp);
	}
	if (bno)
		mfree(swapmap, ctod((int)btoc(NCARGS + MAXBSIZE)), bno);
	if (ip)
		iput(ip);
}

/*
 * execve1 is a helper routine to speed up execve.
 */
static
execve1()
{
	/*
	 * 4.3 source
	 *
	 * Reset caught signals.  Held signals
	 * remain held through p_sigmask.
	 *
	 * In an effort to avoid exspensive long masking operations
	 * we use two integer loops rather than one long loop.
	 * The constant 16 is the number of bits per int.
	 *
	 *	while (u.u_procp->p_sigcatch) {
	 *		nc = ffs((long)u.u_procp->p_sigcatch);
	 *		u.u_procp->p_sigcatch &= ~sigmask(nc);
	 *		u.u_signal[nc] = SIG_DFL;
	 *	}
	 */
{
	register int cnt, imask;
	register int (**sigp)();
	long mask;

	mask = u.u_procp->p_sigcatch;
	u.u_procp->p_sigcatch = 0;
	sigp = &u.u_signal[1];
	for (cnt = 16, imask = loint(mask); cnt > 0; cnt--, imask >>= 1, sigp++)
		if (imask&1)
			*sigp = SIG_DFL;
	for (cnt = NSIG-16, imask = hiint(mask); cnt > 0; cnt--, imask >>= 1, sigp++)
		if (imask&1)
			*sigp = SIG_DFL;
}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	u.u_onstack = 0;
	u.u_sigsp = 0;
	u.u_sigonstack = 0;

	/*
	 *	for (nc = u.u_lastfile; nc >= 0; --nc) {
	 *		if (u.u_pofile[nc] & UF_EXCLOSE) {
	 *			closef(u.u_ofile[nc]);
	 *			u.u_ofile[nc] = NULL;
	 *			u.u_pofile[nc] = 0;
	 *		}
	 *		u.u_pofile[nc] &= ~UF_MAPPED;
	 *	}
	 */
{
	register int cnt;
	register struct file **ofilep = u.u_ofile;
	register char *pofilep = u.u_pofile;

	for (cnt = u.u_lastfile;cnt >= 0; cnt--, ofilep++, pofilep++)
		if (*pofilep & UF_EXCLOSE) {
			closef(*ofilep,1);
			*ofilep = NULL;
			*pofilep = 0;
		}
}
}

/*
 * Read in and set up memory for executed file.
 * u.u_error set on error
 */
getxfile(ip, ep, nargc, uid, gid)
	struct inode *ip;
	register struct exec *ep;
	int nargc, uid, gid;
{
	struct u_ovd sovdata;
	long lsize;
	u_int ds, ts, ss;
	u_int ovhead[NOVL + 1];
	int sep, overlay, ovflag, ovmax;

	overlay = sep = ovflag = 0;
	switch(ep->a_magic) {
		case A_MAGIC1:
			lsize = (long)ep->a_data + ep->a_text;
			ep->a_data = (u_int)lsize;
			if (lsize != ep->a_data) {	/* check overflow */
				u.u_error = ENOMEM;
				return;
			}
			ep->a_text = 0;
			break;
		case A_MAGIC3:
			sep++;
			break;
		case A_MAGIC4:
			overlay++;
			break;
		case A_MAGIC5:
			ovflag++;
			break;
		case A_MAGIC6:
			sep++;
			ovflag++;
			break;
	}

	if (ip->i_text && (ip->i_text->x_flag & XTRC)) {
		u.u_error = ETXTBSY;
		return;
	}
	if (ep->a_text != 0 && (ip->i_flag&ITEXT) == 0 &&
	    ip->i_count != 1) {
		register struct file *fp;

		for (fp = file; fp < fileNFILE; fp++) {
			if (fp->f_type == DTYPE_INODE &&
			    fp->f_count > 0 &&
			    (struct inode *)fp->f_data == ip &&
			    (fp->f_flag&FWRITE)) {
				u.u_error = ETXTBSY;
				return;
			}
		}
	}

	/*
	 * find text and data sizes try; them out for possible
	 * overflow of max sizes
	 */
	ts = btoc(ep->a_text);
	lsize = (long)ep->a_data + ep->a_bss;
	if (lsize != (u_int)lsize) {
		u.u_error = ENOMEM;
		return;
	}
	ds = btoc(lsize);
	ss = SSIZE + btoc(nargc);

	/*
	 * if auto overlay get second header
	 */
	sovdata = u.u_ovdata;
	u.u_ovdata.uo_ovbase = 0;
	u.u_ovdata.uo_curov = 0;
	if (ovflag) {
		u.u_base = (caddr_t)ovhead;
		u.u_count = sizeof(ovhead);
		u.u_offset = sizeof(struct exec);
		u.u_segflg = UIO_SYSSPACE;
		readi(ip);
		u.u_segflg = UIO_USERSPACE;
		if (u.u_count != 0)
			u.u_error = ENOEXEC;
		if (u.u_error) {
			u.u_ovdata = sovdata;
			return;
		}
		/* set beginning of overlay segment */
		u.u_ovdata.uo_ovbase = ctos(ts);

		/* 0th entry is max size of the overlays */
		ovmax = btoc(ovhead[0]);

		/* set max number of segm. registers to be used */
		u.u_ovdata.uo_nseg = ctos(ovmax);

		/* set base of data space */
		u.u_ovdata.uo_dbase = stoc(u.u_ovdata.uo_ovbase +
		    u.u_ovdata.uo_nseg);

		/*
		 * Set up a table of offsets to each of the overlay
		 * segements. The ith overlay runs from ov_offst[i-1]
		 * to ov_offst[i].
		 */
		u.u_ovdata.uo_ov_offst[0] = ts;
		{
			register int t, i;

			/* check if any overlay is larger than ovmax */
			for (i = 1; i <= NOVL; i++) {
				if ((t = btoc(ovhead[i])) > ovmax) {
					u.u_error = ENOEXEC;
					u.u_ovdata = sovdata;
					return;
				}
				u.u_ovdata.uo_ov_offst[i] =
					t + u.u_ovdata.uo_ov_offst[i - 1];
			}
		}
	}
	if (overlay) {
		if (u.u_sep == 0 && ctos(ts) != ctos(u.u_tsize) || nargc) {
			u.u_error = ENOMEM;
			return;
		}
		ds = u.u_dsize;
		ss = u.u_ssize;
		sep = u.u_sep;
		xfree();
		xalloc(ip,ep);
		u.u_ar0[PC] = ep->a_entry & ~01;
	} else {
		if (estabur(ts, ds, ss, sep, RO)) {
			u.u_ovdata = sovdata;
			return;
		}

		/*
		 * allocate and clear core at this point, committed
		 * to the new image
		 */
		u.u_prof.pr_scale = 0;
		if (u.u_procp->p_flag & SVFORK)
			endvfork();
		else
			xfree();
		expand(ds, S_DATA);
		{
			register u_int numc, startc;

			startc = btoc(ep->a_data);	/* clear BSS only */
			if (startc != 0)
				startc--;
			numc = ds - startc;
			clear(u.u_procp->p_daddr + startc, numc);
		}
		expand(ss, S_STACK);
		clear(u.u_procp->p_saddr, ss);
		xalloc(ip, ep);

		/*
		 * read in data segment
		 */
		estabur((u_int)0, ds, (u_int)0, 0, RO);
		u.u_base = 0;
		u.u_offset = sizeof(struct exec);
		if (ovflag) {
			u.u_offset += sizeof(ovhead);
			u.u_offset += (((long)u.u_ovdata.uo_ov_offst[NOVL]) << 6);
		}
		else
			u.u_offset += ep->a_text;
		u.u_count = ep->a_data;
		readi(ip);

		/*
		 * set SUID/SGID protections, if no tracing
		 */
		if ((u.u_procp->p_flag&STRC)==0) {
			u.u_uid = uid;
			u.u_procp->p_uid = uid;
			u.u_gid = gid;
		} else
			psignal(u.u_procp, SIGTRAP);
	}
	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = ss;
	u.u_sep = sep;
	estabur(ts, ds, ss, sep, RO);
}
