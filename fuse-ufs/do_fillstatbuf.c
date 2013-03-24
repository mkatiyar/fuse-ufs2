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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static inline dev_t old_decode_dev(__u16 val)
{
	return makedev((val >> 8) & 255, val & 255);
}

static inline dev_t new_decode_dev(__u32 dev)
{
	unsigned major = (dev & 0xfff00) >> 8;
	unsigned minor = (dev & 0xff) | ((dev >> 12) & 0xfff00);
	return makedev(major, minor);
}

void do_fillstatbuf (uufsd_t *ufs, ino_t ino, struct inode *inode, struct stat *st)
{
	debugf("enter");
	memset(st, 0, sizeof(*st));
	/* XXX workaround
	 * should be unique and != existing devices */
	st->st_dev = (dev_t) ((long) ufs);
	st->st_ino = inode->i_number;
	st->__st_ino = inode->i_number;
	st->st_mode = inode->i_mode;
	st->st_nlink = inode->i_nlink;
	st->st_uid = inode->i_uid;	/* add in uid_high */
	st->st_gid = inode->i_gid;	/* add in gid_high */
	st->st_size = inode->i_size;
#if __FreeBSD__ == 10
	st->st_gen = inode->i_gen;
#endif
	st->st_blksize = ufs->d_fs.fs_fsize;
	st->st_blocks = inode->i_blocks;
	st->st_atime = inode->i_atime;
	st->st_mtime = inode->i_mtime;
	st->st_ctime = inode->i_ctime;
	debugf("leave");
}
