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
 * Player login/save/logout related functions. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <loader.h>

extern spell spells[NROFREALSPELLS];
extern long pticks;

/** Objects link of DMs. */
objectlink *dm_list = NULL;

/** Minimum length a player name must have. */
#define PLAYER_NAME_MIN 2

/** Maximum length a player name can have. */
#define PLAYER_NAME_MAX 12

/**
 * Save all players.
 * @param flag If non zero, it means that we want to try and save
 * everyone, but keep the game running. Thus, we don't want to free any
 * information. */
void emergency_save(int flag)
{
#ifndef NO_EMERGENCY_SAVE
	trying_emergency_save = 1;

	LOG(llevSystem, "Emergency save:  ");

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!pl->ob)
		{
			LOG(llevSystem, "No name, ignoring this.\n");
			continue;
		}

		LOG(llevSystem, "%s ", pl->ob->name);
		new_draw_info(NDI_UNIQUE, 0, pl->ob, "Emergency save...");

		/* If we are not exiting the game (ie, this is sort of a backup
		 * save), then don't change the location back to the village.
		 * Note that there are other options to have backup saves be done
		 * at the starting village */
		if (!flag)
		{
			strcpy(pl->maplevel, first_map_path);

			if (pl->ob->map != NULL)
			{
				pl->ob->map = NULL;
			}

			pl->ob->x = -1;
			pl->ob->y = -1;
		}

		container_unlink(pl, NULL);

		if (!save_player(pl->ob, flag))
		{
			LOG(llevSystem, "(failed) ");
			new_draw_info(NDI_UNIQUE, 0, pl->ob, "Emergency save failed, checking score...");
		}

		check_score(pl->ob, 1);
	}

	LOG(llevSystem, "\n");
#else
	(void) flag;
	LOG(llevSystem, "Emergency saves disabled, no save attempted\n");
#endif
}

/**
 * Checks to see if the passed player name is valid or not. Does checks
 * like min/max name length, whether there is anyone else playing by that
 * name, and whether there are illegal characters in the player name.
 * @param me Player
 * @param name Name to check
 * @return 1 if the name is ok, 0 otherwise. */
int check_name(player *me, char *name)
{
	unsigned int name_len = strlen(name);
	char buf[MAX_BUF];

	if (name_len < PLAYER_NAME_MIN || name_len > PLAYER_NAME_MAX)
	{
		strcpy(buf, "X3 That name has an invalid length.");
		Write_String_To_Socket(&me->socket, BINARY_CMD_DRAWINFO, buf, strlen(buf));
		me->socket.can_write = 1;
		write_socket_buffer(&me->socket);
		return 0;
	}

	if (!playername_ok(name))
	{
		strcpy(buf, "X3 That name contains illegal characters.");
		Write_String_To_Socket(&me->socket, BINARY_CMD_DRAWINFO, buf, strlen(buf));
		me->socket.can_write = 1;
		write_socket_buffer(&me->socket);
		return 0;
	}

	return 1;
}

long calculate_checksum_new(char *buf, int checkdouble)
{
#ifdef USE_CHECKSUM
	long checksum = 0;
	int offset = 0;
	char *cp;

	(void) checkdouble;

#if 0
	if (checkdouble && !strncmp(buf, "checksum", 8))
		continue;
#endif

	for (cp = buf; *cp; cp++)
	{
		if (++offset > 28)
		{
			offset = 0;
		}

		checksum ^= (*cp << offset);
	}

	return checksum;
#else
	(void) buf;
	(void) checkdouble;
	return 0;
#endif
}

/**
 * Saves a player to database.
 * @param op Player to save.
 * @param flag If set, it's only backup, i.e. don't remove objects from
 * inventory. If BACKUP_SAVE_AT_HOME is set, and the flag is set, then
 * the player will be saved at the emergency save location.
 * @return Non zero if successful.
 * @todo Optimize and simplify the database saving. */
int save_player(object *op, int flag)
{
	char *sqlbuf, *p, *invbuf;
	player *pl = CONTR(op);
	int i, wiz = QUERY_FLAG(op, FLAG_WIZ), do_update = 0, sqlresult, size = HUGE_BUF * 4, n, old_size;
	long checksum;
	sqlite3 *db;
	sqlite3_stmt *statement;
#ifdef BACKUP_SAVE_AT_HOME
	sint16 backup_x, backup_y;
#endif

	/* no experience, no save */
	if (!op->stats.exp && (!CONTR(op) || !CONTR(op)->player_loaded))
		return 0;

	flag &= 1;

	/* Sanity check - some stuff changes this when player is exiting */
	if (op->type != PLAYER)
		return 0;

	/* Prevent accidental saves if connection is reset after player has
	 * mostly exited. */
	if (pl->state != ST_PLAYING)
		return 0;

	/* perhaps we don't need it here ?*/
	/*container_unlink(pl, NULL);*/

	sqlbuf = (char *) malloc(size);

	sprintf(sqlbuf, "password %s\n", pl->password);

	sprintf(sqlbuf, "%sdm_stealth %d\n", sqlbuf, pl->dm_stealth);
	sprintf(sqlbuf, "%sgen_hp %d\n", sqlbuf, pl->gen_hp);
	sprintf(sqlbuf, "%sgen_sp %d\n", sqlbuf, pl->gen_sp);
	sprintf(sqlbuf, "%sgen_grace %d\n", sqlbuf, pl->gen_grace);
	sprintf(sqlbuf, "%sspell %d\n",sqlbuf, pl->chosen_spell);
	sprintf(sqlbuf, "%sshoottype %d\n", sqlbuf, pl->shoottype);
	sprintf(sqlbuf, "%sdigestion %d\n", sqlbuf, pl->digestion);
#if 0
	sprintf(sqlbuf, "%soutputs_sync %d\n", sqlbuf, pl->outputs_sync);
	sprintf(sqlbuf, "%soutputs_count %d\n", sqlbuf, pl->outputs_count);
#endif

#ifdef BACKUP_SAVE_AT_HOME
	if (op->map != NULL && flag == 0)
#else
	if (op->map != NULL)
#endif
		sprintf(sqlbuf, "%smap %s\n", sqlbuf, op->map->path);
	else
		sprintf(sqlbuf, "%smap %s\n", sqlbuf, EMERGENCY_MAPPATH);

	sprintf(sqlbuf, "%ssavebed_map %s\n", sqlbuf, pl->savebed_map);
	sprintf(sqlbuf, "%sbed_x %d\nbed_y %d\n", sqlbuf, pl->bed_x, pl->bed_y);
	sprintf(sqlbuf, "%sStr %d\n", sqlbuf, pl->orig_stats.Str);
	sprintf(sqlbuf, "%sDex %d\n", sqlbuf, pl->orig_stats.Dex);
	sprintf(sqlbuf, "%sCon %d\n", sqlbuf, pl->orig_stats.Con);
	sprintf(sqlbuf, "%sInt %d\n", sqlbuf, pl->orig_stats.Int);
	sprintf(sqlbuf, "%sPow %d\n", sqlbuf, pl->orig_stats.Pow);
	sprintf(sqlbuf, "%sWis %d\n", sqlbuf, pl->orig_stats.Wis);
	sprintf(sqlbuf, "%sCha %d\n", sqlbuf, pl->orig_stats.Cha);

	/* save hp table */
	sprintf(sqlbuf, "%slev_hp %d\n", sqlbuf, op->level);
	for (i = 1; i <= op->level; i++)
		sprintf(sqlbuf, "%s%d\n", sqlbuf, pl->levhp[i]);

	/* save sp table */
	sprintf(sqlbuf, "%slev_sp %d\n", sqlbuf, pl->sp_exp_ptr->level);
	for (i = 1; i <= pl->sp_exp_ptr->level; i++)
		sprintf(sqlbuf, "%s%d\n", sqlbuf, pl->levsp[i]);

	sprintf(sqlbuf, "%slev_grace %d\n", sqlbuf, pl->grace_exp_ptr->level);
	for (i = 1; i <= pl->grace_exp_ptr->level; i++)
		sprintf(sqlbuf, "%s%d\n", sqlbuf, pl->levgrace[i]);

	for (i = 0; i < pl->nrofknownspells; i++)
		sprintf(sqlbuf, "%sknown_spell %s\n", sqlbuf, spells[pl->known_spells[i]].name);

	sprintf(sqlbuf, "%sendplst\n", sqlbuf);

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);
	CLEAR_FLAG(op, FLAG_WIZ);

	invbuf = (char *)malloc(size);
	old_size = size;

	while (1)
	{
		invbuf[0] = '\0';

#ifdef BACKUP_SAVE_AT_HOME
		/* Save objects, but not unpaid objects.  Don't remove objects from
		  * inventory. */
		n = save_player_object(invbuf, op, 2, size - strlen(invbuf) - 1);
#else
		/* Don't check and don't remove */
		n = save_player_object(invbuf, op, 3, size - strlen(invbuf) - 1);
#endif

		if (n == 0)
			break;
		else
			size += n + 1;

		/* We need more... */
		if ((p = realloc(invbuf, size)) == NULL)
		{
			LOG(llevError, "ERROR: Out of memory.\n");
			break;
		}
		else
			invbuf = p;
	}

	if (old_size != size)
	{
		if ((p = (char *)realloc(sqlbuf, size + 1)) == NULL)
		{
			LOG(llevError, "ERROR: Out of memory.\n");
			return 0;
		}
		else
			sqlbuf = p;
	}

	strcat(sqlbuf, invbuf);

	free(invbuf);

#ifdef BACKUP_SAVE_AT_HOME
	if (flag)
	{
		op->x = backup_x;
		op->y = backup_y;
	}
#endif

	checksum = calculate_checksum_new(sqlbuf, 0);

	/* Open database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL query.
	 * this way we check which to do:
	 *  - insert (new player)
	 *  - update (player already in database) */
	if (!db_prepare_format(db, &statement, "SELECT playerName FROM players WHERE playerName = '%s';", op->name))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Can't run database query.");
		LOG(llevBug, "BUG: save_player(): SQL prepare query failed for %s! (%s)", op->name, db_errmsg(db));
		db_close(db);
		CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
		return 0;
	}

	/* Actually run the query. */
	db_step(statement);

	/* If the selected player name matches, we are going to do an update. */
	if (db_column_text(statement, 0) && strcmp((char*)db_column_text(statement, 0), op->name) == 0)
		do_update = 1;

	/* Finalize the SQL */
	db_finalize(statement);

	/* Prepare the SQL and determine right query to use. */
	if (do_update)
		sqlresult = db_prepare_format(db, &statement, "UPDATE players SET data = 'checksum %lx\n%s' WHERE playerName = '%s';", checksum, db_sanitize_input(sqlbuf), op->name);
	else
		sqlresult = db_prepare_format(db, &statement, "INSERT INTO players (playing, playerName, data) VALUES (1, '%s', 'checksum %lx\n%s');", op->name, checksum, db_sanitize_input(sqlbuf));

	if (!sqlresult)
	{
		LOG(llevBug, "BUG: save_player(): SQL prepare query failed for %s! (%s)\n", op->name, db_errmsg(db));
		new_draw_info(NDI_UNIQUE, 0, op, "Can't run database query.");
		db_close(db);
		CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
		return 0;
	}

	/* Run the query */
	db_step(statement);

	/* Finalize */
	db_finalize(statement);

	/* And close the database. */
	db_close(db);

	/* Free the buf */
	free(sqlbuf);

#if 0
	/* Eneq(@csd.uu.se): Reveal the container if we have one. */
	if (flag && container != NULL)
		pl->container = container;
#endif

	if (wiz)
		SET_FLAG(op, FLAG_WIZ);

	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
	return 1;
}

/* calculate_checksum:
 * Evil scheme to avoid tampering with the player-files 8)
 * The cheat-flag will be set if the file has been changed. */
long calculate_checksum(char *filename, int checkdouble)
{
#ifdef USE_CHECKSUM
	long checksum = 0;
	int offset = 0;
	FILE *fp;
	char buf[MAX_BUF], *cp;

	if ((fp = fopen(filename, "r")) == NULL)
	{
		return 0;
	}

	while (fgets(buf, MAX_BUF, fp))
	{
		if (checkdouble && !strncmp(buf, "checksum", 8))
		{
			continue;
		}

		for (cp = buf; *cp; cp++)
		{
			if (++offset > 28)
			{
				offset = 0;
			}

			checksum ^= (*cp << offset);
		}
	}

	fclose(fp);

	return checksum;
#else
	(void) filename;
	(void) checkdouble;

	return 0;
#endif
}

/**
 * Sort loaded spells by comparing a1 against a2.
 * @param a1 Spell ID to compare
 * @param a2 Spell ID to compare
 * @return Return value of strcmp on the two spell names. */
static int spell_sort(const void *a1, const void *a2)
{
	return strcmp(spells[(int)*(sint16 *) a1].name, spells[(int)*(sint16 *) a2].name);
}

/**
 * Helper function to reorder the reverse loaded player inventory.
 *
 * This will recursively reorder the container inventories.
 * @param op The inventory object to reorder. */
static void reorder_inventory(object *op)
{
	object *tmp, *tmp2;

	tmp2 = op->inv->below;
	op->inv->above = NULL;
	op->inv->below = NULL;

	if (op->inv->inv)
	{
		reorder_inventory(op->inv);
	}

	for (; tmp2; )
	{
		tmp = tmp2;
		/* save the following element */
		tmp2 = tmp->below;
		tmp->above = NULL;
		/* resort it like in insert_ob_in_ob() */
		tmp->below = op->inv;
		tmp->below->above = tmp;
		op->inv = tmp;

		if (tmp->inv)
		{
			reorder_inventory(tmp);
		}
	}
}

static void wrong_password(player *pl)
{
	pl->socket.password_fails++;

	pl->last_value = -1;

	if (pl->socket.password_fails >= MAX_PASSWORD_FAILURES)
	{
		char buf[MAX_BUF];

		LOG(llevSystem, "SHACK: %s@%s: Failed to provide a correct password too many times!\n", query_name(pl->ob, NULL), pl->socket.host);
		strcpy(buf, "X3 You have failed to provide a correct password too many times.");
		Write_String_To_Socket(&pl->socket, BINARY_CMD_DRAWINFO, buf, strlen(buf));
		pl->socket.can_write = 1;
		write_socket_buffer(&pl->socket);
		pl->socket.status = Ns_Dead;
	}
	else
	{
		FREE_AND_COPY_HASH(pl->ob->name, "noname");
		get_name(pl->ob);
	}
}

/**
 * Login a player.
 * @param op Player.
 * @todo Optimize and simplify the database loading. */
void check_login(object *op)
{
	FILE *fp;
	void *mybuffer;
	char filename[MAX_BUF], buf[MAX_BUF], bufall[MAX_BUF], banbuf[256], sqlfailbuf[256];
	int i, value, comp, correct = 0;
	long checksum = 0;
	player *pl = CONTR(op), *pltmp;
	time_t elapsed_save_time = 0;
	struct stat	statbuf;
	object *tmp, *tmp2;
	sqlite3 *db;
	sqlite3_stmt *statement;

	strcpy(pl->maplevel, first_map_path);

	/* Check if this matches a connected player, and if so disconnect old
	 * and connect new. */
	for (pltmp = first_player; pltmp != NULL; pltmp = pltmp->next)
	{
		if (pltmp != pl && pltmp->ob->name != NULL && !strcmp(pltmp->ob->name, op->name))
		{
			if (check_password(pl->write_buf + 1, pltmp->password))
			{
				pltmp->socket.status = Ns_Dead;
				break;
			}
			else
			{
				wrong_password(pl);
				return;
			}
		}
	}

	/* a good point to add this i to a 10 minute temp ban...
	 * if needed, i add it... its not much work but i better
	 * want a real login server in the future */
	if (pl->state == ST_PLAYING)
	{
		LOG(llevSystem, "HACK-BUG: >%s< from ip %s - double login!\n", op->name, pl->socket.host);
		new_draw_info_format(NDI_UNIQUE, 0, op, "You manipulated the login procedure.\nYour IP is ... >%s< - hack flag set!\nserver break", pl->socket.host);
		pl->socket.status = Ns_Dead;
		return;
	}

	if (checkbanned((char *) op->name, pl->socket.host))
	{
		LOG(llevInfo, "Banned player tried to login. [%s@%s]\n", op->name, pl->socket.host);
		strcpy(banbuf, "X3 Connection refused.\nYou are banned!");
		Write_String_To_Socket(&pl->socket, BINARY_CMD_DRAWINFO, banbuf, strlen(banbuf));
		pl->socket.can_write = 1;
		write_socket_buffer(&pl->socket);
		pl->socket.status = Ns_Dead;
		return;
	}

	LOG(llevInfo, "LOGIN: >%s< from ip %s\n", op->name, pl->socket.host);

	/* The player file will be temporarily stored in temporary directory. */
	sprintf(filename, "%s/%s.player", settings.tmpdir, op->name);

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL query */
	if (!db_prepare_format(db, &statement, "SELECT data FROM players WHERE playerName = '%s';", op->name))
	{
		LOG(llevBug, "BUG: check_login(): SQL prepare query failed for %s! (%s)\n", op->name, db_errmsg(db));
		strcpy(sqlfailbuf, "X3 SQL QUERY FAILED: CONTACT ADMINISTRATOR.");
		Write_String_To_Socket(&pl->socket, BINARY_CMD_DRAWINFO, sqlfailbuf, strlen(sqlfailbuf));
		pl->socket.can_write = 1;
		write_socket_buffer(&pl->socket);
		pl->socket.status = Ns_Dead;
		return;
	}

	/* Run the query and check if we got a valid output. */
	if (db_step(statement) == SQLITE_ROW)
	{
		fp = fopen(filename, "w");

		fputs((char *)db_column_text(statement, 0), fp);

		fclose(fp);
	}

	/* Finalize it */
	db_finalize(statement);

	/* And close. */
	db_close(db);

	/* If no file, must be a new player, so lets get confirmation of
	 * the password.  Return control to the higher level dispatch,
	 * since the rest of this just deals with loading of the file. */
	if ((fp = open_and_uncompress(filename, 1, &comp)) == NULL)
	{
		confirm_password(op);
		return;
	}

	if (fstat(fileno(fp), &statbuf))
	{
		LOG(llevBug, "BUG: Unable to stat %s?\n", filename);
		elapsed_save_time = 0;
	}
	else
	{
		elapsed_save_time = time(NULL) - statbuf.st_mtime;
		if (elapsed_save_time < 0)
		{
			LOG(llevBug, "BUG: Player file %s was saved in the future? (%d time)\n", filename, elapsed_save_time);
			elapsed_save_time = 0;
		}
	}

	if (fgets(bufall, MAX_BUF, fp) != NULL)
	{
		if (!strncmp(bufall, "checksum ", 9))
		{
			checksum = strtol_local(bufall + 9, (char **) NULL, 16);
			if (fgets(bufall, MAX_BUF, fp) == NULL)
				LOG(llevDebug, "DEBUG: check_login(): fgets failed for %s.\n", op->name);
		}

		if (sscanf(bufall, "password %s\n", buf))
		{
			/* New password scheme: */
			correct = check_password(pl->write_buf + 1, buf);
		}
	}

	if (!correct)
	{
		wrong_password(pl);

		/* Once again, rest of code just loads the char */
		return;
	}

	pl->afk = 0;

#ifdef SAVE_INTERVAL
	pl->last_save_time = time(NULL);
#endif

	pl->party_number = -1;

#ifdef SEARCH_ITEMS
	pl->search_str[0] = '\0';
#endif

	pl->name_changed = 1;
	pl->orig_stats.Str = 0;
	pl->orig_stats.Dex = 0;
	pl->orig_stats.Con = 0;
	pl->orig_stats.Int = 0;
	pl->orig_stats.Pow = 0;
	pl->orig_stats.Wis = 0;
	pl->orig_stats.Cha = 0;
	strcpy(pl->savebed_map, first_map_path);
	pl->bed_x = 0, pl->bed_y = 0;

	/* Loop through the file, loading the rest of the values */
	while (fgets(bufall, MAX_BUF, fp) != NULL)
	{
		sscanf(bufall, "%s %d\n", buf, &value);
		if (!strcmp(buf, "endplst"))
			break;

		else if (!strcmp(buf, "dm_stealth"))
			pl->dm_stealth = value;

		else if (!strcmp(buf, "gen_hp"))
			pl->gen_hp = value;

		else if (!strcmp(buf, "shoottype"))
			pl->shoottype = (rangetype)value;

		else if (!strcmp(buf, "gen_sp"))
			pl->gen_sp = value;

		else if (!strcmp(buf, "gen_grace"))
			pl->gen_grace = value;

		else if (!strcmp(buf, "spell"))
			pl->chosen_spell = value;

		else if (!strcmp(buf, "digestion"))
			pl->digestion = value;

#if 0
		else if (!strcmp(buf, "outputs_sync"))
			pl->outputs_sync = value;
		else if (!strcmp(buf, "outputs_count"))
			pl->outputs_count = value;
#endif
		else if (!strcmp(buf, "map"))
			sscanf(bufall, "map %s", pl->maplevel);

		else if (!strcmp(buf, "savebed_map"))
			sscanf(bufall, "savebed_map %s", pl->savebed_map);

		else if (!strcmp(buf, "bed_x"))
			pl->bed_x = value;

		else if (!strcmp(buf, "bed_y"))
			pl->bed_y = value;

		else if (!strcmp(buf, "Str"))
			pl->orig_stats.Str = value;

		else if (!strcmp(buf, "Dex"))
			pl->orig_stats.Dex = value;

		else if (!strcmp(buf, "Con"))
			pl->orig_stats.Con = value;

		else if (!strcmp(buf, "Int"))
			pl->orig_stats.Int = value;

		else if (!strcmp(buf, "Pow"))
			pl->orig_stats.Pow = value;

		else if (!strcmp(buf, "Wis"))
			pl->orig_stats.Wis = value;

		else if (!strcmp(buf, "Cha"))
			pl->orig_stats.Cha = value;

		else if (!strcmp(buf, "lev_hp"))
		{
			int j;
			for (i = 1; i <= value; i++)
			{
				if (fscanf(fp, "%d\n", &j))
					pl->levhp[i] = j;
			}
		}

		else if (!strcmp(buf, "lev_sp"))
		{
			int j;
			for (i = 1; i <= value; i++)
			{
				if (fscanf(fp, "%d\n", &j))
					pl->levsp[i] = j;
			}
		}

		else if (!strcmp(buf, "lev_grace"))
		{
			int j;
			for (i = 1; i <= value; i++)
			{
				if (fscanf(fp, "%d\n", &j))
					pl->levgrace[i] = j;
			}
		}

		else if (!strcmp(buf, "known_spell"))
		{
			char *cp = strchr(bufall, '\n');
			*cp = '\0';
			cp = strchr(bufall, ' ');
			cp++;
			for (i = 0; i < NROFREALSPELLS; i++)
				if (!strcmp(spells[i].name, cp))
				{
					pl->known_spells[pl->nrofknownspells++] = i;
					break;
				}

			if (i == NROFREALSPELLS)
				LOG(llevDebug, "Error: unknown spell (%s)\n", cp);
		}
	}

	/* Take the player ob out from the void */
	if (!QUERY_FLAG(op, FLAG_REMOVED))
		remove_ob(op);

	/* We transfer it to a new object */
	op->custom_attrset = NULL;

	LOG(llevDebug, "load obj for player: %s\n", op->name);

	/* Create a new object for the real player data */
	op = get_object();

	/* This loads the standard objects values. */
	mybuffer = create_loader_buffer(fp);
	load_object(fp, op, mybuffer, LO_REPEAT, 0);
	delete_loader_buffer(mybuffer);
	close_and_delete(fp, comp);
	unlink(filename);

	/* at this moment, the inventory is reverse loaded.
	 * Lets exchange it here.
	 * Caution: We do it on the hard way here without
	 * calling remove/insert again. */
	if (op->inv)
	{
		tmp2 = op->inv->below;
		op->inv->above = NULL;
		op->inv->below = NULL;

		if (op->inv->inv)
			reorder_inventory(op->inv);

		for (; tmp2; )
		{
			tmp = tmp2;
			/* save the following element */
			tmp2 = tmp->below;
			tmp->above = NULL;
			/* resort it like in insert_ob_in_ob() */
			tmp->below = op->inv;
			tmp->below->above = tmp;
			op->inv = tmp;
			if (tmp->inv)
				reorder_inventory(tmp);
		}

	}

	op->custom_attrset = pl;
	pl->ob = op;
	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

	/* If player saved beyond some time ago, and the feature is
	 * enabled, put the player back on his savebed map. */
	if ((settings.reset_loc_time > 0) && (elapsed_save_time > settings.reset_loc_time))
	{
		strcpy(pl->maplevel, pl->savebed_map);
		op->x = pl->bed_x, op->y = pl->bed_y;
	}

	/* make sure he's a player -- needed because of class change. */
	op->type = PLAYER;

	/* this is a funny thing: what happens when the autosave function saves a player
	 * with negative hp? (never thought thats possible but happens in a 0.95 server)
	 * Well, the sever tries to create a gravestone and heals the player... and then
	 * server tries to insert gravestone and anim on a map - but player is still in login!
	 * So, we are nice and set hp to 1 if here negative. */
	if (op->stats.hp < 0)
		op->stats.hp = 1;

	pl->name_changed = 1;

	/* Open database once again. */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL query to update status of player.
	 * If this fails, do not return, because it's not trivial. */
	if (db_prepare_format(db, &statement, "UPDATE players SET playing = 1 WHERE playerName = '%s';", op->name))
	{
		db_step(statement);

		db_finalize(statement);
	}
	else
		LOG(llevBug, "BUG: check_login(): SQL query failed for %s! (%s)\n", op->name, db_errmsg(db));

	/* Close the database */
	db_close(db);

	pl->state = ST_PLAYING;
#ifdef AUTOSAVE
	pl->last_save_tick = pticks;
#endif
	op->carrying = sum_weight(op);

	/* Need to call fix_player now - program modified so that it is not
	 * called during the load process (FLAG_NO_FIX_PLAYER set when
	 * saved)
	 * Moved ahead of the esrv functions, so proper weights will be
	 * sent to the client. */
	(void) init_player_exp(op);
	(void) link_player_skills(op);

	if (!legal_range (op, pl->shoottype))
		pl->shoottype = range_none;

	fix_player(op);

	/* if it's a dragon player, set the correct title here */
	if (is_dragon_pl(op) && op->inv != NULL)
	{
		object *tmp, *abil = NULL, *skin = NULL;
		for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		{
			if (tmp->type == FORCE)
			{
				if (strcmp(tmp->arch->name, "dragon_ability_force") == 0)
					abil = tmp;
				else if (strcmp(tmp->arch->name, "dragon_skin_force") == 0)
					skin = tmp;
			}
		}
		set_dragon_name(op, abil, skin);
	}

	/* important: there is a player file */
	pl->player_loaded = 1;

	/* Display Message of the Day */
	display_motd(op);

	if (!pl->dm_stealth)
	{
		new_draw_info_format(NDI_UNIQUE | NDI_ALL, 5, NULL, "%s has entered the game.", query_name(pl->ob, NULL));

		if (dm_list)
		{
			objectlink *tmp_dm_list;
			player *pl_tmp;
			int players;

			for (pl_tmp = first_player, players = 0; pl_tmp != NULL; pl_tmp = pl_tmp->next, players++);

			for (tmp_dm_list = dm_list; tmp_dm_list != NULL; tmp_dm_list = tmp_dm_list->next)
				new_draw_info_format(NDI_UNIQUE, 0, tmp_dm_list->ob, "DM: %d players now playing.", players);
		}
	}

	/* Trigger the global LOGIN event */
	trigger_global_event(EVENT_LOGIN, pl, pl->socket.host);

#ifdef ENABLE_CHECKSUM
	LOG(llevDebug, "Checksums: %x %x\n", checksum, calculate_checksum(filename, 1));
	if (calculate_checksum(filename, 1) != checksum)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Since your savefile has been tampered with, you will not be able to save again.");
		set_cheat(op);
	}
#endif
	/* If the player should be dead, call kill_player for them
	 * Only check for hp - if player lacks food, let the normal
	 * logic for that to take place.  If player is permanently
	 * dead, and not using permadeath mode, the kill_player will
	 * set the play_again flag, so return. */
	if (op->stats.hp < 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Your character was dead last you played.");
		kill_player(op);
		if (pl->state != ST_PLAYING)
			return;
	}

	/* Do this after checking for death - no reason sucking up bandwidth if
	 * the data isn't needed. */
	esrv_new_player(pl, op->weight + op->carrying);
	esrv_send_inventory(op, op);

	pl->last_value = -1;

	/* This seems to compile without warnings now.  Don't know if it works
	 * on SGI's or not, however. */
	qsort((void *) pl->known_spells, pl->nrofknownspells, sizeof(pl->known_spells[0]), (int (*)()) spell_sort);

	/* hm, this is for secure - be SURE our player is on
	 * friendly list. If friendly is set, this was be done
	 * in loader.c. */
	if (!QUERY_FLAG(op, FLAG_FRIENDLY))
	{
		LOG(llevBug, "BUG: Player %s was loaded without friendly flag!", query_name(op, NULL));
		SET_FLAG(op, FLAG_FRIENDLY);
		add_friendly_object(op);
	}

	/* ok, we are done with the login.
	 * Lets put the player on the map and send all player lists to the client.
	 * The player is active now. */
	/* kick player on map - load map if needed */
	enter_exit(op, NULL);

	pl->socket.update_tile = 0;
	pl->socket.look_position = 0;
	pl->socket.ext_title_flag = 1;

	pl->ob->direction = 4;
	esrv_new_player(pl, op->weight + op->carrying);
	/* send the known spells as list to client */
	send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);
	send_skilllist_cmd(op, NULL, SPLIST_MODE_ADD);
	return;
}
