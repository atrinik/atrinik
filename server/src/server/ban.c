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
* the Free Software Foundation; either version 3 of the License, or     *
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

/* Ban.c
 * Code was grabbed from the netrek source and modified to work with
 * crossfire. This function checks database table for any banned players.
 * If it finds one it returns a 1. Wildcards can be used. */

#include <global.h>
#include <sproto.h>
#ifndef WIN32 /* ---win32 : remove unix headers */
#include <sys/ioctl.h>
#endif /* win32 */
#ifdef hpux
#include <sys/ptyio.h>
#endif

#ifndef WIN32 /* ---win32 : remove unix headers */
#ifdef NO_ERRNO_H
    extern int errno;
#else
#   include <errno.h>
#endif
#include <stdio.h>
#include <sys/file.h>
#endif /* win32 */

int checkbanned(char *login, char *host)
{
  	sqlite3 *db;
	sqlite3_stmt *statement;
  	char log_buf[64], host_buf[64];
	/* Hits == 2 means we're banned */
  	int Hits = 0;

	/* Open the database */
  	db_open(&db);

	/* Prepare the query */
	if (!db_prepare(db, "SELECT name, host FROM bans;", &statement))
	{
		LOG(llevBug, "BUG: checkbanned(): Could not prepare SQL query! (%s)\n", db_errmsg(db));
		return 0;
	}

	/* Loop through the results */
	while (db_step(statement) == SQLITE_ROW)
	{
		sprintf(log_buf, "%s", db_column_text(statement, 0));
		sprintf(host_buf, "%s", db_column_text(statement, 1));

#if 0
      	LOG(llevDebug, "Login: <%s>; host: <%s>\n", login, host);
      	LOG(llevDebug, "Checking Banned <%s> and <%s>.\n", log_buf, host_buf);
#endif

		if (*log_buf == '*')
			Hits = 1;
		else if (!strcmp(login, log_buf))
			Hits = 1;

		if (Hits == 1)
		{
			/* Lock out any host */
        	if (*host_buf == '*')
			{
          		Hits++;
				/* break out now. otherwise Hits will get reset to one */
          		break;
        	}
        	else if (strstr(host, host_buf) != NULL)
			{
				/* Lock out subdomains (eg, "*@usc.edu") */
          		Hits++;
				 /* break out now. otherwise Hits will get reset to one */
          		break;
        	}
        	else if (!strcmp(host, host_buf))
			{
				/* Lock out specific host */
          		Hits++;
				/* break out now. otherwise Hits will get reset to one */
          		break;
        	}
      	}
	}

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	if (Hits >= 2)
    	return 1;
  	else
    	return 0;
}
