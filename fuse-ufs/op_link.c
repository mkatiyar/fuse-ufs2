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

int op_link (const char *source, const char *dest)
{
	int rc;
	char *p_path;
	char *r_path;
	ino_t d_ino, ino;
	struct ufs_vnode *vnode;
	struct inode *inode;
	uufsd_t *ufs = current_ufs();

	RETURN_IF_RDONLY(ufs);

	debugf("source: %s, dest: %s", source, dest);

	rc = do_check(source);
	if (rc != 0) {
		debugf("do_check(%s); failed", source);
		return rc;
	}

	rc = do_check_split(dest, &p_path, &r_path);
	if (rc != 0) {
		debugf("do_check(%s); failed", dest);
		return rc;
	}

	debugf("parent: %s, child: %s", p_path, r_path);

	rc = do_readvnode(ufs, p_path, &d_ino, &vnode);
	if (rc) {
		debugf("do_readvnode(%s, &d_ino, &inode); failed", p_path);
		free_split(p_path, r_path);
		return rc;
	}

	vnode_put(vnode, 0);

	rc = do_readvnode(ufs, source, &ino, &vnode);
	if (rc) {
		debugf("do_readvnode(%s, &d_ino, &inode); failed", source);
		free_split(p_path, r_path);
		return rc;
	}

	inode = vnode2inode(vnode);

	do {
		rc = ufs_link(ufs, d_ino, r_path, vnode, inode->i_mode);
		if (rc == ENOSPC) {
			debugf("calling ufs_expand_dir(ufs, &d)", d_ino);
			if (ufs_expand_dir(ufs, d_ino)) {
				debugf("error while expanding directory %s (%d)", p_path, d_ino);
				vnode_put(vnode, 0);
				free_split(p_path, r_path);
				return -ENOSPC;
			}
		}
	} while (rc == ENOSPC);
	if (rc) {
		vnode_put(vnode, 0);
		free_split(p_path, r_path);
		return -EIO;
	}

	inode->i_mtime = inode->i_atime = inode->i_ctime = ufs->now ? ufs->now : time(NULL);
	rc = vnode_put(vnode, 1);
	if (rc) {
		debugf("vnode_put(vnode,1); failed");
		free_split(p_path, r_path);
		return -EIO;
	}
	debugf("done");

	return 0;
}
