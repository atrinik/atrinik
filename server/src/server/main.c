/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Server main related functions. */

#include <global.h>
#include <check_proto.h>

#ifdef HAVE_DES_H
#	include <des.h>
#else
#	ifdef HAVE_CRYPT_H
#		include <crypt.h>
#	endif
#endif

#ifdef MEMPOOL_OBJECT_TRACKING
extern void check_use_object_list();
#endif

/** Object used in proccess_events(). */
static object marker;

static char *unclean_path(const char *src);
static void process_players1();
static void process_players2();
static void dequeue_path_requests();
static void do_specials();

/**
 * Meant to be called whenever a fatal signal is intercepted. It will
 * call the emergency_save() and the clean_tmp_files() functions.
 * @param err Error level. */
void fatal(int err)
{
	LOG(llevSystem, "Fatal: Shutdown server. Reason: %s\n", err == llevError ? "Fatal Error" : "BUG flood");

	if (init_done)
	{
		emergency_save(0);
		clean_tmp_files();
	}

	abort();
	LOG(llevSystem, "Exiting...\n");
	exit(-1);
}

/**
 * Shows version information.
 * @param op If NULL the version is logged using LOG(), otherwise it is
 * shown to the player object using new_draw_info_format(). */
void version(object *op)
{
	if (op)
	{
		new_draw_info_format(NDI_UNIQUE, op, "This is Atrinik v%s", VERSION);
	}
	else
	{
		LOG(llevInfo, "This is Atrinik v%s.\n", VERSION);
	}
}

/**
 * Encrypt a string. Used for password storage on disk.
 * @param str The string to crypt.
 * @param salt Salt, if NULL, random will be chosen.
 * @return The crypted string. */
char *crypt_string(char *str, char *salt)
{
#ifndef WIN32
	static const char *const c = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
	char s[2];

	if (salt == NULL)
	{
		size_t stringlen = strlen(c);

		s[0] = c[rndm(1, stringlen) - 1];
		s[1] = c[rndm(1, stringlen) - 1];
	}
	else
	{
		s[0] = salt[0];
		s[1] = salt[1];
	}

#	ifdef HAVE_LIBDES
	return (char *) des_crypt(str, s);
#	endif

	return (char *) crypt(str, s);
#endif

	return str;
}

/**
 * Check if typed password and crypted password in the player file are
 * the same.
 * @param typed The typed password.
 * @param crypted Crypted password from file.
 * @return 1 if the passwords match, 0 otherwise. */
int check_password(char *typed, char *crypted)
{
	return !strcmp(crypt_string(typed, crypted), crypted);
}

/**
 * This is a basic little function to put the player back to his
 * savebed. We do some error checking - it's possible that the
 * savebed map may no longer exist, so we make sure the player
 * goes someplace.
 * @param op The player object entering his savebed. */
void enter_player_savebed(object *op)
{
	mapstruct *oldmap = op->map;
	object *tmp = get_object();

	FREE_AND_COPY_HASH(EXIT_PATH(tmp), CONTR(op)->savebed_map);
	EXIT_X(tmp) = CONTR(op)->bed_x;
	EXIT_Y(tmp) = CONTR(op)->bed_y;
	enter_exit(op, tmp);

	/* If the player has not changed maps and the name does not match
	 * that of the savebed, his savebed map is gone.  Lets go back
	 * to the emergency path.  Update what the players savebed is
	 * while we're at it. */
	if (oldmap == op->map && strcmp(CONTR(op)->savebed_map, oldmap->path))
	{
		LOG(llevDebug, "Player %s savebed location %s is invalid - going to EMERGENCY_MAPPATH (%s)\n", query_name(op, NULL), CONTR(op)->savebed_map, EMERGENCY_MAPPATH);
		strcpy(CONTR(op)->savebed_map, EMERGENCY_MAPPATH);
		CONTR(op)->bed_x = EMERGENCY_X;
		CONTR(op)->bed_y = EMERGENCY_Y;
		FREE_AND_COPY_HASH(EXIT_PATH(tmp), CONTR(op)->savebed_map);
		EXIT_X(tmp) = CONTR(op)->bed_x;
		EXIT_Y(tmp) = CONTR(op)->bed_y;
		enter_exit(op, tmp);
	}
}

/**
 * All this really is is a glorified remove_object that also updates the
 * counts on the map if needed and sets map timeout if needed.
 * @param op The object leaving the map. */
void leave_map(object *op)
{
	mapstruct *oldmap = op->map;

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

	if (oldmap && !oldmap->player_first)
	{
		set_map_timeout(oldmap);
	}
}

/**
 * Moves the player from current map (if any) to new map.
 * map, x, y must be set.
 *
 * If default map coordinates are to be used, then the function that
 * calls this should figure them out.
 * @param op The object we are moving
 * @param newmap Map to move the object to - it could be the map he just
 * came from if the load failed for whatever reason.
 * @param x X position on the new map
 * @param y Y position on the new map
 * @param pos_flag If set, the function will not look for a free space
 * and move the object, even if the position is blocked. */
static void enter_map(object *op, mapstruct *newmap, int x, int y, int pos_flag)
{
	int i = 0;
	object *tmp;
	mapstruct *oldmap = op->map;

	if (op->head)
	{
		op = op->head;
		LOG(llevBug, "BUG: enter_map(): called from tail of object! (obj:%s map: %s (%d,%d))\n", op->name, newmap->path, x, y);
	}

	/* this is a last secure check. In fact, newmap MUST legal and we only
	 * check x and y. No get_map_from_coord() - we want check that x,y is part of this newmap.
	 * if not, we have somewhere missed some checks - give a note to the log. */
	if (OUT_OF_REAL_MAP(newmap, x, y))
	{
		LOG(llevBug, "BUG: enter_map(): supplied coordinates are not within the map! (obj:%s map: %s (%d,%d))\n", op->name, newmap->path, x, y);
		x = MAP_ENTER_X(newmap);
		y = MAP_ENTER_Y(newmap);
	}

	/* try to find a spot for our object - (single arch or multi head)
	 * but only when we don't put it on fix position  */
	if (!pos_flag && arch_blocked(op->arch, op, newmap, x, y))
	{
		/* First choice blocked */
		/* We try to find a spot for the player, starting closest in.
		 * We could use find_first_free_spot, but that doesn't randomize it at all,
		 * So for example, if the north space is free, you would always end up there even
		 * if other spaces around are available.
		 * Note that for the second and third calls, we could start at a position other
		 * than one, but then we could end up on the other side of walls and so forth. */
		i = find_free_spot(op->arch, NULL, newmap, x, y, 1, SIZEOFFREE1 + 1);

		if (i == -1)
		{
			i = find_free_spot(op->arch, NULL, newmap, x, y, 1, SIZEOFFREE2 + 1);

			if (i == -1)
			{
				i = find_free_spot(op->arch, NULL, newmap, x, y, 1, SIZEOFFREE + 1);
			}
		}

		if (i == -1)
		{
			i = 0;
		}
	}

	/* If it is a player login, he has yet to be inserted anyplace.
	 * Otherwise, we need to deal with removing the player here. */
	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		remove_ob(op);

		if (check_walk_off(op, NULL, MOVE_APPLY_DEFAULT) != CHECK_WALK_OK)
		{
			return;
		}
	}

	if (op->map && op->type == PLAYER && !op->head && MAP_PLUGINS(op->map))
	{
		/* Trigger the global MAPLEAVE event */
		trigger_global_event(EVENT_MAPLEAVE, op, NULL);
	}

	/* Set single or all part of a multi arch */
	for (tmp = op; tmp != NULL; tmp = tmp->more)
	{
		tmp->x = tmp->arch->clone.x + x + freearr_x[i];
		tmp->y = tmp->arch->clone.y + y + freearr_y[i];
	}

	if (!insert_ob_in_map(op, newmap, NULL, 0))
	{
		return;
	}

	if (MAP_PLUGINS(newmap))
	{
		/* Trigger the global MAPENTER event */
		trigger_global_event(EVENT_MAPENTER, op, NULL);
	}

	newmap->timeout = 0;

	/* Do some action special for players after we have inserted them */
	if (op->type == PLAYER)
	{
		if (CONTR(op))
		{
			strcpy(CONTR(op)->maplevel, newmap->path);
			CONTR(op)->count = 0;
		}

		op->direction = 0;

		/* If the player is changing maps, we need to do some special things
		 * Do this after the player is on the new map - otherwise the force swap of the
		 * old map does not work. */
		if (oldmap != newmap && oldmap && !oldmap->player_first)
		{
			set_map_timeout(oldmap);
		}
	}
}

/**
 * Sets map timeout value.
 * @param map The map to set the timeout for. */
void set_map_timeout(mapstruct *map)
{
#if MAP_DEFAULTTIMEOUT
	uint32 swap_time = MAP_SWAP_TIME(map);

	if (swap_time == 0)
	{
		swap_time = MAP_DEFAULTTIMEOUT;
	}

	if (swap_time >= MAP_MAXTIMEOUT)
	{
		swap_time = MAP_MAXTIMEOUT;
	}

	map->timeout = swap_time;
#else
	/* Save out the map. */
	swap_map(map, 0);
#endif
}

/**
 * Takes a path and replaces all / with $.
 *
 * We use strncpy so that we do not change the original string.
 * @param file Path to clean.
 * @return Cleaned up path. */
char *clean_path(const char *file)
{
	static char newpath[MAX_BUF], *cp;

	strncpy(newpath, file, MAX_BUF - 1);
	newpath[MAX_BUF - 1] = '\0';

	for (cp = newpath; *cp != '\0'; cp++)
	{
		if (*cp == '/')
		{
			*cp = '$';
		}
	}

	return newpath;
}

/**
 * Takes a path and replaces all $ with /.
 *
 * This basically undoes clean_path().
 *
 * We use strncpy so that we do not change the original string.
 *
 * We are smart enough to start after the last / in case we are getting
 * passed a string that points to a unique map path.
 * @param src The path to unclean.
 * @return Uncleaned up path. */
static char *unclean_path(const char *src)
{
	static char newpath[MAX_BUF], *cp2;
	const char *cp;

	cp = strrchr(src, '/');

	if (cp)
	{
		strncpy(newpath, cp + 1, MAX_BUF - 1);
	}
	else
	{
		strncpy(newpath, src, MAX_BUF - 1);
	}

	newpath[MAX_BUF - 1] = '\0';

	for (cp2 = newpath; *cp2 != '\0'; cp2++)
	{
		if (*cp2 == '$')
		{
			*cp2 = '/';
		}
	}

	return newpath;
}

/**
 * The player is trying to enter a randomly generated map. In this case,
 * generate the random map as needed.
 * @param pl The player object entering the map.
 * @param exit_ob Exit object the player entered from. */
static void enter_random_map(object *pl, object *exit_ob)
{
	mapstruct *new_map;
	char newmap_name[HUGE_BUF];
	static uint64 reference_number = 0;
	RMParms rp;

	memset(&rp, 0, sizeof(RMParms));
	rp.Xsize = -1;
	rp.Ysize = -1;

	if (exit_ob->msg)
	{
		set_random_map_variable(&rp, exit_ob->msg);
	}

	rp.origin_x = exit_ob->x;
	rp.origin_y = exit_ob->y;
	strcpy(rp.origin_map, pl->map->path);

	/* Pick a new pathname for the new map. Currently, we just use a
	 * static variable and increment the counter one each time. */
	snprintf(newmap_name, sizeof(newmap_name), "/random/%" FMT64U, reference_number++);

	/* Now to generate the actual map. */
	new_map = generate_random_map(newmap_name, &rp);

	/* Update the exit_ob so it now points directly at the newly created
	 * random map. */
	if (new_map)
	{
		int x, y;

		x = EXIT_X(exit_ob) = MAP_ENTER_X(new_map);
		y = EXIT_Y(exit_ob) = MAP_ENTER_Y(new_map);
		FREE_AND_COPY_HASH(EXIT_PATH(exit_ob), newmap_name);
		FREE_AND_COPY_HASH(new_map->path, newmap_name);
		enter_map(pl, new_map, x, y, QUERY_FLAG(exit_ob, FLAG_USE_FIX_POS));
	}
}

/**
 * Code to enter/detect a character entering a unique map.
 * @param op Player object entering the map
 * @param exit_ob Exit object the player is entering from */
static void enter_unique_map(object *op, object *exit_ob)
{
	char apartment[HUGE_BUF];
	mapstruct *newmap;

	/* Absolute path */
	if (EXIT_PATH(exit_ob)[0] == '/')
	{
		snprintf(apartment, sizeof(apartment), "%s/%s/%s/%s", settings.localdir, settings.playerdir, op->name, clean_path(EXIT_PATH(exit_ob)));
		newmap = ready_map_name(apartment, MAP_PLAYER_UNIQUE);

		if (!newmap)
		{
			newmap = load_original_map(create_pathname(EXIT_PATH(exit_ob)), MAP_PLAYER_UNIQUE);
		}
	}
	/* Relative path */
	else
	{
		char reldir[HUGE_BUF], tmpc[HUGE_BUF], tmp_path[HUGE_BUF], *cp;

		if (MAP_UNIQUE(exit_ob->map))
		{
			strncpy(reldir, unclean_path(exit_ob->map->path), sizeof(reldir) - 1);

			/* Need to copy this over, as clean_path only has one static return buffer */
			strncpy(tmpc, clean_path(reldir), sizeof(tmpc) - 1);

			/* Remove final component, if any */
			if ((cp = strrchr(tmpc, '$')) != NULL)
			{
				*cp = 0;
			}

			snprintf(apartment, sizeof(apartment), "%s/%s/%s/%s_%s", settings.localdir, settings.playerdir, op->name, tmpc, clean_path(EXIT_PATH(exit_ob)));
			newmap = ready_map_name(apartment, MAP_PLAYER_UNIQUE);

			if (!newmap)
			{
				newmap = load_original_map(create_pathname(normalize_path(reldir, EXIT_PATH(exit_ob), tmp_path)), MAP_PLAYER_UNIQUE);
			}
		}
		else
		{
			/* The exit is unique, but the map we are coming from is not unique. So
			 * use the basic logic - don't need to demangle the path name */
			snprintf(apartment, sizeof(apartment), "%s/%s/%s/%s", settings.localdir, settings.playerdir, op->name, clean_path(normalize_path(exit_ob->map->path, EXIT_PATH(exit_ob), tmp_path)));
			newmap = ready_map_name(apartment, MAP_PLAYER_UNIQUE);

			if (!newmap)
			{
				newmap = ready_map_name(normalize_path(exit_ob->map->path, EXIT_PATH(exit_ob), tmp_path), 0);
			}
		}
	}

	if (newmap)
	{
		FREE_AND_COPY_HASH(newmap->path, apartment);
		newmap->map_flags |= MAP_FLAG_UNIQUE;

		enter_map(op, newmap, EXIT_X(exit_ob), EXIT_Y(exit_ob), QUERY_FLAG(exit_ob, FLAG_USE_FIX_POS));
	}
	else
	{
		new_draw_info_format(NDI_UNIQUE, op, "The %s is closed.", query_name(exit_ob, NULL));
LOG(llevDebug, "DEBUG: enter_unique_map: Exit %s (%d,%d) on map %s leads no where.\n", query_name(exit_ob, NULL), exit_ob->x, exit_ob->y, exit_ob->map ? exit_ob->map->path ? exit_ob->map->path : "NO_PATH (script?)" : "NO_MAP (script?)");
	}
}

/**
 * Tries to move object to exit object.
 * @param op Player or monster object using the exit.
 * @param exit_ob Exit object (boat, exit, etc). If NULL, then
 * CONTR(op)->maplevel contains that map to move the object to, which is
 * used when loading the player object. */
void enter_exit(object *op, object *exit_ob)
{
	object *tmp;

	if (op->head)
	{
		op = op->head;
	}

	/* First, lets figure out what map we go */
	if (exit_ob)
	{
		/* check to see if we make a randomly generated map */
		if (EXIT_PATH(exit_ob) && EXIT_PATH(exit_ob)[1] == '!')
		{
			if (op->type != PLAYER)
			{
				return;
			}

			if (exit_ob->sub_type == ST1_EXIT_SOUND && exit_ob->map)
			{
				play_sound_map(exit_ob->map, CMD_SOUND_EFFECT, "teleport.ogg", exit_ob->x, exit_ob->y, 0, 0);
			}

			enter_random_map(op, exit_ob);
		}
		else if (exit_ob->last_eat == MAP_PLAYER_MAP)
		{
			if (op->type != PLAYER)
			{
				return;
			}

			if (exit_ob->sub_type == ST1_EXIT_SOUND && exit_ob->map)
			{
				play_sound_map(exit_ob->map, CMD_SOUND_EFFECT, "teleport.ogg", exit_ob->x, exit_ob->y, 0, 0);
			}

			enter_unique_map(op, exit_ob);
		}
		else
		{
			int x = EXIT_X(exit_ob), y = EXIT_Y(exit_ob);
			char tmp_path[HUGE_BUF];
			/* 'Normal' exits that do not do anything special
			 * Simple enough we don't need another routine for it. */
			mapstruct *newmap = NULL;

			if (exit_ob->map)
			{
				if (strcmp(EXIT_PATH(exit_ob), "/random/"))
				{
					if (strncmp(exit_ob->map->path, settings.localdir, strlen(settings.localdir)))
					{
						newmap = ready_map_name(normalize_path(exit_ob->map->path, EXIT_PATH(exit_ob), tmp_path), 0);
					}
					else
					{
						newmap = ready_map_name(normalize_path("", EXIT_PATH(exit_ob), tmp_path), 0);
					}
				}

				/* This is either a new random map to load, or we failed
				 * to load an old random map. */
				if (!newmap && !strncmp(EXIT_PATH(exit_ob), "/random/", 8))
				{
					if (op->type != PLAYER)
					{
						return;
					}

					/* Maps that go down have a message set. However, maps
					 * that go up, don't. If the going home has reset, there
					 * isn't much point generating a random map, because it
					 * won't match the maps, so just teleport the player
					 * to their savebed map. */
					if (exit_ob->msg)
					{
						if (exit_ob->sub_type == ST1_EXIT_SOUND && op->map)
						{
							play_sound_map(exit_ob->map, CMD_SOUND_EFFECT, "teleport.ogg", exit_ob->x, exit_ob->y, 0, 0);
						}

						enter_random_map(op, exit_ob);
					}
					else
					{
						enter_player_savebed(op);
					}

					/* For exits that cause damages (like pits).  Don't know if any
					 * random maps use this or not. */
					if (exit_ob->stats.dam && op->type == PLAYER)
					{
						hit_player(op, exit_ob->stats.dam, exit_ob, AT_INTERNAL);
					}

					return;
				}
			}
			else
			{
				/* For word of recall and other force objects
				 * They contain the full pathname of the map to go back to,
				 * so we don't need to normalize it.
				 * But we do need to see if it is unique or not  */
				if (!strncmp(EXIT_PATH(exit_ob), settings.localdir, strlen(settings.localdir)))
				{
					newmap = ready_map_name(EXIT_PATH(exit_ob), MAP_NAME_SHARED|MAP_PLAYER_UNIQUE);
				}
				else
				{
					newmap = ready_map_name(EXIT_PATH(exit_ob), MAP_NAME_SHARED);
				}
			}

			if (!newmap)
			{
				if (op->type == PLAYER)
				{
					new_draw_info_format(NDI_UNIQUE, op, "The %s is closed.", query_name(exit_ob, NULL));
				}

				return;
			}

			/* -1, -1 marks to use the default ENTER_xx position of the map */
			if (x == -1 && y == -1)
			{
				x = MAP_ENTER_X(newmap);
				y = MAP_ENTER_Y(newmap);
			}

			/* If exit is damned, update player's death and WoR home-position
			 * and delete town portal. */
			if (QUERY_FLAG(exit_ob, FLAG_DAMNED))
			{
				if (op->type != PLAYER)
				{
					return;
				}

				/* Remove an old force with a slaying field == PORTAL_DESTINATION_NAME */
				for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
				{
					if (tmp->type == FORCE && tmp->slaying && tmp->slaying == shstr_cons.portal_destination_name)
					{
						break;
					}
				}

				if (tmp)
				{
					remove_ob(tmp);
				}

				if (exit_ob->map)
				{
					strcpy(CONTR(op)->savebed_map, normalize_path(exit_ob->map->path, EXIT_PATH(exit_ob), tmp_path));
				}
				else
				{
					strcpy(CONTR(op)->savebed_map, EXIT_PATH(exit_ob));
				}

				CONTR(op)->bed_x = EXIT_X(exit_ob), CONTR(op)->bed_y = EXIT_Y(exit_ob);
				save_player(op, 1);
			}

			if (exit_ob->sub_type == ST1_EXIT_SOUND && exit_ob->map)
			{
				play_sound_map(exit_ob->map, CMD_SOUND_EFFECT, "teleport.ogg", exit_ob->x, exit_ob->y, 0, 0);
			}

			enter_map(op, newmap, x, y, QUERY_FLAG(exit_ob, FLAG_USE_FIX_POS));
		}

		/* For exits that cause damage (like pits) */
		if (exit_ob->stats.dam && op->type == PLAYER)
		{
			hit_player(op, exit_ob->stats.dam, exit_ob, AT_INTERNAL);
		}
	}
	else if (op->type == PLAYER)
	{
		int flags = 0;
		mapstruct *newmap;

		/* Hypothetically, I guess its possible that a standard map matches
		 * the localdir, but that seems pretty unlikely - unlikely enough that
		 * I'm not going to attempt to try to deal with that possibility.
		 * We use the fact that when a player saves on a unique map, it prepends
		 * the localdir to that name.  So its an easy way to see of the map is
		 * unique or not. */
		if (!strncmp(CONTR(op)->maplevel, settings.localdir, strlen(settings.localdir)))
		{
			flags = MAP_PLAYER_UNIQUE;
		}

		newmap = ready_map_name(CONTR(op)->maplevel, flags);

		if (!newmap)
		{
			if (strncmp(CONTR(op)->maplevel, "/random/", 8))
			{
				LOG(llevBug, "BUG: enter_exit(): Pathname to map does not exist! player: %s (%s)\n", op->name, CONTR(op)->maplevel);
				newmap = ready_map_name(EMERGENCY_MAPPATH, 0);
				op->x = EMERGENCY_X;
				op->y = EMERGENCY_Y;

				/* If we can't load the emergency map, something is probably
				 * really screwed up, so bail out now. */
				if (!newmap)
				{
					LOG(llevError, "ERROR: enter_exit(): could not load emergency map? Fatal error! (player: %s)\n", op->name);
				}
			}
			else
			{
				enter_player_savebed(op);
				return;
			}
		}

		/* -1,-1 marks to use the default ENTER_xx position of the map */
		if ((op->x == -1 && op->y == -1) || MAP_FIXEDLOGIN(newmap))
		{
			op->x = MAP_ENTER_X(newmap);
			op->y = MAP_ENTER_Y(newmap);
		}

		enter_map(op, newmap, op->x, op->y, 1);
	}
}

/**
 * Do all player-related stuff before objects have been updated.
 * @sa process_players2(). */
static void process_players1()
{
	player *pl, *plnext;
	int retval;

	for (pl = first_player; pl; pl = plnext)
	{
		plnext = pl->next;

		while ((retval = handle_newcs_player(pl)) == 1)
		{
		}

		if (retval == -1)
		{
			continue;
		}

		if (pl->followed_player[0])
		{
			player *followed = find_player(pl->followed_player);

			if (followed && followed->ob && followed->ob->map)
			{
				rv_vector rv;

				if (pl->ob->map != followed->ob->map || (get_rangevector(pl->ob, followed->ob, &rv, 0) && rv.distance > 4))
				{
					int space = find_free_spot(pl->ob->arch, pl->ob, followed->ob->map, followed->ob->x, followed->ob->y, 1, SIZEOFFREE2 + 1);

					if (space != -1 && followed->ob->x + freearr_x[space] >= 0 && followed->ob->y + freearr_y[space] >= 0 && followed->ob->x + freearr_x[space] < MAP_WIDTH(followed->ob->map) && followed->ob->y + freearr_y[space] < MAP_HEIGHT(followed->ob->map))
					{
						remove_ob(pl->ob);
						pl->ob->x = followed->ob->x + freearr_x[space];
						pl->ob->y = followed->ob->y + freearr_y[space];
						insert_ob_in_map(pl->ob, followed->ob->map, NULL, 0);
					}
				}
			}
			else
			{
				new_draw_info_format(NDI_UNIQUE | NDI_RED, pl->ob, "Player %s left.", pl->followed_player);
				pl->followed_player[0] = '\0';
			}
		}

		pl->ob->weapon_speed_left -= pl->ob->weapon_speed_add;

		/* Use the target system to hit our target - don't hit friendly
		 * objects, ourselves or when we are not in combat mode. */
		if (pl->target_object && pl->combat_mode && OBJECT_ACTIVE(pl->target_object) && pl->target_object_count != pl->ob->count && ((pl->target_object->type == PLAYER && pvp_area(pl->ob, pl->target_object)) || (pl->target_object->type == MONSTER && !QUERY_FLAG(pl->target_object, FLAG_FRIENDLY))))
		{
			if (pl->ob->weapon_speed_left <= 0)
			{
				/* Now we force target as enemy */
				pl->ob->enemy = pl->target_object;
				pl->ob->enemy_count = pl->target_object_count;

				if (!OBJECT_VALID(pl->ob->enemy, pl->ob->enemy_count) || pl->ob->enemy->owner == pl->ob)
				{
					pl->ob->enemy = NULL;
				}
				else if (is_melee_range(pl->ob, pl->ob->enemy))
				{
					if (!OBJECT_VALID(pl->ob->enemy->enemy, pl->ob->enemy->enemy_count))
					{
						set_npc_enemy(pl->ob->enemy, pl->ob, NULL);
					}
					/* Our target already has an enemy - then note we had attacked */
					else
					{
						pl->ob->enemy->attacked_by = pl->ob;
						pl->ob->enemy->attacked_by_distance = 1;
					}

					pl->praying = 0;
					skill_attack(pl->ob->enemy, pl->ob, 0, NULL);
					/* We want only *one* swing - not several swings per tick */
					pl->ob->weapon_speed_left += FABS((int) pl->ob->weapon_speed_left) + 1;
				}
			}
		}
		else
		{
			if (pl->ob->weapon_speed_left <= 0)
			{
				pl->ob->weapon_speed_left = 0;
			}
		}

		do_some_living(pl->ob);

#ifdef AUTOSAVE
		/* Check for ST_PLAYING state so that we don't try to save off when
		 * the player is logging in. */
		if ((pl->last_save_tick + AUTOSAVE) < pticks && pl->state == ST_PLAYING)
		{
			save_player(pl->ob, 1);
			pl->last_save_tick = pticks;
			hiscore_check(pl->ob, 1);
		}
#endif
	}
}

/**
 * Do all player-related stuff after objects have been updated.
 * @sa process_players1(). */
static void process_players2()
{
	player *pl;

	for (pl = first_player; pl; pl = pl->next)
	{
		/* Check if our target is still valid - if not, update client. */
		if (pl->ob->map && (!pl->target_object || (pl->target_object != pl->ob && pl->target_object_count != pl->target_object->count) || QUERY_FLAG(pl->target_object, FLAG_SYS_OBJECT) || (QUERY_FLAG(pl->target_object, FLAG_IS_INVISIBLE) && !QUERY_FLAG(pl->ob, FLAG_SEE_INVISIBLE))))
		{
			send_target_command(pl);
		}

		if (pl->ob->speed_left > pl->ob->speed)
		{
			pl->ob->speed_left = pl->ob->speed;
		}
	}
}

/**
 * Process objects with speed, like teleporters, players, etc.
 * @param map If not NULL, only process objects on that map. */
void process_events(mapstruct *map)
{
	object *op;
	tag_t tag;

	process_players1();

	/* Put marker object at beginning of active list */
	marker.active_next = active_objects;

	if (marker.active_next)
	{
		marker.active_next->active_prev = &marker;
	}

	marker.active_prev = NULL;
	active_objects = &marker;

	while (marker.active_next)
	{
		op = marker.active_next;
		tag = op->count;

		/* Move marker forward - swap op and marker */
		op->active_prev = marker.active_prev;

		if (op->active_prev)
		{
			op->active_prev->active_next = op;
		}
		else
		{
			active_objects = op;
		}

		marker.active_next = op->active_next;

		if (marker.active_next)
		{
			marker.active_next->active_prev = &marker;
		}

		marker.active_prev = op;
		op->active_next = &marker;

		/* Now process op */
		if (OBJECT_FREE(op))
		{
			LOG(llevBug, "BUG: process_events(): Free object on active list\n");
			op->speed = 0;
			update_ob_speed(op);
			continue;
		}

		if (QUERY_FLAG(op, FLAG_REMOVED))
		{
			op->speed = 0;
			update_ob_speed(op);
			continue;
		}

		if (!op->speed)
		{
			LOG(llevBug, "BUG: process_events(): Object %s (%s, type:%d count:%d) has no speed, but is on active list\n", op->arch->name, query_name(op, NULL), op->type, op->count);
			update_ob_speed(op);
			continue;
		}

		if (op->map == NULL && op->env == NULL && op->name && op->type != MAP && map == NULL)
		{
			if (op->type == PLAYER && CONTR(op)->state != ST_PLAYING)
			{
				continue;
			}

			LOG(llevBug, "BUG: process_events(): Object without map or inventory is on active list: %s (%d)\n", query_name(op, NULL), op->count);
			op->speed = 0;
			update_ob_speed(op);
			continue;
		}

		if (map && op->map != map)
		{
			continue;
		}

		/* As long we are > 0, we are not ready to swing. */
		if (op->weapon_speed_left > 0)
		{
			op->weapon_speed_left -= op->weapon_speed_add;
		}

		if (op->speed_left > 0)
		{
			--op->speed_left;
			process_object(op);

			if (was_destroyed(op, tag))
			{
				continue;
			}
		}

		/* Handle archetype-field anim_speed differently when it comes to
		 * the animation. If we have a value on this we don't animate it
		 * at speed-events. */
		if (QUERY_FLAG(op, FLAG_ANIMATE))
		{
			if (op->last_anim >= op->anim_speed)
			{
				animate_object(op, 1);

				/* Check for direction changing */
				if (op->type == PLAYER && NUM_FACINGS(op) >= 25)
				{
					if (op->anim_moving_dir != -1)
					{
						op->anim_last_facing = op->anim_moving_dir;
						op->anim_moving_dir = -1;
					}

					if (op->anim_enemy_dir != -1)
					{
						op->anim_last_facing = op->anim_enemy_dir;
						op->anim_enemy_dir = -1;
					}
				}

				op->last_anim = 1;
			}
			else
			{
				/* Check for direction changing */
				if (NUM_FACINGS(op) >= 25)
				{
					animate_object(op, 0);
				}

				op->last_anim++;
			}
		}

		if (op->speed_left <= 0)
		{
			op->speed_left += FABS(op->speed);
		}
	}

	/* Remove marker object from active list */
	if (marker.active_prev)
	{
		marker.active_prev->active_next = NULL;
	}
	else
	{
		active_objects = NULL;
	}

	process_players2();
}

/**
 * Clean temporary map files. */
void clean_tmp_files()
{
	mapstruct *m, *next;

	LOG(llevInfo, "Cleaning up...\n");

	/* We save the maps - it may not be intuitive why, but if there are
	 * unique items, we need to save the map so they get saved off. */
	for (m = first_map; m != NULL; m = next)
	{
		next = m->next;

		if (m->in_memory == MAP_IN_MEMORY)
		{
#if RECYCLE_TMP_MAPS
			swap_map(m, 0);
#else
			new_save_map(m, 0);
			clean_tmp_map(m);
#endif
		}
	}

	/* Write the clock */
	write_todclock();
}

/**
 * Clean up everything before exiting. */
void cleanup()
{
	LOG(llevDebug, "Cleanup called. Freeing data.\n");
	clean_tmp_files();
#if MEMORY_DEBUG
	free_all_maps();
	free_style_maps();
	free_all_object_data();
	free_all_archs();
	free_all_treasures();
	free_all_images();
	free_all_newserver();
	free_all_recipes();
	free_all_readable();
	free_all_god();
	free_all_anim();
	free_strings();
	race_free();
	free_exp_objects();
	free_srv_files();
	free_regions();
	free_mempools();
	remove_plugins();
#endif
	exit(0);
}

/**
 * Dequeue path requests.
 * @todo Only compute time if there is something more in the queue,
 * something like if (path_request_queue_empty()) { break; } */
static void dequeue_path_requests()
{
#ifdef LEFTOVER_CPU_FOR_PATHFINDING
	static struct timeval new_time;
	long leftover_sec, leftover_usec;
	object *wp;

	while ((wp = get_next_requested_path()))
	{
		waypoint_compute_path(wp);

		(void) GETTIMEOFDAY(&new_time);

		leftover_sec = last_time.tv_sec - new_time.tv_sec;
		leftover_usec = max_time - (new_time.tv_usec - last_time.tv_usec);

		/* This is very ugly, but probably the fastest for our use: */
		while (leftover_usec < 0)
		{
			leftover_usec += 1000000;
			leftover_sec -= 1;
		}
		while (leftover_usec > 1000000)
		{
			leftover_usec -= 1000000;
			leftover_sec +=1;
		}

		/* Try to save about 10 ms */
		if (leftover_sec < 1 && leftover_usec < 10000)
		{
			break;
		}
	}
#else
	object *wp = get_next_requested_path();

	if (wp)
	{
		waypoint_compute_path(wp);
	}
#endif
}

/**
 * Swap one apartment (unique) map for another.
 * @param mapold Old map path.
 * @param mapnew Map to switch for.
 * @param x X position where player's items from old map will go to.
 * @param y Y position where player's items from old map will go to.
 * @param op Player we're doing the switching for.
 * @return 1 on success, 0 on failure. */
int swap_apartments(char *mapold, char *mapnew, int x, int y, object *op)
{
	char oldmappath[HUGE_BUF], newmappath[HUGE_BUF];
	int i, j;
	object *ob, *tmp, *tmp2, *dummy;
	mapstruct *oldmap, *newmap;

	snprintf(oldmappath, sizeof(oldmappath), "%s/%s/%s/%s", settings.localdir, settings.playerdir, op->name, clean_path(mapold));
	snprintf(newmappath, sizeof(newmappath), "%s/%s/%s/%s", settings.localdir, settings.playerdir, op->name, clean_path(mapnew));

	/* So we can transfer our items from the old apartment. */
	oldmap = ready_map_name(oldmappath, 2);

	if (!oldmap)
	{
		LOG(llevBug, "BUG: swap_apartments(): Could not get oldmap using ready_map_name().\n");
		return 0;
	}

	/* Our new map. */
	newmap = ready_map_name(create_pathname(mapnew), 6);

	if (!newmap)
	{
		LOG(llevBug, "BUG: swap_apartments(): Could not get newmap using ready_map_name().\n");
		return 0;
	}

	/* Goes to player directory. */
	FREE_AND_COPY_HASH(newmap->path, newmappath);
	newmap->map_flags |= MAP_FLAG_UNIQUE;

	/* Go through every square on old apartment map, looking for things
	 * to transfer. */
	for (i = 0; i < MAP_WIDTH(oldmap); i++)
	{
		for (j = 0; j < MAP_HEIGHT(oldmap); j++)
		{
			for (ob = get_map_ob(oldmap, i, j); ob; ob = tmp2)
			{
				tmp2 = ob->above;

				/* We teleport any possible players here to emergency map. */
				if (ob->type == PLAYER)
				{
					dummy = get_object();
					dummy->map = ob->map;
					FREE_AND_COPY_HASH(EXIT_PATH(dummy), EMERGENCY_MAPPATH);
					FREE_AND_COPY_HASH(dummy->name, EMERGENCY_MAPPATH);
					enter_exit(ob, dummy);
					continue;
				}

				/* If it's sys_object 1, there's no need to transfer it. */
				if (QUERY_FLAG(ob, FLAG_SYS_OBJECT))
				{
					continue;
				}

				/* A pickable item... Tranfer it */
				if (!QUERY_FLAG(ob, FLAG_NO_PICK))
				{
					remove_ob(ob);
					ob->x = x;
					ob->y = y;
					insert_ob_in_map(ob, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
				}
				/* Fixed part of map */
				else
				{
					/* Now we test for containers, because player
					 * can have items stored in it. So, go through
					 * the container and look for things to transfer. */
					for (tmp = ob->inv; tmp; tmp = tmp2)
					{
						tmp2 = tmp->below;

						if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_NO_PICK))
						{
							continue;
						}

						remove_ob(tmp);
						tmp->x = x;
						tmp->y = y;
						insert_ob_in_map(tmp, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
					}
				}
			}
		}
	}

	/* Save the map */
	new_save_map(newmap, 0);

	/* Check for old save bed */
	if (strcmp(oldmap->path, CONTR(op)->savebed_map) == 0)
	{
		strcpy(CONTR(op)->savebed_map, EMERGENCY_MAPPATH);
		CONTR(op)->bed_x = EMERGENCY_X;
		CONTR(op)->bed_y = EMERGENCY_Y;
	}

	unlink(oldmap->path);

	/* Free the maps */
	free_map(newmap, 1);
	free_map(oldmap, 1);

	return 1;
}

/**
 * Collection of functions to call from time to time. */
static void do_specials()
{
	if (!(pticks % 2))
	{
		dequeue_path_requests();
	}

	/* If watchdog is enabled */
	if (settings.watchdog)
	{
		if (!(pticks % 503))
		{
			watchdog();
		}
	}

	if (!(pticks % PTICKS_PER_CLOCK))
	{
		tick_the_clock();
	}

	/* Clears the tmp-files of maps which have reset */
	if (!(pticks % 509))
	{
		flush_old_maps();
	}

	if (settings.meta_on && !(pticks % 2521))
	{
		metaserver_info_update();
	}
}

/**
 * Check if key was pressed in the interactive mode.
 * @return 0 if no key was pressed, non zero otherwise */
static int keyboard_press()
{
#ifndef WIN32
	struct timeval tv = {0L, 0L};
	fd_set fds;

	FD_SET(0, &fds);

	return select(1, &fds, NULL, NULL, &tv);
#endif

	return 0;
}

/**
 * Process keyboard input by the interactive mode
 * @param input The keyboard input */
static void process_keyboard_input(char *input)
{
	player *pl;

	/* Show help */
	if (strncmp(input, "help", 4) == 0)
	{
		LOG(llevInfo, "\nAtrinik Interactive Server Interface Help\n");
		LOG(llevInfo, "The Atrinik Interface Server Interface is used to allow server administrators\nto easily maintain their servers if ran from terminal.\nThe following commands are available:\n\n");
		LOG(llevInfo, "help: Display this help.\n");
		LOG(llevInfo, "list: Display logged in players and their IPs.\n");
		LOG(llevInfo, "kill <player name or IP>: Kill player's socket or all players on specified IP.\n");
		LOG(llevInfo, "system <message>: Send system message in green to all players.\n");
	}
	/* Show list of players online */
	else if (strncmp(input, "list", 4) == 0)
	{
		int count = 0;

		/* Loop through the players, if the player is playing, show
		 * their name, otherwise show "(not playing)". */
		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			if (pl->state == ST_PLAYING)
			{
				LOG(llevInfo, "%s (%s)\n", pl->ob->name, pl->socket.host);
			}
			else
			{
				LOG(llevInfo, "(not playing) %s\n", pl->socket.host);
			}

			count++;
		}

		/* Show count of players online */
		if (count)
		{
			LOG(llevInfo, "\nTotal of %d players online.\n", count);
		}
		else
		{
			LOG(llevInfo, "Currently, there are no players online.\n");
		}
	}
	/* Kill a player's socket. If IP was presented, kill all players on that IP. */
	else if (strncmp(input, "kill ", 5) == 0)
	{
		int success = 0;

		input += 5;

		/* Loop through the players */
		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			/* If name matches, kill this player's socket, and break out */
			if (strcasecmp(pl->ob->name, input) == 0)
			{
				success = 1;

				pl->socket.status = Ns_Dead;
				LOG(llevInfo, "Killed socket for player %s successfully.\n", pl->ob->name);

				break;
			}
			/* Otherwise if player's IP matches, kill this player's socket,
			 * but do not break out, since there may be more players on this IP.*/
			else if (strcmp(pl->socket.host, input) == 0)
			{
				success = 1;

				pl->socket.status = Ns_Dead;
				LOG(llevInfo, "Killed socket for IP %s (%s) successfully.\n", pl->socket.host, pl->ob->name);
			}
		}

		if (!success)
		{
			LOG(llevInfo, "Failed to find player/IP: '%s'\n", input);
		}
	}
	/* Send a system message */
	else if (strncmp(input, "system ", 7) == 0)
	{
		input += 7;

		input = cleanup_chat_string(input);

		LOG(llevInfo, "CLOG SYSTEM: %s\n", input);
		new_draw_info_format(NDI_UNIQUE | NDI_ALL | NDI_GREEN | NDI_PLAYER, NULL, "[System]: %s", input);
	}
	/* Ban command */
	else if (strncmp(input, "ban ", 4) == 0)
	{
		input += 4;

		/* Add a new ban */
		if (strncmp(input, "add ", 4) == 0)
		{
			input += 4;

			if (add_ban(input))
			{
				LOG(llevInfo, "Added new ban successfully.\n");
			}
			else
			{
				LOG(llevInfo, "Failed to add new ban!\n");
			}
		}
		/* Remove ban */
		else if (strncmp(input, "remove ", 7) == 0)
		{
			input += 7;

			if (remove_ban(input))
			{
				LOG(llevInfo, "Removed ban successfully.\n");
			}
			else
			{
				LOG(llevInfo, "Failed to remove ban!\n");
			}
		}
		/* List bans */
		else if (strncmp(input, "list", 4) == 0)
		{
			list_bans(NULL);
		}
	}
	/* Unknown command */
	else
	{
		LOG(llevInfo, "Unknown interactive server command: %s\nTry 'help' for available commands.\n", input);
	}
}

/**
 * Iterate the main loop. */
static void iterate_main_loop()
{
	nroferrors = 0;

	/* Check and run a shutdown count (with messages and shutdown) */
	shutdown_agent(-1, NULL);

	doeric_server();

#ifdef MEMPOOL_OBJECT_TRACKING
	check_use_object_list();
#endif

	/* Global round ticker. */
	global_round_tag++;

	/* "do" something with objects with speed */
	process_events(NULL);

	/* Process the timers */
	cftimer_process_timers();

#ifdef PLUGINS_X
	/* Trigger the global CLOCK event */
	trigger_global_event(EVENT_CLOCK, NULL, NULL);
#endif

	/* Removes unused maps after a certain timeout */
	check_active_maps();

	/* Routines called from time to time. */
	do_specials();

	doeric_server_write();

	/* Clean up the object pool */
	object_gc();

	/* Sleep proper amount of time before next tick */
	sleep_delta();
}

/**
 * The main function.
 * @param argc Number of arguments.
 * @param argv Arguments.
 * @return 0. */
int main(int argc, char **argv)
{
	char input[HUGE_BUF];

#ifdef WIN32 /* ---win32 this sets the win32 from 0d0a to 0a handling */
	_fmode = _O_BINARY;
#endif

	init(argc, argv);
	init_plugins();
	compile_info();

#ifdef HAVE_CHECK
	/* Now that we have everything loaded, we can run unit tests. */
	if (settings.unit_tests)
	{
		LOG(llevInfo, "\nRunning unit tests...\n");
		check_main();
		exit(0);
	}
#endif

	memset(&marker, 0, sizeof(struct obj));

	LOG(llevInfo, "Server ready.\nWaiting for connections...\n");

	if (settings.interactive)
	{
		for (; ;)
		{
			/* Do all the necessary functions as long as keyboard input was not entered */
			while (!keyboard_press())
			{
				iterate_main_loop();
			}

			/* Otherwise we've got some keyboard input, parse it */
			if (scanf("\n%4096[^\n]", input))
			{
				process_keyboard_input(input);
			}
		}
	}
	else
	{
		for (; ;)
		{
			iterate_main_loop();
		}
	}

	return 0;
}
