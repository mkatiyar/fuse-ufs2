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

struct rmdir_st {
	ino_t parent;
	int empty;
};

static int rmdir_proc (struct direct *dirent, int offset,
		       char *buf, void *private)
{
	int *p_empty= (int *) private;

	if (dirent->d_ino==0) /* skip unused dentry */
		return 0;

	debugf("enter");
	debugf("walking on: %s", dirent->d_name);

	if (
			(((dirent->d_namlen & 0xFF) == 1) && (dirent->d_name[0] == '.')) ||
			(((dirent->d_namlen & 0xFF) == 2) && (dirent->d_name[0] == '.') && 
			 (dirent->d_name[1] == '.'))) {
		debugf("leave");
		return 0;
	}
	*p_empty = 0;
	debugf("leave (not empty)");
	return 0;
}

int do_check_empty_dir(uufsd_t *ufs, ino_t ino)
{
	errcode_t rc;
	int empty = 1;

	rc = ufs_dir_iterate(ufs, ino, rmdir_proc, &empty);
	if (rc) {
		debugf("while iterating over directory");
		return -EIO;
	}

	if (empty == 0) {
		debugf("directory not empty");
		return -ENOTEMPTY;
	}

	return 0;
}

int op_rmdir (const char *path)
{
	int rt;
	errcode_t rc;

	char *p_path;
	char *r_path;

	ino_t p_ino;
	struct ufs_vnode *p_vnode;
	ino_t r_ino;
	struct ufs_vnode *r_vnode;
	struct inode *p_inode, *r_inode;

	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s", path);

	rt=do_check_split(path, &p_path, &r_path);
	if (rt != 0) {
		debugf("do_check_split: failed");
		return rt;
	}

	debugf("parent: %s, child: %s", p_path, r_path);

	rt = do_readvnode(ufs, p_path, &p_ino, &p_vnode);
	if (rt) {
		debugf("do_readvnode(%s, &p_ino, &p_inode); failed", p_path);
		free_split(p_path, r_path);
		return rt;
	}
	rt = do_readvnode(ufs, path, &r_ino, &r_vnode);
	if (rt) {
		debugf("do_readvnode(%s, &r_ino, &r_inode); failed", path);
		vnode_put(p_vnode, 0);
		free_split(p_path, r_path);
		return rt;

	}

	r_inode = vnode2inode(r_vnode);
	p_inode = vnode2inode(p_vnode);

	if (!LINUX_S_ISDIR(r_inode->i_mode)) {
		debugf("%s is not a directory", path);
		rt = -ENOTDIR;
		goto out;
	}
	if (r_ino == ROOTINO) {
		debugf("root dir cannot be removed", path);
		rt = -EIO;
		goto out;
	}

	rt = do_check_empty_dir(ufs, r_ino);
	if (rt) {
		debugf("do_check_empty_dir filed");
		goto out;
	}

	rc = ufs_unlink(ufs, p_ino, r_path, r_ino, 0);
	if (rc) {
		debugf("while unlinking ino %d", (int) r_ino);
		rt = -EIO;
		goto out;
	}

	r_inode->i_nlink = 0;
	r_inode->i_ctime = r_inode->i_mtime = ufs->now ? ufs->now : time(NULL);

	if (p_inode->i_nlink > 1) {
		p_inode->i_nlink--;
	}

out:
	vnode_put(p_vnode, 1);
	vnode_put(r_vnode, 1);

	free_split(p_path, r_path);

	debugf("leave");
	return rt;
}
