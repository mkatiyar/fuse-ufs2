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

static void
ufs_clear_inode(struct ufs_vnode *vnode)
{
	struct inode *inode = vnode2inode(vnode);
	inode->i_size = 0;
	inode->i_mtime = inode->i_ctime = inode->i_atime = 0;
	inode->i_blocks = 0;
	inode->i_effnlink = 0;
	inode->i_count = 0;
	inode->i_endoff = 0;
	inode->i_diroff = 0;
	inode->i_offset = 0;
	inode->i_reclen = 0;
	inode->i_mode = 0;
	inode->i_nlink = 0;
	inode->i_flags = 0;
	inode->i_uid = 0;
	inode->i_gid = 0;
}

int do_killfilebyinode (uufsd_t *ufs, ino_t ino, struct ufs_vnode *vnode)
{
	int rc;
	debugf("enter");
	struct inode *inode = vnode2inode(vnode);

	inode->i_nlink = 0;

	if (inode->i_blocks) {
		ufs_truncate(ufs, vnode, 0);
	}

	rc = ufs_free_inode(ufs, vnode, ino, inode->i_mode);
	if (rc) {
		debugf("Unable to free inode\n");
		return -EIO;
	}

	ufs_clear_inode(vnode);
	rc = ufs_write_inode(ufs, ino, vnode);
	if (rc) {
		debugf("ufs_write_inode(ufs, ino, inode); failed");
		return -EIO;
	}

	debugf("leave");
	return 0;
}
