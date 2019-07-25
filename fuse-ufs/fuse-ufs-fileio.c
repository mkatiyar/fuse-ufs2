/*
 * fileio.c --- Simple file I/O routines
 *
 * Copyright (C) 1997 Theodore Ts'o.
 * Copyright (C) 2013 Manish Katiyar <mkatiyar@gmail.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "fuse-ufs.h"

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/param.h>

#define UFS_FILE_SHARED_INODE 0x8000

int
ufs_inode_io_size(struct inode *inode, int offset, int write)
{
	struct fs *fs = inode->i_fs;
	size_t filesize = UFS_DINODE(inode)->di_size;
	int blksize = fs->fs_bsize;
	int fragsize = fs->fs_fsize;

	if (filesize >= blksize * NDADDR) {
		return blksize;
	} else {
		if (write) {
			/* Need to differentiate between load for read
			 * and write. For writes, offset means iosize is from
			 * start of the block to offset. For reads, iosize is
			 * from offset till valid contents in block.
			 */
			int nfrags = numfrags(fs, fragroundup(fs, offset));
			if (nfrags) {
				return (nfrags % fs->fs_frag) ? (nfrags % fs->fs_frag) * fragsize : blksize;
			} else {
				return 0;
			}
		} else {
			//return MIN(blksize, fragroundup(fs, inode->i_size - offset + 1));
			return MIN(blksize, fragroundup(fs, inode->i_size - offset));
		}
	}
}

static int
ufs_io_size(ufs_file_t file, int offset, int write)
{
	return ufs_inode_io_size(vnode2inode(file->inode), offset, write);
}

int ufs_file_open2(uufsd_t *fs, ino_t ino,
			    struct ufs_vnode *vnode,
			    int flags, ufs_file_t *ret)
{
	ufs_file_t 	file;
	int		retval;

	/*
	 * Don't let caller create or open a file for writing if the
	 * filesystem is read-only.
	 */
	if ((flags & (UFS_FILE_WRITE | UFS_FILE_CREATE)) && (fs->d_fs.fs_ronly))
		return EROFS;

	retval = ufs_get_mem(sizeof(struct ufs_file), &file);
	if (retval)
		return retval;

	memset(file, 0, sizeof(struct ufs_file));
	file->magic = UFS_MAGIC_FILE;
	file->fs = fs;
	file->ino = ino;
	file->flags = flags & (UFS_FILE_MASK | UFS_FILE_SHARED_INODE);
	file->inode = vnode;

	/*
	if (flags & UFS_FILE_SHARED_INODE)
		file->inode = vnode;
	else {
		retval = ufs_get_mem(sizeof(struct ufs_vnode), &file->inode);
		if (retval)
			goto fail_inode_alloc;
		if (vnode) {
			memcpy(file->inode, vnode, sizeof(struct ufs_vnode));
		} else {
			file->inode = vnode_get(fs, ino);
			if (!file->inode)
				goto fail;
		}
	}

	retval = ufs_get_array(3, fs->d_fs.fs_bsize, &file->buf);
	*/
	retval = ufs_get_mem(fs->d_fs.fs_bsize, &file->buf);
	if (retval)
		goto fail_inode_alloc;

	*ret = file;
	return 0;

fail_inode_alloc:
	if (file->buf)
		ufs_free_mem(&file->buf);
	ufs_free_mem(&file);
	return retval;
}

int ufs_file_open(uufsd_t * fs, ino_t ino,
			   int flags, ufs_file_t *ret)
{
	return ufs_file_open2(fs, ino, NULL, flags & ~(UFS_FILE_SHARED_INODE), ret);
}

/*
 * This function returns the filesystem handle of a file from the structure
 */
uufsd_t * ufs_file_get_fs(ufs_file_t file)
{
	if (file->magic != UFS_MAGIC_FILE)
		return NULL;
	return file->fs;
}

int
ufs_set_block(uufsd_t *fs, struct inode *inode, blk_t fbn, ufs2_daddr_t blockno)
{
	ufs2_daddr_t tempblock = 0;
	char *blockbuf = 0;
	int ret, blksize = fs->d_fs.fs_bsize;
	int nindir = fs->d_fs.fs_nindir;

	ret = ufs_get_memzero(blksize, &blockbuf);
	if (ret) {
		return ret;
	}

	if (fbn < NDADDR) {
		inode->i_din2.di_db[fbn] = blockno;
	} else {
		fbn = fbn - NDADDR;
		int indir = fbn / nindir;
		int index;
		if (indir == 0) {
			/* Block lies in 1st indirect block */
			index = fbn % nindir;
			tempblock = inode->i_din2.di_ib[0];
			if (tempblock) {
				if(blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", tempblock);
					ufs_free_mem(&blockbuf);
					return -ENOMEM;
				}
			} else {
				/* Need to allocate an indirect block */
				ret = ufs_block_alloc(fs, inode, blksize, &tempblock);
				if (ret) {
					debugf("Unable to allocate block %d\n", tempblock);
					ufs_free_mem(&blockbuf);
					return ret;
				}
				inode->i_din2.di_ib[0] = tempblock;
			}
			*((ufs2_daddr_t *)blockbuf + index) = blockno;
			ret = blkwrite(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize);
			if (ret <= 0) {
				debugf("Unable to write block %d\n", tempblock);
				ufs_free_mem(&blockbuf);
				return -EIO;
			}
		} else {
			fbn = fbn - nindir;
			indir = fbn / (nindir * nindir);
			if (indir == 0) {
				/* Block lies in 2nd indirect block */
				index = fbn / nindir;
				tempblock = inode->i_din2.di_ib[1];
				if (tempblock) {
					if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
						debugf("Unable to read block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return -ENOMEM;
					}
				} else {
					/* Need to allocate an indirect block */
					ret = ufs_block_alloc(fs, inode, blksize, &tempblock);
					if (ret) {
						debugf("Unable to allocate block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return ret;
					}
					ret = blkwrite(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize);
					if (ret <= 0) {
						debugf("Unable to write block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return -EIO;
					}
					inode->i_din2.di_ib[1] = tempblock;
				}
				ufs2_daddr_t l1block = tempblock;
				tempblock = *((ufs2_daddr_t *)blockbuf + index);

				/* Read the 2nd level buf */
				if (tempblock) {
					if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
						debugf("Unable to read block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return -ENOMEM;
					}
				} else {
					/* Need to allocate an indirect block */
					ret = ufs_block_alloc(fs, inode, blksize, &tempblock);
					if (ret) {
						debugf("Unable to allocate block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return ret;
					}
					*((ufs2_daddr_t *)blockbuf + index) = tempblock;
					ret = blkwrite(fs, fsbtodb(&fs->d_fs, l1block), blockbuf, blksize);
					if (ret <= 0) {
						debugf("Unable to write block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return -EIO;
					}

					bzero(blockbuf, blksize);
					ret = blkwrite(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize);
					if (ret <= 0) {
						debugf("Unable to write block %d\n", tempblock);
						ufs_free_mem(&blockbuf);
						return -EIO;
					}
				}
				index = fbn % nindir;
				*((ufs2_daddr_t *)blockbuf + index) = blockno;
				ret = blkwrite(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize);
				if (ret <= 0) {
					debugf("Unable to write block %d\n", tempblock);
					ufs_free_mem(&blockbuf);
					return -EIO;
				}
			} else {
				debugf("File too big for me....");
				exit(-1);
				/* Block lies in 3nd indirect block */
				fbn = fbn - nindir * nindir;
				tempblock = inode->i_din2.di_ib[1];
				if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", tempblock);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn / (nindir * nindir);
				tempblock = *((ufs2_daddr_t *)blockbuf + index);
				/* At 2nd level */
				if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", tempblock);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn / nindir;
				tempblock = *((ufs2_daddr_t *)blockbuf + index);
				/* At 3rd level */
				if (blkread(fs, fsbtodb(&fs->d_fs, tempblock), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", tempblock);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn % nindir;
				tempblock = *((ufs2_daddr_t *)blockbuf + index);
			}
		}
	}
	return 0;
}

/*
 * This function flushes the dirty block buffer out to disk if
 * necessary.
 */
int ufs_file_flush(ufs_file_t file)
{
	int	retval;
	uufsd_t * fs = file->fs;
	struct inode *inode = vnode2inode(file->inode);
	int oldfilesize = UFS_DINODE(inode)->di_size;

	if (!(file->flags & UFS_FILE_BUF_VALID) ||
	    !(file->flags & UFS_FILE_BUF_DIRTY))
		return 0;

	if (lblkno(&fs->d_fs, oldfilesize) == file->blockno) {
	}

	/*
	 * OK, the physical block hasn't been allocated yet.
	 * Allocate it.
	 */
	int size = ufs_io_size(file, file->pos, 1);

	if (!size) {
		return 0;
	}

again:
	if (!file->physblock) {
		retval = ufs_block_alloc(fs, inode, size, &file->physblock);
		if (retval)
			return retval;
		retval = ufs_set_block(fs, inode, file->blockno, file->physblock);
		if (retval)
			return retval;
	} else {
		int nfrags = numfrags(&fs->d_fs, fragroundup(&fs->d_fs, size));
		int osize = ufs_io_size(file, oldfilesize, 1);
		int ofrags = numfrags(&fs->d_fs, fragroundup(&fs->d_fs, osize));

		if (nfrags > ofrags) {
			ufs_block_free(fs, file->inode, file->physblock, osize, inode->i_ino);
			retval = ufs_set_block(fs, inode, file->blockno, 0);
			if (retval)
				return retval;
			file->physblock = 0;
			goto again;
		}
	}

	retval = blkwrite(fs, fsbtodb(&fs->d_fs, file->physblock), file->buf, size);
	if (retval <= 0)
		return -EIO;

	file->flags &= ~UFS_FILE_BUF_DIRTY;

	if (file->ino) {
		retval = ufs_write_inode(file->fs, file->ino, file->inode);
		if (retval)
			return retval;
	}

	return 0;
}

/*
 * This function synchronizes the file's block buffer and the current
 * file position, possibly invalidating block buffer if necessary
 */
static int sync_buffer_position(ufs_file_t file)
{
	blk_t	b;
	int	retval;

	b = lblkno(&(file->fs->d_fs), file->pos);
	if (b != file->blockno) {
		retval = ufs_file_flush(file);
		if (retval)
			return retval;
		file->flags &= ~UFS_FILE_BUF_VALID;
	}
	file->blockno = b;
	return 0;
}

int
ufs_bmap(uufsd_t *fs, struct ufs_vnode *vnode, blk_t fbn, ufs2_daddr_t *pbno)
{
	int ret, blksize = fs->d_fs.fs_bsize;
	char *blockbuf = 0;
	int nindir = fs->d_fs.fs_nindir;
	struct inode *inode = vnode2inode(vnode);

	debugf("Enter");

	ret = ufs_get_mem(blksize, &blockbuf);
	if (ret) {
		return ret;
	}

	*pbno = 0;

	if (fbn < NDADDR) {
		*pbno = inode->i_din2.di_db[fbn];
	} else {
		fbn = fbn - NDADDR;
		int indir = fbn / nindir;
		int index;
		if (indir == 0) {
			/* Block lies in 1st indirect block */
			index = fbn % nindir;
			*pbno = inode->i_din2.di_ib[0];
			if (blkread(fs, fsbtodb(&fs->d_fs, *pbno), blockbuf, blksize) == -1) {
				debugf("Unable to read block %d\n", *pbno);
				ufs_free_mem(&blockbuf);
				return -1;
			}
			*pbno = *((ufs2_daddr_t *)blockbuf + index);
		} else {
			fbn = fbn - nindir;
			indir = fbn / (nindir * nindir);
			if (indir == 0) {
				/* Block lies in 2nd indirect block */
				index = fbn / nindir;
				*pbno = inode->i_din2.di_ib[1];
				if (blkread(fs, fsbtodb(&fs->d_fs, *pbno), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", *pbno);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				*pbno = *((ufs2_daddr_t *)blockbuf + index);
				/* Read the 2nd level buf */
				if (blkread(fs, fsbtodb(&fs->d_fs, *pbno), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", *pbno);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn % nindir;
				*pbno = *((ufs2_daddr_t *)blockbuf + index);
			} else {
				/* Block lies in 3nd indirect block */
				fbn = fbn - nindir * nindir;
				*pbno = inode->i_din2.di_ib[1];
				if (blkread(fs, fsbtodb(&fs->d_fs, *pbno), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", *pbno);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn / (nindir * nindir);
				*pbno = *((ufs2_daddr_t *)blockbuf + index);
				/* At 2nd level */
				if (blkread(fs, fsbtodb(&fs->d_fs, *pbno), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", *pbno);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn / nindir;
				*pbno = *((ufs2_daddr_t *)blockbuf + index);
				/* At 3rd level */
				if (blkread(fs, fsbtodb(&fs->d_fs, *pbno), blockbuf, blksize) == -1) {
					debugf("Unable to read block %d\n", *pbno);
					ufs_free_mem(&blockbuf);
					return -1;
				}
				index = fbn % nindir;
				*pbno = *((ufs2_daddr_t *)blockbuf + index);
			}
		}
	}

	debugf("Leave");

	free(blockbuf);

	if (*pbno == 1852140901) {
		exit(1);
	}
	return 0;
}

/*
 * This function loads the file's block buffer with valid data from
 * the disk as necessary.
 *
 * If dontfill is true, then skip initializing the buffer since we're
 * going to be replacing its entire contents anyway.  If set, then the
 * function basically only sets file->physblock and UFS_FILE_BUF_VALID
 */
#define DONTFILL 1
static int load_buffer(ufs_file_t file, int dontfill)
{
	uufsd_t *	fs = file->fs;
	int	retval;
	int fsize;

	if (!(file->flags & UFS_FILE_BUF_VALID)) {
		retval = ufs_bmap(fs, file->inode,
				     file->blockno,
				     &file->physblock);

		fsize = ufs_io_size(file, file->pos, 0);

		debugf("Inum %d: Reading %d at pos %d\n", (int)file->ino, fsize, file->pos);

		if (retval)
			return retval;
		if (!dontfill) {
			if (file->physblock) {
				retval = bread(fs, fsbtodb(&fs->d_fs, file->physblock), file->buf, fsize);
				if (retval == -1)
					return retval;
			} else
				memset(file->buf, 0, fsize);
		}
		file->flags |= UFS_FILE_BUF_VALID;
	}
	return 0;
}


int ufs_file_close2 (ufs_file_t file, void (*close_callback) (struct ufs_vnode *inode, int flags))
{
	int retval;

	debugf("enter");

	retval = ufs_file_flush(file);

	if (file->buf) {
		ufs_free_mem(&file->buf);
	}
	if (!(file->flags & UFS_FILE_SHARED_INODE)) {
		/*
		 * Write inode to disk unless we're mounted read-only
		 * (access time field should have changed at least?)
		 * TODO: Track whether the inode is actually dirty
		 */
		int dirty = !file->fs->d_fs.fs_ronly;
		vnode_put(file->inode, dirty);
	} else if (close_callback != NULL) {
		close_callback(file->inode, file->flags & UFS_FILE_MASK);
	}

	ufs_free_mem(&file);

	return retval;
}

int ufs_file_close(ufs_file_t file)
{
	return ufs_file_close2(file, NULL);
}

int ufs_file_read(ufs_file_t file, void *buf,
			   unsigned int wanted, unsigned int *got)
{
	debugf("enter");
	uufsd_t *	fs;
	int		retval = 0;
	unsigned int	start, c, count = 0;
	__u64		left;
	char		*ptr = (char *) buf;
	struct inode *inode = vnode2inode(file->inode);

	fs = file->fs;

	while ((file->pos < inode->i_size) && (wanted > 0)) {
		retval = sync_buffer_position(file);
		if (retval)
			goto fail;
		retval = load_buffer(file, 0);
		if (retval)
			goto fail;

		start = file->pos % fs->d_fs.fs_bsize;
		c = fs->d_fs.fs_bsize - start;
		if (c > wanted)
			c = wanted;
		left = inode->i_size - file->pos ;
		if (c > left)
			c = left;

		memcpy(ptr, file->buf + start, c);
		file->pos += c;
		ptr += c;
		count += c;
		wanted -= c;
	}

fail:
	if (got)
		*got = count;
	return retval;
}


int ufs_file_write(ufs_file_t file, const void *buf,
			    unsigned int nbytes, unsigned int *written)
{
	uufsd_t *	fs;
	int		retval = 0;
	unsigned int	start, c, count = 0;
	const char	*ptr = (const char *) buf;

	fs = file->fs;

	if (!(file->flags & UFS_FILE_WRITE))
		return EROFS;

	while (nbytes > 0) {
		retval = sync_buffer_position(file);
		if (retval)
			goto fail;

		start = file->pos % fs->d_fs.fs_bsize;
		c = fs->d_fs.fs_bsize - start;
		if (c > nbytes)
			c = nbytes;

		/*
		*
		 * We only need to do a read-modify-update cycle if
		 * we're doing a partial write.
		 */
		retval = load_buffer(file, (c == fs->d_fs.fs_bsize));
		if (retval)
			goto fail;

		file->flags |= UFS_FILE_BUF_DIRTY;
		memcpy(file->buf+start, ptr, c);
		file->pos += c;
		ptr += c;
		count += c;
		nbytes -= c;
	}

fail:
	if (written)
		*written = count;
	return retval;
}

int ufs_file_lseek(ufs_file_t file, __u64 offset,
			    int whence, __u64 *ret_pos)
{
	struct inode *inode = vnode2inode(file->inode);
	if (whence == UFS_SEEK_SET)
		file->pos = offset;
	else if (whence == UFS_SEEK_CUR)
		file->pos += offset;
	else if (whence == UFS_SEEK_END)
		file->pos = inode->i_size + offset;
	else
		return EINVAL;

	if (ret_pos)
		*ret_pos = file->pos;

	return 0;
}

/*
 * This function returns the size of the file, according to the inode
 */
int ufs_file_get_size(ufs_file_t file, __u64 *ret_size)
{
	struct inode *inode = vnode2inode(file->inode);
	if (file->magic != UFS_MAGIC_FILE)
		return EINVAL;
	*ret_size = inode->i_size;
	return 0;
}

/*
 * This function sets the size of the file, truncating it if necessary
 *
 */
int ufs_file_set_size(ufs_file_t file, __u64 size)
{
	struct inode *inode = vnode2inode(file->inode);
	int retval = 0;

	ufs_file_flush(file);

	if (size < inode->i_size && inode->i_blocks) {
		retval = ufs_truncate(file->fs, file->inode, size);
	}

	inode->i_size = size & 0xffffffff;

	return retval;
}

int
blkread(struct uufsd *disk, ufs2_daddr_t blockno, void *data, size_t size)
{
	return bread(disk, blockno, data, size);
}
int
blkwrite(struct uufsd *disk, ufs2_daddr_t blockno, void *data, size_t size)
{
	return bwrite(disk, blockno, data, size);
}
