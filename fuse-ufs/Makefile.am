moddir = $(libdir)/umview/modules

DEVELFLAGS =

bin_PROGRAMS = \
	fuse-ufs \
	fuse-ufs.probe

mod_LTLIBRARIES = umfuseufs.la

fuse_ufs_probe_SOURCES = \
	fuse-ufs.h \
	fuse-ufs.probe.c \
	do_probe.c

fuse_ufs_probe_CFLAGS = \
	-Wall \
	-DHAVE_CONFIG_H \
	-I/usr/local/include

fuse_ufs_probe_LDADD = \
	-L/usr/local/lib \
	../libufs/libufs.a \
	-lfuse

fuse_ufs_SOURCES =	\
	fuse-ufs.h \
	fuse-ufs.c \
	fuse-ufs-utils.c \
	fuse-ufs-tables.c \
	fuse-inodeops.c \
	fuse-inodealloc.c \
	fuse-ufs-fileio.c \
	vnode_hash.c \
	do_probe.c \
	do_check.c \
	do_fillstatbuf.c \
	do_readinode.c \
	do_killfilebyinode.c \
	op_init.c \
	op_destroy.c \
	op_access.c \
	op_fgetattr.c \
	op_getattr.c \
	op_open.c \
	op_read.c \
	op_readdir.c \
	op_readlink.c \
	op_release.c \
	op_statfs.c \
	op_chown.c \
	op_chmod.c \
	op_create.c \
	op_flush.c \
	op_fsync.c \
	op_mkdir.c \
	op_rmdir.c \
	op_unlink.c \
	op_utimens.c \
	op_write.c \
	op_mknod.c \
	op_symlink.c \
	op_truncate.c \
	op_link.c \
	op_rename.c

umfuseufs_la_SOURCES = \
	fuse-ufs.h \
	fuse-ufs.c \
	fuse-ufs-utils.c \
	fuse-inodeops.c \
	fuse-inodealloc.c \
	fuse-ufs-tables.c \
	fuse-ufs-fileio.c \
	vnode_hash.c \
	do_probe.c \
	do_check.c \
	do_fillstatbuf.c \
	do_readinode.c \
	do_killfilebyinode.c \
	op_init.c \
	op_destroy.c \
	op_access.c \
	op_fgetattr.c \
	op_getattr.c \
	op_open.c \
	op_read.c \
	op_readdir.c \
	op_readlink.c \
	op_release.c \
	op_statfs.c \
	op_chown.c \
	op_chmod.c \
	op_create.c \
	op_flush.c \
	op_fsync.c \
	op_mkdir.c \
	op_rmdir.c \
	op_unlink.c \
	op_utimens.c \
	op_write.c \
	op_mknod.c \
	op_symlink.c \
	op_truncate.c \
	op_link.c \
	op_rename.c

umfuseufs_la_CFLAGS = \
	-Wall \
	-DHAVE_CONFIG_H \
	-D_GNU_SOURCE \
	$(DEVELFLAGS) \
	-I$(includedir)/umview \
	-I/usr/local/include

umfuseufs_la_LDFLAGS = \
	-module \
	-avoid-version \
	-export-dynamic

fuse_ufs_CFLAGS = \
	-Wall \
	-DHAVE_CONFIG_H \
	-I/usr/local/include

fuse_ufs_LDADD = \
	-L/usr/local/lib \
	../libufs/libufs.a \
	-lcom_err \
	-lfuse

install-data-hook:
	cd "$(DESTDIR)/$(moddir)" && rm -f $(mod_LTLIBRARIES)

install-exec-local:
	$(INSTALL) -d "$(DESTDIR)/usr/sbin"
	$(LN_S) -f "$(DESTDIR)/$(bindir)/fuse-ufs" "$(DESTDIR)/sbin/mount.fuse-ufs"
