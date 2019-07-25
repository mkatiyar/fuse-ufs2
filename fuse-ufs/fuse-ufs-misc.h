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

#ifndef __UFS_MISC__
#define __UFS_MISC__

#include "fuse-ufs.h"

#define _INLINE_ static inline

#define UFS_FILE_WRITE		0x0001
#define UFS_FILE_CREATE	0x0002

#define UFS_FILE_MASK		0x00FF
#define UFS_FLAG_RW			0x01
#define UFS_FLAG_CHANGED		0x02
#define UFS_FLAG_DIRTY			0x04
#define UFS_FLAG_VALID			0x08
#define UFS_FLAG_IB_DIRTY		0x10
#define UFS_FLAG_BB_DIRTY		0x20
#define UFS_FLAG_SWAP_BYTES		0x40
#define UFS_FLAG_SWAP_BYTES_READ	0x80
#define UFS_FLAG_SWAP_BYTES_WRITE	0x100
#define UFS_FLAG_MASTER_SB_ONLY	0x200
#define UFS_FLAG_FORCE			0x400
#define UFS_FLAG_SUPER_ONLY		0x800
#define UFS_FLAG_JOURNAL_DEV_OK	0x1000
#define UFS_FLAG_IMAGE_FILE		0x2000
#define UFS_FLAG_EXCLUSIVE		0x4000
#define UFS_FLAG_SOFTSUPP_FEATURES	0x8000
#define UFS_FLAG_NOFREE_ON_ERROR	0x10000

#define UFS_MAGIC_FILE	0xbadb00b


#define UFS_FILE_BUF_DIRTY	0x4000
#define UFS_FILE_BUF_VALID	0x2000

#define UFS_SEEK_SET	0
#define UFS_SEEK_CUR	1
#define UFS_SEEK_END	2

struct ufs_file {
	long		magic;
	struct uufsd 	*fs;
	ino_t		ino;
	struct ufs_vnode *inode;
	int 			flags;
	__u64			pos;
	blk_t			blockno;
	ufs2_daddr_t		physblock;
	char 			*buf;
	size_t lread; /* Size of valid data in buf */
};

typedef struct ufs_file *ufs_file_t;

/*
 *  Allocate memory
 */
_INLINE_ int ufs_get_mem(unsigned long size, void *ptr)
{
	void *pp;

	pp = malloc(size);
	if (!pp)
		return ENOMEM;
	memcpy(ptr, &pp, sizeof (pp));
	return 0;
}

_INLINE_ int ufs_get_memzero(unsigned long size, void *ptr)
{
	void *pp;

	pp = malloc(size);
	if (!pp)
		return ENOMEM;
	memset(pp, 0, size);
	memcpy(ptr, &pp, sizeof(pp));
	return 0;
}

_INLINE_ int ufs_get_array(unsigned long count, unsigned long size, void *ptr)
{
	if (count && (-1UL)/count<size)
		return ENOMEM;
	return ufs_get_mem(count*size, ptr);
}

_INLINE_ int ufs_get_arrayzero(unsigned long count,
					unsigned long size, void *ptr)
{
	void *pp;

	if (count && (-1UL)/count<size)
		return ENOMEM;
	pp = calloc(count, size);
	if (!pp)
		return ENOMEM;
	memcpy(ptr, &pp, sizeof(pp));
	return 0;
}

/*
 * Free memory
 */
_INLINE_ int ufs_free_mem(void *ptr)
{
	void *p;

	memcpy(&p, ptr, sizeof(p));
	free(p);
	p = 0;
	memcpy(ptr, &p, sizeof(p));
	return 0;
}

/*
 *  Resize memory
 */
_INLINE_ int ufs_resize_mem(unsigned long old_size,
				     unsigned long size, void *ptr)
{
	void *p;

	/* Use "memcpy" for pointer assignments here to avoid problems
	 * with C99 strict type aliasing rules. */
	memcpy(&p, ptr, sizeof(p));
	p = realloc(p, size);
	if (!p)
		return ENOMEM;
	memcpy(ptr, &p, sizeof(p));
	return 0;
}
#endif
