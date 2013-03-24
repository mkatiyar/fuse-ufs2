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

int op_readlink (const char *path, char *buf, size_t size)
{
	int rt;
	size_t s;
	errcode_t rc;
	ino_t ino;
	char *b = NULL;
	char *pathname;
	struct ufs_vnode *vnode;
	struct inode *inode;
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s", path);

	rt = do_readvnode(ufs, path, &ino, &vnode);
	if (rt) {
		debugf("do_readinode(%s, &ino, &inode); failed", path);
		return rt;
	}

	inode = vnode2inode(vnode);

	if (ino != inode->i_number) {
		debugf("Inum mismatch for inode %d\n", ino);
		rt = -EINVAL;
		goto out;
	}

	if (!LINUX_S_ISLNK(inode->i_mode)) {
		debugf("%s is not a link", path);
		rt = -EINVAL;
		goto out;
	}

	if (inode->i_blocks) {
		rc = ufs_get_mem(ufs->d_fs.fs_bsize, &b);
		if (rc) {
			debugf("ufs_get_mem(fs->d_fs.fs_bsize, &b); failed");
			rt = -ENOMEM;
			goto out;
		}
		rc = blkread(ufs, fsbtodb(&ufs->d_fs, inode->i_din2.di_db[0]), b, ufs->d_fs.fs_bsize);
		if (rc == -1) {
			debugf("blkread(ufs, fsbtodb(&ufs->d_fs, inode->i_din2.di_db[0]), b, ufs->d_fs.fs_bsize) failed\n");
			rt = -EIO;
			goto out1;
		}
		pathname = b;
	} else {
		pathname = (char *) &(UFS_DINODE(inode)->di_db[0]);
	}

	debugf("pathname: %s", pathname);

	s = (size < strlen(pathname) + 1) ? size : strlen(pathname) + 1;
	snprintf(buf, s, "%s", pathname);

out1:
	if (b) {
		ufs_free_mem(&b);
	}

out:
	vnode_put(vnode, 0);
	debugf("leave");
	return rt;
}
