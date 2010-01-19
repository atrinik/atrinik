/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

#if !defined(AFX_STDAFX_H__31666CA1_2474_11D5_AE6C_F07569C10000__INCLUDED_)
#define AFX_STDAFX_H__31666CA1_2474_11D5_AE6C_F07569C10000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
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
#define __STDC__ 1
#endif

#ifndef HAVE_SNPRINTF
#define HAVE_SNPRINTF 1
#define snprintf _snprintf
#endif

#include "version.h"

/* include all needed autoconfig.h defines */
#define HAVE_STRICMP
#define HAVE_STRNICMP

#define HAVE_SRAND
#ifndef HAVE_FCNTL_H
#define HAVE_FCNTL_H
#endif
#ifndef HAVE_STDDEF_H
#define HAVE_STDDEF_H
#endif
#define GETTIMEOFDAY_TWO_ARGS
#define MAXPATHLEN 256
#ifndef SIZEOF_LONG
#define SIZEOF_LONG 8
#endif

/* Many defines to redirect unix functions or fake standard unix values */
#define inline __inline
#define unlink(__a) _unlink(__a)
#define mkdir(__a, __b) mkdir(__a)
#define getpid() _getpid()
#define popen(__a, __b) _popen(__a, __b)
#define pclose(__a) _pclose(__a)
#define vsnprintf _vsnprintf

#ifndef R_OK
#define R_OK 6
#endif

#ifndef F_OK
#define F_OK 6
#endif

#define PREFIXDIR ""

#ifndef S_ISDIR
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISGID
#define S_ISGID 0002000
#endif
#ifndef S_IWOTH
#define S_IWOTH 0000200
#endif
#ifndef S_IWGRP
#define S_IWGRP 0000020
#endif
#ifndef S_IWUSR
#define S_IWUSR 0000002
#endif
#ifndef S_IROTH
#define S_IROTH 0000400
#endif
#ifndef S_IRGRP
#define S_IRGRP 0000040
#endif
#ifndef S_IRUSR
#define S_IRUSR 0000004
#endif

#define COMPRESS "/usr/bin/compress"
#define UNCOMPRESS "/usr/bin/uncompress"
#define GZIP "/bin/gzip"
#define GUNZIP "/bin/gunzip"
#define BZIP "/usr/bin/bzip2"
#define BUNZIP "/usr/bin/bunzip2"

#define YY_NEVER_INTERACTIVE 1

#ifndef MSG_DONWAIT
#define MSG_DONTWAIT 0
#endif

/* struct dirent - same as Unix */

typedef struct dirent
{
	/* inode (always 1 in WIN32) */
	long d_ino;

	/* offset to this dirent */
	off_t d_off;

	/* length of d_name */
	unsigned short d_reclen;

	/* filename (null terminated) */
	char d_name[_MAX_FNAME + 1];
} dirent;

#define NAMLEN(dirent) strlen((dirent)->d_name)

/* typedef DIR - not the same as Unix */
typedef struct
{
	/* _findfirst/_findnext handle */
	long handle;

	/* offset into directory */
	short offset;

	/* 1 if there are not more files */
	short finished;

	/* from _findfirst/_findnext */
	struct _finddata_t fileinfo;

	/* the dir we are reading */
	char *dir;

	/* the dirent to return */
	struct dirent dent;
} DIR;

/** Timezone structure, for gettimeofday(). */
struct timezone
{
	int tz_minuteswest;
	int tz_dsttime;
};

/* Function prototypes */
extern int gettimeofday(struct timeval *tv, struct timezone *timezone_Info);
extern DIR *opendir(const char *);
extern struct dirent *readdir(DIR *);
extern int closedir(DIR *);
extern void rewinddir(DIR *);
