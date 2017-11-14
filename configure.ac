AC_PREREQ(2.50)
AC_INIT(faac, 1.29.9.2, https://sourceforge.net/p/faac/bugs/)
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([subdir-objects])

AM_CONFIG_HEADER(config.h)
AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /* CONFIG_H */])

AC_ARG_ENABLE( drm,  [  --enable-drm            Digital Radio Mondiale support], enable_drm=$enableval, enable_drm=no)

AC_DEFUN([MY_DEFINE], [ AC_DEFINE($1, 1, [define if needed]) ])

CFLAGS=${CFLAGS:-"-O2 -Wall -fexceptions"}

AC_PROG_CC
AC_PROG_CXX
AM_PROG_LIBTOOL

AC_CHECK_HEADERS(getopt.h immintrin.h)

AC_CHECK_DECL(strcasecmp, MY_DEFINE(HAVE_STRCASECMP))

AC_CHECK_LIB(gnugetopt, getopt_long)

dnl Check for DRM mode
if test "x$enable_drm" = "xyes"; then
    AC_DEFINE(DRM, 1, [Define if you want to encode for DRM])
    AM_CONDITIONAL(USE_DRM, true)
      CFLAGS="$CFLAGS -DDRM"
else
    AM_CONDITIONAL(USE_DRM, false)
fi

AM_CONDITIONAL(MINGW, test "$host_os" = "mingw32")
AM_CONDITIONAL(CPUSSE, test "$host_cpu" = "x86_64" || test "$host_cpu" = "i686")

AC_CHECK_TOOL(WINDRES, windres)

AC_C_BIGENDIAN

dnl Checks for header files required for mp4.h
AC_HEADER_STDC
AC_SYS_LARGEFILE
AC_CHECK_HEADERS(stdint.h inttypes.h)
AC_CHECK_HEADERS(mathf.h)
AC_CHECK_HEADERS(float.h)
AC_CHECK_DECL(strchr, MY_DEFINE(HAVE_STRCHR))
AC_CHECK_DECL(memcpy, MY_DEFINE(HAVE_MEMCPY))
AC_CHECK_DECL(strsep, MY_DEFINE(HAVE_STRSEP))
AC_CHECK_HEADERS(sys/time.h)
AC_HEADER_TIME
AC_TYPE_OFF_T
AC_CHECK_TYPES([in_port_t, socklen_t], , , 
	[#include <sys/types.h>
	 #include <sys/socket.h>
	 #include <netinet/in.h>])
AC_CHECK_MEMBERS(fpos_t.__pos,,, [#include <stdio.h>])

AC_OUTPUT([
common/Makefile
libfaac/Makefile
frontend/Makefile
include/Makefile
Makefile])
