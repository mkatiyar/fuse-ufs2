
noinst_LIBRARIES = libufs.a

libufs_a_SOURCES = \
	block.c \
	cgroup.c \
	inode.c \
	sblock.c \
	type.c

libufs_a_CFLAGS = \
	-Wall \
	-include config.h \
	-D_LIBUFS

if LIBUFS_DEBUG
libufs_a_CFLAGS += -D_LIBUFS_DEBUGGING
endif
