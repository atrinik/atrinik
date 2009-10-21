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

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/**
 * @file
 * Includes high score related functions */

/** The score structure is used when treating new high-scores */
typedef struct scr
{
	/** Name. */
	char name[BIG_NAME];

	/** Title. */
	char title[BIG_NAME];

	/** Name (+ title) or "left". */
	char killer[BIG_NAME];

	/** Killed on what level. */
	char maplevel[BIG_NAME];

	/** Experience. */
	long exp;

	/** Max hp, sp, grace when killed. */
	int maxhp, maxsp, maxgrace;

	/** Position in the highscore list. */
	int position;
} score;

/**
 * Does what it says, copies the contents of the first score structure
 * to the second one.
 * @param sc1 First score structure
 * @param sc2 Second score structure */
static void copy_score(const score *sc1, score *sc2)
{
	strncpy(sc2->name, sc1->name, BIG_NAME);
	sc2->name[BIG_NAME - 1] = '\0';

	strncpy(sc2->title, sc1->title, BIG_NAME);
	sc2->title[BIG_NAME - 1] = '\0';

	strncpy(sc2->killer, sc1->killer, BIG_NAME);
	sc2->killer[BIG_NAME - 1] = '\0';

	sc2->exp = sc1->exp;
	strcpy(sc2->maplevel, sc1->maplevel);
	sc2->maxhp = sc1->maxhp;
	sc2->maxsp = sc1->maxsp;
	sc2->maxgrace = sc1->maxgrace;
}

/**
 * The opposite of put_score, this reads from the given buffer into
 * a static score structure, and returns a pointer to it.
 * @param statement SQLite statement handle
 * @return The score structure */
static score *get_score(sqlite3_stmt *statement)
{
	static score sc;

	sscanf((char *)db_column_text(statement, 0), "%ld", &sc.exp);

	strncpy(sc.killer, (char *)db_column_text(statement, 1), BIG_NAME);
	sc.killer[BIG_NAME - 1] = '\0';

	strncpy(sc.maplevel, (char *)db_column_text(statement, 2), BIG_NAME);
	sc.maplevel[BIG_NAME - 1] = '\0';

	sscanf((char *)db_column_text(statement, 3), "%d", &sc.maxgrace);

	sscanf((char *)db_column_text(statement, 4), "%d", &sc.maxhp);

	sscanf((char *)db_column_text(statement, 5), "%d", &sc.maxsp);

	strncpy(sc.name, (char *)db_column_text(statement, 6), BIG_NAME);
	sc.name[BIG_NAME - 1] = '\0';

	strncpy(sc.title, (char *)db_column_text(statement, 7), BIG_NAME);
	sc.title[BIG_NAME - 1] = '\0';

	return &sc;
}

/**
 * Draw one high score.
 * @param sc Score structure
 * @param buf Buffer
 * @param size Buffer size
 * @return Return the buffer */
static char *draw_one_high_score(const score *sc, char *buf, int size)
{
	if (!strncmp(sc->killer, "left", MAX_NAME))
		snprintf(buf, size, "%3d %10ld %s the %s left the game on map %s <%d><%d><%d>.", sc->position, sc->exp, sc->name, sc->title, sc->maplevel, sc->maxhp, sc->maxsp, sc->maxgrace);
	else
		snprintf(buf, size, "%3d %10ld %s the %s was killed by %s on map %s <%d><%d><%d>.", sc->position, sc->exp, sc->name, sc->title, sc->killer, sc->maplevel, sc->maxhp, sc->maxsp, sc->maxgrace);

	return buf;
}

/**
 * Adds the given score structure to the high score list, but
 * only if it was good enough to deserve a place.
 * @param new_score The new score structure
 * @return NULL if an error, or the old score. */
static score *add_score(score *new_score)
{
	static score old_score;
	score *tmp_score, pscore[HIGHSCORE_LENGTH];
	int nrofscores = 0, flag = 0, i;
	sqlite3 *db;
	sqlite3_stmt *statement;

	new_score->position = HIGHSCORE_LENGTH + 1;
	old_score.position = -1;

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL */
	if (!db_prepare(db, "SELECT * FROM highscore;", &statement))
	{
		LOG(llevBug, "BUG: add_score(): Failed to prepare SQL query to select all highscores! (%s)\n", db_errmsg(db));
		db_close(db);
		return NULL;
	}

	/* Loop through all rows (or until we hit scores limit) */
	while (db_step(statement) == SQLITE_ROW && nrofscores < HIGHSCORE_LENGTH)
	{
		if ((tmp_score = get_score(statement)) == NULL)
		{
			LOG(llevBug, "Failed to get score!\n");
			break;
		}

		if (!flag && new_score->exp >= tmp_score->exp)
		{
			copy_score(new_score, &pscore[nrofscores]);
			new_score->position = nrofscores;
			flag = 1;
			if (++nrofscores >= HIGHSCORE_LENGTH)
				break;
		}

		/* Another entry */
		if (!strcmp(new_score->name, tmp_score->name))
		{
			copy_score(tmp_score, &old_score);
			old_score.position = nrofscores;
			if (flag)
				continue;
		}
		copy_score(tmp_score, &pscore[nrofscores++]);
	}

	/* Finalize it */
	db_finalize(statement);

	/* Did not beat old score */
	if (old_score.position != -1 && old_score.exp >= new_score->exp)
	{
		db_close(db);
		return &old_score;
	}

	if (!flag && nrofscores < HIGHSCORE_LENGTH)
		copy_score(new_score, &pscore[nrofscores++]);

	/* Prepare the SQL to delete all entries in the highscore database */
	if (!db_prepare(db, "DELETE FROM highscore;", &statement))
	{
		LOG(llevBug, "BUG: add_score(): Failed to prepare SQL query to delete all highscore entries! (%s)\n", db_errmsg(db));
		db_close(db);
		return NULL;
	}

	/* Run the query */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);

	/* Loop through our scores */
	for (i = 0; i < nrofscores; i++)
	{
		/* Prepare the SQL query to insert the score*/
		if (!db_prepare_format(db, &statement, "INSERT INTO highscore (exp, killer, maplevel, maxgrace, maxhp, maxsp, name, title) VALUES (%ld, '%s', '%s', %d, %d, %d, '%s', '%s');", pscore[i].exp, pscore[i].killer, db_sanitize_input(pscore[i].maplevel), pscore[i].maxgrace, pscore[i].maxhp, pscore[i].maxsp, pscore[i].name, db_sanitize_input(pscore[i].title)))
		{
			LOG(llevBug, "BUG: add_score(): Failed to prepare SQL query to insert highscore data! (%s)\n", db_errmsg(db));
			db_close(db);
			return NULL;
		}

		/* Run the query */
		db_step(statement);

		/* Finalize it */
		db_finalize(statement);
	}

	/* Close the database */
	db_close(db);

	if (flag)
	{
		/* Eneq(@csd.uu.se): Patch to fix error in adding a new score to the hiscore-list */
		if (old_score.position == -1)
			return new_score;

		return &old_score;
	}
	new_score->position = -1;

	if (old_score.position != -1)
		return &old_score;
	if (nrofscores)
	{
		copy_score(&pscore[nrofscores - 1], &old_score);
		return &old_score;
	}

	LOG(llevError, "Highscore error.\n");
	return NULL;
}

void check_score(object *op, int quiet)
{
	score new_score;
	score *old_score;
	char bufscore[MAX_BUF];

	if (op->stats.exp == 0)
		return;

	if (QUERY_FLAG(op, FLAG_WAS_WIZ))
	{
		if (!quiet)
			new_draw_info(NDI_UNIQUE, 0, op, "Since you have been in wizard mode, you can't enter the high-score list.");

		return;
	}

	if (!op->stats.exp)
	{
		if (!quiet)
			new_draw_info(NDI_UNIQUE, 0, op, "You don't deserve to save your character yet.");

		return;
	}

	strncpy(new_score.name, op->name, BIG_NAME);
	new_score.name[BIG_NAME - 1] = '\0';

	strncpy(new_score.title, op->race, BIG_NAME);
	new_score.title[BIG_NAME - 1] = '\0';

	strncpy(new_score.killer, CONTR(op)->killer, BIG_NAME);

	if (new_score.killer[0] == '\0')
		strcpy(new_score.killer, "a dungeon collapse");

	new_score.killer[BIG_NAME - 1] = '\0';
	new_score.exp = op->stats.exp;

	if (op->map == NULL)
		*new_score.maplevel = '\0';
	else
	{
		strncpy(new_score.maplevel, op->map->name ? op->map->name : op->map->path, BIG_NAME - 1);
		new_score.maplevel[BIG_NAME - 1] = '\0';
	}

	new_score.maxhp = (int)op->stats.maxhp;
	new_score.maxsp = (int)op->stats.maxsp;
	new_score.maxgrace = (int)op->stats.maxgrace;
	if ((old_score = add_score(&new_score)) == NULL)
	{
		if (!quiet)
			new_draw_info(NDI_UNIQUE, 0, op, "Error in the highscore list.");

		return;
	}

	/* Everything below here is just related to print messages
	 * to the player.  If quiet is set, we can just return
	 * now. */
	if (quiet)
		return;

	if (new_score.position == -1)
	{
		/* Not strictly correct... */
		new_score.position = HIGHSCORE_LENGTH + 1;

		if (!strcmp(old_score->name, new_score.name))
			new_draw_info(NDI_UNIQUE, 0, op, "You didn't beat your last highscore:");
		else
			new_draw_info(NDI_UNIQUE, 0, op, "You didn't enter the highscore list:");

		new_draw_info(NDI_UNIQUE, 0, op, draw_one_high_score(old_score, bufscore, sizeof(bufscore)));

		new_draw_info(NDI_UNIQUE, 0, op, draw_one_high_score(&new_score, bufscore, sizeof(bufscore)));

		return;
	}

	if (old_score->exp >= new_score.exp)
		new_draw_info(NDI_UNIQUE, 0, op, "You didn't beat your last score:");
	else
		new_draw_info(NDI_UNIQUE, 0, op, "You beat your last score:");

	new_draw_info(NDI_UNIQUE, 0, op, draw_one_high_score(old_score, bufscore, sizeof(bufscore)));

	new_draw_info(NDI_UNIQUE, 0, op, draw_one_high_score(&new_score, bufscore, sizeof(bufscore)));
}

/**
 * Displays the high score file.
 * @param op Object calling this
 * @param max Maximum scores to show
 * @param match Only match scores with match. */
void display_high_score(object *op, int max, const char *match)
{
	char scorebuf[MAX_BUF];
	int i = 0, j = 0;
	score *sc;
	sqlite3 *db;
	sqlite3_stmt *statement;

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL to grab scores and order them by exp */
	if (!db_prepare(db, "SELECT * FROM highscore ORDER BY exp DESC;", &statement))
	{
		LOG(llevBug, "BUG: display_high_score(): Failed to prepare SQL query to select all highscores! (%s)", db_errmsg(db));

		if (op != NULL)
			new_draw_info(NDI_UNIQUE, 0, op, "There is no highscore file.");

		return;
	}

	new_draw_info(NDI_UNIQUE, 0, op, "Nr    Score    Who <max hp><max sp><max grace>");

	/* Loop through the scores */
	while (db_step(statement) == SQLITE_ROW)
	{
		if (j >= HIGHSCORE_LENGTH || i >= (max - 1))
			break;

		if ((sc = get_score(statement)) == NULL)
			break;

		sc->position = ++j;
		if (match == NULL)
		{
			draw_one_high_score(sc, scorebuf, sizeof(scorebuf));
			i++;
		}
		else
			continue;

		if (op == NULL)
			LOG(llevDebug, "%s\n", scorebuf);
		else
			new_draw_info(NDI_UNIQUE, 0, op, scorebuf);
	}

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);
}
