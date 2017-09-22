#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([BHFS], [0.1])
AC_CONFIG_SRCDIR([src/bhfs.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE
AC_OUTPUT(Makefile src/Makefile)

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h string.h sys/time.h unistd.h])

# Check for development environments
PKG_CHECK_MODULES(FUSE, fuse)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([ftruncate lchown memset mkdir mkfifo rmdir])

AC_OUTPUT
