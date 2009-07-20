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

/*
 * BecomeDaemon.c
 * shamelessly swiped from xdm source code.
 * Appropriate copyrights kept, and hereby follow
 * ERic mehlhaff, 3/11/92
 *
 * xdm - display manager daemon
 *
 * $XConsortium: daemon.c,v 1.5 89/01/20 10:43:49 jim Exp $
 *
 * Copyright 1988 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * Author:  Keith Packard, MIT X Consortium
 */


#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <sys/ioctl.h>
#ifdef hpux
#include <sys/ptyio.h>
#endif

#ifdef NO_ERRNO_H
    extern int errno;
#else
#   include <errno.h>
#endif
#include <stdio.h>
#include <sys/file.h>

FILE *BecomeDaemon(char *filename)
{
	FILE *logfile;
	register int i;
	int forkresult;

	if ((logfile = fopen(filename, "a")) == NULL)
	{
		printf("Couldn't create logfile %s.\n", filename);
		exit(0);
	}
	fputs("\n========================\n", logfile);
	fputs("Begin New Server Session\n", logfile);
	fputs("========================\n\n", logfile);
	fflush(logfile);
    /* fork so that the process goes into the background automatically. Also
     * has a nice side effect of having the child process get inherited by
     * init (pid 1). */

	/* if parent */
    if ((forkresult = fork()))
	{
	  	if (forkresult < 0 )
			perror("Fork error!");

		/* then no more work to do */
	  	exit (0);
 	}

    /* Close standard file descriptors and get rid of controlling tty */

    close(0);
    close(1);
    close(2);

    /* Set up the standard file descriptors. */
	/* root inode already in core */
    (void) open("/", O_RDONLY);
    (void) dup2(0, 1);
    (void) dup2(0, 2);

	/* did open succeed? */
    if ((i = open("/dev/tty", O_RDWR)) >= 0)
	{
#if (defined(SYSV) || defined(hpux)) && defined(TIOCTTY)
		int zero = 0;
		(void) ioctl (i, TIOCTTY, &zero);
#else

#  ifdef HAVE_SYS_TERMIOS_H
#    include <sys/termios.h>
#  else
#    ifdef HAVE_SYS_TTYCOM_H
#      include <sys/ttycom.h>
#    endif
#  endif
		/* detach, BSD style */
		(void) ioctl(i, TIOCNOTTY, (char *) 0);
#endif
		(void) close(i);
    }


#ifdef HAVE_SETSID
    setsid();
#else
/* Are these really different?  */
#  if defined(SYSV) || defined(SVR4)
	setpgrp(0, 0);
#  else /* Non SYSV machines */
	setpgrp(0, getpid());
#  endif
#endif

  	return logfile;
}
