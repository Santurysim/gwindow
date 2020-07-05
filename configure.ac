#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([GWindow], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_PROG_AR
LT_INIT
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
#AC_CHECK_LIB([X11], [XOpenDisplay], [], [], [])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([floor memset select sqrt])

AC_CONFIG_FILES([Makefile
                 r2graph/Makefile
                 gwindow/Makefile
                 samples/Makefile])
AC_OUTPUT
