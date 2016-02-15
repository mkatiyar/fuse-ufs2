LIB =	ufs
SHLIB_MAJOR= 2
LIBSRCS= block.c cgroup.c inode.c sblock.c type.c
INCS=	libufs.h

ALL_CFLAGS+= -D_LIBUFS
ifdef LIBUFS_DEBUG
ALL_CFLAGS+= -D_LIBUFS_DEBUGGING
endif
INCLUDES = -I.

include ./Makefile.common
