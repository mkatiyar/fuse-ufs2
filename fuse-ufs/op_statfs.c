/**
 * Copyright (c) 2013 Manish Katiyar <mkatiyar@gmail.com>
 * Copyright (c) 2016 Jan Blumschein <jan@jan-blumschein.de>
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

	/* Filesystem block size -- FreeBSD defines: "The preferred
	 * length of I/O requests for files on this file system"
	 */
	buf->f_bsize = fs->fs_bsize;

	/* Filesystem fragment size -- FreeBSD: "The size in bytes
	 * of the minimum unit of allocation on this file system"
	 */
	buf->f_frsize = fs->fs_fsize;


	/* Block counts (FreeBSD: "Allocation-block counts") */

	/* -- Total block count (GNU/Linux: "size of fs in f_frsize units") */
	buf->f_blocks = fs->fs_dsize;

	/* -- Number of free blocks */
	buf->f_bfree = blkstofrags(fs, fs->fs_cstotal.cs_nbfree) + fs->fs_cstotal.cs_nffree + dbtofsb(fs, fs->fs_pendingblocks);;

	/* -- Number of free blocks for unprivileged users */
	buf->f_bavail = freespace(fs, fs->fs_minfree) + dbtofsb(fs, fs->fs_pendingblocks);


	/* Inode counts (FreeBSD: "Counts of file serial numbers") */

	/* -- Total number of inodes */
	buf->f_files = fs->fs_ncg * fs->fs_ipg - ROOTINO;

	/* -- Number of free inodes */
	buf->f_ffree = fs->fs_cstotal.cs_nifree + fs->fs_pendinginodes;

	/* -- Number of free inodes for unprivileged users */
	buf->f_favail = buf->f_ffree; /* UFS does not reserve inodes */


	/* File system ID - "Not meaningful" on FreeBSD */
	buf->f_fsid = 0;

	/* "Flags describing mount options for this file system" */
	buf->f_flag = 0; /* The FUSE layer will set some flags */

	/* "The maximum length in bytes of a file name on this file system" */
	buf->f_namemax = MAXNAMLEN; /* = 255 on BSD */

	debugf("leave");
	return 0;
}
