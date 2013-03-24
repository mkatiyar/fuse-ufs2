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

int op_chmod (const char *path, mode_t mode)
{
	int rt;
	int mask;
	int rc;
	ino_t ino;
	struct ufs_vnode *vnode;
	struct inode *inode;
	uufsd_t *ufs = current_ufs();

	RETURN_IF_RDONLY(ufs);

	debugf("enter");
	debugf("path = %s 0%o", path, mode);

	rt = do_check(path);
	if (rt != 0) {
		debugf("do_check(%s); failed", path);
		return rt;
	}

	rt = do_readvnode(ufs, path, &ino, &vnode);
	if (rt) {
		debugf("do_readvnode(%s, &ino, &vnode); failed", path);
		return rt;
	}
	inode = vnode2inode(vnode);

	mask = S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX;
	inode->i_mode = (inode->i_mode & ~mask) | (mode & mask);
	inode->i_ctime = time(NULL);

	rc = vnode_put(vnode,1);
	if (rc) {
		debugf("vnode_put(vnode,1); failed");
		return -EIO;
	}

	debugf("leave");
	return 0;
}
