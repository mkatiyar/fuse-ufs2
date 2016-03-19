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

void * op_init (struct fuse_conn_info *conn)
{
	int rc;
	struct fuse_context *cntx=fuse_get_context();
	struct ufs_data *ufsdata=cntx->private_data;
	struct fs *fs;
	char *buf;
	int i;

	debugf("enter %s", ufsdata->device);

	rc = ufs_disk_fillout(&ufsdata->ufs, ufsdata->device);
	if (rc) {
		debugf("Error while trying to open %s", ufsdata->device);
		exit(1);
	}

	fs = &ufsdata->ufs.d_fs;

	buf = malloc(fs->fs_bsize);
	if (!buf) {
		exit(1);
	}

	int size = fs->fs_cssize;
	int blks = howmany(size, fs->fs_fsize);
	int *lp;
	if (fs->fs_contigsumsize > 0)
		size += fs->fs_ncg * sizeof(int32_t);
	size += fs->fs_ncg * sizeof(u_int8_t);
	void *space = malloc(size);
	fs->fs_csp = space;

	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		if (bread(&ufsdata->ufs, fsbtodb(fs, fs->fs_csaddr + i), (void *)buf, size) == -1) {
			free(fs->fs_csp);
			exit(1);
		}
		bcopy(buf, space, (u_int)size);
		space = (char *)space + size;
	}
	if (fs->fs_contigsumsize > 0) {
		fs->fs_maxcluster = lp = space;
		for (i = 0; i < fs->fs_ncg; i++)
			*lp++ = fs->fs_contigsumsize;
		space = lp;
	}
	size = fs->fs_ncg * sizeof(u_int8_t);
	fs->fs_contigdirs = (u_int8_t *)space;
	bzero(fs->fs_contigdirs, size);
	fs->fs_active = NULL;

	/* honour readonly mount option: copy into temporary superblock field */
	fs->fs_ronly = ufsdata->readonly;

	debugf("FileSystem %s", (ufsdata->ufs.d_fs.fs_ronly == 0) ? "Read&Write" : "ReadOnly");
	debugf("leave");

	return ufsdata;
}
