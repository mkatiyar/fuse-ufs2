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

int op_access (const char *path, int mask)
{
	uufsd_t *ufs = current_ufs();

	debugf("enter");
	debugf("path = %s, mask = 0%o", path, mask);

	/* This is redundant: it should have been handled by the fuse layer
	 * (it will be unless we disagree about the readonly mount flag)
	 */
	if ((mask & W_OK) && (ufs->d_fs.fs_ronly)) {
		debugf("FIXME: fuse calls access(W_OK) on read-only mount");
		return -EROFS;
	}

	debugf("leave");
	return 0;
}
