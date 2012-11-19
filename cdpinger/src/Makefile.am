## Process this file with automake to produce Makefile.in

bin_PROGRAMS = ltspfs ltspfsd lbmount 
sbin_PROGRAMS = cdpinger
ltspfs_SOURCES = ltspfs.c common.c common.h ltspfs.h
ltspfsd_SOURCES = ltspfsd.c ltspfsd_functions.c ltspfsd_functions.h common.c common.h ltspfs.h
lbmount_SOURCES = lbmount.c
cdpinger_SOURCES = cdpinger.c

ltspfs_CFLAGS = -Wall -W -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22 -D_REENTRANT
ltspfsd_CFLAGS = -Wall -W -D_FILE_OFFSET_BITS=64 -DAUTOMOUNT -D_REENTRANT
cdpinger_CFLAGS = $(GLIB_CFLAGS) $(GOBJECT_CFLAGS)

cdpinger_LDADD = $(GLIB_LIBS) $(GOBJECT_LIBS)
ltspfs_LDADD = $(LTSPFS_LIBS)
