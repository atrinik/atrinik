/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Structures and types used to implement opendir/readdir/closedir
 * on Windows 95/NT and set the low level defines.
 *
 * Also some Windows-specific includes and tweaks. */

#ifndef AFX_STDAFX_H__31666CA1_2474_11D5_AE6C_F07569C10000__INCLUDED_
#define AFX_STDAFX_H__31666CA1_2474_11D5_AE6C_F07569C10000__INCLUDED_

#if _MSC_VER > 1000
#	pragma once
#endif

#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <time.h>
#include <direct.h>
#include <math.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <process.h>

#ifndef __STDC__
#	define __STDC__ 1
#endif

#ifndef HAVE_SNPRINTF
#	define HAVE_SNPRINTF 1
#	define snprintf _snprintf
#endif

#include "version.h"

/* include all needed autoconfig.h defines */
#define HAVE_STRICMP
#define HAVE_STRNICMP
#define HAVE_STRERROR
#define HAVE_SRAND

#ifndef HAVE_FCNTL_H
#	define HAVE_FCNTL_H
#endif

#ifndef HAVE_STDDEF_H
#	define HAVE_STDDEF_H
#endif

#define MAXPATHLEN 256

#ifndef SIZEOF_LONG
#	define SIZEOF_LONG 8
#endif

/* Many defines to redirect UNIX functions */
#define inline __inline
#define unlink(__a) _unlink(__a)
#define mkdir(__a, __b) _mkdir(__a)
#define getpid() _getpid()
#define popen(__a, __b) _popen(__a, __b)
#define pclose(__a) _pclose(__a)
#define vsnprintf _vsnprintf

#ifndef MINGW
#	define strdup _strdup
#	define fileno _fileno
#	define lseek _lseek
#	define access _access
#	define stricmp _stricmp
#	define strnicmp _strnicmp
#	define chmod _chmod
#	define umask _umask
#	define hypot _hypot
	/* Conversion from 'xxx' to 'yyy', possible loss of data */
#	pragma warning(disable: 4244)
	/* Conversion from 'size_t' to 'int', possible loss of data */
#	pragma warning(disable: 4267)
	/* Initializing float f = 0.05; instead of f = 0.05f; */
#	pragma warning(disable: 4305)
	/* Right shift by too large amount, data loss */
#	pragma warning(disable: 4333)
#endif

#ifndef R_OK
#	define R_OK 6
#endif

#ifndef W_OK
#	define W_OK 2
#endif

#ifndef F_OK
#	define F_OK 6
#endif

#ifndef S_ISDIR
#	define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#	define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISGID
#	define S_ISGID 0002000
#endif

#ifndef S_IWOTH
#	define S_IWOTH 0000200
#endif

#ifndef S_IWGRP
#	define S_IWGRP 0000020
#endif

#ifndef S_IWUSR
#	define S_IWUSR 0000002
#endif

#ifndef S_IROTH
#	define S_IROTH 0000400
#endif

#ifndef S_IRGRP
#	define S_IRGRP 0000040
#endif

#ifndef S_IRUSR
#	define S_IRUSR 0000004
#endif

#define COMPRESS "/usr/bin/compress"
#define UNCOMPRESS "/usr/bin/uncompress"
#define GZIP "/bin/gzip"
#define GUNZIP "/bin/gunzip"
#define BZIP "/usr/bin/bzip2"
#define BUNZIP "/usr/bin/bunzip2"
#define PLUGIN_SUFFIX ".dll"

#define YY_NEVER_INTERACTIVE 1

#ifndef MSG_DONWAIT
#	define MSG_DONTWAIT 0
#endif

#define WIFEXITED(x) 1
#define WEXITSTATUS(x) x

/* Doesn't exist, just a plain int */
#ifndef socklen_t
#	define socklen_t int
#endif

/* Same as socklen_t, doesn't exist... */
#ifndef pid_t
	#define pid_t int
#endif

#define sleep(x) Sleep(x * 1000)

/**
 * Dirent - same structure data as UNIX. */
typedef struct dirent
{
	/** inode (always 1 on WIN32). */
	long d_ino;

	/** Offset to this dirent. */
	off_t d_off;

	/** Length of d_name. */
	unsigned short d_reclen;

	/** Filename (NULL terminated). */
	char d_name[_MAX_FNAME + 1];
} dirent;

#define NAMLEN(dirent) strlen((dirent)->d_name)

/**
 * Directory handle. */
typedef struct
{
	/** _findfirst/_findnext handle. */
	long handle;

	/** Offset into directory. */
	short offset;

	/** 1 if there are no more files. */
	short finished;

	/** From _findfirst/_findnext. */
	struct _finddata_t fileinfo;

	/** The dir we are reading. */
	char *dir;

	/** The dirent to return. */
	struct dirent dent;
} DIR;

/** Timezone structure, for gettimeofday(). */
struct timezone
{
	int tz_minuteswest;
	int tz_dsttime;
};

/* Function prototypes */
int gettimeofday(struct timeval *tv, struct timezone *timezone_Info);
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);
void rewinddir(DIR *);
