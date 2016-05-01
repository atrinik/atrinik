/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Cross-platform support header file.
 */

#ifndef PORTING_H
#define PORTING_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __GNUC__
#define likely(x)           __builtin_expect(!!(x), 1)
#define unlikely(x)         __builtin_expect(!!(x), 0)
#else
/* If we're not using GNU C, ignore __attribute__ */
#define  __attribute__(x)
/* Ignore likely/unlikely branch prediction when not using GNU C.*/
#define likely(x)           (x)
#define unlikely(x)         (x)
#endif

#ifdef WIN32
#ifndef WINVER
#define WINVER 0x502
#endif
#endif

#include <cmake.h>
#include <toolkit_cmake.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <inttypes.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) (strlen((dirent)->d_name))
#elif defined(HAVE_SYS_NDIR_H) || defined(HAVE_SYS_DIR_H) || defined(HAVE_NDIR_H)
#define dirent direct
#define NAMLEN(dirent) ((dirent)->d_namlen)
#ifdef HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#ifdef HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#ifdef HAVE_X11_XMU
#include <X11/Xmu/Atoms.h>
#endif
#endif

#include <pthread.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <imagehlp.h>

#ifdef __MINGW32__
#include <ws2tcpip.h>

#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0x00000008
#endif

#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif
#endif

#define mkdir(__a, __b) mkdir(__a)
#define sleep(_x) Sleep((_x) * 1000)

#ifdef __MINGW32__
#define _set_fmode(_mode) \
        { \
            _fmode = (_mode); \
        }
#endif

#undef X509_NAME
#endif

#define GETTIMEOFDAY(last_time) gettimeofday(last_time, NULL);

#ifdef HAVE_SRANDOM
#define RANDOM() random()
#define SRANDOM(xyz) srandom(xyz)
#else
#ifdef HAVE_SRAND48
#define RANDOM() lrand48()
#define SRANDOM(xyz) srand48(xyz)
#else
#ifdef HAVE_SRAND
#define RANDOM() rand()
#define SRANDOM(xyz) srand(xyz)
#else
#error "Could not find a usable random routine"
#endif
#endif
#endif

#ifdef HAVE_STRICMP
#define strcasecmp _stricmp
#endif

#ifdef HAVE_STRNICMP
#define strncasecmp _strnicmp
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef FABS
#define FABS(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

/* Make sure M_PI is defined. */
#ifndef M_PI
#define M_PI 3.141592654
#endif

#ifndef F_OK
#define F_OK 6
#endif

#ifndef R_OK
#define R_OK 6
#endif

#ifndef W_OK
#define W_OK 2
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifndef HAVE_IPV6
#define sockaddr_storage sockaddr_in
#endif

/** Used for faces. */
typedef uint16_t Fontindex;

/** Object unique IDs. */
typedef uint32_t tag_t;

/* Only C99 has lrint. */
#if !defined(_ISOC99_SOURCE) && (!defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200112L)
#define lrint(x) (floor((x) + ((x) > 0) ? 0.5 : -0.5))
#endif

#if defined(HAVE_X11)
typedef Display *x11_display_type;
typedef Window x11_window_type;
#elif defined(WIN32)
typedef HWND x11_display_type;
typedef HWND x11_window_type;
#else
typedef void *x11_display_type;
typedef void *x11_window_type;
#endif

#ifndef HAVE_STRTOK_R
extern char *strtok_r(char *s, const char *delim, char **save_ptr);
#endif

#ifndef HAVE_TEMPNAM
extern char *tempnam(const char *dir, const char *pfx);
#endif

#ifndef HAVE_STRDUP
extern char *strdup(const char *s);
#endif

#ifndef HAVE_STRNDUP
extern char *strndup(const char *s, size_t n);
#endif

#ifndef HAVE_STRERROR
extern char *strerror(int errnum);
#endif

#ifndef HAVE_STRCASESTR
extern const char *strcasestr(const char *haystack, const char *needle);
#endif

#ifndef HAVE_GETTIMEOFDAY

struct timezone {
    /* Minutes west of Greenwich. */
    int tz_minuteswest;
    /* Type of DST correction. */
    int tz_dsttime;
};

extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#ifndef HAVE_GETLINE
extern ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

#ifndef HAVE_USLEEP
extern int usleep(uint32_t usec);
#endif

#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t max);
#endif

#ifndef HAVE_MKSTEMP
int mkstemp(char *tmpl);
#endif

#ifndef HAVE_SINCOS
void sincos(double x, double *s, double *c);
#endif

#endif
