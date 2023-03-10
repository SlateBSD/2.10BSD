/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ufs_subr.c	1.1 (2.10BSD Berkeley) 12/1/86
 */

#include "param.h"
#include "../machine/seg.h"

#include "user.h"
#include "proc.h"
#include "fs.h"
#include "inode.h"
#include "buf.h"
#include "mount.h"
#include "kernel.h"
#include "systm.h"

/*
 * Update is the internal name of 'sync'.  It goes through the disk
 * queues to initiate sandbagged IO; goes through the inodes to write
 * modified nodes; and it goes through the mount table to initiate
 * the writing of the modified super blocks.
 */
update()
{
	register struct mount *mp;
	register struct buf *bp;
	struct fs *fsdst;

	if (updlock)
		return;
	updlock++;
	/*
	 * Write back modified superblocks.
	 * Consistency check that the superblock
	 * of each file system is still in the buffer cache.
	 */
	for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++) {
		register struct fs *fs;

		if (mp->m_inodp == NULL || mp->m_dev == NODEV)
			continue;
		fs = &mp->m_filsys;
		if (fs->fs_fmod == 0)
			continue;
		if (fs->fs_ronly != 0) {		/* XXX */
			printf("fs = %s\n", fs->fs_fsmnt);
			panic("update: rofs mod");
		}
		if (fs->fs_ilock || fs->fs_flock)
			continue;
		bp = getblk(mp->m_dev, SUPERB);
		if (bp->b_flags & B_ERROR)
			continue;
		fs->fs_fmod = 0;
		fs->fs_time = time.tv_sec;
		fsdst = (struct fs *)mapin(bp);
		*fsdst = *fs;
		mapout(bp);
		bwrite(bp);
	}
	/*
	 * Write back each (modified) inode.
	 */
	{
		register struct inode *ip;

		for (ip = inode; ip < inodeNINODE; ip++) {
			if ((ip->i_flag & ILOCKED) != 0 || ip->i_count == 0 ||
			    (ip->i_flag & (IMOD|IACC|IUPD|ICHG)) == 0)
				continue;
			ip->i_flag |= ILOCKED;
			ip->i_count++;
			iupdat(ip, &time, &time, 0);
			iput(ip);
		}
	}
	updlock = 0;
	/*
	 * Force stale buffer cache information to be flushed,
	 * for all devices.
	 */
	bflush(NODEV);
}

/*
 * Flush all the blocks associated with an inode.
 * There are two strategies based on the size of the file;
 * large files are those with more than (nbuf / 2) blocks.
 * Large files
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the device on which the file resides.
 * Small files
 *	Look up each block in the file to see if it is in the 
 *	buffer pool writing any that are found to disk.
 *	Note that we make a more stringent check of
 *	writing out any block in the buffer pool that may
 *	overlap the inode. This brings the inode up to
 *	date with recent mods to the cooked device.
 */
syncip(ip)
	struct inode *ip;
{
	register struct buf *bp;
	register struct buf *lastbufp;
	long lbn, lastlbn;
	register int s;
	daddr_t blkno;

	lastlbn = howmany(ip->i_size, DEV_BSIZE);
	if (lastlbn < NBUF / 2) {
		for (lbn = 0; lbn < lastlbn; lbn++) {
			blkno = fsbtodb(bmap(ip, lbn, B_READ, 0));
			blkflush(ip->i_dev, blkno);
		}
	} else {
		lastbufp = &buf[NBUF];
		for (bp = buf; bp < lastbufp; bp++) {
			if (bp->b_dev != ip->i_dev ||
			    (bp->b_flags & B_DELWRI) == 0)
				continue;
			s = splbio();
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				sleep((caddr_t)bp, PRIBIO+1);
				splx(s);
				bp--;
				continue;
			}
			splx(s);
			notavail(bp);
			bwrite(bp);
		}
	}
	ip->i_flag |= ICHG;
	iupdat(ip, &time, &time, 1);
}

/*
 * Check that a specified block number is in range.
 */
badblock(fp, bn)
	register struct fs *fp;
	daddr_t bn;
{

	if (bn < fp->fs_isize || bn >= fp->fs_fsize) {
		printf("bad block %D, ",bn);
		fserr(fp, "bad block");
		return (1);
	}
	return (0);
}

/*
 * Getfs maps a device number into a pointer to the incore super block.
 *
 * The algorithm is a linear search through the mount table. A
 * consistency check of the super block magic number is performed.
 *
 * panic: no fs -- the device is not mounted.
 *	this "cannot happen"
 */
struct fs *
getfs(dev)
	dev_t dev;
{
	register struct mount *mp;
	register struct fs *fs;

	for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++) {
		if (mp->m_inodp == NULL || mp->m_dev != dev)
			continue;
		fs = &mp->m_filsys;
		if (fs->fs_nfree > NICFREE || fs->fs_ninode > NICINOD) {
			fserr(fs, "bad count");
			fs->fs_nfree = fs->fs_ninode = 0;
		}
		return(fs);
	}
	printf("no fs on dev %u/%u\n",major(dev), minor(dev));
	return((struct fs *) NULL);
}

scanc(size, cp, table, mask)
	u_int size;
	register u_char *cp, table[];
	register u_char mask;
{
	register u_char *end = &cp[size];

	while (cp < end && (table[*cp] & mask) == 0)
		cp++;
	return (end - cp);
}

locc(mask, size, cp)
	register u_char mask;
	u_int size;
	register u_char *cp;
{
	register u_char *end = &cp[size];

	while (cp < end && *cp != mask)
		cp++;
	return (end - cp);
}
