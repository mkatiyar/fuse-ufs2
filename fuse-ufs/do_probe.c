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

#define VOLNAME_SIZE_MAX 16

int do_probe (struct ufs_data *opts)
{
	errcode_t rc;
	uufsd_t ufs_disk;
	struct fs *fs = &ufs_disk.d_fs;
	char *buf;
	int i, error;

	rc = ufs_disk_fillout(&ufs_disk, opts->device);
	if (rc == -1) {
		debugf_main("Error while trying to open %s : %s)",
					opts->device, ufs_disk.d_error);
		return -1;
	}

	buf = malloc(fs->fs_bsize);
	if (!buf) {
		return -1;
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
		if ((error = bread(&ufs_disk, fsbtodb(fs, fs->fs_csaddr + i), (void *)buf, size)) != 0) {
			free(fs->fs_csp);
			goto out;
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

	opts->volname = (char *) malloc(sizeof(char) * (VOLNAME_SIZE_MAX + 1));
	if (opts->volname != NULL) {
		memset(opts->volname, 0, sizeof(char) * (VOLNAME_SIZE_MAX + 1));
		strncpy(opts->volname, ufs_disk.d_name, VOLNAME_SIZE_MAX);
		opts->volname[VOLNAME_SIZE_MAX] = '\0';
	}

out:
	/* Lazy to free the other stuff, this is userspace */
	free(buf);
	ufs_disk_close(&ufs_disk);
	return 0;
}
