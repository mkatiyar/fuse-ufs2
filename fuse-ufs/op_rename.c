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

static int fix_dotdot_proc (
		struct direct *dirent,
		int offset,
		int blocksize,
		char *buf, void *private)
{
	ino_t *p_dotdot = (ino_t *) private;

	debugf("enter");
	debugf("walking on: %s", dirent->d_name);

	if ((dirent->d_namlen & 0xFF) == 2 && strncmp(dirent->d_name, "..", 2) == 0) {
		dirent->d_ino = *p_dotdot;

		debugf("leave (found '..')");
		return DIRENT_ABORT | DIRENT_CHANGED;
	} else {
		debugf("leave");
		return 0;
	}
}

static int do_fix_dotdot(uufsd_t *ufs, ino_t ino, ino_t dotdot)
{
	int rc;

	debugf("enter");
	rc = ufs_dir_iterate(ufs, ino, fix_dotdot_proc, &dotdot);
	if (rc) {
		debugf("while iterating over directory");
		return -EIO;
	}
	debugf("leave");
	return 0;
}

int op_rename(const char *source, const char *dest)
{
	int rt = 0;
	int rc;
	int destrt;

	char *p_src;
	char *r_src;
	char *p_dest;
	char *r_dest;

	ino_t src_ino;
	ino_t dest_ino;
	ino_t d_src_ino;
	ino_t d_dest_ino;
	struct inode *src_inode;
	struct inode *dest_inode;
	struct ufs_vnode *d_src_vnode = NULL;
	struct ufs_vnode *d_dest_vnode = NULL;
	struct ufs_vnode *dest_vnode = NULL;
	struct ufs_vnode *src_vnode = NULL;
	uufsd_t *ufs = current_ufs();

	debugf("source: %s, dest: %s", source, dest);

	rt = do_check_split(source, &p_src, &r_src);
	if (rt != 0) {
		debugf("do_check(%s); failed", source);
		return rt;
	}

	debugf("src_parent: %s, src_child: %s", p_src, r_src);

	rt = do_check_split(dest, &p_dest, &r_dest);
	if (rt != 0) {
		debugf("do_check(%s); failed", dest);
		goto out_free_vnodes;
	}

	debugf("dest_parent: %s, dest_child: %s", p_dest, r_dest);

	rt = do_readvnode(ufs, p_src, &d_src_ino, &d_src_vnode);
	if (rt != 0) {
		debugf("do_readinode(%s, &d_src_ino, &d_src_inode); failed", p_src);
		goto out_free_vnodes;
	}

	rt = do_readvnode(ufs, p_dest, &d_dest_ino, &d_dest_vnode);
	if (rt != 0) {
		debugf("do_readinode(%s, &d_dest_ino, &d_dest_inode); failed", p_dest);
		goto out_free_vnodes;
	}

	rt = do_readvnode(ufs, source, &src_ino, &src_vnode);
	if (rt != 0) {
		debugf("do_readvnode(%s, &src_ino, &src_vnode); failed", source);
		goto out_free_vnodes;
	}

	/* dest == ENOENT is okay */
	destrt = do_readvnode(ufs, dest, &dest_ino, &dest_vnode);
	if (destrt != 0 && destrt != -ENOENT) {
		debugf("do_readinode(%s, &dest_ino, &dest_inode); failed", dest);
		goto out_free_vnodes;
	}

	src_inode = vnode2inode(src_vnode);
	dest_inode = vnode2inode(dest_vnode);

	/* If  oldpath  and  newpath are existing hard links referring to the same
		 file, then rename() does nothing, and returns a success status. */
	if (destrt == 0 && src_ino == dest_ino) {
		rt = 0;
		goto out_free_vnodes;
	}

	/* error cases */
	/* EINVAL The  new  pathname  contained a path prefix of the old:
		 this should be checked by fuse */
	if (destrt == 0) {
		if (S_ISDIR(dest_inode->i_mode)) {
			/* EISDIR newpath  is  an  existing directory, but oldpath is not a direc‐
			   tory. */
			if (!(S_ISDIR(src_inode->i_mode))) {
				debugf("newpath is dir && oldpath is not a dir -> EISDIR");
				rt = -EISDIR;
				goto out_free_vnodes;
			}
			/* ENOTEMPTY newpath is a non-empty  directory */
			rt = do_check_empty_dir(ufs, dest_ino);
			if (rt != 0) {
				debugf("do_check_empty_dir dest %s failed",dest);
				goto out_free_vnodes;
			}
		}
		/* ENOTDIR: oldpath  is a directory, and newpath exists but is not a 
			 directory */
		if (S_ISDIR(src_inode->i_mode) &&
				!(S_ISDIR(dest_inode->i_mode))) {
			debugf("oldpath is dir && newpath is not a dir -> ENOTDIR");
			rt = -ENOTDIR;
			goto out_free_vnodes;
		}
	}

	/* Step 1: if destination exists: delete it */
	if (destrt == 0) {
		/* unlink in both cases */
		rc = ufs_unlink(ufs, d_dest_ino, r_dest, dest_ino, 0);
		if (rc) {
			debugf("ufs_unlink(ufs, %d, %s, %d, 0); failed", d_dest_ino, r_dest, dest_ino);
			rt = -EIO;
			goto out_free_vnodes;
		}

		if (S_ISDIR(dest_inode->i_mode)) {
			/* empty dir */
			rt = do_killfilebyinode(ufs, dest_ino, inode2vnode(dest_inode));
			if (rt) {
				debugf("do_killfilebyinode(r_ino, &r_inode); failed");
				goto out_free_vnodes;
			}
			/*
			rt = do_readinode(ufs, p_dest, &d_dest_ino, &d_dest_inode);
			if (rt) {
				debugf("do_readinode(p_dest, &d_dest_ino, &d_dest_inode); failed");
				goto out_free;
			}
			*/
			if (vnode2inode(d_dest_vnode)->i_nlink > 1) {
				vnode2inode(d_dest_vnode)->i_nlink--;
			}
			/*
			rc = ufs_write_inode(ufs, d_dest_ino, &d_dest_inode);
			if (rc) {
				debugf("ufs_write_inode(ufs, ino, inode); failed");
				rt = -EIO;
			}
			vnode_put(dest_vnode, 0);
			vnode_put(d_dest_vnode, 1);
			*/
		} else {
			/* file */
			if (dest_inode->i_nlink > 0) {
				dest_inode->i_nlink -= 1;
			}
			/*
			rc = vnode_put(dest_vnode, 1);
			if (rc) {
				debugf("vnode_put(dest_vnode,1); failed");
				rt = -EIO;
				goto out_free_vnodes;
			}
			*/
		}
	}
  	
	/* Step 2: add the link
	if (dest_ino == 0) {
		dest_ino = src_ino;
	}

	dest_vnode = vnode_get(ufs, dest_ino);
	if (dest_vnode == NULL) {
		debugf("do_readinode(%s, &dest_ino, &dest_inode); failed", dest);
		rt = -ENOMEM;
		goto out_free_vnodes;
	}
	*/
	do {
		debugf("calling ufs_link(ufs, %d, %s, %d, %d);", d_dest_ino, r_dest, src_ino, do_modetoufslag(src_inode->i_mode));
		rc = ufs_link(ufs, d_dest_ino, r_dest, src_vnode, src_inode->i_mode);
		if (rc != 0) {
	//		vnode_put(src_vnode, 1);
			debugf("ufs_link(ufs, %d, %s, %d, %d); failed", d_dest_ino, r_dest, src_ino, do_modetoufslag(src_inode->i_mode));
			rt = -EIO;
			goto out_free_vnodes;
		}

		if (rc == ENOSPC) {
			debugf("calling ufs_expand_dir(ufs, &d)", src_ino);
			if (ufs_expand_dir(ufs, d_dest_ino)) {
				debugf("error while expanding directory %s (%d)", p_dest, d_dest_ino);
				rt = -ENOSPC;
				goto out_free_vnodes;
			}
			rt = do_readvnode(ufs, p_dest, &d_dest_ino, &d_dest_vnode);
			if (rt != 0) {
				debugf("do_readvnode(%s, &d_dest_ino, &d_dest_inode); failed", p_dest);
				goto out_free_vnodes;
			}
		}
	} while (rc == ENOSPC);
	//vnode_put(dest_vnode, 1);
	//dest_vnode = NULL;

	/* Special case: if moving dir across different parents 
		 fix counters and '..' */
	if (S_ISDIR(src_inode->i_mode) && d_src_ino != d_dest_ino) {
		vnode2inode(d_dest_vnode)->i_nlink++;
		if (vnode2inode(d_src_vnode)->i_nlink > 1)
			vnode2inode(d_src_vnode)->i_nlink--;
		rc = ufs_write_inode(ufs, d_src_ino, d_src_vnode);
		if (rc != 0) {
			debugf("ufs_write_inode(ufs, src_ino, &src_inode); failed");
			rt = -EIO;
			goto out_free_vnodes;
		}
		rt = do_fix_dotdot(ufs, src_ino, d_dest_ino);
		if (rt != 0) {
			debugf("do_fix_dotdot failed");
			goto out_free_vnodes;
		}
	}

	/* utimes and inodes update */
	vnode2inode(d_dest_vnode)->i_mtime = vnode2inode(d_dest_vnode)->i_ctime = src_inode->i_ctime = ufs->now ? ufs->now : time(NULL);
	/*
	rc = ufs_write_inode(ufs, d_dest_ino, d_dest_vnode);
	if (rc != 0) {
		debugf("ufs_write_inode(ufs, d_dest_ino, &d_dest_inode); failed");
		rt = -EIO;
		goto out_free_vnodes;
	}
	rc = vnode_put(src_vnode, 1);
	if (rc != 0) {
		debugf("vnode_put(src_vnode,1); failed");
		rt = -EIO;
		goto out_free;
	}
	*/
	debugf("done");

	/* Step 3: delete the source */

	rc = ufs_unlink(ufs, d_src_ino, r_src, src_ino, 0);
	if (rc) {
		debugf("while unlinking src ino %d", (int) src_ino);
		rt = -EIO;
		goto out_free;
	} else {
		vnode2inode(src_vnode)->i_nlink--;
	}

out_free_vnodes:
	if (dest_vnode) {
		vnode_put(dest_vnode, 1);
	}
	if (src_vnode) {
		vnode_put(src_vnode, 1);
	}
	if (d_dest_vnode) {
		vnode_put(d_dest_vnode, 1);
	}
	if (d_src_vnode) {
		vnode_put(d_src_vnode, 1);
	}
out_free:
	free_split(p_src, r_src);
	free_split(p_dest, r_dest);
	//free_split(p_src, r_src);

	return rt;
}
