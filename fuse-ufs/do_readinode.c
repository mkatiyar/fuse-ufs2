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

static int
calc_num_blocks(struct inode *inode)
{
	int l1_indir, l2_indir, l3_indir;
	struct fs *fs = inode->i_fs;
	int nfrags, numblocks = howmany(inode->i_size, fs->fs_bsize);

	/* Guess(!) whether we're on a "short" symlink --> no blocks used */
	size_t max_symlinklen = (NDADDR + NIADDR) * sizeof(ufs2_daddr_t);
	if (S_ISLNK(inode->i_mode) && inode->i_size < max_symlinklen)
		return 0;

	if (numblocks < NDADDR) {
		nfrags = numfrags(fs, fragroundup(fs, inode->i_size));
	} else {
		int nindirs = fs->fs_nindir;
		/* Calculate how many indirects do we need to hold these many blocks */
		l1_indir = howmany(numblocks - NDADDR, nindirs);
		numblocks += l1_indir;

		l2_indir = howmany(l1_indir - 1, nindirs);
		numblocks += l2_indir;

		if (l2_indir) {
			l3_indir = howmany(l2_indir - 1, nindirs);
			numblocks += l3_indir;
		}

		nfrags = numblocks * fs->fs_frag;
	}

	return nfrags * (fs->fs_fsize  >> DEV_BSHIFT);
}

void
copy_incore_to_ondisk(struct inode *inode, struct ufs2_dinode *dinop)
{
	inode->i_blocks = calc_num_blocks(inode);
	dinop->di_nlink = inode->i_effnlink;
	dinop->di_mode = inode->i_mode;
	dinop->di_nlink = inode->i_nlink;
	dinop->di_size = inode->i_size;
	dinop->di_flags = inode->i_flags;
	dinop->di_gen = inode->i_gen;
	dinop->di_uid = inode->i_uid;
	dinop->di_gid = inode->i_gid;
	dinop->di_blocks = inode->i_blocks;
	dinop->di_atime = inode->i_atime;
	dinop->di_mtime = inode->i_mtime;
	dinop->di_ctime = inode->i_ctime;
}

void
copy_ondisk_to_incore(uufsd_t *ufsp, struct inode *inode,
			struct ufs2_dinode *dinop, ino_t ino)
{
	bcopy(dinop, &inode->dinode_u.din2, sizeof(*dinop));
	inode->i_dev = (struct cdev *)ufsp;
	inode->i_number = ino;
	inode->i_effnlink = dinop->di_nlink;
	inode->i_fs = &ufsp->d_fs;

	inode->i_count = 0;
	inode->i_endoff = 0;
	inode->i_diroff = 0;
	inode->i_offset = 0;
	inode->i_ino = ino;
	inode->i_reclen = 0;

	inode->i_mode = dinop->di_mode;
	inode->i_nlink = dinop->di_nlink;
	inode->i_size = dinop->di_size;
	inode->i_flags = dinop->di_flags;
	inode->i_gen = dinop->di_gen;
	inode->i_uid = dinop->di_uid;
	inode->i_gid = dinop->di_gid;
	inode->i_blocks = dinop->di_blocks;
	inode->i_atime = dinop->di_atime;
	inode->i_mtime = dinop->di_mtime;
	inode->i_ctime = dinop->di_ctime;
}

int do_readinode(uufsd_t *ufs, const char *path, ino_t *ino, struct inode *inode)
{
	int rc;
	int mode;
	struct ufs2_dinode *dinop = NULL;

	rc = ufs_namei(ufs, ROOTINO, ROOTINO, path, ino);
	if (rc || !*ino) {
		debugf("ufs_namei(ufs, ROOTINO, ROOTINO, %s, ino); failed", path);
		return -ENOENT;
	}
	rc = getino(ufs, (void **)&dinop, *ino, &mode);
	if (rc) {
		debugf("getino(ufs, *ino, inode, &mode); failed");
		return -EIO;
	}
	copy_ondisk_to_incore(ufs, inode, dinop, *ino);
	return 0;
}

int do_readvnode (uufsd_t *ufs, const char *path, ino_t *ino, struct ufs_vnode **vnode)
{
	int rc;
	rc = ufs_namei(ufs, ROOTINO, ROOTINO, path, ino);
	if (rc || !*ino) {
		debugf("ufs_namei(ufs, ROOTINO, ROOTINO, %s, ino); failed", path);
		return -ENOENT;
	}
	*vnode = vnode_get(ufs, *ino);
	if (*vnode==NULL) {
		debugf("vnode_get(ufs, *ino); failed");
		return -EIO;
	}
	return 0;
}
