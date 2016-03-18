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

int do_modetoufslag (mode_t mode)
{
	if (S_ISREG(mode)) {
		return DT_REG;
	} else if (S_ISDIR(mode)) {
		return DT_DIR;
	} else if (S_ISCHR(mode)) {
		return DT_CHR;
	} else if (S_ISBLK(mode)) {
		return DT_BLK;
	} else if (S_ISFIFO(mode)) {
		return DT_FIFO;
	} else if (S_ISSOCK(mode)) {
		return DT_SOCK;
	} else if (S_ISLNK(mode)) {
		return DT_LNK;
	}
	return DT_UNKNOWN;
}

static inline int old_valid_dev(dev_t dev)
{
	return major(dev) < 256 && minor(dev) < 256;
}

static inline __u16 old_encode_dev(dev_t dev)
{
	return (major(dev) << 8) | minor(dev);
}

static inline __u32 new_encode_dev(dev_t dev)
{
	unsigned major = major(dev);
	unsigned minor = minor(dev);
	return (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
}

int do_create (uufsd_t *ufs, const char *path, mode_t mode, dev_t dev, const char *fastsymlink)
{
	int rt;
	time_t tm;
	int rc;

	char *p_path;
	char *r_path;

	ino_t ino;
	struct ufs_vnode *vnode;
	struct ufs_vnode *dirnode;
	struct inode *inode = NULL;
	int ret = 0;

	struct fuse_context *ctx;

	debugf("enter");
	debugf("path = %s, mode: 0%o", path, mode);

	rt=do_check_split(path, &p_path, &r_path);

	debugf("parent: %s, child: %s", p_path, r_path);

	rt = do_readvnode(ufs, p_path, &ino, &dirnode);
	if (rt) {
		debugf("do_readvnode(%s, &ino, &vnode); failed", p_path);
		free_split(p_path, r_path);
		return rt;
	}

	rc = ufs_valloc(dirnode, mode, &vnode);
	if (rc) {
		debugf("ufs_new_inode(ep.fs, ino, mode, 0, &n_ino); failed");
		ret = -ENOMEM;
		goto out;
	}

	do {
		rc = ufs_link(ufs, ino, r_path, vnode, mode);
		if (rc == ENOSPC) {
			debugf("calling ufs_expand_dir(ufs, &d)", ino);
			if (ufs_expand_dir(ufs, ino)) {
				debugf("error while expanding directory %s (%d)", p_path, ino);
				free_split(p_path, r_path);
				return ENOSPC;
			}
		}
	} while (rc == ENOSPC);
	if (rc) {
		ret = -EIO;
		goto out;
	}

	inode = vnode2inode(vnode);
	tm = ufs->now ? ufs->now : time(NULL);
	inode->i_mode = mode;
	inode->i_atime = inode->i_ctime = inode->i_mtime = tm;
	inode->i_nlink = 1;
	inode->i_size = 0;
	ctx = fuse_get_context();
	if (ctx) {
		inode->i_uid = ctx->uid;
		inode->i_gid = ctx->gid;
	}

	if (S_ISCHR(mode) || S_ISBLK(mode)) {
		if (old_valid_dev(dev))
			UFS_DINODE(inode)->di_db[0]= old_encode_dev(dev);
		else
			UFS_DINODE(inode)->di_db[1]= new_encode_dev(dev);
	}

	if (S_ISLNK(mode) && fastsymlink != NULL) {
		inode->i_size = strlen(fastsymlink);
		strncpy((char *)&(UFS_DINODE(inode)->di_db[0]),fastsymlink,
				((NDADDR + NIADDR) * sizeof(UFS_DINODE(inode)->di_db[0])));
	}

	/* update parent dir */
	rt = do_readvnode(ufs, p_path, &ino, &dirnode);
	if (rt) {
		debugf("do_readinode(%s, &ino, &inode); dailed", p_path);
		ret = -EIO;
		goto out1;
	}
	inode = vnode2inode(dirnode);
	inode->i_ctime = inode->i_mtime = tm;

out1:
	if (dirnode)
		vnode_put(dirnode, 1);
out:
	if (vnode)
		vnode_put(vnode, 1);
	free_split(p_path, r_path);

	debugf("leave");
	return ret;
}

int op_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int rt;
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s, mode: 0%o", path, mode);

	if (op_open(path, fi) == 0) {
		debugf("leave");
		return 0;
	}

	rt = do_create(ufs, path, mode, 0, NULL);
	if (rt != 0) {
		return rt;
	}

	if (op_open(path, fi)) {
		debugf("op_open(path, fi); failed");
		return -EIO;
	}

	debugf("leave");
	return 0;
}
