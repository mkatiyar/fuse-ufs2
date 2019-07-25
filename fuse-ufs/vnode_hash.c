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

//#define VNODE_DEBUG 1

#define VNODE_HASH_SIZE 256
#define VNODE_HASH_MASK (VNODE_HASH_SIZE-1)

#if !defined(VNODE_DEBUG)
#undef debugf
#define debugf(a...) do { } while (0)
#endif

static struct ufs_vnode *ht_head[VNODE_HASH_SIZE];

static inline struct ufs_vnode * vnode_alloc (void)
{
	struct ufs_vnode *new;
	new = (struct ufs_vnode *) malloc(sizeof(struct ufs_vnode));
	if (new) {
		bzero(new, sizeof(struct ufs_vnode));
	}
	return new;
}

static inline void vnode_free (struct ufs_vnode *vnode)
{
	vnode->ino = 0;
	free(vnode);
}

static inline int vnode_hash_key(uufsd_t *ufsp, ino_t ino)
{
	return ((int) ufsp + ino) & VNODE_HASH_MASK;
}

struct ufs_vnode * vnode_get(uufsd_t *ufsp, ino_t ino)
{
	int hash_key = vnode_hash_key(ufsp, ino);
	struct ufs_vnode *rv = ht_head[hash_key];

	while (rv != NULL && rv->ino != ino) {
		rv = rv->nexthash;
	}
	if (rv != NULL) {
//		rv->retaddr[rv->count] = __builtin_return_address(0);
		rv->count++;
		debugf("increased hash:%p use count:%d", rv, rv->count);
		return rv;
	} else {
		struct ufs_vnode *new = vnode_alloc();
		if (new != NULL) {
			int rc;
			int mode;
			struct ufs2_dinode *dinop = NULL;

			if (ino) {
				rc = getino(ufsp, (void **)&dinop, ino, &mode);
				if (rc != 0) {
					vnode_free(new);
					debugf("leave error");
					return NULL;
				}
				copy_ondisk_to_incore(ufsp, &new->inode, dinop, ino);
			}

			new->inode.i_vnode = (struct vnode *)new;
			new->ufsp = ufsp;
			new->ino = ino;
			new->count = 1;

			if (ht_head[hash_key] != NULL) {
				ht_head[hash_key]->pprevhash = &(new->nexthash);
			}
			new->nexthash = ht_head[hash_key];
			new->pprevhash = &(ht_head[hash_key]);
			ht_head[hash_key] = new;
			debugf("added hash:%p", new);
		}
		return new;
	}
}

int vnode_put (struct ufs_vnode *vnode, int dirty)
{
	int rt = 0;
	vnode->count--;
	if (dirty) {
		copy_incore_to_ondisk(vnode2inode(vnode), UFS_DINODE(vnode2inode(vnode)));
	}

	if (vnode->count <= 0) {
		debugf("deleting hash:%p", vnode);
		if (vnode->inode.i_nlink < 1) {
			rt = do_killfilebyinode(vnode->ufsp, vnode->ino, vnode);
		} else if (dirty) {
			rt = ufs_write_inode(vnode->ufsp, vnode->ino, vnode);
		}
		*(vnode->pprevhash) = vnode->nexthash;
		if (vnode->nexthash) {
			vnode->nexthash->pprevhash = vnode->pprevhash;
		}
		vnode_free(vnode);
	} else if (dirty) {
		rt = ufs_write_inode(vnode->ufsp, vnode->ino, vnode);
	}
	return rt;
}
