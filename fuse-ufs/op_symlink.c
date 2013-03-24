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

int op_symlink (const char *sourcename, const char *destname)
{
	int rt;
	size_t wr;
	ufs_file_t efile;
	uufsd_t *ufs = current_ufs();
	int sourcelen = strlen(sourcename);

	debugf("enter");
	debugf("source: %s, dest: %s", sourcename, destname);

	/* a short symlink is stored in the inode (recycling the i_block array) */
	if (sourcelen < ((NDADDR + NIADDR) * sizeof(__u32))) {
		rt = do_create(ufs, destname, LINUX_S_IFLNK | 0777, 0, sourcename);
		if (rt != 0) {
			debugf("do_create(%s, LINUX_S_IFLNK | 0777, FAST); failed", destname);
			return rt;
		}
	} else {
		rt = do_create(ufs, destname, LINUX_S_IFLNK | 0777, 0, NULL);
		if (rt != 0) {
			debugf("do_create(%s, LINUX_S_IFLNK | 0777); failed", destname);
			return rt;
		}
		efile = do_open(ufs, destname, O_WRONLY);
		if (efile == NULL) {
			debugf("do_open(%s); failed", destname);
			return -EIO;
		}
		wr = do_write(efile, sourcename, sourcelen, 0);
		if (wr != strlen(sourcename)) {
			debugf("do_write(efile, %s, %d, 0); failed", sourcename, strlen(sourcename) + 1);
			return -EIO;
		}
		rt = do_release(efile);
		if (rt != 0) {
			debugf("do_release(efile); failed");
			return rt;
		}
	}
	debugf("leave");
	return 0;
}
