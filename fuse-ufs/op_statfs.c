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

int op_statfs (const char *path, struct statvfs *buf)
{
	uufsd_t *ufs = current_ufs();
	struct fs *fs = &ufs->d_fs;

	debugf("enter");

	memset(buf, 0, sizeof(struct statvfs));

	buf->f_blocks = fs->fs_dsize;
	buf->f_bfree = blkstofrags(fs, fs->fs_cstotal.cs_nbfree) + fs->fs_cstotal.cs_nffree + dbtofsb(fs, fs->fs_pendingblocks);;
	buf->f_bsize  = fs->fs_bsize;
	buf->f_frsize = fs->fs_fsize;
	buf->f_bavail = freespace(fs, fs->fs_minfree) + dbtofsb(fs, fs->fs_pendingblocks);
	buf->f_files = fs->fs_ncg * fs->fs_ipg - ROOTINO;
	buf->f_ffree = fs->fs_cstotal.cs_nifree + fs->fs_pendinginodes;

	debugf("leave");
	return 0;
}
