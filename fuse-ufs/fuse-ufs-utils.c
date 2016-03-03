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
#include <sys/param.h>
#define UFS_FILE_NOT_FOUND ENOENT
#define DIRENT_ABORT 2

static int
ufs_open_namei(uufsd_t *ufs, ino_t root, ino_t base, const char *path, int pathlen,
		int follow, int link_count, ino_t *res_inode);

struct lookup_struct  {
	const char *name;
	int	len;
	ino_t	*inode;
	int	found;
};

static int lookup_proc(struct direct *dirent, int inum, char *buf, void *priv_data)
{
	struct lookup_struct *ls = (struct lookup_struct *) priv_data;

	if (dirent->d_ino==0) /* skip unused dentry */
		return 0;

	if (ls->len != (dirent->d_namlen & 0xFF))
		return 0;
	if (strncmp(ls->name, dirent->d_name, (dirent->d_namlen & 0xFF)))
		return 0;
	*ls->inode = dirent->d_ino;
	ls->found++;
	return DIRENT_ABORT;
}

int ufs_dir_iterate(uufsd_t *ufs, ino_t dirino,
		    int (*func)(
					  struct direct *dirent,
					  int n,
					  char *buf,
					  void	*priv_data),
			      void *priv_data)
{
	int i, ret = 0;
	ufs2_daddr_t ndb;
	ufs2_daddr_t blkno;
	int blksize = ufs->d_fs.fs_bsize;
	u_int64_t dir_size;
	char *dirbuf = NULL;
	struct ufs_vnode *vnode;

	vnode = vnode_get(ufs, dirino);
	if (vnode == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	dir_size = vnode2inode(vnode)->i_size;

	dirbuf = malloc(blksize);
	if (!dirbuf) {
		ret = -ENOMEM;
		goto out;
	}

	ndb = howmany(dir_size, ufs->d_fs.fs_bsize);
	int offset, pos = 0;
	for (i = 0; i < ndb ; i++) {
		ret = ufs_bmap(ufs, dirino, vnode, i, &blkno);
		if (ret) {
			ret = -EIO;
			goto out;
		}
		blksize = ufs_inode_io_size(vnode2inode(vnode), pos, 0);
		if (blkread(ufs, fsbtodb(&ufs->d_fs, blkno), dirbuf, blksize) == -1) {
			debugf("Unable to read block %d\n",blkno);
			ret = -EIO;
			goto out;
		}
		offset = 0;
		while (offset < blksize && pos + offset < dir_size) {
			struct direct *de = (struct direct *)(dirbuf + offset);
			ret = (*func)(de, offset, dirbuf, priv_data);
			if (ret & DIRENT_CHANGED) {
				if (blkwrite(ufs, fsbtodb(&ufs->d_fs, blkno), dirbuf, blksize) == -1) {
					debugf("Unable to write block %d\n",blkno);
					ret = -EIO;
					goto out;
				}
			}
			if (ret & DIRENT_ABORT) {
				ret = 0;
				goto out;
			}
			offset += de->d_reclen;
		}
		pos += blksize;
	}

out:
	if (vnode) {
		vnode_put(vnode, 0);
	}
	if (dirbuf) {
		free(dirbuf);
	}
	return ret;
}

int ufs_lookup(uufsd_t *ufs, ino_t dir, const char *name, int namelen,
		ino_t *ino)
{
	int	retval;
	struct lookup_struct ls;

	/* Initialize it to something sensible */
	*ino = 0;

	ls.name = name;
	ls.len = namelen;
	ls.inode = ino;
	ls.found = 0;

	retval = ufs_dir_iterate(ufs, dir, lookup_proc, &ls);
	if (retval)
		return retval;

	return (ls.found) ? 0 : UFS_FILE_NOT_FOUND;
}

static int
ufs_dir_namei(uufsd_t *ufs, ino_t root, ino_t dir, const char *pathname, int pathlen,
		int link_count, const char **name, int *namelen,
		ino_t *res_ino)
{
	char c;
	const char *thisname;
	int len;
	ino_t inode;
	int retval;

	if ((c = *pathname) == '/') {
		dir = root;
		pathname++;
		pathlen--;
	}
	while (1) {
		thisname = pathname;
		for (len=0; --pathlen >= 0;len++) {
			c = *(pathname++);
			if (c == '/')
				break;
		}
		if (pathlen < 0)
			break;
		retval = ufs_lookup (ufs, dir, thisname, len, &inode);
		if (retval) return retval;
		dir = inode;
	}
	*name = thisname;
	*namelen = len;
	*res_ino = dir;
	return 0;
}

static int
ufs_follow_link(uufsd_t *ufs, ino_t root, ino_t dir, ino_t inode, int link_count,
				ino_t *res_inode)
{
	char *pathname;
	char *buffer = 0;
	errcode_t retval;
	struct ufs_vnode *vnode;
	struct inode *inodep;
	int blocksize = ufs->d_fs.fs_bsize;

	vnode = vnode_get(ufs, inode);
	if (!vnode) {
		return -ENOMEM;
	}

	inodep = vnode2inode(vnode);

	if (!LINUX_S_ISLNK(inodep->i_mode)) {
		*res_inode = inode;
		retval = 0;
		goto out;
	}

	if (link_count++ > 5) {
		retval = -EMLINK;
		goto out;
	}
	if (inodep->i_blocks) {
		retval = ufs_get_mem(blocksize, &buffer);
		if (retval)
			goto out;
		retval = blkread(ufs, UFS_DINODE(inodep)->di_db[0], buffer, blocksize);
		if (retval) {
			goto out;
		}
		pathname = buffer;
	} else
		pathname = (char *)&(UFS_DINODE(inodep)->di_db[0]);
	retval = ufs_open_namei(ufs, root, dir, pathname, inodep->i_size, 1,
			link_count, res_inode);
out:
	vnode_put(vnode, 0);

	if (buffer)
		ufs_free_mem(&buffer);
	return retval;
}

static int
ufs_open_namei(uufsd_t *ufs, ino_t root, ino_t base, const char *path, int pathlen,
		int follow, int link_count, ino_t *res_inode)
{
	int retval, namelen;
	const char *base_name;
	ino_t dir, inode;

	retval = ufs_dir_namei(ufs, root, base, path, pathlen, link_count, &base_name, &namelen, &dir);
	if (retval) return retval;

	if (!namelen) {
		*res_inode = dir;
		return 0;
	}

	retval = ufs_lookup(ufs, dir, base_name, namelen, &inode);

	if (follow) {
		retval = ufs_follow_link(ufs, root, dir, inode, link_count,
				&inode);
		if (retval)
			return retval;
	}

	if (retval) {
		*res_inode = 0;
	} else {
		*res_inode = inode;
	}
	return retval;
}

/* Lookup a filename in a directory inode and return the
 * the inode number if found.
 */
int ufs_namei(uufsd_t *ufs, ino_t root_ino, ino_t cur_ino, const char *filename, ino_t *ino)
{
	int ret = ufs_open_namei(ufs, root_ino, cur_ino, filename,
				 strlen(filename), 0, 0, ino);

	return ret;
}
