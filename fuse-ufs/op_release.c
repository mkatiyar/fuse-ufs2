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

static void release_callback (struct ufs_vnode *vnode, int flags)
{
	/* TODO: Track whether the inode is actually dirty */
	vnode_put(vnode, (flags & UFS_FILE_WRITE) != 0);
}

int do_release (ufs_file_t file)
{
	errcode_t rc;

	debugf("enter");
	debugf("path = (%p)", file);

	if (file == NULL) {
		return -ENOENT;
	}
	rc = ufs_file_close2(file, release_callback);
	if (rc) {
		return -EIO;
	}

	debugf("leave");
	return 0;
}

int op_release (const char *path, struct fuse_file_info *fi)
{
	int rt;
	ufs_file_t file = UFS_FILE(fi->fh);

	debugf("enter");
	debugf("path = %s (%p)", path, file);
	rt = do_release(file);
	if (rt != 0) {
		debugf("do_release() failed");
		return rt;
	}

	debugf("leave");
	return 0;
}
