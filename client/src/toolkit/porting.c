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
 * Cross-platform support. */

#include <global.h>

TOOLKIT_API();

TOOLKIT_INIT_FUNC(porting)
{
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(porting)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

#ifndef __CPROTO__

#ifndef HAVE_STRTOK_R

/**
 * Re-entrant string tokenizer; glibc version, licensed under GNU LGPL
 * version 2.1. */
char *strtok_r(char *s, const char *delim, char **save_ptr)
{
    char *token;

    if (s == NULL) {
        s = *save_ptr;
    }

    /* Scan leading delimiters.  */
    s += strspn(s, delim);

    if (*s == '\0') {
        *save_ptr = s;
        return NULL;
    }

    /* Find the end of the token.  */
    token = s;
    s = strpbrk(token, delim);

    if (s == NULL) {
        /* This token finishes the string.  */
        *save_ptr = strchr(token, '\0');
    } else {
        /* Terminate the token and make *SAVE_PTR point past it.  */
        *s = '\0';
        *save_ptr = s + 1;
    }

    return token;
}
#endif

#ifndef HAVE_TEMPNAM
static uint32_t curtmp = 0;

char *tempnam(const char *dir, const char *pfx)
{
    char *name;
    pid_t pid = getpid();

    if (!pfx) {
        pfx = "tmp.";
    }

    /* This is a pretty simple method - put the pid as a hex digit and
     * just keep incrementing the last digit. Check to see if the file
     * already exists - if so, we'll just keep looking - eventually we
     * should find one that is free. */
    if (dir) {
        if (!(name = (char *) malloc(MAXPATHLEN))) {
            return NULL;
        }

        do {
            snprintf(name, MAXPATHLEN, "%s/%s%hx.%d", dir, pfx, pid, curtmp);
            curtmp++;
        }        while (access(name, F_OK) != -1);

        return name;
    }

    return NULL;
}
#endif

#ifndef HAVE_STRDUP

char *strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    void *new = malloc(len);

    if (!new) {
        return NULL;
    }

    return (char *) memcpy(new, s, len);
}
#endif

#ifndef HAVE_STRNDUP

char *strndup(const char *s, size_t n)
{
    size_t len;
    char *new;

    len = strlen(s);

    if (n < len) {
        len = n;
    }

    new = malloc(len + 1);

    if (!new) {
        return NULL;
    }

    new[len] = '\0';

    return (char *) memcpy(new, s, len);
}
#endif

#ifndef HAVE_STRERROR

char *strerror(int errnum)
{
    return "";
}
#endif

#ifndef HAVE_STRCASESTR

const char *strcasestr(const char *haystack, const char *needle)
{
    char c, sc;
    size_t len;

    if ((c = *needle++) != 0) {
        c = tolower(c);
        len = strlen(needle);

        do {
            do {
                if ((sc = *haystack++) == 0) {
                    return NULL;
                }
            }            while (tolower(sc) != c);
        }        while (strncasecmp(haystack, needle, len) != 0);

        haystack--;
    }

    return haystack;
}
#endif

#ifndef HAVE_GETTIMEOFDAY

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
#ifdef WIN32
    FILETIME time;
    unsigned __int64 res;

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH 11644473600000000Ui64
#else
#define DELTA_EPOCH 11644473600000000ULL
#endif

    GetSystemTimeAsFileTime(&time);

    res = (((unsigned __int64) time.dwHighDateTime << 32) | time.dwLowDateTime) / 10 - DELTA_EPOCH;
    tv->tv_sec = (long) (res / 1000000UL);
    tv->tv_usec = (long) (res % 1000000UL);

    /* Get the timezone, if they want it. */
    if (tz) {
        _tzset();

        tz->tz_minuteswest = _timezone;
        tz->tz_dsttime = _daylight;
    }

    return 0;
#else
    (void) tv;
    (void) tz;
    return 0;
#endif
}
#endif

#ifndef HAVE_GETLINE

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    char *buf;
    size_t bufsize, numread;
    int c;

    if (!lineptr || !n) {
        errno = EINVAL;
        return -1;
    }

    buf = *lineptr;
    bufsize = *n;
    numread = 0;

    c = fgetc(stream);

    if (c == EOF) {
        errno = EINVAL;
        return -1;
    }

    if (!buf) {
        bufsize = 1;
        buf = malloc(bufsize);

        if (!buf) {
            return -1;
        }
    }

    while (c != EOF) {
        if (numread > bufsize - 1) {
            bufsize += 1;
            buf = realloc(buf, bufsize);

            if (!buf) {
                return -1;
            }
        }

        buf[numread++] = c;

        if (c == '\n') {
            break;
        }

        c = fgetc(stream);
    }

    buf[numread] = '\0';
    *lineptr = buf;
    *n = bufsize;

    return numread;
}
#endif

#ifndef HAVE_USLEEP

int usleep(uint32_t usec)
{
    struct timeval tv1, tv2;

    if (gettimeofday(&tv1, NULL) != 0) {
        return -1;
    }

    do {
        if (gettimeofday(&tv2, NULL) != 0) {
            return -1;
        }
    }    while ((tv2.tv_usec - tv1.tv_usec) < usec);

    return 0;
}
#endif

#ifndef HAVE_STRNLEN

size_t strnlen(const char *s, size_t max)
{
    const char *p;

    for (p = s; *p && max--; p++) {
    }

    return p - s;
}
#endif

#ifndef HAVE_MKSTEMP

/* mkstemp extracted from libc/sysdeps/posix/tempname.c.  Copyright
   (C) 1991-1999, 2000, 2001, 2006 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.  */

static const char letters[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Generate a temporary file name based on TMPL.  TMPL must match the
   rules for mk[s]temp (i.e. end in "XXXXXX").  The name constructed
   does not exist at the time of the call to mkstemp.  TMPL is
   overwritten with the result.  */
int mkstemp(char *tmpl)
{
    int len;
    char *XXXXXX;
    static unsigned long long value;
    unsigned long long random_time_bits;
    unsigned int count;
    int fd = -1;
    int save_errno = errno;

    /* A lower bound on the number of temporary files to attempt to
       generate.  The maximum total number of temporary file names that
       can exist for a given template is 62**6.  It should never be
       necessary to try all these combinations.  Instead if a reasonable
       number of names is tried (we define reasonable as 62**3) fail to
       give the system administrator the chance to remove the problems.  */
#define ATTEMPTS_MIN (62 * 62 * 62)

    /* The number of times to attempt to generate a temporary file.  To
       conform to POSIX, this must be no smaller than TMP_MAX.  */
#if ATTEMPTS_MIN < TMP_MAX
    unsigned int attempts = TMP_MAX;
#else
    unsigned int attempts = ATTEMPTS_MIN;
#endif

    len = strlen(tmpl);

    if (len < 6 || strcmp(&tmpl[len - 6], "XXXXXX")) {
        errno = EINVAL;
        return -1;
    }

    /* This is where the Xs start.  */
    XXXXXX = &tmpl[len - 6];

    /* Get some more or less random data.  */
    {
        SYSTEMTIME stNow;
        FILETIME ftNow;

        // get system time
        GetSystemTime(&stNow);
        stNow.wMilliseconds = 500;

        if (!SystemTimeToFileTime(&stNow, &ftNow)) {
            errno = -1;
            return -1;
        }

        random_time_bits = (((unsigned long long) ftNow.dwHighDateTime << 32)
                | (unsigned long long) ftNow.dwLowDateTime);
    }

    value += random_time_bits ^ (unsigned long long) GetCurrentThreadId();

    for (count = 0; count < attempts; value += 7777, ++count) {
        unsigned long long v = value;

        /* Fill in the random bits.  */
        XXXXXX[0] = letters[v % 62];
        v /= 62;
        XXXXXX[1] = letters[v % 62];
        v /= 62;
        XXXXXX[2] = letters[v % 62];
        v /= 62;
        XXXXXX[3] = letters[v % 62];
        v /= 62;
        XXXXXX[4] = letters[v % 62];
        v /= 62;
        XXXXXX[5] = letters[v % 62];

        fd = open (tmpl, O_RDWR | O_CREAT | O_EXCL, _S_IREAD | _S_IWRITE);

        if (fd >= 0) {
            errno = save_errno;
            return fd;
        } else if (errno != EEXIST) {
            return -1;
        }
    }

    /* We got out of the loop because we ran out of combinations to try.  */
    errno = EEXIST;
    return -1;
}
#endif

#ifndef HAVE_SINCOS
void
sincos (double x, double *s, double *c)
{
    if (s != NULL) {
        *s = sin(x);
    }

    if (c != NULL) {
        *c = cos(x);
    }
}
#endif

#endif
