AC_PREREQ([2.69])
AC_INIT(faac, 1.31.1, [https://github.com/knik0/faac])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_HEADERS([config.h])
AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /* CONFIG_H */])

AC_DEFUN([MY_DEFINE], [ AC_DEFINE($1, 1, [define if needed]) ])

CFLAGS=${CFLAGS:-"-O2 -Wall -fexceptions"}

AC_PROG_CC
AC_PROG_CXX
LT_INIT

AC_CHECK_HEADERS(getopt.h immintrin.h)

AC_CHECK_DECL(strcasecmp, MY_DEFINE(HAVE_STRCASECMP))

AC_CHECK_LIB(gnugetopt, getopt_long)

AM_CONDITIONAL(MINGW, test "$host_os" = "mingw32")
AM_CONDITIONAL(CPUSSE, test "$host_cpu" = "x86_64" || test "$host_cpu" = "i686")

AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE([[
#if !defined(__clang__)
#error not clang
#endif
	]])],
	[AM_CONDITIONAL(CLANG, true)],
	[AM_CONDITIONAL(CLANG, false)])

AC_CHECK_TOOL(WINDRES, windres)

AC_C_BIGENDIAN

AC_SYS_LARGEFILE
AC_CHECK_HEADERS(stdint.h inttypes.h)
AC_CHECK_HEADERS(mathf.h)
AC_CHECK_HEADERS(float.h)
AC_CHECK_DECL(strchr, MY_DEFINE(HAVE_STRCHR))
AC_CHECK_DECL(memcpy, MY_DEFINE(HAVE_MEMCPY))
AC_CHECK_DECL(strsep, MY_DEFINE(HAVE_STRSEP))
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_TYPE_OFF_T
AC_CHECK_TYPES([in_port_t, socklen_t], , , 
	[#include <sys/types.h>
	 #include <sys/socket.h>
	 #include <netinet/in.h>])
AC_CHECK_MEMBERS(fpos_t.__pos,,, [#include <stdio.h>])

AC_CONFIG_FILES([
common/Makefile
libfaac/Makefile
libfaac/faac.pc
frontend/Makefile
include/Makefile
Makefile])
AC_OUTPUT
