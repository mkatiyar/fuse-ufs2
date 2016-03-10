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

static int ufs_new_dir_block(uufsd_t *ufs, ino_t dir_ino,
		struct ufs_vnode *parent, char **block)
{
	struct direct 	*dir = NULL;
	errcode_t		retval;
	char			*buf;
	int			rec_len;
	struct fs *fs = &ufs->d_fs;
	int dirsize = DIRBLKSIZ;
	int blocksize = fragroundup(fs, dirsize);

	retval = ufs_get_mem(blocksize, &buf);
	if (retval)
		return retval;
	memset(buf, 0, blocksize);
	dir = (struct direct *) buf;

	//retval = ufs_set_rec_len(ufs, dirsize, dir);
	//if (retval)
		//return retval;

	if (dir_ino) {
		/*
		 * Set up entry for '.'
		 */
		dir->d_ino = dir_ino;
		dir->d_namlen = 1;
		dir->d_name[0] = '.';
		dir->d_type = DT_DIR;
		rec_len = dirsize - UFS_DIR_REC_LEN(1);
		dir->d_reclen = UFS_DIR_REC_LEN(1);

		/*
		 * Set up entry for '..'
		 */
		dir = (struct direct *) (buf + dir->d_reclen);
		//retval = ufs_set_rec_len(ufs, rec_len, dir);
		//if (retval)
			//return retval;
		dir->d_ino = vnode2inode(parent)->i_number;
		dir->d_namlen = 2;
		dir->d_name[0] = '.';
		dir->d_name[1] = '.';
		dir->d_type = DT_DIR;
		dir->d_reclen = rec_len;
	}
	*block = buf;
	return 0;
}

static int ufs_mkdir(uufsd_t *ufs, ino_t parent, ino_t inum, char *name)
{
	errcode_t		retval;
	struct ufs_vnode	*parent_vnode = NULL, *vnode = NULL;
	struct inode *parent_inode, *inode;
	ino_t		ino = inum;
	ino_t		scratch_ino;
	ufs2_daddr_t		blk;
	char			*block = 0;
	struct fs *fs = &ufs->d_fs;
	int dirsize = DIRBLKSIZ;
	int blocksize = fragroundup(fs, dirsize);

	parent_vnode = vnode_get(ufs, parent);
	if (!parent_vnode) {
		return ENOENT;
	}

	parent_inode = vnode2inode(parent_vnode);
	/*
	 * Allocate an inode, if necessary
	 */
	if (!ino) {
		retval = ufs_valloc(parent_vnode, DTTOIF(DT_DIR), &vnode);
		if (retval)
			goto cleanup;
		ino = vnode->inode.i_number;
		inode = vnode2inode(vnode);
	}

	/*
	 * Allocate a data block for the directory
	 */
	retval = ufs_block_alloc(ufs, inode, fragroundup(fs, dirsize), &blk);
	if (retval)
		goto cleanup;

	/*
	 * Create a scratch template for the directory
	 */
	retval = ufs_new_dir_block(ufs, vnode->inode.i_number, parent_vnode, &block);
	if (retval)
		goto cleanup;

	/*
	 * Get the parent's inode, if necessary
	if (parent != ino) {
		parent_vnode = vnode_get(ufs, parent);
		if (retval)
			goto cleanup;
	} else
		memset(&parent_inode, 0, sizeof(parent_inode));
	 */

	/*
	 * Create the inode structure....
	 */
	inode->i_mode = DT_DIR | (0777);
	inode->i_uid = inode->i_gid = 0;
	UFS_DINODE(inode)->di_db[0] = blk;
	inode->i_nlink = 1;
	inode->i_size = dirsize;

	/*
	 * Write out the inode and inode data block
	 */
	retval = blkwrite(ufs, fsbtodb(fs, blk), block, blocksize);
	if (retval == -1)
		goto cleanup;

	/*
	 * Link the directory into the filesystem hierarchy
	 */
	if (name) {
		retval = ufs_lookup(ufs, parent, name, strlen(name),
				       &scratch_ino);
		if (!retval) {
			retval = EEXIST;
			name = 0;
			goto cleanup;
		}
		if (retval != ENOENT)
			goto cleanup;
		retval = ufs_link(ufs, parent, name, vnode, DTTOIF(DT_DIR));
		if (retval)
			goto cleanup;
	}

	/*
	 * Update parent inode's counts
	 */
	if (parent != ino) {
		parent_inode->i_nlink++;
	}

cleanup:
	if (vnode)
		vnode_put(vnode, 1);

	if (parent_vnode)
		vnode_put(parent_vnode, 1);
	if (block)
		ufs_free_mem(&block);
	return retval;


}

int op_mkdir (const char *path, mode_t mode)
{
	int rt;
	time_t tm;
	errcode_t rc;

	char *p_path;
	char *r_path;

	ino_t ino;
	struct ufs_vnode *vnode;
	struct ufs_vnode *child_vnode;
	struct inode *inode;

	struct fuse_context *ctx;

	uufsd_t *ufs = current_ufs();

	RETURN_IF_RDONLY(ufs);

	debugf("enter");
	debugf("path = %s, mode: 0%o, dir:0%o", path, mode, S_IFDIR);

	rt=do_check_split(path, &p_path ,&r_path);
	if (rt != 0) {
		debugf("do_check(%s); failed", path);
		return rt;
	}

	debugf("parent: %s, child: %s, pathmax: %d", p_path, r_path, PATH_MAX);

	rt = do_readvnode(ufs, p_path, &ino, &vnode);
	if (!vnode) {
		debugf("do_readvnode(%s, &ino, &vnode); failed", p_path);
		free_split(p_path, r_path);
		return rt;
	}

	do {
		debugf("calling ufs_mkdir(ufs, %d, 0, %s);", ino, r_path);
		rc = ufs_mkdir(ufs, ino, 0, r_path);
		if (rc == ENOSPC) {
			debugf("calling ufs_expand_dir(ufs, &d)", ino);
			/*
			if (ufs_expand_dir(ufs, ino)) {
				debugf("error while expanding directory %s (%d)", p_path, ino);
				free_split(p_path, r_path);
				return -ENOSPC;
			}
			*/
		}
	} while (rc == ENOSPC);
	if (rc) {
		debugf("ufs_mkdir(ufs, %d, 0, %s); failed (%d)", ino, r_path, rc);
		free_split(p_path, r_path);
		return -EIO;
	}

	rt = do_readvnode(ufs, path, &ino, &child_vnode);
	if (rt) {
		debugf("do_readvnode(%s, &ino, &child_vnode); failed", path);
		return -EIO;
	}
	tm = ufs->now ? ufs->now : time(NULL);
	inode = vnode2inode(child_vnode);
	inode->i_mode = S_IFDIR | mode;
	inode->i_ctime = inode->i_atime = inode->i_mtime = tm;
	ctx = fuse_get_context();
	if (ctx) {
		inode->i_uid = ctx->uid;
		inode->i_gid = ctx->gid;
	}

	vnode_put(child_vnode, 1);

	inode = vnode2inode(vnode);
	inode->i_ctime = inode->i_mtime = tm;

	vnode_put(vnode, 1);

	free_split(p_path, r_path);

	debugf("leave");
	return 0;
}
