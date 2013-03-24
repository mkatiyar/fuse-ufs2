/**
 * Copyright (c) 2008-2009 Alper Akcan <alper.akcan@gmail.com>
 * Copyright (c) 2009 Renzo Davoli <renzo@cs.unibo.it>
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

int op_truncate(const char *path, off_t length)
{
	int rt;
	ufs_file_t file;
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s", path);

	rt = do_check(path);
	if (rt != 0) {
		debugf("do_check(%s); failed", path);
		return rt;
	}
	file = do_open(ufs, path, O_WRONLY);
	if (file == NULL) {
		debugf("do_open(%s); failed", path);
		return -ENOENT;
	}

	rt = ufs_file_set_size(file, length);
	if (rt) {
		do_release(file);
		debugf("ufs_file_set_size(file, %d); failed", length);
		return rt;
	}

	rt = do_release(file);
	if (rt != 0) {
		debugf("do_release(file); failed");
		return rt;
	}

	debugf("leave");
	return 0;
}

int op_ftruncate(const char *path, off_t length, struct fuse_file_info *fi)
{
	size_t rt;
	ufs_file_t file = UFS_FILE(fi->fh);

	debugf("enter");
	debugf("path = %s", path);

	rt = ufs_file_set_size(file, length);
	if (rt) {
		debugf("ufs_file_set_size(file, %d); failed", length);
		return rt;
	}

	debugf("leave");
	return 0;
}
