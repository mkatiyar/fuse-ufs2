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

int op_unlink (const char *path)
{
	int rt;

	char *p_path;
	char *r_path;

	ino_t p_ino;
	struct ufs_vnode *p_vnode;
	ino_t r_ino;
	struct ufs_vnode *r_vnode;
	struct inode *r_inode;
	struct inode *p_inode;

	uufsd_t *ufs = current_ufs();

	RETURN_IF_RDONLY(ufs);

	debugf("enter");
	debugf("path = %s", path);

	rt = do_check_split(path, &p_path, &r_path);
	if (rt != 0) {
		debugf("do_check_split: failed");
		return rt;
	}

	debugf("parent: %s, child: %s", p_path, r_path);

	rt = do_readvnode(ufs, p_path, &p_ino, &p_vnode);
	if (rt) {
		debugf("do_readinode(%s, &p_ino, &p_inode); failed", p_path);
		free_split(p_path, r_path);
		return rt;
	}
	rt = do_readvnode(ufs, path, &r_ino, &r_vnode);
	if (rt) {
		debugf("do_readvnode(%s, &r_ino, &r_vnode); failed", path);
		free_split(p_path, r_path);
		return rt;

	}
	r_inode = vnode2inode(r_vnode);

	if(S_ISDIR(r_inode->i_mode)) {
		debugf("%s is a directory", path);
		vnode_put(r_vnode, 0);
		free_split(p_path, r_path);
		return -EISDIR;
	}

	rt = ufs_unlink(ufs, p_ino, r_path, r_ino, 0);
	if (rt) {
		debugf("ufs_unlink(ufs, %d, %s, %d, 0); failed", p_ino, r_path, r_ino);
		vnode_put(r_vnode, 0);
		vnode_put(p_vnode, 0);
		free_split(p_path, r_path);
		return -EIO;
	}

	if (r_inode->i_nlink > 0) {
		r_inode->i_nlink -= 1;
	}

	p_inode = vnode2inode(p_vnode);
	p_inode->i_ctime = p_inode->i_mtime = ufs->now ? ufs->now : time(NULL);
	rt = vnode_put(p_vnode, 1);
	if (rt) {
		debugf("ufs_write_inode(ufs, p_ino, &p_inode); failed");
		vnode_put(r_vnode,1);
		free_split(p_path, r_path);
		return -EIO;
	}

	r_inode->i_ctime = ufs->now ? ufs->now : time(NULL);
	rt = vnode_put(r_vnode, 1);
	if (rt) {
		debugf("vnode_put(r_vnode, 1); failed");
		free_split(p_path, r_path);
		return -EIO;
	}

	free_split(p_path, r_path);
	debugf("leave");
	return 0;
}
