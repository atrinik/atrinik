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

/* This file contains database related stuff. */

/* This overrides default SQLite behaviour of writing data. Basically,
 * by default SQLite checks that each transaction write was successful.
 * As you can imagine, this can be slow. On reliable hardware, backup
 * power, and backups, checks like this are not really needed. */
#define DB_NO_WRITE_CHECK 1

#include <global.h>

/* Opens SQLite database. */
int db_open(char *file, sqlite3 **db)
{
	int success = sqlite3_open(file, db);

#ifdef DB_NO_WRITE_CHECK
	sqlite3_stmt *statement;

	/* Prepare the SQL */
	db_prepare(*db, "PRAGMA synchronous = 0;", &statement);

	/* Execute the SQL */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);
#endif

	return success;
}

/* Prepare SQL query. Return values: 1 (everything ok), 0 (something went wrong) */
int db_prepare(sqlite3 *db, const char *sql, sqlite3_stmt **statement)
{
	if (sqlite3_prepare(db, sql, strlen(sql), statement, 0) == SQLITE_OK)
		return 1;
	else
		return 0;
}

/* Prepare SQL query with format arguments, like sprintf, and call db_prepare(). */
int db_prepare_format(sqlite3 *db, sqlite3_stmt **statement, const char *format, ...)
{
	/* Guess we need no more than 100 bytes. */
	int n, size = 100, result;
	char *p, *np;
	va_list ap;

	if ((p = malloc(size)) == NULL)
	{
		LOG(llevError, "ERROR: Out of memory.\n");
		return 0;
	}

	while (1)
	{
		/* Try to print in the allocated space. */
		va_start(ap, format);
		n = vsnprintf(p, size, format, ap);
		va_end(ap);

		/* If that worked, prepare the query and return result. */
		if (n > -1 && n < size)
		{
			result = db_prepare(db, p, statement);
			free(p);
			return result;
		}

		/* Else try again with more space. */
		/* glibc 2.1 */
		if (n > -1)
		{
			/* precisely what is needed */
			size = n + 1;
		}
		/* glibc 2.0 */
		else
		{
			/* twice the old size */
			size *= 2;
		}

		if ((np = realloc(p, size)) == NULL)
		{
			free(p);
			return 0;
		}
		else
		{
			p = np;
		}
	}
}

/* Run the SQL query prepared by db_prepare() */
int db_step(sqlite3_stmt *statement)
{
	return sqlite3_step(statement);
}

/* Grab the text in database after running db_step() */
const unsigned char *db_column_text(sqlite3_stmt *statement, int col)
{
	return sqlite3_column_text(statement, col);
}

/* Grab the value in database after running db_step() */
sqlite3_value *db_column_value(sqlite3_stmt *statement, int col)
{
	return sqlite3_column_value(statement, col);
}

/* Finalize SQL query previously prepared by db_prepare() */
int db_finalize(sqlite3_stmt *statement)
{
	return sqlite3_finalize(statement);
}

/* Close database. */
int db_close(sqlite3 *db)
{
	return sqlite3_close(db);
}

/* Sanitize database input. Run for ANY value you don't control, like /bug command. */
char *db_sanitize_input(char *sql_input)
{
	char *p, *np;
	int size = strlen(sql_input), n;

	if ((p = (char *)malloc(size)) == NULL)
	{
		LOG(llevError, "ERROR: Out of memory.\n");
		return sql_input;
	}

	/* Replace any 's with ''s */
	while (1)
	{
		n = replace(sql_input, "'", "''", p, size);

		/* Just as needed */
		if (n == -1)
			break;
		else
			size += n + 1;

		/* We need more... */
		if ((np = (char *)realloc(p, size)) == NULL)
		{
			LOG(llevError, "ERROR: Out of memory.\n");
			break;
		}
		else
			p = np;
	}

	if (p)
	{
		/* Copy it to the original pointer */
		strcpy(sql_input, p);

		/* Free the temporary pointer */
		free(p);
	}

	return sql_input;
}

/* Return the last error message returned by doing anything with database. */
const char *db_errmsg(sqlite3* db)
{
	return sqlite3_errmsg(db);
}
