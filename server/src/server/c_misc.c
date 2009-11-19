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
#include <sproto.h>

#undef SS_STATISTICS
#include <shstr.h>

/** @file
 * Handles misc. input request - things like hash table, malloc, maps,
 * who, etc. */

/**
 * Show maps information.
 * @param op The object to show the information to. */
void map_info(object *op)
{
	mapstruct *m;
	char buf[MAX_BUF], map_path[MAX_BUF];
	long sec = seconds();

#ifdef MAP_RESET
	LOG(llevSystem, "Current time is: %02ld:%02ld:%02ld.\n", (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60);

	new_draw_info_format(NDI_UNIQUE, 0, op, "Current time is: %02ld:%02ld:%02ld.", (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60);
	new_draw_info(NDI_UNIQUE, 0, op, "Path               Pl PlM IM   TO Dif Reset");
#else
	new_draw_info(NDI_UNIQUE, 0, op, "Pl Pl-M IM   TO Dif");
#endif

	for (m = first_map; m != NULL; m = m->next)
	{
#ifndef MAP_RESET
		if (m->in_memory == MAP_SWAPPED)
			continue;
#endif
		/* Print out the last 18 characters of the map name... */
		if (strlen(m->path) <= 18)
			strcpy(map_path, m->path);
		else
			strcpy(map_path, m->path + strlen(m->path) - 18);

#ifndef MAP_RESET
		sprintf(buf, "%-18.18s %c %2d   %c %4ld %2ld", map_path, m->in_memory ? (m->in_memory == MAP_IN_MEMORY ? 'm' : 's') : 'X', players_on_map(m), m->in_memory, m->timeout, m->difficulty);
#else
		LOG(llevSystem,"%s pom:%d status:%c timeout:%d diff:%d  reset:%02d:%02d:%02d\n", m->path, players_on_map(m), m->in_memory ? (m->in_memory == MAP_IN_MEMORY ? 'm' : 's') : 'X', m->timeout, m->difficulty, (MAP_WHEN_RESET(m) % 86400) / 3600, (MAP_WHEN_RESET(m) % 3600) / 60, MAP_WHEN_RESET(m) % 60);

		sprintf(buf, "%-18.18s %2d   %c %4d %2d  %02d:%02d:%02d", map_path, players_on_map(m), m->in_memory ? (m->in_memory == MAP_IN_MEMORY ? 'm' : 's') : 'X', m->timeout, m->difficulty, (MAP_WHEN_RESET(m) % 86400) / 3600, (MAP_WHEN_RESET(m) % 3600) / 60, MAP_WHEN_RESET(m) % 60);
#endif

		new_draw_info(NDI_UNIQUE, 0, op, buf);
	}
}

/**
 * Show message of the day.
 * @param op The object calling this
 * @param params Parameters
 * @return Always returns 1 */
int command_motd(object *op, char *params)
{
	(void) params;

	display_motd(op);
	return 1;
}

/* Command to report a bug. */
/**
 * Report a bug and store it in database.
 * @param op Object reporting this bug
 * @param params The bug message
 * @return 1 on success, 0 on failure */
int command_bug(object *op, char *params)
{
	sqlite3 *db;
	sqlite3_stmt *statement;

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "What bug?");
		return 1;
	}

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the query */
	if (!db_prepare_format(db, &statement, "INSERT INTO bugs (reporter, bug) VALUES ('%s', '%s');", op->name, db_sanitize_input(params)))
	{
		LOG(llevBug, "BUG: command_bug(): Failed to prepare SQL query for %s! (%s)", op->name, db_errmsg(db));
		new_draw_info(NDI_UNIQUE, 0, op, "Something strange happened.\nYou forgot what you wanted to report?!");
		db_close(db);
		return 0;
	}

	/* Run the query */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	new_draw_info(NDI_UNIQUE, 0, op, "OK, thank you for reporting this bug!");
	return 1;
}

/**
 * Command to roll a magical die.
 * Parameters should be XdY where X is
 * how many times to roll the die and
 * Y how many sides should the die have.
 * @param op Object rolling the die
 * @param params Parameters
 * @return Always returns 1 */
int command_roll(object *op, char *params)
{
	int times, sides, i;
	char buf[MAX_BUF];

	/* No params, or params not in format of <times>d<sides>. */
	if (params == NULL || !sscanf(params, "%dd%d", &times, &sides))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /roll <times>d<sides>");
		return 1;
	}

	/* Make sure times is a valid value. */
	if (times > 10)
		times = 10;
	else if (times <= 0)
		times = 1;

	/* Make sure sides is a valid value. */
	if (sides > 100)
		sides = 100;
	else if (sides <= 0)
		sides = 1;

	sprintf(buf, "%s rolls a magical die (%dd%d) and gets: ", op->name, times, sides);

	for (i = 1; i <= times; i++)
		sprintf(buf, "%s%d%s", buf, rndm(1, sides), i < times ? ", " : ".");

	new_draw_info(NDI_ORANGE | NDI_UNIQUE, 0, op, buf);
	new_info_map_except(NDI_ORANGE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
	return 1;
}

/**
 * Returns the number of objects on the list of active objects.
 * @return The number of active objects */
static int count_active()
{
	int i = 0;
	object *tmp = active_objects;

	while (tmp != NULL)
		tmp = tmp->active_next, i++;

	return i;
}


/**
 * Give out of info about memory usage.
 * @param op The object requesting this */
void malloc_info(object *op)
{
	int players,nrofmaps;
	int nrm = 0, mapmem = 0, anr, anims, sum_alloc = 0, sum_used = 0, i, j, tlnr, alnr;
	treasurelist *tl;
	player *pl;
	mapstruct *m;
	archetype *at;
	artifactlist *al;

	for (tl = first_treasurelist, tlnr = 0; tl != NULL; tl = tl->next, tlnr++);

	for (al = first_artifactlist, alnr = 0; al != NULL; al = al->next, alnr++);

	for (at = first_archetype, anr = 0, anims = 0; at != NULL; at = at->more == NULL ? at->next : at->more, anr++);

	for (i = 1; i < num_animations; i++)
		anims += animations[i].num_animations;

	for (pl = first_player, players = 0; pl != NULL; pl = pl->next, players++);

	for (m = first_map, nrofmaps = 0; m != NULL; m = m->next, nrofmaps++)
		if (m->in_memory == MAP_IN_MEMORY)
		{
			mapmem += MAP_WIDTH(m) * MAP_HEIGHT(m) * (sizeof(object *) + sizeof(MapSpace));
			nrm++;
		}

	sprintf(errmsg, "Sizeof: object=%ld  player=%ld  map=%ld", (long)sizeof(object), (long)sizeof(player), (long)sizeof(mapstruct));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);

	for (j = 0; j < NROF_MEMPOOLS; j++)
	{
		int ob_used = mempools[j].nrof_used, ob_free = mempools[j].nrof_free;

		sprintf(errmsg, "%4d used %s:    %8d", ob_used, mempools[j].chunk_description, i = (ob_used * (mempools[j].chunksize + sizeof(struct mempool_chunk))));
		new_draw_info(NDI_UNIQUE, 0, op, errmsg);
		sum_used += i;
		sum_alloc += i;

		sprintf(errmsg, "%4d free %s:    %8d", ob_free, mempools[j].chunk_description, i = (ob_free * (mempools[j].chunksize + sizeof(struct mempool_chunk))));
		new_draw_info(NDI_UNIQUE, 0, op, errmsg);
		sum_alloc += i;
	}

	sprintf(errmsg, "%4d active objects", count_active());
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);

	sprintf(errmsg, "%4d maps allocated:  %8d", nrofmaps, i = (nrofmaps * sizeof(mapstruct)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += nrm * sizeof(mapstruct);

	sprintf(errmsg, "%4d maps in memory:  %8d", nrm, mapmem);
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += mapmem;
	sum_used += mapmem;

	sprintf(errmsg, "%4d archetypes:      %8d", anr, i = (anr * sizeof(archetype)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4d animations:      %8d", anims, i = (anims * sizeof(Fontindex)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4d spells:          %8d", NROFREALSPELLS, i = (NROFREALSPELLS * sizeof(spell)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4d treasurelists    %8d", tlnr, i = (tlnr * sizeof(treasurelist)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4ld treasures        %8d", nroftreasures, i = (nroftreasures * sizeof(treasure)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4ld artifacts        %8d", nrofartifacts, i = (nrofartifacts * sizeof(artifact)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4ld artifacts strngs %8d", nrofallowedstr, i = (nrofallowedstr * sizeof(linked_char)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "%4d artifactlists    %8d", alnr, i = (alnr * sizeof(artifactlist)));
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	sum_alloc += i;
	sum_used += i;

	sprintf(errmsg, "Total space allocated:%8d", sum_alloc);
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);

	sprintf(errmsg, "Total space used:     %8d", sum_used);
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
}

/**
 * Give out some info about the map op is located at.
 * @param op The object requesting this information */
void current_map_info(object *op)
{
	mapstruct *m = op->map;
	char buf[128];

	if (!m)
		return;

	strcpy(buf, m->name);
	new_draw_info_format(NDI_UNIQUE, 0, op, "%s (%s)", buf, m->path);

	if (QUERY_FLAG(op, FLAG_WIZ))
		new_draw_info_format(NDI_UNIQUE, 0, op, "players:%d difficulty:%d size:%dx%d start:%dx%d timeout %ld", players_on_map(m), m->difficulty, MAP_WIDTH(m), MAP_HEIGHT(m), MAP_ENTER_X(m), MAP_ENTER_Y(m), MAP_TIMEOUT(m));

	if (m->msg)
		new_draw_info(NDI_UNIQUE, NDI_NAVY, op, m->msg);
}

#ifdef DEBUG_MALLOC_LEVEL
int command_malloc_verify(object *op, char *parms)
{
	extern int malloc_verify(void);

	if (!malloc_verify())
		new_draw_info(NDI_UNIQUE, 0, op, "Heap is corrupted.");
	else
		new_draw_info(NDI_UNIQUE, 0, op, "Heap checks out OK.");

	return 1;
}
#endif

/**
 * Print out a list of all logged in players in the game.
 * @param op The object requesting this
 * @param params Command parameters
 * @return Always returns 1.
 * @todo Perhaps make a GUI from this like the party GUI
 * and you would be able to press enter on player name
 * and that would bring up the /tell console? */
int command_who(object *op, char *params)
{
	player *pl;
	int ip = 0, il = 0;
	char buf[MAX_BUF];

	(void) params;

	if (first_player)
		new_draw_info(NDI_UNIQUE, 0, op, " ");

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (pl->dm_stealth && !QUERY_FLAG(op, FLAG_WIZ))
			continue;

		if (pl->ob->map == NULL)
		{
			il++;
			continue;
		}

		ip++;
		if (pl->state == ST_PLAYING)
		{
			char *sex = "neuter";
			if (QUERY_FLAG(pl->ob, FLAG_IS_MALE))
				sex = QUERY_FLAG(pl->ob, FLAG_IS_FEMALE) ? "hermaphrodite" : "male";
			else if (QUERY_FLAG(pl->ob, FLAG_IS_FEMALE))
				sex = "female";

			if (op == NULL || QUERY_FLAG(op, FLAG_WIZ))
				(void) sprintf(buf, "%s the %s %s (@%s) [%s]%s%s (%d)", pl->ob->name, sex, pl->ob->race, pl->socket.host, pl->ob->map->path, QUERY_FLAG(pl->ob, FLAG_WIZ) ? " [WIZ]" : "", pl->afk ? " [AFK]" : "", pl->ob->count);
			else
				sprintf(buf, "%s the %s %s (lvl %d)%s%s", pl->ob->name, sex, pl->ob->race, pl->ob->level, QUERY_FLAG(pl->ob, FLAG_WIZ) ? " [WIZ]" : "", pl->afk ? " [AFK]" : "");

			new_draw_info(NDI_UNIQUE, 0, op, buf);
		}
	}

	sprintf(buf, "There %s %d player%s online (%d in login).", ip + il > 1 ? "are" : "is", ip + il, ip + il > 1 ? "s" : "", il);
	new_draw_info(NDI_UNIQUE, 0, op, buf);

	return 1;
}

/**
 * Malloc info command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_malloc(object *op, char *params)
{
#ifdef MEMPOOL_TRACKING
	if (params)
	{
		int force_flag = 0, i;

		if (strcmp(params, "free") && strcmp(params, "force"))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Usage: /malloc [free | force]");
			return 1;
		}

		if (strcmp(params, "force") == 0)
			force_flag = 1;

		for (i = 0; i < NROF_MEMPOOLS; i++)
			if (force_flag == 1 || mempools[i].flags & MEMPOOL_ALLOW_FREEING)
				free_empty_puddles(i);
	}
#endif

	malloc_info(op);
	return 1;
}

/**
 * Map info command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_mapinfo(object *op, char *params)
{
	(void) params;

	current_map_info(op);
	return 1;
}

/**
 * Maps command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_maps(object *op, char *params)
{
	(void) params;

	map_info(op);
	return 1;
}

/**
 * Strings command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_strings(object *op, char *params)
{
	char buf[HUGE_BUF];

	(void) params;

	ss_dump_statistics(buf, sizeof(buf));
	new_draw_info(NDI_UNIQUE, 0, op, buf);

	ss_dump_table(SS_DUMP_TOTALS, buf, sizeof(buf));
	new_draw_info(NDI_UNIQUE, 0, op, buf);

	return 1;
}

int command_ssdumptable(object *op, char *params)
{
	(void) params;
	(void) op;
	ss_dump_table(SS_DUMP_TABLE, NULL, 0);
	return 1;
}

/**
 * Time command. Print out game time information.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_time(object *op, char *params)
{
	(void) params;

	time_info(op);
	return 1;
}

/**
 * Arches command. Print out information about the arches.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_archs(object *op, char *params)
{
	(void) params;

	arch_info(op);
	return 1;
}

/**
 * Highscore command, shows the highscore.
 * @param op Object requesting this
 * @param params Parameters
 * @return Always returns 1 */
int command_hiscore(object *op, char *params)
{
	display_high_score(op, op == NULL ? 9999 : 50, params);
	return 1;
}

/**
 * Set debug level command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_debug(object *op, char *params)
{
	int i;
	char buf[MAX_BUF];

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		sprintf(buf, "Global debug level is %d.", settings.debug);
		new_draw_info(NDI_UNIQUE, 0, op, buf);
		return 1;
	}

	if (op != NULL && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Privileged command.");
		return 1;
	}

	settings.debug = (enum LogLevel) FABS(i);
	sprintf(buf, "Set debug level to %d.", i);
	new_draw_info(NDI_UNIQUE, 0, op, buf);
	return 1;
}

/**
 * Full dump of objects below the DM.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpbelowfull(object *op, char *params)
{
	object *tmp;

	(void) params;
	new_draw_info(NDI_UNIQUE, 0, op, "DUMP OBJECTS OF THIS TILE");
	new_draw_info(NDI_UNIQUE, 0, op, "-------------------");
	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above)
	{
		/* exclude the DM player object */
		if (tmp == op)
			continue;

		dump_object(tmp);
		new_draw_info(NDI_UNIQUE, 0, op, errmsg);

		if (tmp->above && tmp->above != op)
			new_draw_info(NDI_UNIQUE, 0, op, ">next object<");
	}
	new_draw_info(NDI_UNIQUE, 0, op, "------------------");

	return 1;
}

/**
 * Dump of objects below the DM.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpbelow(object *op, char *params)
{
	object *tmp;
	char buf[5 * 1024];
	int i = 0;

	(void) params;
	new_draw_info(NDI_UNIQUE, 0, op, "DUMP OBJECTS OF THIS TILE");
	new_draw_info(NDI_UNIQUE, 0, op, "-------------------");
	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above, i++)
	{
		/* exclude the DM player object */
		if (tmp == op)
			continue;

		sprintf(buf, "#%d  >%s<  >%s<  >%s<", i, query_name(tmp, NULL), tmp->arch ? (tmp->arch->name ? tmp->arch->name : "no arch name") : "NO ARCH", tmp->env ? query_name(tmp->env, NULL) : "");
		new_draw_info(NDI_UNIQUE, 0, op, buf);
	}
	new_draw_info(NDI_UNIQUE, 0, op, "------------------");

	return 1;
}

/**
 * Wizpass command. Used by DMs to toggle walking through walls on/off.
 * @param op Object requesting this
 * @param params Command parameters
 * @return 1 on success, 0 on failure */
int command_wizpass(object *op, char *params)
{
	int i;

	if (!op)
		return 0;

	if (!params)
		i = (QUERY_FLAG(op, FLAG_WIZPASS)) ? 0 : 1;
	else
		i = onoff_value(params);

	if (i)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You will now walk through walls.\n");
		SET_FLAG(op, FLAG_WIZPASS);
	}
	else
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You will now be stopped by walls.\n");
		CLEAR_FLAG(op, FLAG_WIZPASS);
	}

	return 1;
}

/**
 * Dump all objects.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpallobjects(object *op, char *params)
{
	(void) params;
	(void) op;

	return 1;
}

/**
 * Dump all friendly objects.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpfriendlyobjects(object *op, char *params)
{
	(void) params;
	(void) op;
	dump_friendly_objects();
	return 1;
}

/**
 * Dump all archetypes.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpallarchetypes(object *op, char *params)
{
	(void) params;
	(void) op;
	dump_all_archetypes();
	return 1;
}

/**
 * DM stealth command. Used by DMs to make the DM hidden
 * from all other players. It also works when DM logs in
 * without DM flag set or leaves the DM mode.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dm_stealth(object *op, char *params)
{
	(void) params;

	if (op->type == PLAYER && CONTR(op))
	{
		if (CONTR(op)->dm_stealth)
		{
			new_draw_info_format(NDI_UNIQUE | NDI_ALL, 5, NULL, "%s has entered the game.", query_name(op, NULL));
			CONTR(op)->dm_stealth = 0;
		}
		else
			CONTR(op)->dm_stealth = 1;

		new_draw_info_format(NDI_UNIQUE, 0, op, "Toggled dm_stealth to %d.", CONTR(op)->dm_stealth);
	}

	return 1;
}

/**
 * Toggle DM light on/off. DM light will light up
 * all maps for the DM.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dm_light(object *op, char *params)
{
	(void) params;

	if (op->type == PLAYER && CONTR(op))
	{
		if (CONTR(op)->dm_light)
			CONTR(op)->dm_light = 0;
		else
			CONTR(op)->dm_light = 1;

		new_draw_info_format(NDI_UNIQUE, 0, op, "toggle dm_light to %d", CONTR(op)->dm_light);
	}

	return 1;
}

/**
 * Dump active list.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpactivelist(object *op, char *params)
{
	char buf[1024];
	int count = 0;
	object *tmp;

	(void) params;

	for (tmp = active_objects; tmp; tmp = tmp->active_next)
	{
		count++;
		snprintf(buf, sizeof(buf), "%08d %03d %f %s (%s)", tmp->count, tmp->type, tmp->speed, query_short_name(tmp, NULL), tmp->arch->name ? tmp->arch->name : "<NA>");
		/* It will overflow the send buffer with many players online. */
		/* new_draw_info(NDI_UNIQUE, 0, op, buf); */
		LOG(llevSystem, "%s\n", buf);
	}

	snprintf(buf, sizeof(buf), "active objects: %d (dumped to log)", count);
	new_draw_info(NDI_UNIQUE, 0, op, buf);
	LOG(llevSystem, "%s\n", buf);

	return 1;
}

/**
 * Start server shutdown command. Used by DM to shutdown the
 * server in order to rebuild it to update maps.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_start_shutdown(object *op, char *params)
{
	char *bp = NULL;
	int i =- 2;

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "DM usage: /start_shutdown <-1 ... x>");
		return 1;
	}

	sscanf(params, "%d ", &i);
	if ((bp = strchr(params, ' ')) != NULL)
		bp++;

	if (bp && bp == 0)
		bp = NULL;

	if (i < -1)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "DM usage: /start_shutdown <-1 ... x>");
		return 1;
	}

	LOG(llevSystem, "Shutdown agent started!\n");
	shutdown_agent(i, bp);
	new_draw_info_format(NDI_UNIQUE | NDI_GREEN, 0, op, "Shutdown agent started! Timer set to %d seconds.", i);

	return 1;
}

/**
 * Set map light by DM.
 * @param op Object requesting this
 * @param params Command parameters
 * @return 1 on success, 0 on failure */
int command_setmaplight(object *op, char *params)
{
	int i;
	char buf[256];

	if (params == NULL || !sscanf(params, "%d", &i))
		return 0;

	if (i < -1)
		i = -1;

	if (i > MAX_DARKNESS)
		i = MAX_DARKNESS;

	op->map->darkness = i;

	if (i == -1)
		i = MAX_DARKNESS;

	op->map->light_value = global_darkness_table[i];

	sprintf(buf, "WIZ: set map darkness: %d -> map:%s (%d)", i, op->map->path, MAP_OUTDOORS(op->map));
	new_draw_info(NDI_UNIQUE, 0, op, buf);

	return 1;
}

/**
 * Dump map information.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpmap(object *op, char *params)
{
	(void) params;

	if (op)
		dump_map(op->map);

	return 1;
}

/**
 * Dump information about all maps.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpallmaps(object *op, char *params)
{
	(void) params;
	(void) op;

	dump_all_maps();

	return 1;
}

/**
 * Print Line of Sight.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_printlos(object *op, char *params)
{
	(void) params;

	if (op)
		print_los(op);

	return 1;
}

/**
 * Output version information.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_version(object *op, char *params)
{
	(void) params;

	version(op);

	return 1;
}

/**
 * Set player's listen level.
 * @param op Object requesting this
 * @param params The listen level to set
 * @return Always returns 1 */
int command_listen(object *op, char *params)
{
	int i;

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "Set listen to what (presently %d)?", CONTR(op)->listening);
		return 1;
	}

	CONTR(op)->listening = (char) i;
	new_draw_info_format(NDI_UNIQUE, 0, op, "Your verbose level is now %d.", i);
	return 1;
}

/**
 * Prints out some useful information for the character.  Everything we print
 * out can be determined by the docs, so we aren't revealing anything extra -
 * rather, we are making it convenient to find the values.  params have
 * no meaning here.
 * @param pl Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_statistics(object *pl, char *params)
{
	(void) params;
	(void) pl;
#if 0
	if (pl->type != PLAYER || !CONTR(pl))
		return 1;

	new_draw_info(NDI_UNIQUE, 0, pl, "                                        === STATISTICS ===");
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Experience: %d", pl->stats.exp);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Next Level: %d", level_exp(pl->level + 1, 1.0));

	new_draw_info(NDI_UNIQUE, 0, pl, "\nStat       Nat/Real/Max");

	new_draw_info_format(NDI_UNIQUE, 0, pl, "Str         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Str, pl->stats.Str, 20 + pl->arch->clone.stats.Str);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Dex         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Dex, pl->stats.Dex, 20 + pl->arch->clone.stats.Dex);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Con         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Con, pl->stats.Con, 20 + pl->arch->clone.stats.Con);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Int         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Int, pl->stats.Int, 20 + pl->arch->clone.stats.Int);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Wis         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Wis, pl->stats.Wis, 20 + pl->arch->clone.stats.Wis);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Pow         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Pow, pl->stats.Pow, 20 + pl->arch->clone.stats.Pow);
	new_draw_info_format(NDI_UNIQUE, 0, pl, "Cha         %2d/ %3d/%3d", CONTR(pl)->orig_stats.Cha, pl->stats.Cha, 20 + pl->arch->clone.stats.Cha);
#endif

	/* Can't think of anything else to print right now */
	return 1;
}

/**
 * Fix me command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_fix_me(object *op, char *params)
{
	(void) params;
	fix_player(op);
	return 1;
}

/**
 * Print out information about players. Currently unused,
 * because the old code will not work with SQLite.
 * @param op Object requesting this
 * @param paramss Command parameters
 * @return Always returns 1
 * @todo Make this work once again, perhaps for DMs only,
 * and allow searching for players by the parameters? */
int command_players(object *op, char *paramss)
{
	(void) paramss;
	(void) op;

	/* Nope, because of SQLite this won't work. */
	return 1;

#if 0
	char buf[MAX_BUF];
	char *t;
	DIR *Dir;

	sprintf(buf, "%s/%s/", settings.localdir, settings.playerdir);
	t = buf + strlen(buf);
	if ((Dir = opendir(buf)) != NULL)
	{
		const struct dirent *Entry;

		while ((Entry = readdir(Dir)) != NULL)
		{
			/* skip '.' , '..' */
			if (!((Entry->d_name[0] == '.' && Entry->d_name[1] == '\0') || (Entry->d_name[0] == '.' && Entry->d_name[1] == '.' && Entry->d_name[2] == '\0')))
			{
				struct stat Stat;

				strcpy(t, Entry->d_name);

				if (stat(buf, &Stat) == 0)
				{
					if ((Stat.st_mode & S_IFMT) == S_IFDIR)
					{
						char buf2[MAX_BUF];

						struct tm *tm = localtime(&Stat.st_mtime);
						sprintf(buf2, "%s\t%04d %02d %02d %02d %02d %02d", Entry->d_name, 1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
						new_draw_info(NDI_UNIQUE, 0, op, buf2);
					}
				}
			}
		}
	}
	closedir(Dir);
	return 1;
#endif
}

/**
 * Logs command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_logs(object *op, char *params)
{
	int first;

	(void) params;

	first = 1;

	if (first)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Nobody is currently logging kills.");
	}
	return 1;
}

/**
 * Print out object's resistances.
 * @param op Object requesting this
 * @param params Command parameters
 * @return 1 on success, 0 on failure */
int command_resistances(object *op, char *params)
{
	int i;

	(void) params;

	if (!op)
		return 0;

	for (i = 0; i < NROFATTACKS; i++)
	{
		if (i == ATNR_INTERNAL)
			continue;

		new_draw_info_format(NDI_UNIQUE, 0, op, "%-20s %+5d", attacktype_desc[i], op->resist[i]);
	}

	return 1;
}

/**
 * Pray command, used to start praying to your god.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_praying(object *op, char *params)
{
	(void) params;

	CONTR(op)->praying = 1;
	return 1;
}

/**
 * Scans input and returns if this is ON value (1) or OFF (0).
 * @param line The input string
 * @return 1 for ON, 0 for OFF. */
int onoff_value(char *line)
{
	int i;

	if (sscanf(line, "%d", &i))
		return (i != 0);

	switch (line[0])
	{
		case 'o':
			switch (line[1])
			{
					/* on */
				case 'n':
					return 1;

					/* o[ff] */
				default:
					return 0;
			}

			/* y[es] */
		case 'y':
			/* k[ylla] */
		case 'k':
		case 's':
		case 'd':
			return 1;

			/* n[o] */
		case 'n':
			/* e[i] */
		case 'e':
		case 'u':
		default:
			return 0;
	}
}

/**
 * Quit command. Currently unused.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_quit(object *op, char *params)
{
	(void) params;
	(void) op;
	return 1;
}

/**
 * Print out to player if he has sounds enabled.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_sound(object *op, char *params)
{
	(void) params;

	if (CONTR(op)->socket.sound)
	{
		CONTR(op)->socket.sound = 0;
		new_draw_info(NDI_UNIQUE, 0, op, "The sounds are disabled.");
	}
	else
	{
		CONTR(op)->socket.sound = 1;
		new_draw_info(NDI_UNIQUE, 0, op, "The sounds are enabled.");
	}

	return 1;
}

/**
 * Receive a player name, and force the first letter to be uppercase.
 * @param op Object
 * @param k Unused */
void receive_player_name(object *op, char k)
{
	unsigned int name_len = strlen(CONTR(op)->write_buf + 1);

	(void) k;

	/* Force a "Xxxxxxx" name */
	if (name_len > 1)
	{
		int i;

		for (i = 1; *(CONTR(op)->write_buf + i) != 0; i++)
		{
			*(CONTR(op)->write_buf + i) = tolower(*(CONTR(op)->write_buf + i));
		}

		*(CONTR(op)->write_buf + 1) = toupper(*(CONTR(op)->write_buf + 1));
	}

	if (!check_name(CONTR(op), CONTR(op)->write_buf + 1))
	{
		get_name(op);
		return;
	}

	FREE_AND_COPY_HASH(op->name, CONTR(op)->write_buf + 1);
#if 0
	new_draw_info(NDI_UNIQUE, 0, op,CONTR(op)->write_buf);
	/* Flag: redraw all stats */
	CONTR(op)->last_value= -1;
#endif

	CONTR(op)->name_changed = 1;
	get_password(op);
}

/**
 * Receive player password.
 * @param op Object
 * @param k Unused */
void receive_player_password(object *op, char k)
{
	unsigned int pwd_len = strlen(CONTR(op)->write_buf);

	(void) k;

	if (pwd_len <= 1 || pwd_len > 17)
	{
		get_name(op);
		return;
	}

	/* To hide the password better */
	new_draw_info(NDI_UNIQUE, 0, op, "          ");

	if (CONTR(op)->state == ST_CONFIRM_PASSWORD)
	{
		char cmd_buf[] = "X";

		if (!check_password(CONTR(op)->write_buf + 1, CONTR(op)->password))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "The passwords did not match.");
			get_name(op);
			return;
		}

		esrv_new_player(CONTR(op), 0);
		Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_NEW_CHAR, cmd_buf, 1);
		LOG(llevInfo, "NewChar send for %s\n", op->name);
		CONTR(op)->state = ST_ROLL_STAT;

		return;
	}

	strcpy(CONTR(op)->password, crypt_string(CONTR(op)->write_buf + 1, NULL));
	CONTR(op)->state = ST_ROLL_STAT;
	check_login(op);
	return;
}

/**
 * Save command.
 * Cannot be used on unholy ground or if you have 0 exp.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_save(object *op, char *params)
{
	(void) params;

	if (blocks_cleric(op->map, op->x, op->y))
		new_draw_info(NDI_UNIQUE, 0, op, "You can not save on unholy ground.");
	else if (!op->stats.exp)
		new_draw_info(NDI_UNIQUE, 0, op, "To avoid too much unused player accounts you must get some experience before you can save! Go kill some ants.");
	else
	{
		if (save_player(op, 1))
			new_draw_info(NDI_UNIQUE, 0, op, "You have been saved.");
		else
			new_draw_info(NDI_UNIQUE, 0, op, "SAVE FAILED!");

#if 0
		/* with the new code we should NOT save "active" maps.
		 * we do a kind of neutralizing when we save now that can have
		 * strange effects when saving! */
		if (op->map && !strncmp(op->map->path, settings.localdir, strlen(settings.localdir)))
		{
			new_save_map(op->map, 0);
			op->map->in_memory = MAP_IN_MEMORY;
		}
#endif
	}

	return 1;
}

/**
 * Style maps info command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_style_map_info(object *op, char *params)
{
	extern mapstruct *styles;
	mapstruct *mp;
	int maps_used = 0, mapmem = 0, objects_used = 0, x,y;
	object *tmp;

	(void) params;

	for (mp = styles; mp != NULL; mp = mp->next)
	{
		maps_used++;
		mapmem += MAP_WIDTH(mp) * MAP_HEIGHT(mp) * (sizeof(object *) + sizeof(MapSpace)) + sizeof(mapstruct);
		for (x = 0; x < MAP_WIDTH(mp); x++)
		{
			for (y = 0; y < MAP_HEIGHT(mp); y++)
			{
				for (tmp = get_map_ob(mp, x, y); tmp != NULL; tmp = tmp->above)
					objects_used++;
			}
		}
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "Style maps loaded:    %d", maps_used);
	new_draw_info(NDI_UNIQUE, 0, op, "Memory used, not");
	new_draw_info_format(NDI_UNIQUE, 0, op, "including objects:    %d", mapmem);
	new_draw_info_format(NDI_UNIQUE, 0, op, "Style objects:        %d", objects_used);
	new_draw_info_format(NDI_UNIQUE, 0, op, "Mem for objects:      %d", objects_used * sizeof(object));

	return 1;
}

/**
 * Apartment command.
 * This command is used to invite other players
 * to your apartment, as well as turn on/off
 * apartment invites, or cancel invitation.
 * @param op Object requesting this
 * @param params Command parameters
 * @return 1 on success, 0 on failure
 * @todo Make the turn on/off inviting use the
 * onoff_value() function to check for on/off. */
int command_apartment(object *op, char *params)
{
	player *pl;
	object *apartment_force, *apartment_force_owner = NULL, *tmp = NULL;
	int dir, num_players = 0;

	/* Go through caller's inventory and look for
	 * force of apartment inviter. */
	tmp = op->inv;
	while (tmp)
	{
		if (strcmp(tmp->arch->name, "force") == 0 && strcmp(tmp->name, "APARTMENT_INVITER") == 0)
		{
			/* Found it! */
			apartment_force_owner = tmp;
			break;
		}

		tmp = tmp->below;
	}

	/* params are null, show some info. */
	if (params == NULL)
	{
		/* If we're having apartment open, show how many players there are. */
		if (apartment_force_owner)
		{
			/* Count all the players that are on this map */
			for (pl = first_player; pl != NULL; pl = pl->next)
			{
				if (pl->state == ST_PLAYING && pl->ob->map && strcmp(pl->ob->map->path, apartment_force_owner->slaying) == 0)
					num_players++;
			}

			new_draw_info_format(NDI_UNIQUE, 0, op, "You're currently having an apartment invitation open and there are %d players.", num_players);
		}
		/* No apartment open */
		else
			new_draw_info(NDI_UNIQUE, 0, op, "You're currently not having apartment invitation open.");

		new_draw_info(NDI_UNIQUE, 0, op, "For help try: /apartment help");

		return 1;
	}

	/* /apartment help, show some help info. */
	if (strncmp(params, "help", 4) == 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "To invite a player to your apartment: /apartment invite <player name>");
		new_draw_info(NDI_UNIQUE, 0, op, "To cancel your apartment invitation and kick all players from your apartment: /apartment cancel");
		new_draw_info(NDI_UNIQUE, 0, op, "To turn on/off your apartment invite settings: /apartment <on|off>");
		new_draw_info(NDI_UNIQUE, 0, op, "Apartment invite setting needs to be on if you want to be invited by other players.");
		return 1;
	}
	/* The tricky part, /apartment invite.*/
	else if (strncmp(params, "invite ", 7) == 0)
	{
		params += 7;

		/* If the inviter IS having apartment open, and
		 * wants to invite other players to another map... */
		if (apartment_force_owner && strcmp(apartment_force_owner->slaying, op->map->path))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "In order to invite players to a different apartment you must first cancel your old apartment invitation! Type '/apartment cancel' for that.");
			return 1;
		}

		if (params == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Usage: /apartment invite <player name>");
			return 1;
		}

		/* First char of a name is always uppercase */
		params[0] = toupper(params[0]);

		/* Check if this map is unique, it is not no_save 1,
		 * and we are the map owner. */
		if (!MAP_UNIQUE(op->map) || MAP_NOSAVE(op->map) || !check_map_owner(op->map, op))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You can invite players to your apartment only if you're in your apartment!");
			return 1;
		}

		if (strcmp(op->name, params) == 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You cannot invite yourself!");
			return 1;
		}

		/* Find the player we want to invite */
		if ((pl = find_player(params)) == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "This player is not in game.");
			return 1;
		}

		/* If he doesn't allow invites */
		if (!pl->apartment_invite)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "%s does not accept apartment invites.", pl->ob->name);
			return 1;
		}

		/* Find a free spot to invite this player */
		dir = find_free_spot(op->arch, NULL, op->map, op->x, op->y, 1, SIZEOFFREE);

		/* No free space?
		 * FIXME: In theory, players *could* be teleported on closed door.
		 * This is also a problem with logging out of the game on door and
		 * logging back in. */
		if ((dir == -1) || wall_blocked(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "No free space to invite %s to.", pl->ob->name);
			return 1;
		}

		/* Check if this player has been invited by someone already */
		tmp = pl->ob->inv;
		while (tmp)
		{
			if (strcmp(tmp->arch->name, "force") == 0 && strcmp(tmp->name, "APARTMENT_INVITE") == 0)
			{
				/* Found it! */
				if (apartment_force_owner && strcmp(apartment_force_owner->slaying, tmp->slaying) == 0)
					new_draw_info_format(NDI_UNIQUE, 0, op, "You have already invited %s to join your apartment.", pl->ob->name);
				else
					new_draw_info_format(NDI_UNIQUE, 0, op, "%s has already been invited by someone else.", pl->ob->name);

				return 1;
			}

			tmp = tmp->below;
		}

		/* Check if this player is in caller's apartment */
		if (apartment_force_owner && pl->ob->map && strcmp(apartment_force_owner->slaying, pl->ob->map->path) == 0)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "%s is already in your apartment.", pl->ob->name);
			return 1;
		}

		apartment_force = get_archetype("force");

		/* No force archetype? Something is very wrong. */
		if (apartment_force == NULL)
		{
			LOG(llevBug, "BUG: command_apartment(): get_archetype() failed (force) for %s!\n", op->name);
			new_draw_info(NDI_UNIQUE, 0, op, "Something went very wrong...");
			return 0;
		}

		/* Store important stuff in the force:
		 *  - slaying: The map path to apartment
		 *  - race: The inviter's name
		 *  - name: The force name to fine it later
		 *  - hp, sp: x and y of the free position */
		FREE_AND_COPY_HASH(apartment_force->slaying, op->map->path);
		FREE_AND_COPY_HASH(apartment_force->race, op->name);
		FREE_AND_COPY_HASH(apartment_force->name, "APARTMENT_INVITE");
		apartment_force->stats.hp = op->x + freearr_x[dir];
		apartment_force->stats.sp = op->y + freearr_y[dir];
		apartment_force->speed = 0.0;

		update_ob_speed(apartment_force);
		insert_ob_in_ob(apartment_force, pl->ob);

		new_draw_info_format(NDI_UNIQUE | NDI_GREEN, 0, pl->ob, "%s has invited you to join %s apartment. If you want to accept this invite, go to the nearest apartment keeper.", op->name, QUERY_FLAG(op, FLAG_IS_MALE) ? (QUERY_FLAG(op, FLAG_IS_FEMALE) ? "its" : "his") : "her");

		/* First invite, make force inside the caller */
		if (apartment_force_owner == NULL)
		{
			apartment_force_owner = get_archetype("force");

			/* Almost same as for the invited player, but we don't need x and y. */
			FREE_AND_COPY_HASH(apartment_force_owner->slaying, op->map->path);
			FREE_AND_COPY_HASH(apartment_force_owner->race, op->name);
			FREE_AND_COPY_HASH(apartment_force_owner->name, "APARTMENT_INVITER");

			apartment_force_owner->speed = 0.0;
			update_ob_speed(apartment_force_owner);

			insert_ob_in_ob(apartment_force_owner, op);
		}

		new_draw_info_format(NDI_UNIQUE | NDI_GREEN, 0, op, "You have invited %s to join your apartment. Type '/apartment cancel' to cancel this invite and kick all players beside yourself from this apartment.");
	}
	/* Cancel an invite and kick all players besides the caller */
	else if (strncmp(params, "cancel", 6) == 0)
	{
		object *dummy;

		/* No open invitations */
		if (apartment_force_owner == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "No open apartment invitations.");
			return 1;
		}

		/* Loop through all players on and compare the map paths */
		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			if (pl->ob != op && strcmp(pl->ob->map->path, apartment_force_owner->slaying) == 0)
			{
				/* If we got a match, teleport the player to emergency map */
				dummy = get_object();
				FREE_AND_COPY_HASH(EXIT_PATH(dummy), EMERGENCY_MAPPATH);
				FREE_AND_COPY_HASH(dummy->name, EMERGENCY_MAPPATH);
				dummy->map = pl->ob->map;
				enter_exit(pl->ob, dummy);

				new_draw_info_format(NDI_UNIQUE | NDI_RED, 0, pl->ob, "%s has canceled the invitation!", op->name);
			}
		}

		/* As last, remove the force from the caller */
		remove_ob(apartment_force_owner);
	}
	/* Changes your apartment invite settings.
	 *  - on: Players can invite you to their apartment
	 *  - off: No invites from other players */
	else if (strncmp(params, "invites ", 8) == 0)
	{
		int invites;

		params += 8;

		if (params == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Usage: /apartment invites <off|on>");
			return 0;
		}

		if (strncmp(params, "on", 2) == 0)
			invites = 1;
		else if (strncmp(params, "off", 3) == 0)
			invites = 0;
		else
			return 0;

		if (CONTR(op)->apartment_invite != invites)
			new_draw_info_format(NDI_UNIQUE, 0, op, "Apartment invites turned %s.", invites ? "on" : "off");

		CONTR(op)->apartment_invite = invites;
	}
	else if (strncmp(params, "off", 3) == 0)
	{
		tmp = op->inv;
		while (tmp)
		{
			if (strcmp(tmp->arch->name, "force") == 0 && strcmp(tmp->name, "APARTMENT_INVITE") == 0)
				break;

			tmp = tmp->below;
		}

		if (tmp == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You have not been invited by anybody.");
			return 0;
		}

		new_draw_info_format(NDI_UNIQUE | NDI_RED, 0, op, "You have canceled %s's apartment invite!", tmp->race);

		if ((pl = find_player((char *)tmp->race)) != NULL)
			new_draw_info_format(NDI_UNIQUE | NDI_RED, 0, pl->ob, "%s has canceled your invite!", op->name);

		remove_ob(tmp);
	}

	return 0;
}

/**
 * Away from keyboard command.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_afk(object *op, char *params)
{
	(void) params;

	if (CONTR(op)->afk)
	{
		CONTR(op)->afk = 0;
		new_draw_info(NDI_UNIQUE, 0, op, "You are no longer AFK.");
	}
	else
	{
		CONTR(op)->afk = 1;
		new_draw_info(NDI_UNIQUE, 0, op, "You are now AFK.");
	}

	CONTR(op)->socket.ext_title_flag = 1;

	return 1;
}
