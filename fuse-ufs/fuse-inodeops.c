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



static int scanc(int size, u_char *cp, u_char table[], int mask0)
{
	const u_char *end;
	u_char mask;
	mask = mask0;
	for (end = &cp[size]; cp < end; ++cp) {
		if (table[*cp] & mask)
			break;
	}
	return (end - cp);
}

int
ffs_isblock(fs, cp, h)
	struct fs *fs;
	unsigned char *cp;
	ufs1_daddr_t h;
{
	unsigned char mask;

	switch ((int)fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
		debugf("ffs_isblock");
	}
	return (0);
}

static int
ffs_isfreeblock(struct fs *fs, u_char *cp, ufs1_daddr_t h)
{

	switch ((int)fs->fs_frag) {
	case 8:
		return (cp[h] == 0);
	case 4:
		return ((cp[h >> 1] & (0x0f << ((h & 0x1) << 2))) == 0);
	case 2:
		return ((cp[h >> 2] & (0x03 << ((h & 0x3) << 1))) == 0);
	case 1:
		return ((cp[h >> 3] & (0x01 << (h & 0x7))) == 0);
	default:
		debugferr("ffs_isfreeblock");
	}
	return (0);
}

/*
 * take a block out of the map
 */
void
ffs_clrblock(fs, cp, h)
	struct fs *fs;
	u_char *cp;
	ufs1_daddr_t h;
{

	switch ((int)fs->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
		debugf("ffs_clrblock");
	}
}

void
ffs_setblock(fs, cp, h)
	struct fs *fs;
	unsigned char *cp;
	ufs1_daddr_t h;
{

	switch ((int)fs->fs_frag) {

	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
		debugferr("ffs_setblock");
	}
}

static ufs1_daddr_t
ffs_mapsearch(fs, cgp, bpref, allocsiz)
	struct fs *fs;
	struct cg *cgp;
	ufs2_daddr_t bpref;
	int allocsiz;
{
	ufs1_daddr_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;
	u_int8_t *blksfree;

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = cgp->cg_frotor / NBBY;
	blksfree = cg_blksfree(cgp);
	len = howmany(fs->fs_fpg, NBBY) - start;
	loc = scanc((u_int)len, (u_char *)&blksfree[start],
		fragtbl[fs->fs_frag],
		(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((u_int)len, (u_char *)&blksfree[0],
			fragtbl[fs->fs_frag],
			(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
		if (loc == 0) {
			printf("start = %d, len = %d, fs = %s\n",
			    start, len, fs->fs_fsmnt);
			debugf("ffs_alloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = bno;
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, blksfree, bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return (bno + pos);
			field <<= 1;
			subfield <<= 1;
		}
	}
	printf("bno = %lu, fs = %s\n", (u_long)bno, fs->fs_fsmnt);
	debugf("ffs_alloccg: block not in map");
	return (-1);
}

void
ffs_clusteracct(struct fs *fs, struct cg *cgp, ufs1_daddr_t blkno, int cnt)
{
	int32_t *sump;
	int32_t *lp;
	u_char *freemapp, *mapp;
	int i, start, end, forw, back, map, bit;

	if (fs->fs_contigsumsize <= 0)
		return;
	freemapp = cg_clustersfree(cgp);
	sump = cg_clustersum(cgp);
	/*
	 * Allocate or clear the actual block.
	 */
	if (cnt > 0)
		setbit(freemapp, blkno);
	else
		clrbit(freemapp, blkno);
	/*
	 * Find the size of the cluster going forward.
	 */
	start = blkno + 1;
	end = start + fs->fs_contigsumsize;
	if (end >= cgp->cg_nclusterblks)
		end = cgp->cg_nclusterblks;
	mapp = &freemapp[start / NBBY];
	map = *mapp++;
	bit = 1 << (start % NBBY);
	for (i = start; i < end; i++) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != (NBBY - 1)) {
			bit <<= 1;
		} else {
			map = *mapp++;
			bit = 1;
		}
	}
	forw = i - start;
	/*
	 * Find the size of the cluster going backward.
	 */
	start = blkno - 1;
	end = start - fs->fs_contigsumsize;
	if (end < 0)
		end = -1;
	mapp = &freemapp[start / NBBY];
	map = *mapp--;
	bit = 1 << (start % NBBY);
	for (i = start; i > end; i--) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != 0) {
			bit >>= 1;
		} else {
			map = *mapp--;
			bit = 1 << (NBBY - 1);
		}
	}
	back = start - i;
	/*
	 * Account for old cluster and the possibly new forward and
	 * back clusters.
	 */
	i = back + forw + 1;
	if (i > fs->fs_contigsumsize)
		i = fs->fs_contigsumsize;
	sump[i] += cnt;
	if (back > 0)
		sump[back] -= cnt;
	if (forw > 0)
		sump[forw] -= cnt;
	/*
	 * Update cluster summary information.
	 */
	lp = &sump[fs->fs_contigsumsize];
	for (i = fs->fs_contigsumsize; i > 0; i--)
		if (*lp-- > 0)
			break;
	fs->fs_maxcluster[cgp->cg_cgx] = i;
}

static ufs2_daddr_t
ufs_alloccgblk(struct inode *ip, char *blockbuf, ufs2_daddr_t bpref)
{
	struct fs *fs;
	struct cg *cgp;
	ufs1_daddr_t bno;
	ufs2_daddr_t blkno;
	u_int8_t *blksfree;

	fs = ip->i_fs;

	cgp = (struct cg *)blockbuf;
	blksfree = cg_blksfree(cgp);
	if (bpref == 0 || dtog(fs, bpref) != cgp->cg_cgx) {
		bpref = cgp->cg_rotor;
	} else {
		bpref = blknum(fs, bpref);
		bno = dtogd(fs, bpref);
		/*
		 * if the requested block is available, use it
		 */
		if (ffs_isblock(fs, blksfree, fragstoblks(fs, bno)))
			goto gotit;
	}
	/*
	 * Take the next available block in this cylinder group.
	 */
	bno = ffs_mapsearch(fs, cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return (0);
	cgp->cg_rotor = bno;
gotit:
	blkno = fragstoblks(fs, bno);
	ffs_clrblock(fs, blksfree, (long)blkno);
	ffs_clusteracct(fs, cgp, blkno, -1);
	cgp->cg_cs.cs_nbfree--;
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, cgp->cg_cgx).cs_nbfree--;
	fs->fs_fmod = 1;
	blkno = cgbase(fs, cgp->cg_cgx) + bno;
	return (blkno);
}

static ufs2_daddr_t
ufs_alloccg(struct inode *ip, int cg, ufs2_daddr_t bpref, int size)
{
	struct fs *fs;
	struct cg *cgp;
	char *blockbuf = NULL;
	ufs1_daddr_t bno;
	ufs2_daddr_t blkno;
	int i, allocsiz, error, frags;
	u_int8_t *blksfree;

	fs = ip->i_fs;

	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return (0);

	blockbuf = malloc(fs->fs_cgsize);

	error = blkread((uufsd_t *)ip->i_dev, fsbtodb(fs, cgtod(fs, cg)), blockbuf, (int)fs->fs_cgsize);

	if (error == -1)
		goto fail;
	cgp = (struct cg *)blockbuf;

	if (!cg_chkmagic(cgp) ||
	    (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize))
		goto fail;

	cgp->cg_old_time = cgp->cg_time = time(NULL);
	if (size == fs->fs_bsize) {
		blkno = ufs_alloccgblk(ip, blockbuf, bpref);
		ACTIVECLEAR(fs, cg);
		blkwrite((uufsd_t *)ip->i_dev, fsbtodb(fs, cgtod(fs, cg)), blockbuf, fs->fs_cgsize);
		return (blkno);
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	blksfree = cg_blksfree(cgp);
	frags = numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0)
			goto fail;
		blkno = ufs_alloccgblk(ip, blockbuf, bpref);
		bno = dtogd(fs, blkno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(blksfree, bno + i);
		i = fs->fs_frag - frags;
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod = 1;
		cgp->cg_frsum[i]++;
		ACTIVECLEAR(fs, cg);
		blkwrite((uufsd_t *)ip->i_dev, fsbtodb(fs, cgtod(fs, cg)), blockbuf, fs->fs_cgsize);
		return (blkno);
	}
	bno = ffs_mapsearch(fs, cgp, bpref, allocsiz);
	if (bno < 0)
		goto fail;
	for (i = 0; i < frags; i++)
		clrbit(blksfree, bno + i);
	cgp->cg_cs.cs_nffree -= frags;
	cgp->cg_frsum[allocsiz]--;
	if (frags != allocsiz)
		cgp->cg_frsum[allocsiz - frags]++;
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod = 1;
	blkno = cgbase(fs, cg) + bno;
	ACTIVECLEAR(fs, cg);
	blkwrite((uufsd_t *)ip->i_dev, fsbtodb(fs, cgtod(fs, cg)), blockbuf, fs->fs_cgsize);
	return (blkno);

fail:
	if (blockbuf) {
		free(blockbuf);
	}
	return (0);
}

ufs2_daddr_t
ufs_hashalloc(struct inode *ip, int cg, int pref, int size, allocfunc_t allocator)
{
	struct fs *fs;
	ufs2_daddr_t result;
	int i, icg = cg;

	fs = ip->i_fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return (result);

	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
	}

	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return (0);
}


/* Allocate a block for inode number ino. nfrags must be less than
 * what a block can hold.
 */
int
ufs_block_alloc(uufsd_t *ufs, struct inode* inode, int size, ufs2_daddr_t *blkno)
{
	struct fs *fs = &ufs->d_fs;
	int fullblock = (size == fs->fs_bsize);

	*blkno = -1;
	if (size > fs->fs_bsize) {
		return -EINVAL;
	}

	if (fullblock && fs->fs_cstotal.cs_nbfree == 0) {
		return -ENOSPC;
	}

	int cgno = ino_to_cg(fs, inode->i_number);
	*blkno = ufs_hashalloc(inode, cgno, 0, size, ufs_alloccg);
	if (*blkno == 0) {
		return -ENOSPC;
	}
	return 0;
}

void
ffs_fragacct(fs, fragmap, fraglist, cnt)
	struct fs *fs;
	int fragmap;
	int32_t fraglist[];
	int cnt;
{
	int inblk;
	int field, subfield;
	int siz, pos;

	inblk = (int)(fragtbl[fs->fs_frag][fragmap]) << 1;
	fragmap <<= 1;
	for (siz = 1; siz < fs->fs_frag; siz++) {
		if ((inblk & (1 << (siz + (fs->fs_frag % NBBY)))) == 0)
			continue;
		field = around[siz];
		subfield = inside[siz];
		for (pos = siz; pos <= fs->fs_frag; pos++) {
			if ((fragmap & field) == subfield) {
				fraglist[siz] += cnt;
				pos += siz;
				field <<= siz;
				subfield <<= siz;
			}
			field <<= 1;
			subfield <<= 1;
		}
	}
}

void
ufs_block_free(
	uufsd_t *ufs,
	struct ufs_vnode *vnode,
	ufs2_daddr_t bno,
	long size,
	ino_t inum)
{
	struct fs *fs = &ufs->d_fs;
	struct cg *cgp;
	ufs1_daddr_t fragno, cgbno;
	ufs2_daddr_t cgblkno;
	int i, cg, blk, frags, bbase;
	u_int8_t *blksfree;
	char *buf;

	cg = dtog(fs, bno);
	cgblkno = fsbtodb(fs, cgtod(fs, cg));

	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0 ||
	    fragnum(fs, bno) + numfrags(fs, size) > fs->fs_frag) {
		debugferr("ffs_blkfree: bad size");
	}

	if ((u_int)bno >= fs->fs_size) {
		debugferr("inum %d :bad block", inum);
		return;
	}

	if (ufs_get_mem(fs->fs_cgsize, &buf)) {
		debugferr("unable to allocate memory\n");
		return;
	}

	if (blkread(ufs, cgblkno, buf, fs->fs_cgsize) == -1) {
		free(buf);
		return;
	}

	cgp = (struct cg *)buf;
	if (!cg_chkmagic(cgp)) {
		free(buf);
		return;
	}
	cgp->cg_old_time = cgp->cg_time = time(NULL);
	cgbno = dtogd(fs, bno);
	blksfree = cg_blksfree(cgp);

	if (size == fs->fs_bsize) {
		fragno = fragstoblks(fs, cgbno);
		if (!ffs_isfreeblock(fs, blksfree, fragno)) {
			debugferr("ffs_blkfree: freeing free block");
		}
		ffs_setblock(fs, blksfree, fragno);
		ffs_clusteracct(fs, cgp, fragno, 1);
		cgp->cg_cs.cs_nbfree++;
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
	} else {
		bbase = cgbno - fragnum(fs, cgbno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, -1);
		/*
		 * deallocate the fragment
		 */
		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(blksfree, cgbno + i)) {
				debugferr("ffs_blkfree: freeing free frag");
			}
			setbit(blksfree, cgbno + i);
		}
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		fragno = fragstoblks(fs, bbase);
		if (ffs_isblock(fs, blksfree, fragno)) {
			cgp->cg_cs.cs_nffree -= fs->fs_frag;
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			ffs_clusteracct(fs, cgp, fragno, 1);
			cgp->cg_cs.cs_nbfree++;
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
		}
	}
	fs->fs_fmod = 1;
	(void)blkwrite(ufs, cgblkno, buf, ufs->d_fs.fs_bsize);
}


int
ufs_clear_indirect(uufsd_t *fs, struct inode *inode, blk_t fbn)
{
	int nindir = fs->d_fs.fs_nindir;
	fbn -= NDADDR;
	ufs2_daddr_t tempblock;
	int index;
	char *blockbuf;
	int blksize = fs->d_fs.fs_bsize;

	if (fbn < NDADDR || (fbn % nindir))
		return 0;


	int indir = fbn / nindir;
	if (indir == 0) {
		return 0;
	} else {

		if (ufs_get_mem(fs->d_fs.fs_bsize, &blockbuf)) {
			return -ENOMEM;
		}

		fbn = fbn - nindir;
		indir = fbn / (nindir * nindir);
		if (indir == 0) {
			/* Block lies in 2nd indirect block */
			index = fbn / nindir;
			tempblock = inode->i_din2.di_ib[1];

			if (!tempblock) {
				return 0;
			}

			if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
				debugf("Unable to read block %d\n", tempblock);
				ufs_free_mem(blockbuf);
				return -1;
			}

			if (*((ufs2_daddr_t *)blockbuf + index)) {
				ufs_block_free(fs, inode2vnode(inode),
					       *((ufs2_daddr_t *)blockbuf + index),
					       blksize, inode->i_ino);
				*((ufs2_daddr_t *)blockbuf + index) = 0;
				(void)blkwrite(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize);
			}
		} else {
			/* Block lies in 3nd indirect block */
			fbn = fbn - nindir * nindir;
			tempblock = inode->i_din2.di_ib[1];
			if (!tempblock) {
				return 0;
			}

			if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
				debugf("Unable to read block %d\n", tempblock);
				ufs_free_mem(blockbuf);
				return -1;
			}
			index = fbn / (nindir * nindir);
			tempblock = *((ufs2_daddr_t *)blockbuf + index);

			if (!tempblock) {
				return 0;
			}

			/* At 2nd level */
			if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
				debugf("Unable to read block %d\n", tempblock);
				ufs_free_mem(blockbuf);
				return -1;
			}
			index = fbn / nindir;
			if (*((ufs2_daddr_t *)blockbuf + index)) {
				ufs_block_free(fs, inode2vnode(inode),
					       *((ufs2_daddr_t *)blockbuf + index),
					       blksize, inode->i_ino);
				*((ufs2_daddr_t *)blockbuf + index) = 0;
				(void)blkwrite(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize);
			}
		}
	}
	return 0;
}

int
ufs_truncate(uufsd_t *ufs, struct ufs_vnode *vnode, int newsize)
{
	struct inode *inode = vnode2inode(vnode);
	int new_fbn = lblkno(&ufs->d_fs, newsize);
	int old_fbn = lblkno(&ufs->d_fs, inode->i_size);
	ufs2_daddr_t blkno;
	int retval = 0;
	blk_t i;
	int blksize = ufs->d_fs.fs_bsize;
	struct fs *fs = &ufs->d_fs;
	int size;
	int partial_truncate = 0;
	char *buf = NULL;
	blk_t filesize = inode->i_size;
	int l0, l1 , l2 , l3;

	l0 = l1 = l2 = l3 = 0;

	for (i = old_fbn ; (int)i >= new_fbn; i--) {
		retval = ufs_bmap(ufs, vnode, i, &blkno);
		if (retval) {
			return retval;
		}
		//if (old_fbn < NDADDR && (i == old_fbn || i == new_fbn)) {
		if (old_fbn < NDADDR && i == old_fbn) {
			if (new_fbn != old_fbn) {
				/* We are completely getting rid of the last block
				 * See how many fragments we need to free
				 */
				if (inode->i_size % blksize) {
					size = fragroundup(fs, inode->i_size % blksize);
				} else {
					size = blksize;
				}
			} else {
				/* We are truncating in the same block
				 * check if we need to free anything or not
				 */
				int ofrags = numfrags(fs, fragroundup(fs, inode->i_size));
				int nfrags = numfrags(fs, fragroundup(fs, newsize));
				if (nfrags == ofrags ) {
					return 0;
				}
				size = (ofrags - nfrags) * fs->fs_fsize;
				partial_truncate = 1;
			}
		} else {
			size = blksize;
		}

		if (partial_truncate && blkno && newsize) {
			buf = malloc(blksize);
			if (!buf) {
				return -ENOMEM;
			}
			memset(buf, 0, blksize);
			(void)blkread(ufs, fsbtodb(fs, blkno), buf, size);
			memset(buf + (newsize % blksize), 0, inode->i_size - newsize);
		}

		if (blkno) {
			ufs_block_free(ufs, vnode, blkno, size, inode->i_ino);
			l0++;
			if (partial_truncate && newsize) {
				retval = ufs_block_alloc(ufs, inode, size, &blkno);
				if (retval) {
					free(buf);
					return retval;
				}
				retval = ufs_set_block(ufs, inode, i, blkno);
				if (retval) {
					free(buf);
					return retval;
				}
				(void)blkwrite(ufs, fsbtodb(fs, blkno), buf, size);
				free(buf);
			} else {
				retval = ufs_set_block(ufs, inode, i, 0);
				if (retval)
					return retval;
			}
		}
	}

	int nindir = ufs->d_fs.fs_nindir;
	/* Fix the indirect block pointers.
	 * This can be optimized with upper loop,
	 * but keep it simple for now.
	 */
	for (i = old_fbn ; (int)i > new_fbn;) {
		if (i >= NDADDR) {
			if ((i - NDADDR) % nindir == 0) {
				ufs_clear_indirect(ufs, inode, i);
			}
			if (i ==  NDADDR) {
				if (inode->i_din2.di_ib[0]) {
					ufs_block_free(ufs, vnode, inode->i_din2.di_ib[0], blksize, inode->i_ino);
					l1++;
				}
				inode->i_din2.di_ib[0] = 0;
			} else if (i == nindir + NDADDR) {
				if (inode->i_din2.di_ib[1]) {
					ufs_block_free(ufs, vnode, inode->i_din2.di_ib[1], blksize, inode->i_ino);
					l2++;
				}
				inode->i_din2.di_ib[1] = 0;
			} else if (i == (nindir*nindir + nindir + NDADDR)) {
				if (inode->i_din2.di_ib[2]) {
					ufs_block_free(ufs, vnode, inode->i_din2.di_ib[2], blksize, inode->i_ino);
					l3++;
				}
				inode->i_din2.di_ib[2] = 0;
			}

			if ((i - NDADDR) % nindir) {
				i -= ((i - NDADDR) % nindir);
			} else {
				i -= nindir;
			}
		} else {
			/* Otherwise it would have been zeroed above */
			break;
		//	inode->i_din2.di_db[i] = 0;
			//i--;
		}
	}

	printf("inum %u (size = %u) : Freed %d L0s and %d L1_indirects %d L2_indirects %d L3_indirects\n",
			(int)inode->i_ino, (int)filesize, l0, l1, l2, l3);
	retval = ufs_write_inode(ufs, inode->i_ino, vnode);
	return retval;
}

int
ufs_write_inode(uufsd_t *ufs, ino_t ino, struct ufs_vnode *vnode)
{
	struct ufs2_dinode *dinop = NULL;
	int rc;

	rc = getino(ufs, (void **)&dinop, ino, NULL);
	if (rc) {
		return rc;
	}

	copy_incore_to_ondisk(vnode2inode(vnode), UFS_DINODE(vnode2inode(vnode)));
	/* getino above sets the inode block */
	bcopy(UFS_DINODE(vnode2inode(vnode)), dinop, sizeof(*dinop));
	rc = blkwrite(ufs, fsbtodb(&ufs->d_fs, ino_to_fsba(&ufs->d_fs, ino)),
				ufs->d_inoblock, ufs->d_fs.fs_bsize);
	if (rc <= 0) {
		return -EIO;
	}
	return 0;
}

struct link_struct  {
	uufsd_t	*ufs;
	const char	*name;
	int		namelen;
	ino_t	inode;
	int		flags;
	int		done;
	unsigned int	blocksize;
	int		err;
};

static int ufs_get_rec_len(uufsd_t *ufs,
			  struct direct *dirent,
			  unsigned int *rec_len)
{
	*rec_len = dirent->d_reclen;
	return 0;
}

int ufs_set_rec_len(uufsd_t *ufs,
			  unsigned int len,
			  struct direct *dirent)
{
	dirent->d_reclen = len;
	return 0;
}

static int link_proc(struct direct *dirent,
		     int	offset,
		     char	*buf,
		     void	*priv_data)
{
	struct link_struct *ls = (struct link_struct *) priv_data;
	struct direct *next;
	int blocksize = ls->blocksize;
	unsigned int rec_len, min_rec_len, curr_rec_len;
	int ret = 0;

	rec_len = UFS_DIR_REC_LEN(ls->namelen);

	ls->err = ufs_get_rec_len(ls->ufs, dirent, &curr_rec_len);
	if (ls->err)
		return DIRENT_ABORT;

	/*
	 * See if the following directory entry (if any) is unused;
	 * if so, absorb it into this one.
	 */
	next = (struct direct *) (buf + offset + curr_rec_len);
	if ((offset + curr_rec_len < blocksize - 8) &&
	    (next->d_ino == 0) &&
	    (offset + curr_rec_len + next->d_reclen <= blocksize)) {
		curr_rec_len += next->d_reclen;
		ls->err = ufs_set_rec_len(ls->ufs, curr_rec_len, dirent);
		if (ls->err)
			return DIRENT_ABORT;
		ret = DIRENT_CHANGED;
	}

	/*
	 * If the directory entry is used, see if we can split the
	 * directory entry to make room for the new name.  If so,
	 * truncate it and return.
	 */
	if (dirent->d_ino) {
		min_rec_len = UFS_DIR_REC_LEN(dirent->d_namlen & 0xFF);
		if (curr_rec_len < (min_rec_len + rec_len))
			return ret;
		rec_len = curr_rec_len - min_rec_len;
		ls->err = ufs_set_rec_len(ls->ufs, min_rec_len, dirent);
		if (ls->err)
			return DIRENT_ABORT;
		next = (struct direct *) (buf + offset +
						  dirent->d_reclen);
		next->d_ino = 0;
		next->d_namlen = 0;
		ls->err = ufs_set_rec_len(ls->ufs, rec_len, next);
		if (ls->err)
			return DIRENT_ABORT;
		return DIRENT_CHANGED;
	}

	/*
	 * If we get this far, then the directory entry is not used.
	 * See if we can fit the request entry in.  If so, do it.
	 */
	if (curr_rec_len < rec_len)
		return ret;
	dirent->d_ino = ls->inode;
	dirent->d_namlen = ls->namelen;
	dirent->d_type = IFTODT(ls->flags);
	strncpy(dirent->d_name, ls->name, ls->namelen);
	dirent->d_name[ls->namelen] = '\0';
	ls->done++;
	return DIRENT_ABORT|DIRENT_CHANGED;
}

int
ufs_addnamedir(uufsd_t *ufs, ino_t dir, const char *name,
		ino_t ino, int flags)
{
	int			retval;
	struct link_struct	ls;

	RETURN_IF_RDONLY(ufs);

	ls.ufs = ufs;
	ls.name = name;
	ls.namelen = name ? strlen(name) : 0;
	ls.inode = ino;
	ls.flags = flags;
	ls.done = 0;
	ls.blocksize = DIRBLKSIZ;
	ls.err = 0;

	retval = ufs_dir_iterate(ufs, dir, link_proc, &ls);
	if (retval)
		return retval;
	if (ls.err)
		return ls.err;

	if (!ls.done)
		return ENOSPC;

	return 0;
}

int
ufs_link(uufsd_t *ufs, ino_t dir_ino, char *r_dest, struct ufs_vnode *vnode, int mode)
{
	struct inode *ip;
	int error;

	ip = vnode2inode(vnode);
	if ((nlink_t)ip->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	/*
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}
	*/
	ip->i_effnlink++;
	ip->i_nlink++;

	error = ufs_addnamedir(ufs, dir_ino, r_dest, ip->i_number, mode);
	if (error) {
		ip->i_effnlink--;
		ip->i_nlink--;
	}
out:
	return (error);
}

/* This should go into ufs_unlink.c */
struct unlink_struct  {
	const char	*name;
	int		namelen;
	ino_t		inode;
	int		flags;
	struct direct *prev_dirent;
	int		done;
};

static int unlink_proc(struct direct *dirent,
		     int	offset,
		     char	*buf,
		     void	*priv_data)
{
	struct unlink_struct *ls = (struct unlink_struct *) priv_data;
	struct direct *prev;

	if (dirent->d_ino==0) /* skip unused dentry */
		return 0;

	prev = ls->prev_dirent;
	ls->prev_dirent = dirent;

	if (ls->name) {
		if ((dirent->d_namlen & 0xFF) != ls->namelen)
			return 0;
		if (strncmp(ls->name, dirent->d_name, dirent->d_namlen & 0xFF))
			return 0;
	}
	if (ls->inode) {
		if (dirent->d_ino != ls->inode)
			return 0;
	} else {
		if (!dirent->d_ino)
			return 0;
	}

	if (offset)
		prev->d_reclen += dirent->d_reclen;
	else
		dirent->d_ino = 0;
	//bzero(dirent, dirent->d_reclen);
	ls->done++;
	return DIRENT_ABORT|DIRENT_CHANGED;
}

int
ufs_unlink(uufsd_t *ufs, ino_t dir_ino, char *name, ino_t file_ino, int flags)
{
	struct unlink_struct ls;
	int retval = 0;

	if (!name && !file_ino)
		return -EINVAL;

	RETURN_IF_RDONLY(ufs);

	ls.name = name;
	ls.namelen = name ? strlen(name) : 0;
	ls.inode = file_ino;
	ls.flags = 0;
	ls.done = 0;
	ls.prev_dirent = 0;


	retval = ufs_dir_iterate(ufs, dir_ino, unlink_proc, &ls);
	if (retval)
		return retval;
	return 0;
}

int
ufs_free_inode(uufsd_t *ufs, struct ufs_vnode *vnode, ino_t ino, int mode)
{
	struct cg *cgp;
	ufs2_daddr_t cgbno;
	int error, cg;
	u_int8_t *inosused;
	struct fs *fs = &ufs->d_fs;
	char *buf;

	error = ufs_get_mem(fs->fs_cgsize, &buf);
	if (error) {
		return -ENOMEM;
	}

	cg = ino_to_cg(fs, ino);
	cgbno = fsbtodb(fs, cgtod(fs, cg));

	if ((u_int)ino >= fs->fs_ipg * fs->fs_ncg) {
		debugferr("Corrupted inode numbers in filesystem\n");
		ufs_free_mem(&buf);
		exit(-1);
	}

	if ((error = blkread(ufs, cgbno, buf, (int)fs->fs_cgsize)) == -1) {
		ufs_free_mem(&buf);
		return (error);
	}

	cgp = (struct cg *)buf;
	if (!cg_chkmagic(cgp)) {
		ufs_free_mem(&buf);
		return (0);
	}

	cgp->cg_old_time = cgp->cg_time = time(NULL);
	inosused = cg_inosused(cgp);
	ino %= fs->fs_ipg;
	if (isclr(inosused, ino)) {
		if (fs->fs_ronly == 0)
			debugferr("ufs_free_inode: freeing free inode");
	}
	clrbit(inosused, ino);
	if (ino < cgp->cg_irotor)
		cgp->cg_irotor = ino;
	cgp->cg_cs.cs_nifree++;
	fs->fs_cstotal.cs_nifree++;
	fs->fs_cs(fs, cg).cs_nifree++;
	if (IFTODT(mode) == IFDIR) {
		cgp->cg_cs.cs_ndir--;
		fs->fs_cstotal.cs_ndir--;
		fs->fs_cs(fs, cg).cs_ndir--;
	}
	fs->fs_fmod = 1;
	error = blkwrite(ufs, cgbno, buf, (int)fs->fs_cgsize);
	if (error == -1) {
		debugf("Unable to write the buffer\n");
		return -EIO;
	}
	ufs_free_mem(&buf);
	return (0);
}

int ufs_expand_dir(uufsd_t *ufs, ino_t d_ino)
{
	return 0;
}
