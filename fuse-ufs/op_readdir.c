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

struct dir_walk_data {
	char *buf;
	fuse_fill_dir_t filler;
};

static int walk_dir (struct direct *de, int offset, char *buf, void *priv_data)
{
	int ret;
	size_t flen;
	char *fname;
	struct dir_walk_data *b = priv_data;
	struct stat st;
	memset(&st, 0, sizeof(st));

	debugf("enter");

	st.st_ino=de->d_ino;
	st.__st_ino=de->d_ino;
//	st.st_mode=type<<12;

	flen = de->d_namlen & 0xff;
	fname = (char *) malloc(sizeof(char) * (flen + 1));
	if (fname == NULL) {
		debugf("s = (char *) malloc(sizeof(char) * (%d + 1)); failed", flen);
		return -ENOMEM;
	}
	snprintf(fname, flen + 1, "%s", de->d_name);
	debugf("b->filler(b->buf, %s, NULL, 0);", fname);
	ret = b->filler(b->buf, fname, &st, 0);
	free(fname);

	debugf("leave");
	return ret;
}

int op_readdir (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	int rt;
	errcode_t rc;
	ino_t ino;
	struct ufs_vnode *vnode;
	struct dir_walk_data dwd={
		.buf = buf,
		.filler = filler};
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s", path);

	rt = do_readvnode(ufs, path, &ino, &vnode);
	if (rt) {
		debugf("do_readinode(%s, &ino, &inode); failed", path);
		return rt;
	}

	rc = ufs_dir_iterate(ufs, ino, 0, buf, walk_dir, &dwd);
	if (rc) {
		debugf("Error while trying to ufs_dir_iterate %s", path);
		vnode_put(vnode, 0);
		return -EIO;
	}

	vnode_put(vnode, 0);
	debugf("leave");
	return 0;
}
