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

errcode_t ufs_file_set_size (ufs_file_t file, __u64 size);

size_t do_write (ufs_file_t efile, const char *buf, size_t size, off_t offset)
{
	int rt;
	const char *tmp;
	unsigned int wr;
	unsigned long long npos;
	unsigned long long fsize;

	debugf("enter");

	rt = ufs_file_get_size(efile, &fsize);
	if (rt != 0) {
		debugf("ufs_file_get_size(efile, &fsize); failed");
		return rt;
	}
	if (offset + size > fsize) {
		rt = ufs_file_set_size(efile, offset + size);
		if (rt) {
			debugf("ufs_file_set_size(efile, %lld); failed", offset + size);
			return rt;
		}
	}

	rt = ufs_file_lseek(efile, offset, SEEK_SET, &npos);
	if (rt) {
		debugf("ufs_file_lseek(efile, %lld, SEEK_SET, &npos); failed", offset);
		return rt;
	}

	for (rt = 0, wr = 0, tmp = buf; size > 0 && rt == 0; size -= wr, tmp += wr) {
		debugf("size: %u, written: %u", size, wr);
		rt = ufs_file_write(efile, tmp, size, &wr);
	}
	if (rt) {
		debugf("ufs_file_write(edile, tmp, size, &wr); failed");
		return rt;
	}

	rt = ufs_file_flush(efile);
	if (rt) {
		debugf("ufs_file_flush(efile); failed");
		return rt;
	}

	debugf("leave");
	return wr;
}

int op_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	size_t rt;
	ufs_file_t efile = UFS_FILE(fi->fh);

	debugf("enter");
	debugf("path = %s", path);

	rt = do_write(efile, buf, size, offset);

	debugf("leave");
	return rt;
}
