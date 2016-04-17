dnl
dnl Contains the following macros
dnl   SANE_SET_CFLAGS(is_release)
dnl   SANE_CHECK_MISSING_HEADERS
dnl   SANE_SET_LDFLAGS
dnl   SANE_CHECK_DLL_LIB
dnl   SANE_EXTRACT_LDFLAGS(LIBS, LDFLAGS)
dnl   SANE_CHECK_JPEG
dnl   SANE_CHECK_IEEE1284
dnl   SANE_CHECK_PTHREAD
dnl   SANE_CHECK_LOCKING
dnl   JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl   SANE_LINKER_RPATH
dnl   SANE_CHECK_U_TYPES
dnl   SANE_CHECK_GPHOTO2
dnl   SANE_CHECK_IPV6
dnl   SANE_CHECK_BACKENDS
dnl   SANE_PROTOTYPES
dnl   AC_PROG_LIBTOOL
dnl

# SANE_SET_CFLAGS(is_release)
# Set CFLAGS. Enable/disable compilation warnings if we gcc is used.
# Warnings are enabled by default when in development cycle but disabled
# when a release is made. The argument is_release is either yes or no.
AC_DEFUN([SANE_SET_CFLAGS],
[
if test "${ac_cv_c_compiler_gnu}" = "yes"; then
  NORMAL_CFLAGS="\
      -W \
      -Wall"
  WARN_CFLAGS="\
      -W \
      -Wall \
      -Wcast-align \
      -Wcast-qual \
      -Wmissing-declarations \
      -Wmissing-prototypes \
      -Wpointer-arith \
      -Wreturn-type \
      -Wstrict-prototypes \
      -pedantic"

  # Some platforms are overly strict with -ansi enabled.  Exclude those.
  ANSI_FLAG=-ansi
  case "${host_os}" in  
    solaris* | hpux* | os2* | darwin* | cygwin* | mingw*)
      ANSI_FLAG=
      ;;
  esac
  NORMAL_CFLAGS="${NORMAL_CFLAGS} ${ANSI_FLAG}"
  WARN_CFLAGS="${WARN_CFLAGS} ${ANSI_FLAG}"

  AC_ARG_ENABLE(warnings,
    AC_HELP_STRING([--enable-warnings],
                   [turn on tons of compiler warnings (GCC only)]),
    [
      if eval "test x$enable_warnings = xyes"; then 
        for flag in $WARN_CFLAGS; do
          JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
        done
      else
        for flag in $NORMAL_CFLAGS; do
          JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
        done
      fi
    ],
    [if test x$1 = xno; then
       # Warnings enabled by default (development)
       for flag in $WARN_CFLAGS; do
         JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
       done
    else
       # Warnings disabled by default (release)
       for flag in $NORMAL_CFLAGS; do
         JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
       done
    fi])
fi # ac_cv_c_compiler_gnu
])

dnl SANE_CHECK_MISSING_HEADERS
dnl Do some sanity checks. It doesn't make sense to proceed if those headers
dnl aren't present.
AC_DEFUN([SANE_CHECK_MISSING_HEADERS],
[
  MISSING_HEADERS=
  if test "${ac_cv_header_fcntl_h}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"fcntl.h\" "
  fi
  if test "${ac_cv_header_sys_time_h}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"sys/time.h\" "
  fi
  if test "${ac_cv_header_unistd_h}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"unistd.h\" "
  fi
  if test "${ac_cv_header_stdc}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"ANSI C headers\" "
  fi
  if test "${MISSING_HEADERS}" != "" ; then
    echo "*** The following essential header files couldn't be found:"
    echo "*** ${MISSING_HEADERS}"
    echo "*** Maybe the compiler isn't ANSI C compliant or not properly installed?"
    echo "*** For details on what went wrong see config.log."
    AC_MSG_ERROR([Exiting now.])
  fi
])

# SANE_SET_LDFLAGS
# Add special LDFLAGS
AC_DEFUN([SANE_SET_LDFLAGS],
[
  # Define stricter linking policy on GNU systems.  This is not
  # added to global LDFLAGS because we may want to create convenience
  # libraries that don't require such strick linking.
  if test "$GCC" = yes; then
    case ${host_os} in
    linux* | solaris*)
      STRICT_LDFLAGS="-Wl,-z,defs"
      ;;
    esac
  fi
  AC_SUBST(STRICT_LDFLAGS)
  case "${host_os}" in  
    aix*) #enable .so libraries, disable archives
      LDFLAGS="$LDFLAGS -Wl,-brtl"
      ;;
    darwin*) #include frameworks
      LIBS="$LIBS -framework CoreFoundation -framework IOKit"
      ;;
  esac
])

# SANE_CHECK_DLL_LIB
# Find dll library
AC_DEFUN([SANE_CHECK_DLL_LIB],
[
  dnl Checks for dll libraries: dl
  DL_LIBS=""
  if test "${enable_dynamic}" = "auto"; then
      # default to disabled unless library found.
      enable_dynamic=no
      # dlopen
      AC_CHECK_HEADERS(dlfcn.h,
      [AC_CHECK_LIB(dl, dlopen, DL_LIBS=-ldl)
       saved_LIBS="${LIBS}"
       LIBS="${LIBS} ${DL_LIBS}"
       AC_CHECK_FUNCS(dlopen, enable_dynamic=yes,)
       LIBS="${saved_LIBS}"
      ],)
      # HP/UX DLL handling
      AC_CHECK_HEADERS(dl.h,
      [AC_CHECK_LIB(dld,shl_load, DL_LIBS=-ldld)
       saved_LIBS="${LIBS}"
       LIBS="${LIBS} ${DL_LIBS}"
       AC_CHECK_FUNCS(shl_load, enable_dynamic=yes,)
       LIBS="${saved_LIBS}"
      ],)
      if test -z "$DL_LIBS" ; then
      # old Mac OS X/Darwin (without dlopen)
      AC_CHECK_HEADERS(mach-o/dyld.h,
      [AC_CHECK_FUNCS(NSLinkModule, enable_dynamic=yes,)
      ],)
      fi
  fi
  AC_SUBST(DL_LIBS)

  DYNAMIC_FLAG=
  if test "${enable_dynamic}" = yes ; then
    DYNAMIC_FLAG=-module
  fi
  AC_SUBST(DYNAMIC_FLAG)
])

#
# Separate LIBS from LDFLAGS to link correctly on HP/UX (and other
# platforms who care about the order of params to ld.  It removes all
# non '-l..'-params from passed in $1(LIBS), and appends them to $2(LDFLAGS).
#
# Use like this: SANE_EXTRACT_LDFLAGS(PKGNAME_LIBS, PKGNAME_LDFLAGS)
 AC_DEFUN([SANE_EXTRACT_LDFLAGS],
[
    tmp_LIBS=""
    for param in ${$1}; do
        case "${param}" in
	-l*)
         tmp_LIBS="${tmp_LIBS} ${param}"
	 ;;
	 *)
         $2="${$2} ${param}"
	 ;;
	 esac
     done
     $1="${tmp_LIBS}"
     unset tmp_LIBS
     unset param
])

#
#
# Checks for ieee1284 library, needed for canon_pp backend.
AC_DEFUN([SANE_CHECK_IEEE1284],
[
  AC_CHECK_HEADER(ieee1284.h, [
    AC_CACHE_CHECK([for libieee1284 >= 0.1.5], sane_cv_use_libieee1284, [
      AC_TRY_COMPILE([#include <ieee1284.h>], [
	struct parport p; char *buf; 
	ieee1284_nibble_read(&p, 0, buf, 1);
 	], 
        [sane_cv_use_libieee1284="yes"; IEEE1284_LIBS="-lieee1284"
      ],[sane_cv_use_libieee1284="no"])
    ],)
  ],)
  if test "$sane_cv_use_libieee1284" = "yes" ; then
    AC_DEFINE(HAVE_LIBIEEE1284,1,[Define to 1 if you have the `ieee1284' library (-lcam).])
  fi
  AC_SUBST(IEEE1284_LIBS)
])

#
# Checks for pthread support
AC_DEFUN([SANE_CHECK_PTHREAD],
[

  case "${host_os}" in
  linux* | darwin* | mingw*) # enabled by default on Linux, MacOS X and MINGW
    use_pthread=yes
    ;;
  *)
    use_pthread=no
  esac
  have_pthread=no

  #
  # now that we have the systems preferences, we check
  # the user

  AC_ARG_ENABLE([pthread],
    AC_HELP_STRING([--enable-pthread],
                   [use pthread instead of fork (default=yes for Linux/MacOS X/MINGW, no for everything else)]),
    [
      if test $enableval = yes ; then
        use_pthread=yes
      else
        use_pthread=no
      fi
    ])

  if test $use_pthread = yes ; then
  AC_CHECK_HEADERS(pthread.h,
    [
       AC_CHECK_LIB(pthread, pthread_create, PTHREAD_LIBS="-lpthread")
       have_pthread=yes
       save_LIBS="$LIBS"
       LIBS="$LIBS $PTHREAD_LIBS"
       AC_CHECK_FUNCS([pthread_create pthread_kill pthread_join pthread_detach pthread_cancel pthread_testcancel],
	,[ have_pthread=no; use_pthread=no ])
       LIBS="$save_LIBS"
    ],)
  fi
 
  if test $use_pthread = yes ; then
    AC_DEFINE_UNQUOTED(USE_PTHREAD, "$use_pthread",
                   [Define if pthreads should be used instead of forked processes.])
  else
    dnl Reset library in case it was found but we are not going to use it.
    PTHREAD_LIBS=""
  fi
  if test "$have_pthread" = "yes" ; then
    CPPFLAGS="${CPPFLAGS} -D_REENTRANT"
  fi
  AC_SUBST(PTHREAD_LIBS)
  AC_MSG_CHECKING([whether to enable pthread support])
  AC_MSG_RESULT([$have_pthread])
  AC_MSG_CHECKING([whether to use pthread instead of fork])
  AC_MSG_RESULT([$use_pthread])
])

#
# Checks for jpeg library >= v6B (61), needed for DC210,  DC240,
# GPHOTO2 and dell1600n_net backends.
AC_DEFUN([SANE_CHECK_JPEG],
[
  AC_CHECK_LIB(jpeg,jpeg_start_decompress, 
  [
    AC_CHECK_HEADER(jconfig.h, 
    [
      AC_MSG_CHECKING([for jpeglib - version >= 61 (6a)])
      AC_EGREP_CPP(sane_correct_jpeg_lib_version_found,
      [
        #include <jpeglib.h>
        #if JPEG_LIB_VERSION >= 61
          sane_correct_jpeg_lib_version_found
        #endif
      ], [sane_cv_use_libjpeg="yes"; JPEG_LIBS="-ljpeg"; 
      AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    ],)
  ],)
  if test "$sane_cv_use_libjpeg" = "yes" ; then
    AC_DEFINE(HAVE_LIBJPEG,1,[Define to 1 if you have the libjpeg library.])
  fi
  AC_SUBST(JPEG_LIBS)
])

# Checks for tiff library dell1600n_net backend.
AC_DEFUN([SANE_CHECK_TIFF],
[
  AC_CHECK_LIB(tiff,TIFFFdOpen, 
  [
    AC_CHECK_HEADER(tiffio.h, 
    [sane_cv_use_libtiff="yes"; TIFF_LIBS="-ltiff"],)
  ],)
  AC_SUBST(TIFF_LIBS)
])

AC_DEFUN([SANE_CHECK_PNG],
[
  AC_CHECK_LIB(png,png_init_io,
  [
    AC_CHECK_HEADER(png.h,
    [sane_cv_use_libpng="yes"; PNG_LIBS="-lpng"],)
  ],)
  if test "$sane_cv_use_libpng" = "yes" ; then
    AC_DEFINE(HAVE_LIBPNG,1,[Define to 1 if you have the libpng library.])
  fi
  AC_SUBST(PNG_LIBS)
])

#
# Checks for pthread support
AC_DEFUN([SANE_CHECK_LOCKING],
[
  LOCKPATH_GROUP=uucp
  use_locking=yes
  case "${host_os}" in
    os2* )
      use_locking=no
      ;;
  esac

  #
  # we check the user
  AC_ARG_ENABLE( [locking],
    AC_HELP_STRING([--enable-locking],
                   [activate device locking (default=yes, but only used by some backends)]),
    [
      if test $enableval = yes ; then
        use_locking=yes
      else
        use_locking=no
      fi
    ])
  if test $use_locking = yes ; then
    AC_ARG_WITH([group],
      AC_HELP_STRING([--with-group],
                     [use the specified group for lock dir @<:@default=uucp@:>@]),
        [LOCKPATH_GROUP="$withval"]
    )
    # check if the group does exist
    lasterror=""
    touch sanetest.file
    chgrp $LOCKPATH_GROUP sanetest.file 2>/dev/null || lasterror=$?
    rm -f sanetest.file
    if test ! -z "$lasterror"; then
      AC_MSG_WARN([Group $LOCKPATH_GROUP does not exist on this system.])
      AC_MSG_WARN([Locking feature will be disabled.])
      use_locking=no
    fi
  fi
  if test $use_locking = yes ; then
    INSTALL_LOCKPATH=install-lockpath
    AC_DEFINE([ENABLE_LOCKING], 1, 
              [Define to 1 if device locking should be enabled.])
  else
    INSTALL_LOCKPATH=
  fi
  AC_MSG_CHECKING([whether to enable device locking])
  AC_MSG_RESULT([$use_locking])
  if test $use_locking = yes ; then
    AC_MSG_NOTICE([Setting lockdir group to $LOCKPATH_GROUP])
  fi
  AC_SUBST(INSTALL_LOCKPATH)
  AC_SUBST(LOCKPATH_GROUP)
])

dnl
dnl JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl
dnl From Japhar.  Report changes to japhar@hungry.com
dnl
AC_DEFUN([JAPHAR_GREP_CFLAGS],
[case "$CFLAGS" in
"$1" | "$1 "* | *" $1" | *" $1 "* )
  ifelse($#, 3, [$3], [:])
  ;;
*)
  $2
  ;;
esac
])

dnl
dnl SANE_LINKER_RPATH
dnl
dnl Detect how to set runtime link path (rpath).  Set variable
dnl LINKER_RPATH.  Typical content will be '-Wl,-rpath,' or '-R '.  If
dnl set, add '${LINKER_RPATH}${libdir}' to $LDFLAGS
dnl

AC_DEFUN([SANE_LINKER_RPATH],
[dnl AC_REQUIRE([AC_SUBST])dnl This line resulted in an empty AC_SUBST() !!
  AC_MSG_CHECKING([whether runtime link path should be used])
  AC_ARG_ENABLE([rpath],
    [AS_HELP_STRING([--enable-rpath],
      [use runtime library search path @<:@default=yes@:>@])])

  LINKER_RPATH=
  AS_IF([test "x$enable_rpath" != xno],
  AC_MSG_RESULT([yes])
    [AC_CACHE_CHECK([linker parameter to set runtime link path], my_cv_LINKER_RPATH,
      [my_cv_LINKER_RPATH=
      case "$host_os" in
      linux* | freebsd* | netbsd* | openbsd* | irix*)
        # I believe this only works with GNU ld [pere 2001-04-16]
        my_cv_LINKER_RPATH="-Wl,-rpath,"
        ;;
      solaris*)
        my_cv_LINKER_RPATH="-R "
        ;;
      esac
      ])
      LINKER_RPATH="$my_cv_LINKER_RPATH"],
    [AC_MSG_RESULT([no])
      LINKER_RPATH=])
  AC_SUBST(LINKER_RPATH)dnl
])

dnl
dnl   SANE_CHECK_U_TYPES
dnl
AC_DEFUN([SANE_CHECK_U_TYPES],
[
dnl Use new style of check types that doesn't take default to use.
dnl The old style would add an #undef of the type check on platforms
dnl that defined that type... That is not portable to platform that
dnl define it as a #define.
AC_CHECK_TYPES([u_char, u_short, u_int, u_long],,,)
])

#
# Checks for gphoto2 libs, needed by gphoto2 backend
AC_DEFUN([SANE_CHECK_GPHOTO2],
[
  AC_ARG_WITH(gphoto2,
	      AC_HELP_STRING([--with-gphoto2],
			     [include the gphoto2 backend @<:@default=yes@:>@]),
	      [# If --with-gphoto2=no or --without-gphoto2, disable backend
               # as "$with_gphoto2" will be set to "no"])

  # If --with-gphoto2=yes (or not supplied), first check if 
  # pkg-config exists, then use it to check if libgphoto2 is
  # present.  If all that works, then see if we can actually link
  # a program.   And, if that works, then add the -l flags to 
  # GPHOTO2_LIBS and any other flags to GPHOTO2_LDFLAGS to pass to 
  # sane-config.
  if test "$with_gphoto2" != "no" ; then 
    AC_CHECK_TOOL(HAVE_GPHOTO2, pkg-config, false)

    if test ${HAVE_GPHOTO2} != "false" ; then
      if pkg-config --exists libgphoto2 ; then
        with_gphoto2="`pkg-config --modversion libgphoto2`"
	GPHOTO2_CPPFLAGS="`pkg-config --cflags libgphoto2`"
        GPHOTO2_LIBS="`pkg-config --libs libgphoto2`"

        saved_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${GPHOTO2_CPPFLAGS}"
	saved_LIBS="${LIBS}"
	LIBS="${LIBS} ${GPHOTO2_LIBS}"
 	# Make sure we an really use the library
        AC_CHECK_FUNCS(gp_camera_init, HAVE_GPHOTO2=true, HAVE_GPHOTO2=false)
	if test "${HAVE_GPHOTO2}" = "true"; then
	  AC_CHECK_FUNCS(gp_port_info_get_path)
	fi
	CPPFLAGS="${saved_CPPFLAGS}"
        LIBS="${saved_LIBS}"
      else
        HAVE_GPHOTO2=false
      fi
      if test "${HAVE_GPHOTO2}" = "false"; then
        GPHOTO2_CPPFLAGS="" 
        GPHOTO2_LIBS="" 
      else
        SANE_EXTRACT_LDFLAGS(GPHOTO2_LIBS, GPHOTO2_LDFLAGS)
      fi
    fi
  fi
  AC_SUBST(GPHOTO2_CPPFLAGS)
  AC_SUBST(GPHOTO2_LIBS)
  AC_SUBST(GPHOTO2_LDFLAGS)
])

#
# Check for AF_INET6, determines whether or not to enable IPv6 support
# Check for ss_family member in struct sockaddr_storage
AC_DEFUN([SANE_CHECK_IPV6],
[
  AC_MSG_CHECKING([whether to enable IPv6]) 
  AC_ARG_ENABLE(ipv6, 
    AC_HELP_STRING([--disable-ipv6],[disable IPv6 support]), 
      [  if test "$enableval" = "no" ; then
         AC_MSG_RESULT([no, manually disabled]) 
         ipv6=no 
         fi
      ])

  if test "$ipv6" != "no" ; then
    AC_TRY_COMPILE([
	#define INET6 
	#include <sys/types.h> 
	#include <sys/socket.h> ], [
	 /* AF_INET6 available check */  
 	if (socket(AF_INET6, SOCK_STREAM, 0) < 0) 
   	  exit(1); 
 	else 
   	  exit(0); 
      ],[
        AC_MSG_RESULT(yes) 
        AC_DEFINE([ENABLE_IPV6], 1, [Define to 1 if the system supports IPv6]) 
        ipv6=yes
      ],[
        AC_MSG_RESULT([no (couldn't compile test program)]) 
        ipv6=no
      ])
  fi

  if test "$ipv6" != "no" ; then
    AC_MSG_CHECKING([whether struct sockaddr_storage has an ss_family member])
    AC_TRY_COMPILE([
	#define INET6
	#include <sys/types.h>
	#include <sys/socket.h> ], [
	/* test if the ss_family member exists in struct sockaddr_storage */
	struct sockaddr_storage ss;
	ss.ss_family = AF_INET;
	exit (0);
    ], [
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAS_SS_FAMILY], 1, [Define to 1 if struct sockaddr_storage has an ss_family member])
    ], [
		AC_TRY_COMPILE([
		#define INET6
		#include <sys/types.h>
		#include <sys/socket.h> ], [
		/* test if the __ss_family member exists in struct sockaddr_storage */
		struct sockaddr_storage ss;
		ss.__ss_family = AF_INET;
		exit (0);
	  ], [
		AC_MSG_RESULT([no, but __ss_family exists])
		AC_DEFINE([HAS___SS_FAMILY], 1, [Define to 1 if struct sockaddr_storage has __ss_family instead of ss_family])
	  ], [
		AC_MSG_RESULT([no])
		ipv6=no
    	  ])
    ])
  fi	
])

#
# Verifies that values in $BACKENDS and updates FILTERED_BACKEND
# with either backends that can be compiled or fails the script.
AC_DEFUN([SANE_CHECK_BACKENDS],
[
if test "${user_selected_backends}" = "yes"; then
  DISABLE_MSG="aborting"
else
  DISABLE_MSG="disabling"
fi

FILTERED_BACKENDS=""
for be in ${BACKENDS}; do
  backend_supported="yes"
  case $be in
    plustek_pp)
    case "$host_os" in
      gnu*) 
      echo "*** $be backend not supported on GNU/Hurd - $DISABLE_MSG"
      backend_supported="no"
      ;;
    esac
    ;;

    dc210|dc240)
    if test "${sane_cv_use_libjpeg}" != "yes"; then
      echo "*** $be backend requires JPEG library - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    canon_pp|hpsj5s)
    if test "${sane_cv_use_libieee1284}" != "yes"; then
      echo "*** $be backend requires libieee1284 library - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    mustek_pp)
    if test "${sane_cv_use_libieee1284}" != "yes" && test "${enable_parport_directio}" != "yes"; then
      echo "*** $be backend requires libieee1284 or parport-directio libraries - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    dell1600n_net) 
    if test "${sane_cv_use_libjpeg}" != "yes" || test "${sane_cv_use_libtiff}" != "yes"; then
      echo "*** $be backend requires JPEG and TIFF library - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    epsonds)
    if test "${sane_cv_use_libjpeg}" != "yes"; then
      echo "*** $be backend requires JPEG library - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    gphoto2)
    if test "${HAVE_GPHOTO2}" != "true" \
      -o "${sane_cv_use_libjpeg}" != "yes"; then
      echo "*** $be backend requires gphoto2 and JPEG libraries - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    pint)
    if test "${ac_cv_header_sys_scanio_h}" = "no"; then
      echo "*** $be backend requires sys/scanio.h - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    qcam)
    if ( test "${ac_cv_func_ioperm}" = "no" || test "${sane_cv_have_sys_io_h_with_inb_outb}" = "no" )\
      && test "${ac_cv_func__portaccess}" = "no"; then
      echo "*** $be backend requires (ioperm, inb and outb) or portaccess functions - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    v4l)
    if test "${have_linux_ioctl_defines}" != "yes" \
      || test "${have_libv4l1}" != "yes"; then
      echo "*** $be backend requires v4l libraries - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    net)
    if test "${ac_cv_header_sys_socket_h}" = "no"; then
      echo "*** $be backend requires sys/socket.h - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;

    mustek_usb2|kvs40xx)
    if test "${have_pthread}" != "yes"; then
      echo "*** $be backend requires pthread library - $DISABLE_MSG"
      backend_supported="no"
    fi
    ;;
  esac
  if test "${backend_supported}" = "no"; then
    if test "${user_selected_backends}" = "yes"; then
      exit 1
    fi
  else
    FILTERED_BACKENDS="${FILTERED_BACKENDS} $be"
  fi
done
])

#
# Generate prototypes for functions not available on the system
AC_DEFUN([SANE_PROTOTYPES],
[
AH_BOTTOM([

#if defined(__MINGW32__)
#define _BSDTYPES_DEFINED
#endif

#ifndef HAVE_U_CHAR
#define u_char unsigned char
#endif
#ifndef HAVE_U_SHORT
#define u_short unsigned short
#endif
#ifndef HAVE_U_INT
#define u_int unsigned int
#endif
#ifndef HAVE_U_LONG
#define u_long unsigned long
#endif

/* Prototype for getenv */
#ifndef HAVE_GETENV
#define getenv sanei_getenv
char * getenv(const char *name);
#endif

/* Prototype for inet_ntop */
#ifndef HAVE_INET_NTOP
#define inet_ntop sanei_inet_ntop
#include <sys/types.h>
const char * inet_ntop (int af, const void *src, char *dst, size_t cnt);
#endif

/* Prototype for inet_pton */
#ifndef HAVE_INET_PTON
#define inet_pton sanei_inet_pton
int inet_pton (int af, const char *src, void *dst);
#endif

/* Prototype for isfdtype */
#ifndef HAVE_ISFDTYPE
#define isfdtype sanei_isfdtype
int isfdtype(int fd, int fdtype);
#endif

/* Prototype for sigprocmask */
#ifndef HAVE_SIGPROCMASK
#define sigprocmask sanei_sigprocmask
int sigprocmask (int how, int *new, int *old);
#endif

/* Prototype for snprintf */
#ifndef HAVE_SNPRINTF
#define snprintf sanei_snprintf
#include <sys/types.h>
int snprintf (char *str,size_t count,const char *fmt,...);
#endif

/* Prototype for strcasestr */
#ifndef HAVE_STRCASESTR
#define strcasestr sanei_strcasestr
char * strcasestr (const char *phaystack, const char *pneedle);
#endif

/* Prototype for strdup */
#ifndef HAVE_STRDUP
#define strdup sanei_strdup
char *strdup (const char * s);
#endif

/* Prototype for strndup */
#ifndef HAVE_STRNDUP
#define strndup sanei_strndup
#include <sys/types.h>
char *strndup(const char * s, size_t n);
#endif

/* Prototype for strsep */
#ifndef HAVE_STRSEP
#define strsep sanei_strsep
char *strsep(char **stringp, const char *delim);
#endif

/* Prototype for usleep */
#ifndef HAVE_USLEEP
#define usleep sanei_usleep
unsigned int usleep (unsigned int useconds);
#endif

/* Prototype for vsyslog */
#ifndef HAVE_VSYSLOG
#include <stdarg.h>
void vsyslog(int priority, const char *format, va_list args);
#endif
])
])

m4_include([m4/libtool.m4])
m4_include([m4/byteorder.m4])
m4_include([m4/stdint.m4])
