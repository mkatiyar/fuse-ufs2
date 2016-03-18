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

int op_utimens (const char *path, const struct timespec tv[2])
{
	int rt;
	ino_t ino;
	struct ufs_vnode *vnode;
	struct inode *inode;
	uufsd_t  *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s", path);

	rt = do_readvnode(ufs, path, &ino, &vnode);
	if (rt) {
		debugf("do_readvnode(%s, &ino, &vnode); failed", path);
		return rt;
	}
	inode = vnode2inode(vnode);

	inode->i_atime = tv[0].tv_sec;
	inode->i_mtime = tv[0].tv_sec;

	rt = vnode_put(vnode, 1);
	if (rt) {
		debugf("vnode_put(vnode,1); failed");
		return -EIO;
	}

	debugf("leave");
	return 0;
}
