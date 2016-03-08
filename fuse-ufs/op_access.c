/**
 * Copyright (c) 2013 Manish Katiyar <mkatiyar@gmail.com>
 * Copyright (c) 2016 Jan Blumschein <jan@jan-blumschein.de>
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

#include <unistd.h>
#include <sys/stat.h>

#include "fuse-ufs.h"


static int inode_access(const struct inode *inode, int mode)
{
	u_int16_t perms;

	/* Fuse context contains user/group IDs of the client process */
	struct fuse_context *context = fuse_get_context();

	/* Should be assured by the caller */
	if (!inode) return -ENOENT;

	/* Test for bare existence - Should have been handled by fuse layer */
	if (mode == F_OK) return 0;

	/* File permissions */
	perms = inode->i_mode;

	/* Test permissions according to uid/gid and requested mode */
	if (context->uid == 0) /* root */
	{
		/* Linux: The super-user can read and write any file,
		          and execute any file that anyone can execute. */
		if ( (mode & X_OK) == 0 ) return 0;
		if ( perms & (S_IXUSR | S_IXGRP | S_IXOTH) ) return 0;
		return -EACCES;
	}
	else if (context->uid == inode->i_uid) /* user */
	{
		if ( (mode & R_OK) && !(perms & S_IRUSR) ) return -EACCES;
		if ( (mode & W_OK) && !(perms & S_IWUSR) ) return -EACCES;
		if ( (mode & X_OK) && !(perms & S_IXUSR) ) return -EACCES;

		return 0;
	}
	/* cases below require the "allow_other" mount option */
	else if (context->gid == inode->i_gid) /* group */
		  /* or groups(context->uid) contains inode->i_gid */
	{
		if ( (mode & R_OK) && !(perms & S_IRGRP) ) return -EACCES;
		if ( (mode & W_OK) && !(perms & S_IWGRP) ) return -EACCES;
		if ( (mode & X_OK) && !(perms & S_IXGRP) ) return -EACCES;

		return 0;
	}
	else /* other */
	{
		if ( (mode & R_OK) && !(perms & S_IROTH) ) return -EACCES;
		if ( (mode & W_OK) && !(perms & S_IWOTH) ) return -EACCES;
		if ( (mode & X_OK) && !(perms & S_IXOTH) ) return -EACCES;

		return 0;
	}
}

int op_access (const char *path, int mask)
{
	int res;
	ino_t ino;
	struct ufs_vnode *vnode;
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s, mask = 0%o", path, mask);

	/* This is redundant: it should have been handled by the fuse layer
	 * (it will be unless we disagree about the readonly mount flag)
	 */
	if ((mask & W_OK) && (ufs->d_fs.fs_ronly)) {
		debugf("FIXME: fuse calls access(W_OK) on read-only mount");
		return -EROFS;
	}

	/* TODO: If called by a different user (enabled with "allow_other"
	 *       mount option), check his permissions on path components
	 */

	res = do_check(path);
	if (res ) {
		debugf("bad path argument: do_check(%s) failed", path);
		return res;
	}

	res = do_readvnode(ufs, path, &ino, &vnode);
	if (res) {
		debugf("no such item: do_readvnode(%s) failed", path);
		return res;
	}

	res = inode_access(vnode2inode(vnode), mask);

	debugf("leave");
	return res;
}
