/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
 * This file contains various functions that are not really unique for
 * Atrinik, but rather provides what should be standard functions for
 * systems that do not have them. In this way, most of the nasty system
 * dependent stuff is contained here, with the program calling these
 * functions. */

#ifdef WIN32
#include "process.h"
#define pid_t int
#else
#include <ctype.h>
#include <sys/stat.h>

#include <sys/param.h>
#include <stdio.h>
#include <autoconf.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Has to be after above includes so we don't redefine some values */
#include <global.h>

/** Used to generate temporary unique name. */
static unsigned int curtmp = 0;

/**
 * A replacement for the tempnam() function since it's not defined
 * at some unix variants.
 * @param dir Directory where to create the file. Can be NULL, in which
 * case NULL is returned.
 * @param pfx prefix to create unique name. Can be NULL.
 * @return Path to temporary file, or NULL if failure. Must be freed by
 * caller. */
char *tempnam_local(char *dir, char *pfx)
{
	char *f, *name;
	pid_t pid = getpid();

	if (!(name = (char *) malloc(MAXPATHLEN)))
	{
		return NULL;
	}

	if (!pfx)
	{
		pfx = "cftmp.";
	}

	/* This is a pretty simple method - put the pid as a hex digit and
	 * just keep incrementing the last digit.  Check to see if the file
	 * already exists - if so, we'll just keep looking - eventually we
	 * should find one that is free. */
	if ((f = (char *) dir) != NULL)
	{
		do
		{
			snprintf(name, MAXPATHLEN, "%s/%s%hx.%d", f, pfx, pid, curtmp);

			curtmp++;
		}
		while (access(name, F_OK) != -1);

		return name;
	}

	return NULL;
}

#if defined(sgi)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define popen popen_local

FILE *popen_local(const char *command, const char *type)
{
	int	fd[2];
	int	pd;
	FILE *ret;

	if (!strcmp(type, "r"))
		pd = STDOUT_FILENO;
	else if (!strcmp(type, "w"))
		pd = STDIN_FILENO;
	else
		return NULL;

	if (pipe(fd) != -1)
	{
		switch (fork())
		{
			case -1:
				close(fd[0]);
				close(fd[1]);
				break;

			case 0:
				close(fd[0]);
				if ((fd[1] == pd) || (dup2(fd[1], pd) == pd))
				{
					if (fd[1] != pd)
						close(fd[1]);

					execl("/bin/sh", "sh", "-c", command, NULL);
					close(pd);
				}
				exit(1);
				break;

			default:
				close(fd[1]);
				if (ret = fdopen(fd[0], type))
					return ret;

				close(fd[0]);
				break;
		}
	}

	return NULL;
}

#endif

/**
 * A replacement of strdup(), since it's not defined at some
 * unix variants.
 * @param str String to duplicate.
 * @return Copy, needs to be freed by caller. NULL on memory allocation
 * error. */
char *strdup_local(const char *str)
{
	char *c = (char *) malloc(strlen(str) + 1);

	if (c != NULL)
	{
		strcpy(c, str);
	}

	return c;
}

#define DIGIT(x)        (isdigit(x) ? (x) - '0' : \
islower (x) ? (x) + 10 - 'a' : (x) + 10 - 'A')
#define MBASE ('z' - 'a' + 1 + 10)

/* A replacement of strtol() since it's not defined at
 * many unix systems. */

long strtol_local(register char *str, char **ptr, register int base)
{
	register long val;
	register int c;
	int xx, neg = 0;

	/* in case no number is formed */
	if (ptr != (char **) 0)
		*ptr = str;

	/* base is invalid */
	if (base < 0 || base > MBASE)
		return 0;

	if (!isalnum (c = *str))
	{
		while (isspace (c))
			c = *++str;

		switch (c)
		{
			case '-':
				neg++;

			case '+':
				c = *++str;
		}
	}

	if (base == 0)
	{
		if (c != '0')
			base = 10;
		else
		{
			if (str[1] == 'x' || str[1] == 'X')
				base = 16;
			else
				base = 8;
		}
	}

	/* For any base > 10, the digits incrementally following
	 * 9 are assumed to be "abc...z" or "ABC...Z" */

	/* no number formed */
	if (!isalnum (c) || (xx = DIGIT (c)) >= base)
		return 0;

	/* skip over leading "0x" or "0X" */
	if (base == 16 && c == '0' && isxdigit (str[2]) && (str[1] == 'x' || str[1] == 'X'))
		c = *(str += 2);

	/* accumulate neg avoids surprises near
	 * MAXLONG */
	for (val = -DIGIT (c); isalnum (c = *++str) && (xx = DIGIT (c)) < base;)
		val = base * val - xx;

	if (ptr != (char **) 0)
		*ptr = str;

	return (neg ? val : -val);
}

/* This seems to be lacking on some system */
#if defined(HAVE_STRNICMP)
#else
#if !defined(HAVE_STRNCASECMP)
int strncasecmp(char *s1, char *s2, int n)
{
	register int c1, c2;

	while (*s1 && *s2 && n)
	{
		c1 = tolower(*s1);
		c2 = tolower(*s2);

		if (c1 != c2)
			return (c1 - c2);

		s1++;
		s2++;
		n--;
	}

	if (!n)
		return 0;

	return (int) (*s1 - *s2);
}
#endif
#endif

#if defined(HAVE_STRICMP)
#else
#if !defined(HAVE_STRCASECMP)
int strcasecmp(char *s1, char*s2)
{
	register int c1, c2;

	while (*s1 && *s2)
	{
		c1 = tolower(*s1);
		c2 = tolower(*s2);

		if (c1 != c2)
			return (c1 - c2);

		s1++;
		s2++;
	}

	if (*s1 == '\0' && *s2 == '\0')
		return 0;

	return (int) (*s1 - *s2);
}
#endif
#endif

/**
 * Takes an error number and returns a string with a description of the
 * error.
 * @param errnum The error number.
 * @return If HAVE_STRERROR is defined, strerror() is used to get the
 * description of the error, otherwise the passed error number is
 * returned. */
char *strerror_local(int errnum)
{
#if defined(HAVE_STRERROR)
	return strerror(errnum);
#else
	static char buf[MAX_BUF];

	(void) errnum;
	snprintf(buf, sizeof(buf), "Error %d", errnum);
	return buf;
#endif
}

/* Based on (n+1)^2 = n^2 + 2n + 1
 * given that	1^2 = 1, then
 *		2^2 = 1 + (2 + 1) = 1 + 3 = 4
 * 		3^2 = 4 + (4 + 1) = 4 + 5 = 1 + 3 + 5 = 9
 * 		4^2 = 9 + (6 + 1) = 9 + 7 = 1 + 3 + 5 + 7 = 16
 *		...
 * In other words, a square number can be express as the sum of the
 * series n^2 = 1 + 3 + ... + (2n-1) */
int isqrt(int n)
{
	int result, sum, prev;
	result = 0;
	prev = sum = 1;
	while (sum <= n)
	{
		prev += 2;
		sum += prev;
		++result;
	}
	return result;
}

/* returns a char-pointer to a static array, in which a representation
 * of the decimal number given will be stored. */
char *ltostr10(signed long n)
{
	static char buf[10];
	char *cp = buf + 9;
	long flag;

	*cp = '\0';

	if (n < 0)
		flag= n = -n;
	else
		flag = 0;

	do
	{
		*(--cp) = '0' + n % 10;
		n /= 10;
	}
	while (n);

	if (flag)
		*(--cp) = '-';

	return cp;
}

/* A fast routine which appends the name and decimal number specified
 * to the given buffer.
 * Could be faster, though, if the strcat()s at the end could be changed
 * into alternate strcat which returned a pointer to the _end_, not the
 * start! */
void save_long(char *buf, char *name, long n)
{
	char buf2[MAX_BUF];
	strcpy(buf2, name);
	strcat(buf2, " ");
	strcat(buf2, ltostr10(n));
	strcat(buf2, "\n");
	strcat(buf, buf2);
}

/* This is a list of the suffix, uncompress and compress functions.  Thus,
 * if you have some other compress program you want to use, the only thing
 * that needs to be done is to extended this.
 * The first entry must be NULL - this is what is used for non
 * compressed files. */
char *uncomp[NROF_COMPRESS_METHODS][3] =
{
	{NULL, 		NULL, 		NULL},
	{".Z", 		UNCOMPRESS, COMPRESS},
	{".gz", 	GUNZIP, 	GZIP},
	{".bz2",	BUNZIP, 	BZIP}
};

/* open_and_uncompress() first searches for the original filename.
 * if it exist, then it opens it and returns the file-pointer.
 * if not, it does two things depending on the flag.  If the flag
 * is set, it tries to create the original file by uncompressing a .Z file.
 * If the flag is not set, it creates a pipe that is used for
 * reading the file (NOTE - you can not use fseek on pipes)
 *
 * The compressed pointer is set to nonzero if the file is
 * compressed (and thus,  fp is actually a pipe.)  It returns 0
 * if it is a normal file
 *
 * (Note, the COMPRESS_SUFFIX is used instead of ".Z", thus it can easily
 * be changed in the config file.) */
FILE *open_and_uncompress(char *name, int flag, int *compressed)
{
	FILE *fp;
	char buf[MAX_BUF], buf2[MAX_BUF], *bufend;
	int try_once = 0;

	strcpy(buf, name);
	bufend = buf + strlen(buf);

	/*LOG(llevDebug, "open_and_uncompress(%s)\n", name);*/

	/* strip off any compression prefixes that may exist */
	for (*compressed = 0; *compressed < NROF_COMPRESS_METHODS; (*compressed)++)
	{
		if ((uncomp[*compressed][0]) && (!strcmp(uncomp[*compressed][0], bufend - strlen(uncomp[*compressed][0]))))
		{
			buf[strlen(buf) - strlen(uncomp[*compressed][0])] = '\0';
			bufend = buf + strlen(buf);
		}
	}

	for (*compressed = 0; *compressed < NROF_COMPRESS_METHODS; (*compressed)++)
	{
		struct stat statbuf;

		if (uncomp[*compressed][0])
			strcpy(bufend, uncomp[*compressed][0]);
		if (stat(buf, &statbuf))
		{

			/*LOG(llevDebug, "Failed to stat %s\n", buf);*/
			continue;
		}
		/*LOG(llevDebug, "Found file %s\n", buf);*/

		if (uncomp[*compressed][0])
		{
			strcpy(buf2, uncomp[*compressed][1]);
			strcat(buf2, " < ");
			strcat(buf2, buf);

			if (flag)
			{
				int i;
				if (try_once)
				{
					LOG(llevBug, "BUG: Failed to open %s after decompression.\n", name);
					return NULL;
				}

				try_once = 1;
				strcat(buf2, " > ");
				strcat(buf2, name);
				LOG(llevDebug, "system(%s)\n", buf2);
				if ((i = system(buf2)))
				{
					LOG(llevBug, "BUG: system(%s) returned %d\n", buf2, i);
					return NULL;
				}
				/* Delete the original */
				unlink(buf);
				/* Restart the loop from the beginning */
				*compressed = '\0';
				chmod(name, statbuf.st_mode);
				continue;
			}

			if ((fp = popen(buf2, "r")) != NULL)
				return fp;
		}
		else if ((fp = fopen(name, "r")) != NULL)
		{
			struct stat statbuf;

			if (fstat(fileno(fp), &statbuf) || !S_ISREG(statbuf.st_mode))
			{
				LOG(llevDebug, "Can't open %s - not a regular file\n", name);
				(void) fclose (fp);
				errno = EISDIR;
				return NULL;
			}
			return fp;
		}
	}

	LOG(llevDebug, "Can't open %s\n", name);
	return NULL;
}

/* See open_and_uncompress(). */
void close_and_delete(FILE *fp, int compressed)
{
	if (compressed)
		pclose(fp);
	else
		fclose(fp);
}

/**
 * Checks if any directories in the given path doesn't exist, and creates
 * if necessary.
 * @param filename File path we'll want to access. Can be NULL. */
void make_path_to_file(char *filename)
{
	char buf[MAX_BUF], *cp = buf;
	struct stat statbuf;

	if (!filename || !*filename)
	{
		return;
	}

	strcpy(buf, filename);

	while ((cp = strchr(cp + 1, '/')))
	{
		*cp = '\0';

		if (stat(buf, &statbuf) || !S_ISDIR(statbuf.st_mode))
		{
			if (mkdir(buf, 0777))
			{
				LOG(llevBug, "Bug: Can't make path to file %s.\n", filename);
				return;
			}
		}

		*cp = '/';
	}
}
