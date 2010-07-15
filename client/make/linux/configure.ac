AC_INIT([Atrinik Client], [1.1.1], [admin@atrinik.org])

AC_CONFIG_AUX_DIR(make_utils)
AC_CANONICAL_SYSTEM

AC_PREFIX_DEFAULT(../../../client-1.1.1)

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL

AC_ARG_ENABLE(debug,
   [  --enable-debug          compile in debug messages],
   [export USE_DEBUG=yes],
   [export USE_DEBUG=no]
)

AC_ARG_ENABLE(sound,
   [  --enable-sound          compile with SDL sound (default)],
   [ if test "x$enableval" = "xyes" ; then
		AC_DEFINE(INSTALL_SOUND,, [ Use the SDL_mixer sound system. Remove when you have no sound card or slow computer])
	 fi
   ],
   [AC_DEFINE(INSTALL_SOUND,, [ Use the SDL_mixer sound system. Remove when you have no sound card or slow computer])]
)

AC_ARG_ENABLE(win32,
   [  --enable-win32          compile with Win32 support],
   [  if test "x$enableval" = "xyes" ; then
		 AC_DEFINE(__WIN_32, 1, [Win32])
      else
		 AC_DEFINE(__LINUX, 1, [Linux])
      fi
   ],
   [ AC_DEFINE(__LINUX, 1, [Linux]) ]
)

# IPv6 support
AC_ARG_ENABLE(getaddrinfo,
   AC_HELP_STRING([--enable-getaddrinfo], [compile with getaddrinfo() (IPv6) support. ]), 
   [AC_CHECK_FUNCS([getaddrinfo])])

AC_PATH_PROG(DEPEND, makedepend)
AC_PATH_PROG(CP,cp)
AC_PATH_PROG(RM,rm)
AC_PATH_PROG(RMDIR,rmdir)
AC_PATH_PROG(MKDIR,mkdir)
AC_PATH_PROG(SED, sed)
AC_PATH_PROG(AR,ar)
AC_PATH_PROG(RANLIB,ranlib)
AC_PATH_PROG(SDL_CONFIG, sdl-config, [ AC_MSG_ERROR([sdl-config not found...]) ])

AC_SUBST(SDL_CFLAGS, `${SDL_CONFIG} --cflags`)
AC_SUBST(SDL_LIBS, `${SDL_CONFIG} --libs`)

# Check for zlib.
AX_CHECK_ZLIB()

AC_CHECK_FUNCS(stricmp strnicmp strcasecmp strncasecmp)
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([fcntl.h])

AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

AC_CHECK_HEADERS(curl/curl.h)
AC_CHECK_LIB(curl, curl_global_init, [LIBS="$LIBS -lcurl"])
AC_PATH_PROGS(CURL, curl, no)

if test "x$prefix" != xNONE; then
   AC_DEFINE_UNQUOTED(PREFIX, "${prefix}", [Installation prefix])
else
   AC_DEFINE_UNQUOTED(PREFIX, "${ac_default_prefix}", [Installation prefix])
fi

AC_CONFIG_HEADER(../../src/include/define.h)
AC_OUTPUT(Makefile ../../src/Makefile)
