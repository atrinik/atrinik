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
 * Handles misc. input request - things like hash table, malloc, maps,
 * who, etc. */

#include <global.h>
#include <sproto.h>

#undef SS_STATISTICS
#include <shstr.h>

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
		{
			continue;
		}
#endif

		/* Print out the last 18 characters of the map name... */
		if (strlen(m->path) <= 18)
		{
			strcpy(map_path, m->path);
		}
		else
		{
			strcpy(map_path, m->path + strlen(m->path) - 18);
		}

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
 * @param op The object calling this.
 * @param params Parameters.
 * @return Always returns 1. */
int command_motd(object *op, char *params)
{
	(void) params;

	display_motd(op);
	return 1;
}

/**
 * Command to roll a magical die.
 *
 * Parameters should be XdY where X is how many times to roll the die and
 * Y how many sides should the die have.
 * @param op Object rolling the die.
 * @param params Parameters.
 * @return Always returns 1. */
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
	{
		times = 10;
	}
	else if (times <= 0)
	{
		times = 1;
	}

	/* Make sure sides is a valid value. */
	if (sides > 100)
	{
		sides = 100;
	}
	else if (sides <= 0)
	{
		sides = 1;
	}

	snprintf(buf, sizeof(buf), "%s rolls a magical die (%dd%d) and gets: ", op->name, times, sides);

	for (i = 1; i <= times; i++)
	{
		char tmp[MAX_BUF];

		snprintf(tmp, sizeof(tmp), "%d%s", rndm(1, sides), i < times ? ", " : ".");
		strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
	}

	new_draw_info(NDI_ORANGE | NDI_UNIQUE, 0, op, buf);
	new_info_map_except(NDI_ORANGE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);

	return 1;
}

/**
 * Counts the number of objects on the list of active objects.
 * @return The number of active objects. */
static int count_active()
{
	int i = 0;
	object *tmp = active_objects;

	while (tmp != NULL)
	{
		tmp = tmp->active_next, i++;
	}

	return i;
}

/**
 * Give out of info about memory usage.
 * @param op The object requesting this. */
void malloc_info(object *op)
{
	int players, nrofmaps;
	int nrm = 0, mapmem = 0, anr, anims, sum_alloc = 0, sum_used = 0, i, j, tlnr, alnr;
	treasurelist *tl;
	player *pl;
	mapstruct *m;
	archetype *at;
	artifactlist *al;

	for (tl = first_treasurelist, tlnr = 0; tl != NULL; tl = tl->next, tlnr++)
	{
	}

	for (al = first_artifactlist, alnr = 0; al != NULL; al = al->next, alnr++)
	{
	}

	for (at = first_archetype, anr = 0, anims = 0; at != NULL; at = at->more == NULL ? at->next : at->more, anr++)
	{
	}

	for (i = 1; i < num_animations; i++)
	{
		anims += animations[i].num_animations;
	}

	for (pl = first_player, players = 0; pl != NULL; pl = pl->next, players++)
	{
	}

	for (m = first_map, nrofmaps = 0; m != NULL; m = m->next, nrofmaps++)
	{
		if (m->in_memory == MAP_IN_MEMORY)
		{
			mapmem += MAP_WIDTH(m) * MAP_HEIGHT(m) * (sizeof(object *) + sizeof(MapSpace));
			nrm++;
		}
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "Sizeof: object=%ld  player=%ld  map=%ld", (long) sizeof(object), (long) sizeof(player), (long) sizeof(mapstruct));

	for (j = 0; j < NROF_MEMPOOLS; j++)
	{
		int ob_used = mempools[j].nrof_used, ob_free = mempools[j].nrof_free;

		new_draw_info_format(NDI_UNIQUE, 0, op, "%4d used %s:    %8d", ob_used, mempools[j].chunk_description, i = (ob_used * (mempools[j].chunksize + sizeof(struct mempool_chunk))));
		sum_used += i;
		sum_alloc += i;

		new_draw_info_format(NDI_UNIQUE, 0, op, "%4d free %s:    %8d", ob_free, mempools[j].chunk_description, i = (ob_free * (mempools[j].chunksize + sizeof(struct mempool_chunk))));
		sum_alloc += i;
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d active objects", count_active());

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d maps allocated:  %8d", nrofmaps, i = (nrofmaps * sizeof(mapstruct)));
	sum_alloc += i;
	sum_used += nrm * sizeof(mapstruct);

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d maps in memory:  %8d", nrm, mapmem);
	sum_alloc += mapmem;
	sum_used += mapmem;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d archetypes:      %8d", anr, i = (anr * sizeof(archetype)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d animations:      %8d", anims, i = (anims * sizeof(Fontindex)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d spells:          %8d", NROFREALSPELLS, i = (NROFREALSPELLS * sizeof(spell)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d treasurelists    %8d", tlnr, i = (tlnr * sizeof(treasurelist)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4ld treasures        %8d", nroftreasures, i = (nroftreasures * sizeof(treasure)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4ld artifacts        %8d", nrofartifacts, i = (nrofartifacts * sizeof(artifact)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4ld artifacts strngs %8d", nrofallowedstr, i = (nrofallowedstr * sizeof(linked_char)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "%4d artifactlists    %8d", alnr, i = (alnr * sizeof(artifactlist)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, 0, op, "Total space allocated:%8d", sum_alloc);
	new_draw_info_format(NDI_UNIQUE, 0, op, "Total space used:     %8d", sum_used);
}

/**
 * Give out some info about the map op is located at.
 * @param op The object requesting this information. */
void current_map_info(object *op)
{
	mapstruct *m = op->map;

	if (!m)
	{
		return;
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "%s (%s)", m->name, m->path);

	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "players: %d difficulty: %d size: %dx%d start: %dx%d", players_on_map(m), MAP_DIFFICULTY(m), MAP_WIDTH(m), MAP_HEIGHT(m), MAP_ENTER_X(m), MAP_ENTER_Y(m));
	}

	if (m->msg)
	{
		new_draw_info(NDI_UNIQUE, NDI_NAVY, op, m->msg);
	}
}

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
	{
		new_draw_info(NDI_UNIQUE, 0, op, " ");
	}

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (pl->dm_stealth && !QUERY_FLAG(op, FLAG_WIZ))
		{
			continue;
		}

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
			{
				sex = QUERY_FLAG(pl->ob, FLAG_IS_FEMALE) ? "hermaphrodite" : "male";
			}
			else if (QUERY_FLAG(pl->ob, FLAG_IS_FEMALE))
			{
				sex = "female";
			}

			if (op == NULL || QUERY_FLAG(op, FLAG_WIZ))
			{
				snprintf(buf, sizeof(buf), "%s the %s %s (@%s) [%s]%s%s (%d)", pl->ob->name, sex, pl->ob->race, pl->socket.host, pl->ob->map->path, QUERY_FLAG(pl->ob, FLAG_WIZ) ? " [WIZ]" : "", pl->afk ? " [AFK]" : "", pl->ob->count);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s the %s %s (lvl %d)%s%s", pl->ob->name, sex, pl->ob->race, pl->ob->level, QUERY_FLAG(pl->ob, FLAG_WIZ) ? " [WIZ]" : "", pl->afk ? " [AFK]" : "");
			}

			new_draw_info(NDI_UNIQUE, 0, op, buf);
		}
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "There %s %d player%s online (%d in login).", ip + il > 1 ? "are" : "is", ip + il, ip + il > 1 ? "s" : "", il);

	return 1;
}

/**
 * Malloc info command.
 *
 * If MEMPOOL_TRACKING is defined, parameters are used to free (and force
 * freeing) empty puddles. Otherwise, malloc_info() is used to display
 * information about memory usage.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
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
		{
			force_flag = 1;
		}

		for (i = 0; i < NROF_MEMPOOLS; i++)
		{
			if (force_flag == 1 || mempools[i].flags & MEMPOOL_ALLOW_FREEING)
			{
				free_empty_puddles(i);
			}
		}
	}
#else
	(void) params;
#endif

	malloc_info(op);
	return 1;
}

/**
 * Map info command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_mapinfo(object *op, char *params)
{
	(void) params;

	current_map_info(op);
	return 1;
}

/**
 * Maps command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_maps(object *op, char *params)
{
	(void) params;

	map_info(op);
	return 1;
}

/**
 * Strings command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
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
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_time(object *op, char *params)
{
	(void) params;

	time_info(op);
	return 1;
}

/**
 * Arches command. Print out information about the arches.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_archs(object *op, char *params)
{
	(void) params;

	arch_info(op);
	return 1;
}

/**
 * Highscore command, shows the highscore.
 * @param op Object requesting this.
 * @param params Parameters.
 * @return Always returns 1. */
int command_hiscore(object *op, char *params)
{
	display_high_score(op, op == NULL ? 9999 : 50, params);
	return 1;
}

/**
 * Set debug level command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_debug(object *op, char *params)
{
	int i;

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "Global debug level is %d.", settings.debug);
		return 1;
	}

	settings.debug = (enum LogLevel) FABS(i);
	new_draw_info_format(NDI_UNIQUE, 0, op, "Set debug level to %d.", i);
	return 1;
}

/**
 * Full dump of objects below the DM.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_dumpbelowfull(object *op, char *params)
{
	object *tmp;

	(void) params;

	new_draw_info(NDI_UNIQUE, 0, op, "DUMP OBJECTS OF THIS TILE");
	new_draw_info(NDI_UNIQUE, 0, op, "-------------------");

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above)
	{
		/* Exclude the DM player object */
		if (tmp == op)
		{
			continue;
		}

		dump_object(tmp);
		new_draw_info(NDI_UNIQUE, 0, op, errmsg);

		if (tmp->above && tmp->above != op)
		{
			new_draw_info(NDI_UNIQUE, 0, op, ">next object<");
		}
	}

	new_draw_info(NDI_UNIQUE, 0, op, "------------------");

	return 1;
}

/**
 * Dump of objects below the DM.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_dumpbelow(object *op, char *params)
{
	object *tmp;
	int i = 0;

	(void) params;

	new_draw_info(NDI_UNIQUE, 0, op, "DUMP OBJECTS OF THIS TILE");
	new_draw_info(NDI_UNIQUE, 0, op, "-------------------");

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above, i++)
	{
		/* Exclude the DM player object */
		if (tmp == op)
		{
			continue;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "#%d  >%s<  >%s<  >%s<", i, query_name(tmp, NULL), tmp->arch ? (tmp->arch->name ? tmp->arch->name : "no arch name") : "NO ARCH", tmp->env ? query_name(tmp->env, NULL) : "");
	}

	new_draw_info(NDI_UNIQUE, 0, op, "------------------");

	return 1;
}

/**
 * Wizpass command. Used by DMs to toggle walking through walls on/off.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1 on success, 0 on failure. */
int command_wizpass(object *op, char *params)
{
	int i;

	if (!op)
	{
		return 0;
	}

	if (!params)
	{
		i = (QUERY_FLAG(op, FLAG_WIZPASS)) ? 0 : 1;
	}
	else
	{
		i = onoff_value(params);
	}

	if (i)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You will now walk through walls.");
		SET_FLAG(op, FLAG_WIZPASS);
	}
	else
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You will now be stopped by walls.");
		CLEAR_FLAG(op, FLAG_WIZPASS);
	}

	return 1;
}

/**
 * Dump all objects.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_dumpallobjects(object *op, char *params)
{
	(void) params;
	(void) op;

	return 1;
}

/**
 * Dump all friendly objects.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_dumpfriendlyobjects(object *op, char *params)
{
	(void) params;
	(void) op;
	dump_friendly_objects();
	return 1;
}

/**
 * Dump all archetypes.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_dumpallarchetypes(object *op, char *params)
{
	(void) params;
	(void) op;
	dump_all_archetypes();
	return 1;
}

/**
 * DM stealth command. Used by DMs to make the DM hidden from all other
 * players. It also works when DM logs in without DM flag set or leaves
 * the DM mode.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
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
		{
			CONTR(op)->dm_stealth = 1;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "Toggled dm_stealth to %d.", CONTR(op)->dm_stealth);
	}

	return 1;
}

/**
 * Toggle DM light on/off. DM light will light up all maps for the DM.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_dm_light(object *op, char *params)
{
	(void) params;

	if (op->type == PLAYER && CONTR(op))
	{
		if (CONTR(op)->dm_light)
		{
			CONTR(op)->dm_light = 0;
		}
		else
		{
			CONTR(op)->dm_light = 1;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "Toggled dm_light to %d.", CONTR(op)->dm_light);
	}

	return 1;
}

/**
 * Dump active list.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
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
		LOG(llevSystem, "%s\n", buf);
	}

	snprintf(buf, sizeof(buf), "Active objects: %d (dumped to log)", count);
	new_draw_info(NDI_UNIQUE, 0, op, buf);
	LOG(llevSystem, "%s\n", buf);

	return 1;
}

/**
 * Start server shutdown command. Used by DM to shutdown the
 * server in order to rebuild it to update maps.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_start_shutdown(object *op, char *params)
{
	char *bp = NULL;
	int i = -2;

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "DM usage: /start_shutdown <-1 ... x>");
		return 1;
	}

	sscanf(params, "%d ", &i);

	if ((bp = strchr(params, ' ')) != NULL)
	{
		bp++;
	}

	if (bp && bp == 0)
	{
		bp = NULL;
	}

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
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1 on success, 0 on failure. */
int command_setmaplight(object *op, char *params)
{
	int i;

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		return 0;
	}

	set_map_darkness(op->map, i);

	new_draw_info_format(NDI_UNIQUE, 0, op, "WIZ: set map darkness: %d -> map:%s (%d)", i, op->map->path, MAP_OUTDOORS(op->map));

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
	{
		dump_map(op->map);
	}

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
	{
		print_los(op);
	}

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
 * @param line The input string.
 * @return 1 for ON, 0 for OFF. */
int onoff_value(char *line)
{
	int i;

	if (sscanf(line, "%d", &i))
	{
		return (i != 0);
	}

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
 * Print out to player if he has sounds enabled.
 * @param op Object requesting this.
 * @param params Command parameters.
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
 * @param op Object. */
void receive_player_name(object *op)
{
	unsigned int name_len = strlen(CONTR(op)->write_buf + 1);

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
 * @param op Object. */
void receive_player_password(object *op)
{
	unsigned int pwd_len = strlen(CONTR(op)->write_buf);

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
 *
 * Cannot be used on unholy ground or if you don't have any experience.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_save(object *op, char *params)
{
	(void) params;

	if (blocks_cleric(op->map, op->x, op->y))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can not save on unholy ground.");
	}
	else if (!op->stats.exp)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "To avoid too much unused player accounts you must get some experience before you can save!");
	}
	else
	{
		if (save_player(op, 1))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You have been saved.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE, 0, op, "SAVE FAILED!");
		}
	}

	return 1;
}

/**
 * Away from keyboard command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
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
