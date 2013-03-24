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

void op_destroy (void *userdata)
{
	errcode_t rc;
	uufsd_t *ufs = current_ufs();
	struct fs *fs = &ufs->d_fs;
	int i;

	if (fs->fs_fmod) {
		for (i = 0; i < fs->fs_cssize; i += fs->fs_bsize) {
			if (blkwrite(ufs, fsbtodb(fs, fs->fs_csaddr + numfrags(fs, i)),
				     (void *)(((char *)fs->fs_csp) + i),
				     (size_t)(fs->fs_cssize - i < fs->fs_bsize ? fs->fs_cssize - i : fs->fs_bsize)) == -1) {
				fprintf(stderr, "Unable to flush superblock: %s", ufs->d_error);
			}
		}
		free(fs->fs_csp);
		fs->fs_csp = NULL;
		sbwrite(ufs, 1);
	}

	debugf("enter");
	rc = ufs_disk_close(ufs);
	if (rc) {
		debugf("Error while trying to close ufs filesystem");
	}
	debugf("leave");
}
