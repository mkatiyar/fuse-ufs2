/**
 * Copyright (c) 2008-2009 Alper Akcan <alper.akcan@gmail.com>
 * Copyright (c) 2009 Renzo Davoli <renzo@cs.unibo.it>
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

ufs_file_t do_open (uufsd_t *ufs, const char *path, int flags)
{
	errcode_t rc;
	ino_t ino;
	ufs_file_t efile;
	struct ufs_vnode *vnode;
	int rt;

	debugf("enter");
	debugf("path = %s", path);

	rt = do_readvnode(ufs, path, &ino, &vnode);
	if (rt) {
		debugf("do_readvnode(%s, &ino, &vnode); failed", path);
		return NULL;
	}

	rc = ufs_file_open2(ufs, ino, vnode,
			    (((flags & O_ACCMODE) != 0) ? UFS_FILE_WRITE : 0),
			    &efile);

	if (rc) {
		vnode_put(vnode,0);
		return NULL;
	}

	debugf("leave");
	return efile;
}

int op_open (const char *path, struct fuse_file_info *fi)
{
	ufs_file_t file;
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s", path);

	file = do_open(ufs, path, fi->flags);
	if (file == NULL) {
		debugf("do_open(%s); failed", path);
		return -ENOENT;
	}
	fi->fh = (unsigned long) file;

	debugf("leave");
	return 0;
}
