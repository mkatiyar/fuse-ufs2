/**
 * Copyright (c) 2013 Manish Katiyar <mkatiyar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the fuse-ufs
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "fuse-ufs.h"
#include <sys/param.h>

static ino_t ufs_dirpref(struct inode *pip);

ufs2_daddr_t
ufs_inode_alloc(struct inode *ip, int cg,
		ufs2_daddr_t ipref, int mode)
{
	struct cg *cgp;
	u_int8_t *inosused;
	struct ufs2_dinode *dp2;
	int error, start, len, loc, map, i;
	char *buf = NULL, *ibp = NULL;
	uufsd_t *ufs = (uufsd_t *)ip->i_dev;
	struct fs *fs = &ufs->d_fs;
	int ret;
	char *locptr;

	if (fs->fs_cs(fs, cg).cs_nifree == 0)
		return (0);

	error = ufs_get_mem(fs->fs_cgsize, &buf);
	if (error) {
		return -ENOMEM;
	}

	error = blkread(ufs, fsbtodb(fs, cgtod(fs, cg)), buf, (int)fs->fs_cgsize);
	if (error == -1) {
		ret = -EIO;
		goto out;
	}

	cgp = (struct cg *)buf;
	if (!cg_chkmagic(cgp) || cgp->cg_cs.cs_nifree == 0) {
		ret = -EIO;
		goto out;
	}

	cgp->cg_old_time = cgp->cg_time = time(NULL);
	inosused = cg_inosused(cgp);
	if (ipref) {
		ipref %= fs->fs_ipg;
		if (isclr(inosused, ipref))
			goto gotit;
	}
	start = cgp->cg_irotor / NBBY;
	len = howmany(fs->fs_ipg - cgp->cg_irotor, NBBY);
	locptr = memchr((void *)&inosused[start], 0, len);
	loc = (char *)locptr - (char *)&inosused[start];
	if (!locptr) {
		start = 0;
		int i = start;
		for (; i < len; i++) {
			if (inosused[i] == 0xff) {
				continue;
			}
			locptr = (char *)&inosused[i];
			break;
		}
	//	len = start + 1;
	//	locptr = memchr((void *)&inosused[0], 0, len);
		loc = (char *)locptr - (char *)&inosused[0];
		if (!locptr) {
			debugf("cg = %d, irotor = %ld, fs = %s\n",
			    cg, (long)cgp->cg_irotor, fs->fs_fsmnt);
			debugferr("ufs_nodealloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	//i = start + len - loc;
	//i = start + loc;
	i = start + loc;
	map = inosused[i];
	ipref = i * NBBY;
	for (i = 1; i < (1 << NBBY); i <<= 1, ipref++) {
		if ((map & i) == 0) {
			cgp->cg_irotor = ipref;
			goto gotit;
		}
	}
	debugferr("ufs_nodealloccg: block not in map");
	/* NOTREACHED */
gotit:
	/*
	 * Check to see if we need to initialize more inodes.
	 */
	ibp = NULL;
	if (fs->fs_magic == FS_UFS2_MAGIC &&
	    ipref + INOPB(fs) > cgp->cg_initediblk &&
	    cgp->cg_initediblk < cgp->cg_niblk) {
		error = ufs_get_mem(fs->fs_bsize, &ibp);
		if (error) {
			ret = -ENOMEM;
			goto out;
		}
		error = blkread(ufs, fsbtodb(fs,
		    ino_to_fsba(fs, cg * fs->fs_ipg + cgp->cg_initediblk)),
		    ibp, (int)fs->fs_bsize);
		if (error == -1) {
			ret = -EIO;
			goto out;
		}
		bzero(ibp, (int)fs->fs_bsize);
		dp2 = (struct ufs2_dinode *)ibp;
		for (i = 0; i < INOPB(fs); i++) {
			dp2->di_gen = rand() / 2 + 1;
			dp2++;
		}
		cgp->cg_initediblk += INOPB(fs);
	}
	setbit(inosused, ipref);
	cgp->cg_cs.cs_nifree--;
	fs->fs_cstotal.cs_nifree--;
	fs->fs_cs(fs, cg).cs_nifree--;
	fs->fs_fmod = 1;
	if (IFTODT(mode) == DT_DIR) {
		cgp->cg_cs.cs_ndir++;
		fs->fs_cstotal.cs_ndir++;
		fs->fs_cs(fs, cg).cs_ndir++;
	}
	error = blkwrite(ufs, fsbtodb(fs, cgtod(fs, cg)), buf, (int)fs->fs_cgsize);
	if (error == -1) {
		ret = -EIO;
		goto out;
	}
	if (ibp != NULL) {
		error = blkwrite(ufs, fsbtodb(fs,
		    ino_to_fsba(fs, cg * fs->fs_ipg + cgp->cg_initediblk)),
		    ibp, (int)fs->fs_bsize);
		if (error == -1) {
			ret = -EIO;
			goto out;
		}
	}

	ret = (cg * fs->fs_ipg + ipref);
out:
	if (buf) {
		ufs_free_mem(&buf);
		ufs_free_mem(&ibp);
	}
	return ret;
}

int
ufs_valloc(
	struct ufs_vnode *pvp,
	int mode,
	struct ufs_vnode **vnodepp)
{
	struct inode *pip;
	struct fs *fs;
	struct inode *ip;
	//struct timespec ts;
	ino_t ino, ipref;
	int cg, error = 0;
	uufsd_t *ufs;

	*vnodepp = NULL;
	pip = vnode2inode(pvp);
	fs = pip->i_fs;
	ufs = (uufsd_t *)pip->i_dev;

	if (fs->fs_cstotal.cs_nifree == 0)
		goto noinodes;

	if (IFTODT(mode) == DT_DIR)
		ipref = ufs_dirpref(pip);
	else
		ipref = pip->i_number;

	if (ipref >= fs->fs_ncg * fs->fs_ipg)
		ipref = 0;
	cg = ino_to_cg(fs, ipref);
	/*
	 * Track number of dirs created one after another
	 * in a same cg without intervening by files.
	 */
	if ((mode & IFMT) == IFDIR) {
		if (fs->fs_contigdirs[cg] < 255)
			fs->fs_contigdirs[cg]++;
	} else {
		if (fs->fs_contigdirs[cg] > 0)
			fs->fs_contigdirs[cg]--;
	}
	ino = (ino_t)ufs_hashalloc(pip, cg, ipref, mode, ufs_inode_alloc);
	if ((int)ino <= 0)
		goto noinodes;
	*vnodepp = vnode_get(ufs, ino);
	if (!*vnodepp) {
		return (error);
	}
	ip = vnode2inode(*vnodepp);
	if (ip->i_mode) {
		debugf("mode = 0%o, inum = %lu\n",
		    ip->i_mode, (u_long)ip->i_number);
		debugferr("ufs_valloc: dup alloc");
	}

	ip->i_blocks = 0;
	ip->i_flags = 0;
	ip->i_size = 0;

	/*
	 * Set up a new generation number for this inode.
	 */
	if (ip->i_gen == 0 || ++ip->i_gen == 0)
		ip->i_gen = rand() / 2 + 1;

	/*
	if (fs->fs_magic == FS_UFS2_MAGIC) {
		vfs_timestamp(&ts);
		ip->i_din2->di_birthtime = ts.tv_sec;
		ip->i_din2->di_birthnsec = ts.tv_nsec;
	}
	*/
	ip->i_flag = 0;
//	vnode_put(*vnodepp, 1);

	return (0);
noinodes:
	return (-ENOSPC);
}


static ino_t ufs_dirpref(struct inode *pip)
{
	struct fs *fs;
	int cg, prefcg, dirsize, cgsize;
	int avgifree, avgbfree, avgndir, curdirsize;
	int minifree, minbfree, maxndir;
	int maxcontigdirs;

	fs = pip->i_fs;

	avgifree = fs->fs_cstotal.cs_nifree / fs->fs_ncg;
	avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
	avgndir = fs->fs_cstotal.cs_ndir / fs->fs_ncg;

	/*
	 * Force allocation in another cg if creating a first level dir.
	if (ITOV(pip)->v_vflag & VV_ROOT) {
		prefcg = arc4random() % fs->fs_ncg;
		mincg = prefcg;
		minndir = fs->fs_ipg;
		for (cg = prefcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		for (cg = 0; cg < prefcg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		return ((ino_t)(fs->fs_ipg * mincg));
	}
	 */

	/*
	 * Count various limits which used for
	 * optimal allocation of a directory inode.
	 */
	maxndir = MIN(avgndir + fs->fs_ipg / 16, fs->fs_ipg);
	minifree = avgifree - avgifree / 4;
	if (minifree < 1)
		minifree = 1;
	minbfree = avgbfree - avgbfree / 4;
	if (minbfree < 1)
		minbfree = 1;
	cgsize = fs->fs_fsize * fs->fs_fpg;
	dirsize = fs->fs_avgfilesize * fs->fs_avgfpdir;
	curdirsize = avgndir ? (cgsize - avgbfree * fs->fs_bsize) / avgndir : 0;
	if (dirsize < curdirsize)
		dirsize = curdirsize;
	if (dirsize <= 0)
		maxcontigdirs = 0;		/* dirsize overflowed */
	else
		maxcontigdirs = MIN((avgbfree * fs->fs_bsize) / dirsize, 255);
	if (fs->fs_avgfpdir > 0)
		maxcontigdirs = MIN(maxcontigdirs,
				    fs->fs_ipg / fs->fs_avgfpdir);
	if (maxcontigdirs == 0)
		maxcontigdirs = 1;

	/*
	 * Limit number of dirs in one cg and reserve space for 
	 * regular files, but only if we have no deficit in
	 * inodes or space.
	 */
	prefcg = ino_to_cg(fs, pip->i_number);
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
	    	    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				return ((ino_t)(fs->fs_ipg * cg));
		}
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
	    	    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				return ((ino_t)(fs->fs_ipg * cg));
		}
	/*
	 * This is a backstop when we have deficit in space.
	 */
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			return ((ino_t)(fs->fs_ipg * cg));
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			break;
	return ((ino_t)(fs->fs_ipg * cg));
}

