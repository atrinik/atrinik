# Process this file with autoconf to produce a configure script.

AC_INIT(info)

# AC_PPREFIX_DEFAULT(./../..)

# we want a config.h file instead of -D options.
AC_CONFIG_HEADER(../../src/include/autoconf.h)
AC_CONFIG_AUX_DIR(make_utils)

# Checks for programs.
# AC_PROG_YACC
AC_CANONICAL_SYSTEM
AC_PROG_CC

case "$target" in
  alpha-dec-osf*)
    # If we are not using gcc, we want the ansi version of cc.
    if test -z "$GCC"; then
#      CFLAGS="$CFLAGS -std1 -warnprotos" # Not yet ready for warnprotos...
      CFLAGS="$CFLAGS -std1"
    fi
    ;;
  *)
    ;;
esac

AC_PROG_CPP
AC_PROG_LEX
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_ARG_ENABLE(old-layout, [ --enable-old-layout    Use the old (0.95.3 and earlier) install layout],
	old_layout=yes, old_layout=no)
AC_ARG_WITH(python,
            [AS_HELP_STRING([--with-python=dir], [Directory to look for python headers/library (default=standard system location)])],
            PYTHON_HOME="$withval")

# check for some other programs
AC_PATH_PROG(MKDIR, mkdir)
AC_PATH_PROG(TAR, tar)
AC_PATH_PROG(DEPEND, makedepend)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(BASENAME, basename)
AC_PATH_PROG(SED, sed)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(LATEX, latex)
AC_PATH_PROG(GZIP, gzip)
AC_PATH_PROG(GUNZIP, gunzip)
AC_PATH_PROG(PERL, perl)

# Python plugin
CHECK_PYTHON(
	[PLUGIN_PYTHON="python.la"]
	[will_build_python="yes"],
	[will_build_python="no"]
	[AC_MSG_NOTICE([No Python found. Python plugin will not be built.])]
)
AM_CONDITIONAL(PYTHON_PLUGIN,test "x$PLUGIN_PYTHON" != "x")
AC_SUBST(PLUGIN_PYTHON)

if test -n "$GZIP" -a -z "$GUNZIP" ; then
    echo "Found gzip, but not gunzip - setting GUNZIP to gzip -c";
    GUNZIP="$GZIP -c"
fi;

if test -z "$COMPRESS" ; then
	AC_PATH_PROG(COMPRESS, compress)
	AC_PATH_PROG(UNCOMPRESS, uncompress)
fi

AC_PATH_PROG(BZIP, bzip2)
AC_PATH_PROG(BUNZIP, bunzip2)

if test -n "$BZIP" -a -z "$BUNZIP" ; then
    echo "Found bzip2, but not bunzip2 - setting BUNZIP to bzip2 -c";
    BUNZIP="$BZIP -c"
fi;

if test -z "$COMPRESS" -a -z "$GZIP" -a -z "$BZIP" ; then
	echo "Unable to find either compress, bzip2,  or gzip - hope you don't plan on compressing";
	echo "any files.";
fi;

AC_PATH_PROGS(BOURNE_SHELL, sh ksh bash)

# The following are useful for debugging
AC_ARG_ENABLE(profiling, [  --enable-profiling      compile with profiling enabled ],
		EXTRA_CFLAGS="$EXTRA_CFLAGS -pg"; LDFLAGS="$LDFLAGS -pg", )

AC_ARG_ENABLE(optimizing, [  --disable-optimizing    compile without optimization ],
		EXTRA_CFLAGS="$EXTRA_CFLAGS -O0", )

# The follow two are really just a way to specify where the xpm/gtk information
# resides if not with the normal X11 stuff (ie, in /opt/X11 and not /usr
# /usr/X11

AC_ARG_WITH(ldflags, [  --with-ldflags=dir      provide addition linker directives to find libraries ],
		EXTRA_LIB="$withval")

AC_ARG_WITH(includes, [  --with-includes=dir     provide different compiler options to find headers with ],
		EXTRA_INC="$withval")

# nsl, socket may be needed for the X-windowing system, so check
# for them before before checking for X.

AC_CHECK_LIB(nsl, main)
AC_CHECK_LIB(socket, main)

# Checks for libraries.
# Start of X11 libraries
AC_PATH_XTRA

CPPFLAGS="$CPPFLAGS $EXTRA_INC"
LDFLAGS="$LDFLAGS $EXTRA_LIB"

AC_CHECK_LIB(m, main)

# Misc libraries.
AC_CHECK_LIB(crypt, main)
AC_CHECK_LIB(des, des_crypt)

# Gros - Changed this to support some external programs that do not like dmalloc (like Python).
# If you really need to make some memory debugging, uncomment the dmalloc line and comment out the dmalloclp one.

AC_CHECK_LIB(dmalloclp, main)
# AC_CHECK_LIB(dmalloc, main)

# Gros - Support for the dynamic loader library
AC_CHECK_LIB(dl, main)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/time.h time.h unistd.h stddef.h stdlib.h crypt.h arpa/inet.h des.h)

AC_CHECK_HEADERS(curl/curl.h)
AC_CHECK_LIB(curl, curl_global_init, [LIBS="$LIBS -lcurl"])
AC_PATH_PROGS(CURL, curl, no)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T

AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(gettimeofday strerror strcasecmp strncasecmp stricmp strnicmp srandom srand48 srand getdtablesize sysconf)

AC_MSG_CHECKING(how many args gettimeofday uses)
AC_CACHE_VAL(ac_cv_gettimeofday_args,
 [AC_TRY_COMPILE([#include <sys/time.h>
	#include <unistd.h>],
	[struct timeval tv; struct timezone tzp;
	gettimeofday(&tv, &tzp);],
	[ac_gettimeofday_args=2],
	[AC_TRY_COMPILE([#include <sys/time.h>
		#include <unistd.h>],
		[struct timeval tv; gettimeofday(&tv);],
		[ac_gettimeofday_args=1],
		[ac_gettimeofday_args=0])])
  ac_cv_gettimeofday_args=$ac_gettimeofday_args])

ac_gettimeofday_args=$ac_cv_gettimeofday_args
if test $ac_gettimeofday_args = 1 ; then
  AC_DEFINE(HAVE_GETTIMEOFDAY)
  AC_MSG_RESULT(one argument)
elif test $ac_gettimeofday_args = 2 ; then
  AC_DEFINE(HAVE_GETTIMEOFDAY)
  AC_DEFINE(GETTIMEOFDAY_TWO_ARGS)
  AC_MSG_RESULT(two arguments)
else
  AC_MSG_RESULT(unknown)
fi

AC_SUBST(x_includes)
AC_SUBST(x_libraries)
AC_SUBST(no_x)
AC_SUBST(X11LIBS)
AC_SUBST(PERL)
AC_SUBST(EXTRA_CFLAGS)

# Do the evals because most of these are made up of sub variables and
# we don't want the include to have something like
# #define DATADIR "${prefix}/somedir"

# hack to set prefix - the AC_OUTPUT does this, but that is too late
# for our purposes here.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

nprefix=`eval echo ${prefix}`

ndatadir=`eval echo ${datadir}`/Atrinik
nlocaldir=`eval echo ${localstatedir}`/Atrinik

# Have to do this one twice, since it uses two levels of indirection

nlibdir=`eval echo ${libdir}`
nlibdir=`eval echo ${nlibdir}`

AC_DEFINE_UNQUOTED(LIBDIR, "./lib")
AC_DEFINE_UNQUOTED(PREFIXDIR, "${nprefix}")
AC_DEFINE_UNQUOTED(DATADIR, "./lib")
AC_DEFINE_UNQUOTED(LOCALDIR, "./data")
AC_DEFINE_UNQUOTED(COMPRESS,"${COMPRESS}")
AC_DEFINE_UNQUOTED(UNCOMPRESS,"${UNCOMPRESS}")
AC_DEFINE_UNQUOTED(GZIP,"${GZIP}")
AC_DEFINE_UNQUOTED(GUNZIP,"${GUNZIP}")
AC_DEFINE_UNQUOTED(BZIP,"${BZIP}")
AC_DEFINE_UNQUOTED(BUNZIP,"${BUNZIP}")

datadir=${ndatadir}
localstatedir=${nlocaldir}

AC_OUTPUT(Makefile
	../../src/random_maps/Makefile ../../src/socket/Makefile
	../../src/server/Makefile ../../src/include/Makefile
	../../src/utils/Makefile ../../src/common/Makefile
	../../src/zlib/Makefile ../../src/utils/atrinikloop
	../../src/types/Makefile
	../../src/plugins/Makefile
	../../src/plugins/plugin_python/Makefile
	../../src/plugins/plugin_arena/Makefile
	)

AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([Configuration summary....])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  Build options])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([    Will build python plugin?            $will_build_python])
