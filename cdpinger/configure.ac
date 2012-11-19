AC_PREREQ(2.59)
AC_INIT(ltspfs, [1.0], [sbalneav@ltsp.org])
AC_CONFIG_SRCDIR([src/ltspfs.c])
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE
# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries.
# Thanks, Ollie!
# PKG_CHECK_MODULES(LTSPFSD, [x11])
PKG_CHECK_MODULES(LTSPFS, [fuse, x11])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([linux/cdrom.h fcntl.h],,AC_MSG_ERROR(Required header missing))

dnl Check for Packages
if test -z "$UDEV_RULES_PATH"; then
        UDEV_RULES_PATH=/etc/udev/rules.d
fi
AC_SUBST(UDEV_RULES_PATH)

if test -z "$UDEV_LIB_PATH"; then
        UDEV_LIB_PATH=/lib/udev
fi
AC_SUBST(UDEV_LIB_PATH)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.6.0)
PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.6.0)

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GOBJECT_CFLAGS)
AC_SUBST(GOBJECT_LIBS)

dnl Output
AC_CONFIG_FILES(Makefile src/Makefile man/Makefile scripts/Makefile udev/Makefile)
AC_OUTPUT
