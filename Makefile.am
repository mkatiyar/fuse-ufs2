
SUBDIRS = libufs
SUBDIRS += fuse-ufs

EXTRA_DIST = \
	fuse-ufs.pc.in

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = fuse-ufs.pc

$(pkgconfig_DATA): config.status

man1_MANS = fuse-ufs.1

if LINUX
package:
	echo "You can use checkinstall or some other equivalent tool to generate install package for your distribution."
endif
