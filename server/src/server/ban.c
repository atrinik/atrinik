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
 * Banning related functions. \n
 * It is possible to use the /ban DM command or interactive
 * server mode ban command to ban specified player or IP
 * from the game. \n \n
 * Syntax for banning: \n
 * IP:player \n \n
 * Where IP is the IP address and player is the player name.
 * It is possible to use * for both IP and player, which
 * means any match. */

#include <global.h>
#include <sproto.h>
#ifndef WIN32
#include <sys/ioctl.h>
#endif
#ifdef hpux
#include <sys/ptyio.h>
#endif

#ifndef WIN32
#ifdef NO_ERRNO_H
    extern int errno;
#else
#   include <errno.h>
#endif
#include <stdio.h>
#include <sys/file.h>
#endif

/**
 * Check if this login or host is banned in the database.
 * @param login Login name to check
 * @param host Host name to check
 * @return 1 if banned, 0 if not */
int checkbanned(char *login, char *host)
{
  	sqlite3 *db;
	sqlite3_stmt *statement;
  	char log_buf[64], host_buf[64];
	/* Hits == 2 means we're banned */
  	int Hits = 0;

	/* Open the database */
  	db_open(DB_DEFAULT, &db);

	/* Prepare the query */
	if (!db_prepare(db, "SELECT name, host FROM bans;", &statement))
	{
		LOG(llevBug, "BUG: checkbanned(): Could not prepare SQL query! (%s)\n", db_errmsg(db));
		db_close(db);
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

/**
 * Add ban to the database. Will take care of getting
 * the right values from input string.
 * @param input The input string with both name and IP
 * @return 1 on success, 0 on failure */
int add_ban(const char *input)
{
	sqlite3 *db;
	sqlite3_stmt *statement;
	char *host, *name, buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "%s", input);

	host = strtok(buf, ":");
	name = strtok(NULL, ":");

	if (!host || !name)
		return 0;

	/* Open the database */
  	db_open(DB_DEFAULT, &db);

	/* Prepare the query */
	if (!db_prepare_format(db, &statement, "INSERT INTO bans (host, name) VALUES ('%s', '%s');", db_sanitize_input(host), db_sanitize_input(name)))
	{
		LOG(llevBug, "BUG: add_ban(): Could not prepare SQL query! (%s)\n", db_errmsg(db));
		db_close(db);
		return 0;
	}

	/* Execute the query */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	return 1;
}

/**
 * Remove a ban from the database. Will take care of getting
 * the right values from input string.
 * @param input The input string with both name and IP
 * @return 1 on success, 0 on failure */
int remove_ban(const char *input)
{
	sqlite3 *db;
	sqlite3_stmt *statement;
	char *host, *name, buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "%s", input);

	host = strtok(buf, ":");
	name = strtok(NULL, ":");

	if (!host || !name)
		return 0;

	/* Open the database */
  	db_open(DB_DEFAULT, &db);

	/* Prepare the query */
	if (!db_prepare_format(db, &statement, "DELETE FROM bans WHERE host = '%s' AND name = '%s';", db_sanitize_input(host), db_sanitize_input(name)))
	{
		LOG(llevBug, "BUG: remove_ban(): Could not prepare SQL query! (%s)\n", db_errmsg(db));
		db_close(db);
		return 0;
	}

	/* Execute the query */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	return 1;
}

/**
 * List all bans.
 * @param op Player object to print this information
 * to, NULL to output it to the log. */
void list_bans(object *op)
{
	sqlite3 *db;
	sqlite3_stmt *statement;

	/* Open the database */
  	db_open(DB_DEFAULT, &db);

	/* Prepare the query */
	if (!db_prepare(db, "SELECT name, host FROM bans;", &statement))
	{
		LOG(llevBug, "BUG: list_bans(): Could not prepare SQL query! (%s)\n", db_errmsg(db));
		db_close(db);
		return;
	}

	if (op)
		new_draw_info(NDI_UNIQUE, 0, op, "List of bans:");
	else
		LOG(llevInfo, "\nList of bans:\n");

	/* Loop through the results */
	while (db_step(statement) == SQLITE_ROW)
	{
		if (op)
			new_draw_info_format(NDI_UNIQUE, 0, op, "%s:%s", db_column_text(statement, 0), db_column_text(statement, 1));
		else
			LOG(llevInfo, "%s:%s\n", db_column_text(statement, 0), db_column_text(statement, 1));
	}

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);
}
