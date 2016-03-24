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

#ifndef FUSEUFS_H_
#define FUSEUFS_H_

#define MAXBSIZE 65536
#define DEV_BSHIFT 9
#define LINK_MAX 255


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libufs/dinode.h>
#include <libufs/fs.h>
#include <fuse.h>
#include <libufs/acl.h>
#include <libufs/quota.h>
#include <libufs/dir.h>
#include <libufs/extattr.h>
#include <libufs/inode.h>
#include <libufs/libufs.h>
#include <ext2fs/ext2fs.h>

#include <fuse-ufs-misc.h>

#if !defined(FUSE_VERSION) || (FUSE_VERSION < 26)
#error "***********************************************************"
#error "*                                                         *"
#error "*     Compilation requires at least FUSE version 2.6.0!   *"
#error "*                                                         *"
#error "***********************************************************"
#endif

#define DEV_BSIZE (1 << DEV_BSHIFT)
#define	UFS_DIR_REC_LEN(namlen)						\
	(((uintptr_t)&((struct direct *)0)->d_name +			\
	  ((namlen)+1)*sizeof(((struct direct *)0)->d_name[0]) + 3) & ~3)


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RETURN_IF_RDONLY(ufs) \
	do {	\
		if (ufs->d_fs.fs_ronly) \
		    return EROFS;	\
	} while(0)

#define UFS_FILE(efile) ((void *) (unsigned long) (efile))

#define MIN(X, Y) X < Y ? X : Y

typedef struct uufsd uufsd_t;

struct ufs_data {
	unsigned char debug;
	unsigned char silent;
	unsigned char force;
	unsigned char readonly;
	char *mnt_point;
	char *options;
	char *device;
	char *volname;
	uufsd_t ufs;
};

struct ufs_vnode {
	struct inode inode;
	uufsd_t *ufsp;
	ino_t ino;
	int count;
	struct ufs_vnode **pprevhash,*nexthash;
};

union dinode {
	struct ufs1_dinode dp1;
	struct ufs2_dinode dp2;
};

static inline uufsd_t *current_ufs(void)
{
	struct fuse_context *mycontext=fuse_get_context();
	struct ufs_data *ufsdata=mycontext->private_data;
	return (uufsd_t *)&(ufsdata->ufs);
}

#if ENABLE_DEBUG

static inline void debug_printf (const char *function,
				 char *file, int line, const char *fmt, ...)
{
	va_list args;
	struct fuse_context *mycontext=fuse_get_context();
	struct ufs_data *e2data=mycontext->private_data;
	if (e2data && (e2data->debug == 0 || e2data->silent == 1)) {
		return;
	}
	printf("%s: ", PACKAGE);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf(" [%s (%s:%d)]\n", function, file, line);
}

#define debugf(a...) { \
	debug_printf(__FUNCTION__, __FILE__, __LINE__, a); \
}

#define debugferr(a...) do { \
	debug_printf(__FUNCTION__, __FILE__, __LINE__, a); \
	char *c = 0; \
	*c = '0'; \
} while(0)

static inline void debug_main_printf (const char *function, char *file, int line, const char *fmt, ...)
{
	va_list args;
	printf("%s: ", PACKAGE);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf(" [%s (%s:%d)]\n", function, file, line);
}

#define debugf_main(a...) { \
	debug_main_printf(__FUNCTION__, __FILE__, __LINE__, a); \
}

#else /* ENABLE_DEBUG */

#define debugf(a...) do { } while(0)
#define debugf_main(a...) do { } while(0)
#define debugferr(a...) do { } while(0)

#endif /* ENABLE_DEBUG */


struct ufs_vnode *vnode_get(uufsd_t *ufs, ino_t ino);

int vnode_put(struct ufs_vnode *vnode, int dirty);

static inline struct inode *vnode2inode(struct ufs_vnode *vnode) {
	return (struct inode *)vnode;
}

static inline struct ufs_vnode *inode2vnode(struct inode *inode) {
	return (struct ufs_vnode *)inode->i_vnode;
}

void * op_init (struct fuse_conn_info *conn);

void op_destroy (void *userdata);

/* helper functions */

int do_probe (struct ufs_data *opts);

int do_label (void);

int do_check (const char *path);

int do_check_split(const char *path, char **dirname,char **basename);

void free_split(char *dirname, char *basename);

void do_fillstatbuf (uufsd_t *ufs, ino_t ino, struct inode *inode, struct stat *st);

int do_readinode (uufsd_t *ufs, const char *path, ino_t *ino, struct inode *inode);

int do_readvnode (uufsd_t *ufs, const char *path, ino_t *ino, struct ufs_vnode **vnode);

int do_killfilebyinode (uufsd_t *ufs, ino_t ino, struct ufs_vnode *inode);

/* read support */

int op_access (const char *path, int mask);

int op_fgetattr (const char *path, struct stat *stbuf, struct fuse_file_info *fi);

int op_getattr (const char *path, struct stat *stbuf);
int op_getxattr (const char *path, struct stat *stbuf);

ufs_file_t do_open (uufsd_t *ufs, const char *path, int flags);

int op_open (const char *path, struct fuse_file_info *fi);

int op_read (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int op_readdir (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

int op_readlink (const char *path, char *buf, size_t size);

int do_release (ufs_file_t file);

int op_release (const char *path, struct fuse_file_info *fi);

int op_statfs(const char *path, struct statvfs *buf);

/* write support */

int op_chmod (const char *path, mode_t mode);

int op_chown (const char *path, uid_t uid, gid_t gid);

int do_create (uufsd_t *ufs, const char *path, mode_t mode, dev_t dev, const char *fastsymlink);

int op_create (const char *path, mode_t mode, struct fuse_file_info *fi);

int op_flush (const char *path, struct fuse_file_info *fi);

int op_fsync (const char *path, int datasync, struct fuse_file_info *fi);

int op_mkdir (const char *path, mode_t mode);

int do_check_empty_dir(uufsd_t *ufs, ino_t ino);

int op_rmdir (const char *path);

int op_unlink (const char *path);

int op_utimens (const char *path, const struct timespec tv[2]);

size_t do_write (ufs_file_t efile, const char *buf, size_t size, off_t offset);

int op_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int op_mknod (const char *path, mode_t mode, dev_t dev);

int op_symlink (const char *sourcename, const char *destname);

int op_truncate(const char *path, off_t length);

int op_ftruncate(const char *path, off_t length, struct fuse_file_info *fi);

int op_link (const char *source, const char *dest);

int op_rename (const char *source, const char *dest);

int ufs_namei(uufsd_t *ufs, ino_t root_ino, ino_t cur_ino, const char *filename, ino_t *ino);
int ufs_bmap(uufsd_t *ufs, struct ufs_vnode *inode, blk_t fbn, ufs2_daddr_t *blkno);

int ufs_dir_iterate(uufsd_t *ufs, ino_t dirino,
                    int (*func)(
                                          struct direct *dirent,
					  int inum,
					  char *buf,
                                          void  *priv_data),
                              void *priv_data);

int blkread(struct uufsd *disk, ufs2_daddr_t blockno, void *data, size_t size);
int blkwrite(struct uufsd *disk, ufs2_daddr_t blockno, void *data, size_t size);

void copy_incore_to_ondisk(struct inode *inode, struct ufs2_dinode *dinop);
void copy_ondisk_to_incore(uufsd_t *ufsp, struct inode *inode,
			struct ufs2_dinode *dinop, ino_t ino);

int ufs_write_inode(uufsd_t *ufs, ino_t ino, struct ufs_vnode *vnode);

int ufs_unlink(uufsd_t *ufs, ino_t d_dest_ino, char *r_dest, ino_t src_ino, int flags);
int ufs_link(uufsd_t *ufs, ino_t dir_ino, char *r_dest, struct ufs_vnode *vnode, int mode);

int ufs_file_write(ufs_file_t file, const void *buf,
			 unsigned int nbytes, unsigned int *written);

int ufs_file_flush(ufs_file_t file);
int ufs_file_lseek(ufs_file_t file, __u64 offset, int whence, __u64 *ret_pos);
int ufs_file_get_size(ufs_file_t file, __u64 *ret_size);
int ufs_file_read(ufs_file_t file, void *buf, unsigned int wanted,
			unsigned int *got);
int ufs_block_alloc(uufsd_t *ufs, struct inode* inode, int size, ufs2_daddr_t *blkno);
int ufs_set_block(uufsd_t *fs, struct inode *inode, blk_t fbn, ufs2_daddr_t blockno);
int ufs_truncate(uufsd_t *ufs, struct ufs_vnode *vnode, int newsize);
void ufs_block_free( uufsd_t *ufs, struct ufs_vnode *vnode, ufs2_daddr_t bno,
			long size, ino_t inum);
int ufs_file_open2(uufsd_t *fs, ino_t ino, struct ufs_vnode *vnode,
			    int flags, ufs_file_t *ret);
int ufs_file_close2(ufs_file_t file,
		    void (*close_callback) (struct ufs_vnode *inode, int flags));
int ufs_file_set_size(ufs_file_t file, __u64 size);
int ufs_free_inode(uufsd_t *ufs, struct ufs_vnode *vnode, ino_t ino, int mode);
ufs2_daddr_t ufs_inode_alloc(struct inode *ip, int cg, ufs2_daddr_t ipref, int mode);
typedef ufs2_daddr_t allocfunc_t(struct inode *ip, int cg, ufs2_daddr_t bpref, int size);
ufs2_daddr_t
ufs_hashalloc(struct inode *ip, int cg, int pref, int size, allocfunc_t allocator);
int ufs_inode_io_size(struct inode *inode, int offset, int write);
int ufs_set_rec_len(uufsd_t *ufs, unsigned int len, struct direct *dirent);
ufs2_daddr_t ufs_inode_alloc(struct inode *ip, int cg, ufs2_daddr_t ipref, int mode);

/* Append DIRBLKSIZ block to directory and fill in a single entry */
int ufs_dir_append(uufsd_t *ufs, ino_t d_ino,
		   ino_t f_ino, int f_flags, const char *f_name);

int ufs_valloc( struct ufs_vnode *pvp, int mode, struct ufs_vnode **vnodepp);
int do_modetoufslag (mode_t mode);
int ufs_lookup(uufsd_t *ufs, ino_t dir, const char *name, int namelen,
		ino_t *ino);
#endif /* FUSEUFS_H_ */
