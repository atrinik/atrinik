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
#ifndef WIN32 /* ---win32 remove headers */
#include <pwd.h>
#endif
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <sounds.h>
#include <newclient.h>

/**
 * @file
 * Player related functions. */

/* i left find_arrow - find_arrow() and find_arrow()_ext should merge
 * when the server sided range mode is removed at last from source */
static object *find_arrow_ext(object *op, const char *type, int tag);

/**
 * Loop through the player list and find player specified by plname.
 * @param plname The player name to find
 * @return Player structure if found, NULL if not */
player *find_player(char *plname)
{
	player *pl;
	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (pl->ob != NULL && !QUERY_FLAG(pl->ob, FLAG_REMOVED) && !strcmp(query_name(pl->ob, NULL), plname))
			return pl;
	}

	return NULL;
}

/**
 * Grab the Message of the Day from the database. Message of the Day
 * row is called 'motd', however, if custom 'motd_custom' is set,
 * prefer that one, and print it to the player.
 * @param op Player object to print the message to */
void display_motd(object *op)
{
	sqlite3 *db;
	sqlite3_stmt *statement;
	char buf[HUGE_BUF];

	buf[0] = '\0';

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the query to select either 'motd' or 'motd_custom' */
	if (!db_prepare(db, "SELECT name, data FROM settings WHERE name = 'motd' OR name = 'motd_custom';", &statement))
	{
		LOG(llevBug, "BUG: Failed to prepare SQL query to select MotD! (%s)\n", db_errmsg(db));
		db_close(db);
		return;
	}

	/* Loop through the rows (should be only 2) */
	while (db_step(statement) == SQLITE_ROW)
	{
		/* We store the database text in buf */
		snprintf(buf, sizeof(buf), "%s", (char *)db_column_text(statement, 1));

		/* If this is custom MotD, break out now. */
		if (strcmp((char *)db_column_text(statement, 0), "motd_custom") == 0)
			break;
	}

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	/* Output the buf */
	if (strcmp(buf, ""))
	{
		new_draw_info(NDI_UNIQUE, 0, op, buf);
	}
}

/**
 * Return is player name is ok, or not.
 * @param cp The player name
 * @return 1 if it is ok, 0 if not */
int playername_ok(char *cp)
{
	for (; *cp != '\0'; cp++)
		if (!((*cp >= 'a' && *cp <= 'z') || (*cp >= 'A' && *cp <= 'Z')) && *cp != '-' && *cp != '_')
			return 0;

	return 1;
}

/**
 * Returns the player structure. If 'p' is null,
 * we create a new one. Otherwise, we recycle
 * the one that is passed.
 * @param p Player structure to recycle or NULL
 * for new structure.
 * @return The player structure */
static player *get_player(player *p)
{
	object *op = arch_to_object(get_player_archetype(NULL));
	int i;

	if (!p)
	{
		player *tmp;

		p = (player *) get_poolchunk(POOL_PLAYER);
		memset(p, 0, sizeof(player));
		if (p == NULL)
			LOG(llevError, "ERROR: get_player(): out of memory\n");

		/* This adds the player in the linked list.  There is extra
		 * complexity here because we want to add the new player at the
		 * end of the list - there is in fact no compelling reason that
		 * that needs to be done except for things like output of
		 * 'who'. */
		tmp = first_player;
		while (tmp != NULL && tmp->next != NULL)
			tmp = tmp->next;

		if (tmp != NULL)
			tmp->next=p;
		else
			first_player = p;

		p->next = NULL;
	}
	else
	{
		/* Clears basically the entire player structure except
		 * for next and socket. */
		memset((void*)((char*)p + offsetof(player, maplevel)), 0, sizeof(player) - offsetof(player, maplevel));
	}

	/* There are some elements we want initialized to non zero value -
	 * we deal with that below this point. */
	p->party_number = -1;

#ifdef AUTOSAVE
	p->last_save_tick = 9999999;
#endif

	/* Init. respawn position */
	strcpy(p->savebed_map, first_map_path);

	p->firemode_type = p->firemode_tag1 = p->firemode_tag2 = -1;
	/* this is where we set up initial CONTR(op) */
	op->custom_attrset = p;
	p->ob = op;
	op->speed_left = 0.5;
	op->speed = 1.0;
	/* So player faces south */
	op->direction = 5;
	/* i let it in but there is no use atm for run_away and player */
	/* Then we panick... */
	op->run_away = 0;
	op->quickslot = 0;

	p->state = ST_ROLL_STAT;

	p->target_hp = -1;
	p->target_hp_p = -1;
	p->gen_sp_armour = 0;
	p->last_speed = -1;
	p->shoottype = range_none;
	p->listening = 9;
	p->last_weapon_sp = -1;
	p->update_los = 1;

	FREE_AND_COPY_HASH(op->race, op->arch->clone.race);

	/* Would be better of '0' was not a defined spell */
	for (i = 0; i < NROFREALSPELLS; i++)
		p->known_spells[i] = -1;

	p->chosen_spell = -1;
	CLEAR_FLAG(op, FLAG_READY_SKILL);

	/* we need to clear these to -1 and not zero - otherwise,
	 * if a player quits and starts a new character, we wont
	 * send new values to the client, as things like exp start
	 * at zero. */
	for (i = 0; i < MAX_EXP_CAT; i++)
	{
		p->last_skill_exp[i] = -1;
		p->last_skill_level[i] = -1;
	}

	/* quick skill reminder for select hand weapon */
	p->set_skill_weapon = NO_SKILL_READY;
	p->set_skill_archery = NO_SKILL_READY;
	p->last_skill_index = -1;
	p->last_stats.exp = -1;

	return p;
}

/**
 * Free a player structure. Takes care of removing
 * this player from the list of players, and frees
 * the socket for this player.
 * @param pl The player structure to free */
void free_player(player *pl)
{
	/* Remove from list of players */
	if (first_player != pl)
	{
		player *prev = first_player;
		while (prev != NULL && prev->next != NULL && prev->next != pl)
			prev = prev->next;

		if (prev->next != pl)
			LOG(llevError, "ERROR: free_player(): Can't find previous player.\n");

		prev->next = pl->next;
	}
	else
		first_player = pl->next;

	/* the inventory delete was before in save_player()... bad bad bad */
	if (pl->ob != NULL)
	{
		SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
		if (!QUERY_FLAG(pl->ob, FLAG_REMOVED))
		{
			remove_ob(pl->ob);
			check_walk_off(pl->ob, NULL, MOVE_APPLY_VANISHED);
		}
	}

	free_newsocket(&pl->socket);
}

/**
 * Tries to add a player on the connection passwd in ns.
 * All we can really get in this is some settings like
 * host and display mode.
 * @param ns The socket of this player.
 * @return 1 on failure (banned host?), 0 on success. */
int add_player(NewSocket *ns)
{
	player *p;
	char *defname = "nobody";

	/* Check for banned players and sites.  usename is no longer accurate,
	 * (can't get it over sockets), so all we really have to go on is
	 * the host. */

	if (checkbanned(defname, ns->host))
	{
		char buf[256];
		strcpy(buf, "X3 Connection refused.\nYou are banned!");
		Write_String_To_Socket(ns, BINARY_CMD_DRAWINFO, buf,strlen(buf));
		LOG(llevInfo, "Banned player tried to add. [%s@%s]\n", defname, ns->host);
		return 1;
	}

	p = get_player(NULL);
	memcpy(&p->socket, ns, sizeof(NewSocket));
	/* Needed because the socket we just copied over needs to be cleared.
	 * Note that this can result in a client reset if there is partial data
	 * on the uncoming socket. */

	/* now, we start the login procedure! */
	p->socket.status = Ns_Login;
	p->socket.below_clear = 0;
	p->socket.update_tile = 0;
	p->socket.look_position = 0;
	p->socket.inbuf.len = 0;

	get_name(p->ob);

	/* Avoid gc of the player */
	insert_ob_in_ob(p->ob, &void_container);

	return 0;
}

/**
 * Returns the next player archetype from archetype
 * list. Not very efficient routine, but used only
 * when creating new players.
 * @note There MUST be at least one player archetype!
 * @param at The archetype list
 * @return The archetype, if not found, fatal error. */
archetype *get_player_archetype(archetype *at)
{
	archetype *start = at;

	for (; ;)
	{
		if (at == NULL || at->next == NULL)
			at = first_archetype;
		else
			at = at->next;

		if (at->clone.type == PLAYER)
			return at;

		if (at == start)
		{
			LOG(llevError, "ERROR: No player achetypes\n");
			exit(-1);
		}
	}
}

/**
 * Get nearest friendly object to object mon. This function will also
 * check if the object is in line of sight.
 * @param mon Monster object
 * @return Object if in range to monster, NULL if no object in range. */
object *get_nearest_player(object *mon)
{
	object *op = NULL;
	objectlink *ol;
	unsigned int lastdist, aggro_range, aggro_stealth;
	rv_vector rv;

	/* lets set our aggro range. If mob is sleeping or blinded - half aggro range.
	 * if target has stealth - sub. -2  */
	aggro_range = mon->stats.Wis;
	if (mon->enemy || mon->attacked_by)
		aggro_range += 3;

	if (QUERY_FLAG(mon, FLAG_SLEEP) || QUERY_FLAG(mon, FLAG_BLIND))
	{
		aggro_range /= 2;
		aggro_stealth = aggro_range - 2;
	}
	else
	{
		aggro_stealth = aggro_range - 2;
	}

	if (aggro_stealth < MIN_MON_RADIUS)
		aggro_stealth = MIN_MON_RADIUS;

	for (ol = first_friendly_object, lastdist = 1000; ol != NULL; ol = ol->next)
	{
		/* We should not find free objects on this friendly list, but it
		 * does periodically happen.  Given that, lets deal with it.
		 * While unlikely, it is possible the next object on the friendly
		 * list is also free, so encapsulate this in a while loop. */
		while (!OBJECT_VALID(ol->ob, ol->id) || (!QUERY_FLAG(ol->ob, FLAG_FRIENDLY) && ol->ob->type != PLAYER))
		{
			object *tmp = ol->ob;

			/* Can't do much more other than log the fact, because the object
			 * itself will have been cleared. */
			LOG(llevDebug, "DEBUG: get_nearest_player: Found free/non friendly object on friendly list (%s)\n", STRING_OBJ_NAME(tmp));
			ol = ol->next;
			remove_friendly_object(tmp);
			if (!ol)
				return op;
		}

		if (!can_detect_target(mon, ol->ob, aggro_range, aggro_stealth, &rv) || !obj_in_line_of_sight(ol->ob, &rv))
			continue;

		if (lastdist > rv.distance)
		{
			op = ol->ob;
			lastdist = rv.distance;
		}
	}

#if 0
	LOG(llevDebug, "DEBUG: get_nearest_player() mob %s (%x) finds friendly obj: %s (%x) aggro range: %d\n", query_name(mon, NULL), mon->count, query_name(op, NULL), op ? op->count : -1, mon->stats.Wis);
#endif
	return op;
}

/** I believe this can safely go to 2, 3 is questionable, 4 will likely
 * result in a monster paths backtracking.  It basically determines how large a
 * detour a monster will take from the direction path when looking
 * for a path to the player.  The values are in the amount of direction
 * the deviation is */
#define DETOUR_AMOUNT	2

/** This is used to prevent infinite loops.  Consider a case where the
 * player is in a chamber (with gate closed), and monsters are outside.
 * with DETOUR_AMOUNT==2, the function will turn each corner, trying to
 * find a path into the chamber.  This is a good thing, but since there
 * is no real path, it will just keep circling the chamber for
 * ever (this could be a nice effect for monsters, but not for the function
 * to get stuck in.  I think for the monsters, if max is reached and
 * we return the first direction the creature could move would result in the
 * circling behaviour.  Unfortunately, this function is also used to determined
 * if the creature should cast a spell, so returning a direction in that case
 * is probably not a good thing. */
#define MAX_SPACES	50

/**
 * Returns the direction to the player, if valid.  Returns 0 otherwise.
 * Modified to verify there is a path to the player.  Does this by stepping towards
 * player and if path is blocked then see if blockage is close enough to player that
 * direction to player is changed (ie zig or zag).  Continue zig zag until either
 * reach player or path is blocked.  Thus, will only return true if there is a free
 * path to player.  Though path may not be a straight line. Note that it will find
 * player hiding along a corridor at right angles to the corridor with the monster.\n
 * Modified by MSW 2001-08-06 to handle tiled maps. Various notes:\n \n
 * 1) With DETOUR_AMOUNT being 2, it should still go and find players hiding
 * down corriders.\n
 * 2) I think the old code was broken if the first direction the monster
 * should move was blocked - the code would store the first direction without
 * verifying that the player can actually move in that direction.  The new
 * code does not store anything in firstdir until we have verified that the
 * monster can in fact move one space in that direction.\n
 * 3) I'm not sure how good this code will be for moving multipart monsters,
 * since only simple checks to blocked are being called, which could mean the monster
 * is blocking itself.
 * @param mon The monsters object
 * @param pl Player object
 * @param mindiff Min distance
 * @return The direction towards the player, 0 if no direction.
 * @todo This should really use pathfinding instead. */
int path_to_player(object *mon, object *pl, int mindiff)
{
	rv_vector rv;
	int	x, y, lastx, lasty, dir, i, diff, firstdir = 0,lastdir, max = MAX_SPACES;
	mapstruct *m ,*lastmap;

	get_rangevector(mon, pl, &rv, 0);

	if ((int) rv.distance < mindiff)
		return 0;

	x = mon->x;
	y = mon->y;
	m = mon->map;
	dir = rv.direction;
	/* perhaps we stand next to pl, init firstdir too */
	lastdir = firstdir = rv.direction;
	diff = FABS(rv.distance_x) > FABS(rv.distance_y) ? FABS(rv.distance_x) : FABS(rv.distance_y);
	/* If we can't solve it within the search distance, return now. */
	if (diff > max)
		return 0;

	while (diff >1 && max>0)
	{
		lastx = x;
		lasty = y;
		lastmap = m;
		x = lastx + freearr_x[dir];
		y = lasty + freearr_y[dir];

		/* Space is blocked - try changing direction a little */
		/* arch blocked controls multi arch with full map flags */
		if (arch_blocked(mon->arch, mon, m, x, y))
		{
			/* recalculate direction from last good location.  Possible
			 * we were not traversing ideal location before. */
			get_rangevector_from_mapcoords(lastmap, lastx, lasty, pl->map, pl->x, pl->y, &rv, 0);
			if (rv.direction != dir)
			{
				/* OK - says direction should be different - lets reset the
				 * the values so it will try again. */
				x = lastx;
				y = lasty;
				m = lastmap;
				dir = firstdir = rv.direction;
			}
			else
			{
				/* direct path is blocked - try taking a side step to
				 * either the left or right.
				 * Note increase the values in the loop below to be
				 * more than -1/1 respectively will mean the monster takes
				 * bigger detour.  Have to be careful about these values getting
				 * too big (3 or maybe 4 or higher) as the monster may just try
				 * stepping back and forth */
				for (i = -DETOUR_AMOUNT; i <= DETOUR_AMOUNT; i++)
				{
					/* already did this, so skip it */
					if (i == 0)
						continue;

					/* Use lastdir here - otherwise,
					 * since the direction that the creature should move in
					 * may change, you could get infinite loops.
					 * ie, player is northwest, but monster can only
					 * move west, so it does that.  It goes some distance,
					 * gets blocked, finds that it should move north,
					 * can't do that, but now finds it can move east, and
					 * gets back to its original point.  lastdir contains
					 * the last direction the creature has successfully
					 * moved. */

					x = lastx + freearr_x[absdir(lastdir + i)];
					y = lasty + freearr_y[absdir(lastdir + i)];
					m = lastmap;

					if (!arch_blocked(mon->arch, mon, m, x, y))
						break;
				}
				/* go through entire loop without finding a valid
				 * sidestep to take - thus, no valid path. */
				if (i == (DETOUR_AMOUNT + 1))
					return 0;

				diff--;
				lastdir=dir;
				max--;
				if (!firstdir)
					firstdir = dir + i;
			}
		}
		else
		{
			/* we moved towards creature, so diff is less */
			diff--;
			max--;
			lastdir = dir;

			if (!firstdir)
				firstdir = dir;
		}

		if (diff <= 1)
		{
			/* Recalculate diff (distance) because we may not have actually
			 * headed toward player for entire distance. */
			get_rangevector_from_mapcoords(m, x, y, pl->map, pl->x, pl->y, &rv, 0);
			diff = FABS(rv.distance_x) > FABS(rv.distance_y) ? FABS(rv.distance_x) : FABS(rv.distance_y);
		}

		if (diff > max)
			return 0;
	}
	/* If we reached the max, didn't find a direction in time */
	if (!max)
		return 0;

	return firstdir;
}

/**
 * Give initial items to object pl. This is used when player creates
 * a new character.
 * @param pl The player object
 * @param items Treasure list of items */
void give_initial_items(object *pl, treasurelist *items)
{
	object *op, *next = NULL;

	if (pl->randomitems != NULL)
		create_treasure(items, pl, GT_ONLY_GOOD | GT_NO_VALUE, 1, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

	for (op = pl->inv; op; op = next)
	{
		next = op->below;

		/* Forces get applied per default */
		if (op->type == FORCE)
			SET_FLAG(op, FLAG_APPLIED);

		/* we never give weapons/armour if these cannot be used
		 * by this player due to race restrictions */
		if (pl->type == PLAYER)
		{
			if ((!QUERY_FLAG(pl, FLAG_USE_ARMOUR) && (op->type == ARMOUR || op->type == BOOTS || op->type == CLOAK || op->type == HELMET || op->type == SHIELD || op->type == GLOVES || op->type == BRACERS || op->type == GIRDLE)) || (!QUERY_FLAG(pl, FLAG_USE_WEAPON) && op->type == WEAPON))
			{
				/* inventory action */
				remove_ob(op);
				continue;
			}
		}

		/* Give starting characters identified, uncursed, and undamned
		 * items.  Just don't identify gold or silver, or it won't be
		 * merged properly. */
		if (need_identify(op))
		{
			SET_FLAG(op, FLAG_IDENTIFIED);
			CLEAR_FLAG(op, FLAG_CURSED);
			CLEAR_FLAG(op, FLAG_DAMNED);
		}

		if (op->type == ABILITY)
		{
			CONTR(pl)->known_spells[CONTR(pl)->nrofknownspells++] = op->stats.sp;
			remove_ob(op);
			continue;
		}
	}
}

/**
 * Send query to op's socket to get player name.
 * @param op Object to send the query to */
void get_name(object *op)
{
	CONTR(op)->write_buf[0] = '\0';
	CONTR(op)->state = ST_GET_NAME;
	send_query(&CONTR(op)->socket, 0, "What is your name?\n:");
}

/**
 * Send query to op's socket to get player's password.
 * @param op Object to send the query to */
void get_password(object *op)
{
	CONTR(op)->write_buf[0] = '\0';
	CONTR(op)->state = ST_GET_PASSWORD;
	send_query(&CONTR(op)->socket, CS_QUERY_HIDEINPUT, "What is your password?\n:");
}

/**
 * If this is a new character, we will need to confirm the password.
 * @param op Object to send the query to */
void confirm_password(object *op)
{
	CONTR(op)->write_buf[0] = '\0';
	CONTR(op)->state = ST_CONFIRM_PASSWORD;
	send_query(&CONTR(op)->socket, CS_QUERY_HIDEINPUT, "Please type your password again.\n:");
}

/**
 * Find an arrow in the inventory and after that
 * in the right type container (quiver).
 * @param op Object to check
 * @param type Ammunition type (bolts, arrows, etc)
 * @return Pointer to the found object, NULL if not found */
object *find_arrow(object *op, const char *type)
{
	object *tmp = NULL;

	for (op = op->inv; op; op = op->below)
	{
		if (!tmp && op->type == CONTAINER && op->race == type && QUERY_FLAG(op, FLAG_APPLIED))
			tmp = find_arrow(op, type);
		else if (op->type == ARROW && op->race == type)
			return op;
	}

	return tmp;
}

/**
 * Player fires a bow.
 * @param op Object firing
 * @param dir Direction to fire */
static void fire_bow(object *op, int dir)
{
	object *left_cont, *bow, *arrow = NULL, *left, *tmp_op;
	tag_t left_tag;

	/* If no dir is specified, attempt to find get the direction
	 * from player's target. */
	if (!dir && op->type == PLAYER && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count))
	{
		rv_vector range_vector;
		dir = get_dir_to_target(op, CONTR(op)->target_object, &range_vector);
	}

	if (!dir)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't shoot yourself!");
		return;
	}

	bow = CONTR(op)->equipment[PLAYER_EQUIP_BOW];
	if (!bow)
		LOG(llevBug, "BUG: fire_bow(): bow without activated bow (%s - %d).\n", op->name, dir);

	if (!bow->race)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "Your %s is broken.", bow->name);
		return;
	}

	if ((arrow = find_arrow_ext(op, bow->race, CONTR(op)->firemode_tag2)) == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You have no %s left.", bow->race);
		return;
	}

	if (wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Something is in the way.");
		return;
	}

	/* this should not happen, but sometimes does */
	if (arrow->nrof == 0)
	{
		LOG(llevDebug, "BUG?: arrow->nrof == 0 in fire_bow() (%s)\n", query_name(arrow, NULL));
		remove_ob(arrow);
		return;
	}

	/* these are arrows left to the player */
	left = arrow;
	left_tag = left->count;
	left_cont = left->env;
	arrow = get_split_ob(arrow, 1);
	set_owner(arrow, op);
	arrow->direction = dir;
	arrow->x = op->x;
	arrow->y = op->y;
	arrow->speed = 1;

	/* now the trick: we transfer the shooting speed in the used
	 * skill - that will allow us to use "set_skill_speed() as global
	 * function. */
	op->chosen_skill->stats.maxsp = bow->stats.sp + arrow->last_grace;
	update_ob_speed(arrow);
	arrow->speed_left = 0;
	SET_ANIMATION(arrow, (NUM_ANIMATIONS(arrow) / NUM_FACINGS(arrow)) * dir);
	/* save original wc and dam */
	arrow->last_heal = arrow->stats.wc;
	/* will be put back in fix_arrow() */
	arrow->stats.hp = arrow->stats.dam;

	/* now we do this: arrow wc = wc base from skill + (wc arrow + magic) + (wc range weapon boni + magic) */
	if ((tmp_op = SK_skill(op)))
		/* wc is in last heal */
		arrow->stats.wc = tmp_op->last_heal;
	else
		arrow->stats.wc = 10;

	/* now we determine how many tiles the arrow will fly.
	 * again we use the skill base and add arrow + weapon values - but no magic add here. */
	arrow->last_sp = tmp_op->last_sp + bow->last_sp + arrow->last_sp;

	/* add in all our wc boni */
	arrow->stats.wc += (bow->magic + arrow->magic + SK_level(op) + thaco_bonus[op->stats.Dex] + bow->stats.wc);

	/* i really like the idea to use here the bow wc_range! */
	arrow->stats.wc_range = bow->stats.wc_range;

	/* monster.c 970 holds the arrow code for monsters */
	arrow->stats.dam += dam_bonus[op->stats.Str] / 2 + bow->stats.dam + bow->magic + arrow->magic;
	arrow->stats.dam = FABS((int)((float)(arrow->stats.dam * lev_damage[SK_level(op)])));

	/* adjust with the lower of condition */
	if (bow->item_condition > arrow->item_condition)
		arrow->stats.dam = (sint16)(((float)arrow->stats.dam / 100.0f) * (float)arrow->item_condition);
	else
		arrow->stats.dam = (sint16)(((float)arrow->stats.dam / 100.0f) * (float)bow->item_condition);

	arrow->level = SK_level (op);
	arrow->map = op->map;
	SET_MULTI_FLAG(arrow, FLAG_FLYING);
	SET_FLAG(arrow, FLAG_IS_MISSILE);
	SET_FLAG(arrow, FLAG_FLY_ON);
	SET_FLAG(arrow, FLAG_WALK_ON);
	/* temp. buffer for "tiles to fly" */
	arrow->stats.grace = arrow->last_sp;
	/* reflection timer */
	arrow->stats.maxgrace = 60 + (RANDOM() % 12);
	play_sound_map(op->map, op->x, op->y, SOUND_FIRE_ARROW, SOUND_NORMAL);

	if (insert_ob_in_map(arrow, op->map, op, 0))
		move_arrow(arrow);

	if (was_destroyed(left, left_tag))
		esrv_del_item(CONTR(op), left_tag, left_cont);
	else
		esrv_send_item(op, left);
}


/**
 * Fire command for spells, range, throwing, etc.
 * @param op Object firing this
 * @param dir Direction to fire to */
void fire(object *op, int dir)
{
	object *weap = NULL;
	int spellcost = 0;

	/* check for loss of invisiblity/hide */
	if (action_makes_visible(op))
		make_visible(op);

	/* a check for players, make sure things are groovy. This routine
	 * will change the skill of the player as appropriate in order to
	 * fire whatever is requested. In the case of spells (range_magic)
	 * it handles whether cleric or mage spell is requested to be cast.
	 * -b.t.  */

	/* ext. fire mode - first step. We map the client side action to a server action. */
	/* forcing the shoottype var from player object to our needed range mode */
	if (op->type == PLAYER)
	{
		if (CONTR(op)->firemode_type == FIRE_MODE_NONE)
			return;

		if (CONTR(op)->firemode_type == FIRE_MODE_BOW)
			CONTR(op)->shoottype = range_bow;
		else if (CONTR(op)->firemode_type == FIRE_MODE_THROW)
		{
			object *tmp;

			/* insert here test for more throwing skills */
			if (!change_skill(op, SK_THROWING))
				return;

			/* special case - we must redirect the fire cmd to throwing something */
			tmp = find_throw_tag(op, (tag_t) CONTR(op)->firemode_tag1);

			if (tmp)
			{
				if (!check_skill_action_time(op, op->chosen_skill))
					return;

				do_throw(op, tmp, dir);
				get_skill_time(op, op->chosen_skill->stats.sp);
				CONTR(op)->action_timer = (float)(CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
				if (CONTR(op)->last_action_timer > 0)
					CONTR(op)->action_timer *= -1;
			}
			return;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_SPELL)
			CONTR(op)->shoottype = range_magic;
		else if (CONTR(op)->firemode_type == FIRE_MODE_WAND)
		{
			/* we do a jump in fire wand if we haven one */
			CONTR(op)->shoottype = range_wand;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_SKILL)
		{
			command_rskill(op, CONTR(op)->firemode_name);
			CONTR(op)->shoottype = range_skill;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_SUMMON)
			CONTR(op)->shoottype = range_scroll;
		else
			CONTR(op)->shoottype = range_none;

		if (!check_skill_to_fire(op))
			return;
	}

	switch (CONTR(op)->shoottype)
	{
		case range_none:
			return;

		case range_bow:
			if (CONTR(op)->firemode_tag2 != -1)
			{
				/* we still recover from range action? */
				if (!check_skill_action_time(op, op->chosen_skill))
					return;

				fire_bow(op, dir);
				get_skill_time(op, op->chosen_skill->stats.sp);
				CONTR(op)->action_timer = (float)(CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
				if (CONTR(op)->last_action_timer > 0)
					CONTR(op)->action_timer *= -1;
				/*op->speed_left -= get_skill_time(op,op->chosen_skill->stats.sp);*/
			}
			return;

			/* Casting spells */
		case range_magic:
			if (!check_skill_action_time(op, op->chosen_skill))
				return;

			spellcost = cast_spell(op, op, dir, CONTR(op)->chosen_spell, 0, spellNormal, NULL);

			if (spells[CONTR(op)->chosen_spell].flags & SPELL_DESC_WIS)
				op->stats.grace -= spellcost;
			else
				op->stats.sp -= spellcost;

			/* Only change the action timer if the spell required mana/grace cost (ie, was successful). */
			if (spellcost)
			{
				get_skill_time(op, op->chosen_skill->stats.sp);
				CONTR(op)->action_timer = (float)(CONTR(op)->action_casting - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;

				if (CONTR(op)->last_action_timer > 0)
					CONTR(op)->action_timer *= -1;
			}

			return;

		case range_wand:
			for (weap = op->inv; weap != NULL; weap = weap->below)
				if (weap->type == WAND && QUERY_FLAG(weap, FLAG_APPLIED))
					break;

			if (weap == NULL)
			{
				CONTR(op)->shoottype=range_rod;
				goto trick_jump;
			}

			if (!check_skill_action_time(op, op->chosen_skill))
				return;

			if (weap->stats.food <= 0)
			{
				play_sound_player_only(CONTR(op), SOUND_WAND_POOF, SOUND_NORMAL, 0, 0);
				new_draw_info(NDI_UNIQUE, 0, op, "The wand says poof.");
				/*op->speed_left -= get_skill_time(op,op->chosen_skill->stats.sp);*/
				return;
			}

			new_draw_info(NDI_UNIQUE, 0, op, "fire wand");
			if (cast_spell(op, weap, dir, weap->stats.sp, 0, spellWand, NULL))
			{
				/* You now know something about it */
				SET_FLAG(op, FLAG_BEEN_APPLIED);
				if (!(--weap->stats.food))
				{
					object *tmp;
					if (weap->arch)
					{
						CLEAR_FLAG(weap, FLAG_ANIMATE);
						weap->face = weap->arch->clone.face;
						weap->speed = 0;
						update_ob_speed(weap);
					}
					if ((tmp = is_player_inv(weap)))
						esrv_update_item(UPD_ANIM, tmp, weap);
				}
			}
			get_skill_time(op, op->chosen_skill->stats.sp);
			CONTR(op)->action_timer = (float)(CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
			if (CONTR(op)->last_action_timer > 0)
				CONTR(op)->action_timer *= -1;
			/*op->speed_left -= get_skill_time(op,op->chosen_skill->stats.sp);*/
			return;

		case range_rod:
		case range_horn:
trick_jump:
			for (weap = op->inv; weap != NULL; weap = weap->below)
				if (QUERY_FLAG(weap, FLAG_APPLIED) && weap->type == (CONTR(op)->shoottype == range_rod ? ROD : HORN))
					break;

			if (weap == NULL)
			{
				if (CONTR(op)->shoottype == range_rod)
				{
					CONTR(op)->shoottype = range_horn;
					goto trick_jump;
				}
				else
				{
					char buf[MAX_BUF];
					sprintf(buf, "You have no tool readied.");
					new_draw_info(NDI_UNIQUE, 0, op, buf);
					return;
				}
			}

			if (!check_skill_action_time(op, op->chosen_skill))
				return;

			/* If the device level is higher than player's skill + 5 */
			if (weap->level > op->chosen_skill->level + 5)
			{
				int level_difference = weap->level - (op->chosen_skill->level + 5);

				/* If the level difference isn't so high, give it a small chance to succeed */
				if (level_difference > 0 && (level_difference > 10 || RANDOM() % weap->level != RANDOM() % (op->chosen_skill->level + 5)))
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "The %s is impossible to handle for you!", weap->name);
					return;
				}
			}

			if (weap->stats.hp < spells[weap->stats.sp].sp)
			{
				play_sound_player_only(CONTR(op), SOUND_WAND_POOF, SOUND_NORMAL, 0, 0);

				if (CONTR(op)->shoottype == range_rod)
					new_draw_info(NDI_UNIQUE, 0, op, "The rod whines for a while, but nothing happens.");
				else
					new_draw_info(NDI_UNIQUE, 0, op, "No matter how hard you try you can't get another note out.");

				return;
			}

			/*new_draw_info_format(NDI_ALL | NDI_UNIQUE, 5, NULL, "Use %s - cast spell %d\n", weap->name, weap->stats.sp);*/
			if (cast_spell(op, weap, dir, weap->stats.sp, 0, CONTR(op)->shoottype == range_rod ? spellRod : spellHorn, NULL))
			{
				/* You now know something about it */
				SET_FLAG(op, FLAG_BEEN_APPLIED);
				drain_rod_charge(weap);
			}
			get_skill_time(op, op->chosen_skill->stats.sp);
			CONTR(op)->action_timer = (float)(CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
			if (CONTR(op)->last_action_timer > 0)
				CONTR(op)->action_timer *= -1;
			return;

			/* Control summoned monsters from scrolls */
		case range_scroll:
			if (CONTR(op)->golem == NULL)
			{
				CONTR(op)->shoottype = range_none;
				CONTR(op)->chosen_spell = -1;
			}
			else
				control_golem(CONTR(op)->golem, dir);

			return;

		case range_skill:
			if (!op->chosen_skill)
			{
				if (op->type == PLAYER)
					new_draw_info(NDI_UNIQUE, 0, op, "You have no applicable skill to use.");
				return;
			}

			if (op->chosen_skill->sub_type1 != ST1_SKILL_USE)
				new_draw_info(NDI_UNIQUE, 0, op, "You can't use this skill in this way.");
			else
				(void) do_skill(op, dir, NULL);

			return;
		default:
			new_draw_info(NDI_UNIQUE, 0, op, "Illegal shoot type.");
			return;
	}
}

/**
 * Move a player.
 * @param op Player object
 * @param dir Direction to move to
 * @return Always returns 0. */
int move_player(object *op, int dir)
{
	/*int face = dir ? (dir - 1) / 2 : -1;*/

	CONTR(op)->praying = 0;

	if (op->map == NULL || op->map->in_memory != MAP_IN_MEMORY)
		return 0;

	/* Do not allow the player to move if he is in player shop interface */
	if (QUERY_FLAG(op, FLAG_PLAYER_SHOP))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Close your player shop before attempting to move.");
		return 0;
	}

	if (dir)
		op->facing = dir;

	if (QUERY_FLAG(op, FLAG_CONFUSED) && dir)
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	op->anim_moving_dir = -1;
	op->anim_enemy_dir = -1;
	op->anim_last_facing = -1;

	/* puh, we should move hide to FLAG_ ASAP */
	if (op->hide)
	{
		op->anim_moving_dir = dir;
		do_hidden_move(op);
	}

	/* firemode is set from client command fire xx xx xx */
	if (CONTR(op)->firemode_type != -1)
	{
		fire(op, dir);

		if (dir)
			op->anim_enemy_dir = dir;
		else
			op->anim_enemy_dir = op->facing;

		CONTR(op)->fire_on = 0;
	}
	else
	{
		if (!move_ob(op, dir, op))
			op->anim_enemy_dir = dir;
		else
			op->anim_moving_dir = dir;
	}

	/* hm, should be not needed - players always animated */
	if (QUERY_FLAG(op, FLAG_ANIMATE))
	{
		if (op->anim_enemy_dir == -1 && op->anim_moving_dir == -1)
			op->anim_last_facing = dir;

		animate_object(op, 0);
	}
	return 0;
}

/**
 * This is similar to handle_player(), but is only used by the
 * new client/server stuff.
 * This is sort of special, in that the new client/server actually uses
 * the new speed values for commands.
 * @param pl Player structure
 * @return 1 if there are more actions we can do, 0 otherwise. */
int handle_newcs_player(player *pl)
{
	object *op;

	/* call this here - we also will call this in do_ericserver, but
	 * the players time has been increased when doericserver has been
	 * called, so we recheck it here. */
	HandleClient(&pl->socket, pl);
	op = pl->ob;

	if (op->speed_left < 0.0f)
		return 0;

	/* if we are here, we never paralyzed anymore */
	CLEAR_FLAG(op, FLAG_PARALYZED);

	if (op->direction && (CONTR(op)->run_on || CONTR(op)->fire_on))
	{
		/* All move commands take 1 tick, at least for now */
		op->speed_left--;
		/* Instead of all the stuff below, let move_player take care
		 * of it.  Also, some of the skill stuff is only put in
		 * there, as well as the confusion stuff. */
		move_player(op, op->direction);

		if (op->speed_left > 0)
			return 1;
		else
			return 0;
	}

	return 0;
}

/**
 * Checks if object op has FLAG_LIFESAVE set. If he does, check
 * his inventory for applied object that saves life. If found,
 * bring the object player to his save bed.
 * @param op Object to check
 * @return 1 if the player has life saving object, 0 if not */
int save_life(object *op)
{
	object *tmp;
	char buf[MAX_BUF];
	if (!QUERY_FLAG(op, FLAG_LIFESAVE))
		return 0;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (QUERY_FLAG(tmp, FLAG_APPLIED) && QUERY_FLAG(tmp, FLAG_LIFESAVE))
		{
			play_sound_map(op->map, op->x, op->y, SOUND_OB_EVAPORATE, SOUND_NORMAL);
			sprintf(buf, "Your %s vibrates violently, then evaporates.", query_name(tmp, NULL));
			new_draw_info(NDI_UNIQUE, 0, op, buf);

			if (CONTR(op))
				esrv_del_item(CONTR(op), tmp->count, tmp->env);

			remove_ob(tmp);
			CLEAR_FLAG(op, FLAG_LIFESAVE);

			if (op->stats.hp < 0)
				op->stats.hp = op->stats.maxhp;

			if (op->stats.food < 0)
				op->stats.food = 999;

			/* bring him home. */
			enter_player_savebed(op);
			return 1;
		}
	}

	LOG(llevBug, "BUG: save_life(): LIFESAVE set without applied object.\n");
	CLEAR_FLAG(op, FLAG_LIFESAVE);
	/* bring him home. */
	enter_player_savebed(op);
	return 0;
}

/**
 * This goes through the inventory and removes unpaid objects, and puts them
 * back in the map (location and map determined by values of env).  This
 * function will descend into containers.
 * @param op Object to start the search from
 * @param env Map location determined by this object */
void remove_unpaid_objects(object *op, object *env)
{
	object *next;

	while (op)
	{
		/* Make sure we have a good value, in case
		 * we remove object 'op' */
		next = op->below;
		if (QUERY_FLAG(op, FLAG_UNPAID))
		{
			remove_ob(op);
			op->x = env->x;
			op->y = env->y;
			insert_ob_in_map(op, env->map, NULL, 0);
		}
		else if (op->inv)
			remove_unpaid_objects(op->inv, env);
		op = next;
	}
}

/**
 * Do some living for player object, like generating grace,
 * hp, mana, starting to pray, etc.
 * @param op Object to do some living */
void do_some_living(object *op)
{
	if (CONTR(op)->state == ST_PLAYING)
	{
		/* hp reg */
		if (CONTR(op)->gen_hp)
		{
			if (--op-> last_heal < 0)
			{
				op->last_heal = CONTR(op)->base_hp_reg;
				/* halfed reg speed */
				if (CONTR(op)->combat_mode)
					op->last_heal += op->last_heal;

				if (op->stats.hp < op->stats.maxhp)
				{
					int last_food = op->stats.food;

					op->stats.hp += CONTR(op)->reg_hp_num;
					if (op->stats.hp > op->stats.maxhp)
						op->stats.hp = op->stats.maxhp;

					/* faster hp reg - faster digestion... evil */
					op->stats.food--;
					if (CONTR(op)->digestion < 0)
						op->stats.food += CONTR(op)->digestion;
					else if (CONTR(op)->digestion > 0 && random_roll(0, CONTR(op)->digestion, op, PREFER_HIGH))
						op->stats.food = last_food;
				}
			}
		}

		/* sp reg */
		if (CONTR(op)->gen_sp)
		{
			if (--op->last_sp < 0)
			{
				op->last_sp = CONTR(op)->base_sp_reg;
				if (op->stats.sp < op->stats.maxsp)
				{
					op->stats.sp += CONTR(op)->reg_sp_num;
					if (op->stats.sp > op->stats.maxsp)
						op->stats.sp = op->stats.maxsp;
				}
			}
		}

		/* "stay and pray" mechanism */
		if (CONTR(op)->praying && !CONTR(op)->was_praying)
		{
			if (op->stats.grace < op->stats.maxgrace)
			{
				object *god = find_god(determine_god(op));
				if (god)
				{
					if (CONTR(op)->combat_mode)
					{
						new_draw_info_format(NDI_UNIQUE, 0, op, "You stop combat and start praying to %s...", god->name);
						CONTR(op)->combat_mode = 0;
						send_target_command(CONTR(op));
					}
					else
						new_draw_info_format(NDI_UNIQUE, 0, op, "You start praying to %s...", god->name);

					CONTR(op)->was_praying = 1;
				}
				else
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You worship no deity to pray to!");
					CONTR(op)->praying = 0;
				}
				op->last_grace = CONTR(op)->base_grace_reg;
			}
			else
			{
				CONTR(op)->praying = 0;
				CONTR(op)->was_praying = 0;
			}
		}
		else if (!CONTR(op)->praying && CONTR(op)->was_praying)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You stop praying.");
			CONTR(op)->was_praying = 0;
			op->last_grace = CONTR(op)->base_grace_reg;
		}

		/* grace reg */
		if (CONTR(op)->praying && CONTR(op)->gen_grace)
		{
			if (--op->last_grace < 0)
			{
				if (op->stats.grace < op->stats.maxgrace)
					op->stats.grace += CONTR(op)->reg_grace_num;

				if (op->stats.grace >= op->stats.maxgrace)
				{
					op->stats.grace = op->stats.maxgrace;
					new_draw_info(NDI_UNIQUE, 0, op, "Your are full of grace and stop praying.");
					CONTR(op)->was_praying = 0;
				}
				op->last_grace = CONTR(op)->base_grace_reg;
			}
		}

		/* Digestion */
		if (--op->last_eat < 0)
		{
			int bonus = CONTR(op)->digestion > 0 ? CONTR(op)->digestion : 0,
						penalty = CONTR(op)->digestion < 0? -CONTR(op)->digestion : 0;
			if (CONTR(op)->gen_hp > 0)
				op->last_eat = 25 * (1 + bonus) / (CONTR(op)->gen_hp + penalty + 1);
			else
				op->last_eat = 25 * (1 + bonus) / (penalty + 1);
			op->stats.food--;
		}

		if (op->stats.food < 0 && op->stats.hp >= 0)
		{
			object *tmp, *flesh = NULL;

			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
			{
				if (!QUERY_FLAG(tmp, FLAG_UNPAID))
				{
					if (tmp->type == FOOD || tmp->type == DRINK || tmp->type == POISON)
					{
						new_draw_info(NDI_UNIQUE, 0, op, "You blindly grab for a bite of food.");
						manual_apply(op, tmp, 0);
						if (op->stats.food >= 0 || op->stats.hp < 0)
							break;
					}
					else if (tmp->type == FLESH)
						flesh = tmp;
				}
			}

			/* If player is still starving, it means they don't have any food, so
			 * eat flesh instead. */
			if (op->stats.food < 0 && op->stats.hp >= 0 && flesh)
			{
				new_draw_info(NDI_UNIQUE, 0, op, "You blindly grab for a bite of food.");
				manual_apply(op, flesh, 0);
			}
		}

		while (op->stats.food < 0 && op->stats.hp > 0)
		{
			op->stats.food++;
			/* new: no dying from food. hp will fall to 1 but not under it.
			 * we must check here for negative because we don't want ADD here */
			if (op->stats.hp)
			{
				op->stats.hp--;
				if (!op->stats.hp)
					op->stats.hp = 1;
			}
		}

		/* we can't die by no food but perhaps by poisoned food? */
		if ((op->stats.hp <= 0 || op->stats.food < 0) && !QUERY_FLAG(op, FLAG_WIZ))
			kill_player(op);
	}
}

/**
 * If the player should die (lack of hp, food, etc), we call this.
 * @param op The player in jeopardy. */
void kill_player(object *op)
{
	char buf[HUGE_BUF];
	int x, y, i;
	/* this is for resurrection */
	mapstruct *map;
	object *tmp;
	int z;
	int num_stats_lose;
	int lost_a_stat;
	int lose_this_stat;
	int this_stat;

	if (save_life(op))
		return;

	/* If player dies on BATTLEGROUND, no stat/exp loss! For Combat-Arenas
	 * in cities ONLY!!! It is very important that this doesn't get abused.
	 * Look at op_on_battleground() for more info -- AndreasV */
	if (pvp_area(NULL, op))
	{
		new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, op, "You have been defeated in combat!");
		new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, op, "Local medics have saved your life...");

		/* restore player */
		cast_heal(op, 110, op, SP_CURE_POISON);
		/*cast_heal(op, op, SP_CURE_CONFUSION);*/
		/* remove any disease */
		cure_disease(op, NULL);
		op->stats.hp = op->stats.maxhp;
		if (op->stats.food <= 0)
			op->stats.food = 999;

		/* Create a bodypart-trophy to make the winner happy */
		tmp = arch_to_object(find_archetype("finger"));
		if (tmp != NULL)
		{
			sprintf(buf, "%s's finger", op->name);
			FREE_AND_COPY_HASH(tmp->name, buf);
			sprintf(buf, "This finger has been cut off %s the %s, when he was defeated at level %d by %s.", op->name, op->race, (int)(op->level), strcmp(CONTR(op)->killer, "") ? CONTR(op)->killer : "something nasty");
			FREE_AND_COPY_HASH(tmp->msg, buf);
			tmp->value = 0, tmp->material = 0, tmp->type = 0;
			tmp->x = op->x, tmp->y = op->y;
			insert_ob_in_map(tmp, op->map, op, 0);
		}

		/* teleport defeated player to new destination */
		transfer_ob(op, MAP_ENTER_X(op->map), MAP_ENTER_Y(op->map), 0, NULL, NULL);
		return;
	}

	/* Trigger the DEATH event */
	if (trigger_event(EVENT_DEATH, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL))
	{
		return;
	}

	/* Trigger the global GDEATH event */
	trigger_global_event(EVENT_GDEATH, NULL, op);

	if (op->stats.food < 0)
	{
		sprintf(buf, "%s starved to death.", op->name);
		strcpy(CONTR(op)->killer, "starvation");
	}
	else
	{
		sprintf(buf, "%s died.", op->name);
	}

	play_sound_player_only(CONTR(op), SOUND_PLAYER_DIES, SOUND_NORMAL, 0, 0);

	/* save the map location for corpse, gravestone */
	x = op->x;
	y = op->y;
	map = op->map;

	/* This basically brings the character back to life
	 * if they are dead - it takes some exp and a random stat.  See the config.h
	 * file for a little more in depth detail about this. */

	/* Basically two ways to go - remove a stat permanently, or just
	 * make it depletion.  This bunch of code deals with that aspect
	 * of death. */
	if (settings.balanced_stat_loss)
	{
		/* If stat loss is permanent, lose one stat only. */
		/* Lower level chars don't lose as many stats because they suffer more
		   if they do. */
		/* Higher level characters can afford things such as potions of
		   restoration, or better, stat potions. So we slug them that little
		   bit harder. */
		/* GD */
		if (settings.stat_loss_on_death)
			num_stats_lose = 1;
		else
			num_stats_lose = 1 + op->level / BALSL_NUMBER_LOSSES_RATIO;
	}
	else
		num_stats_lose = 1;
	lost_a_stat = 0;

	/* the rule is:
	 * only decrease stats when you are level 3 or higher! */
	for (z = 0; z < num_stats_lose; z++)
	{
		if (settings.stat_loss_on_death && op->level > 3)
		{
			/* Pick a random stat and take a point off it.  Tell the player
			 * what he lost. */
			i = RANDOM() % 7;
			change_attr_value(&(op->stats), i, -1);
			check_stat_bounds(&(op->stats));
			change_attr_value(&(CONTR(op)->orig_stats), i, -1);
			check_stat_bounds(&(CONTR(op)->orig_stats));
			new_draw_info(NDI_UNIQUE, 0, op, lose_msg[i]);
			lost_a_stat = 1;
		}
		else if (op->level > 3)
		{
			/* deplete a stat */
			archetype *deparch = find_archetype("depletion");
			object *dep;

			i = RANDOM() % 7;
			dep = present_arch_in_ob(deparch, op);
			if (!dep)
			{
				dep = arch_to_object(deparch);
				insert_ob_in_ob(dep, op);
			}
			lose_this_stat = 1;
			if (settings.balanced_stat_loss)
			{
				/* GD */
				/* Get the stat that we're about to deplete. */
				this_stat = get_attr_value(&(dep->stats), i);
				if (this_stat < 0)
				{
					int loss_chance = 1 + op->level / BALSL_LOSS_CHANCE_RATIO;
					int keep_chance = this_stat * this_stat;
					/* Yes, I am paranoid. Sue me. */
					if (keep_chance < 1)
						keep_chance = 1;

					/* There is a maximum depletion total per level. */
					if (this_stat < -1 - op->level / BALSL_MAX_LOSS_RATIO)
					{
						lose_this_stat = 0;
					}
					else
					{
						/* Take loss chance vs keep chance to see if we retain the stat. */
						if (random_roll(0, loss_chance + keep_chance - 1, op, PREFER_LOW) < keep_chance)
							lose_this_stat = 0;
						/*LOG(llevDebug, "Determining stat loss. Stat: %d Keep: %d Lose: %d Result: %s.\n", this_stat, keep_chance, loss_chance, lose_this_stat ? "LOSE" : "KEEP");*/
					}
				}
			}

			if (lose_this_stat)
			{
				this_stat = get_attr_value(&(dep->stats), i);
				/* We could try to do something clever like find another
				 * stat to reduce if this fails.  But chances are, if
				 * stats have been depleted to -50, all are pretty low
				 * and should be roughly the same, so it shouldn't make a
				 * difference. */
				if (this_stat >= -50)
				{
					change_attr_value(&(dep->stats), i, -1);
					SET_FLAG(dep, FLAG_APPLIED);
					new_draw_info(NDI_UNIQUE, 0, op, lose_msg[i]);
					fix_player(op);
					lost_a_stat = 1;
				}
			}
		}
	}

	/* If no stat lost, tell the player. */
	if (!lost_a_stat)
	{
		/* determine_god() seems to not work sometimes... why is this?
		 * Should I be using something else? GD */
		const char *god = determine_god(op);
		if (god && (strcmp(god, "none")))
			new_draw_info_format(NDI_UNIQUE, 0, op, "For a brief moment you feel the holy presence of %s protecting you.", god);
		else
			new_draw_info(NDI_UNIQUE, 0, op, "For a brief moment you feel a holy presence protecting you.");
	}

	/* Put a gravestone up where the character 'almost' died.  List the
	 * exp loss on the stone. */
	tmp = arch_to_object(find_archetype("gravestone"));
	sprintf(buf, "%s's gravestone", op->name);
	FREE_AND_COPY_HASH(tmp->name, buf);
	FREE_AND_COPY_HASH(tmp->msg, gravestone_text(op));
	tmp->x = op->x, tmp->y = op->y;
	insert_ob_in_map(tmp, op->map, NULL, 0);

	/**************************************/
	/*                                    */
	/* Subtract the experience points,    */
	/* if we died cause of food, give us  */
	/* food, and reset HP's...            */
	/*                                    */
	/**************************************/

	/* remove any poisoning and confusion the character may be suffering. */
	cast_heal(op, 110, op, SP_CURE_POISON);
	/*cast_heal(op, op, SP_CURE_CONFUSION);*/
	/* remove any disease */
	cure_disease(op, NULL);

	apply_death_exp_penalty(op);

	if (op->stats.food < 0)
		op->stats.food = 900;

	op->stats.hp = op->stats.maxhp;
	op->stats.sp = op->stats.maxsp;
	op->stats.grace = op->stats.maxgrace;

	check_score(op, 1);

	/* Check to see if the player is in a shop.  IF so, then check to see if
	 * the player has any unpaid items.  If so, remove them and put them back
	 * in the map. */

	tmp = get_map_ob(op->map, op->x, op->y);
	if (tmp && tmp->type == SHOP_FLOOR)
		remove_unpaid_objects(op->inv, op);

	/****************************************/
	/*                                      */
	/* Move player to his current respawn-  */
	/* position (usually last savebed)      */
	/*                                      */
	/****************************************/

	enter_player_savebed(op);

	/**************************************/
	/*                                    */
	/* Repaint the characters inv, and    */
	/* stats, and show a nasty message ;) */
	/*                                    */
	/**************************************/

	new_draw_info(NDI_UNIQUE, 0, op, "YOU HAVE DIED.");
	save_player(op, 1);
	return;
}

/**
 * Check recursively the weight of all players, and fix
 * what needs to be fixed.  Refresh windows and fix speed if anything
 * was changed. */
void fix_weight()
{
	player *pl;
	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		int old = pl->ob->carrying, sum = sum_weight(pl->ob);

		if (old == sum)
			continue;

		fix_player(pl->ob);
		esrv_update_item(UPD_WEIGHT, pl->ob, pl->ob);
		/*LOG(llevDebug, "DEBUG: fix_weight: Fixed inventory in %s (%d -> %d)\n", query_name(pl->ob, NULL), old, sum);*/
	}
}

/**
 * Handles object throwing objects of type "DUST".
 * @warning This function needs to be rewritten.\n
 * Works for area effect spells only now.
 * @param op Object throwing this
 * @param throw_ob Object being thrown
 * @param dir Direction to throw */
void cast_dust (object *op, object *throw_ob, int dir)
{
	archetype *arch = NULL;

	if (!(spells[throw_ob->stats.sp].flags & SPELL_DESC_DIRECTION))
	{
		LOG(llevBug, "DEBUG: Warning, dust %s is not a ae spell!!\n", query_name(throw_ob, NULL));
		return;
	}

	if (spells[throw_ob->stats.sp].archname)
		arch = find_archetype(spells[throw_ob->stats.sp].archname);

	/* casting POTION 'dusts' is really a use_magic_item skill */
	if (op->type == PLAYER && throw_ob->type == POTION && !change_skill(op, SK_USE_MAGIC_ITEM))
		return;

	if (throw_ob->type == POTION && arch != NULL)
		cast_cone(op,throw_ob,dir,10,throw_ob->stats.sp,arch,1);
	/* dust_effect */
	else if ((arch = find_archetype("dust_effect")) != NULL)
		cast_cone(op, throw_ob, dir, 1, 0, arch, 0);
	/* problem occured! */
	else
		LOG(llevBug, "BUG: cast_dust() can't find an archetype to use!\n");

	if (op->type == PLAYER && arch)
		new_draw_info_format(NDI_UNIQUE, 0, op, "You cast %s.", query_name(throw_ob, NULL));

	if (!QUERY_FLAG(throw_ob, FLAG_REMOVED))
		destruct_ob(throw_ob);
}

/**
 * Make an object visible. Currently unused.
 * @param op The object */
void make_visible (object *op)
{
	(void) op;

#if 0
	if (op->type == PLAYER)
		if (QUERY_FLAG(op, FLAG_UNDEAD) && !is_true_undead(op))
			CLEAR_FLAG(op, FLAG_UNDEAD);
	update_object(op, UP_OBJ_FACE);
#endif
}

/**
 * Check if object is true undead. Currently unused.
 * @param op Object to check
 * @return Always returns 1. */
int is_true_undead(object *op)
{
	(void) op;

#if 0
	object *tmp = NULL;

	if (QUERY_FLAG(&op->arch->clone, FLAG_UNDEAD))
		return 1;

	if (op->type == PLAYER)
		for (tmp = op->inv; tmp; tmp = tmp->below)
			if (tmp->type == EXPERIENCE && tmp->stats.Wis)
				if (QUERY_FLAG(tmp, FLAG_UNDEAD))
					return 1;
#endif

	return 0;
}

/**
 * Look at the surrounding terrain to determine
 * the hideability of this object. Positive levels
 * indicate greater hideability. Currently unused.
 * @param ob Object we are checking
 * @return Always returns 1. */
int hideability(object *ob)
{
	(void) ob;

#if 0
	int i, x, y, level = 0;

	if (!ob || !ob->map)
		return 0;

	/* scan through all nearby squares for terrain to hide in */
	for (i = 0, x = ob->x, y = ob->y; i < 9; i++, x = ob->x + freearr_x[i], y = ob->y + freearr_y[i])
	{
		/* something to hide near! */
		if (blocks_view(ob->map, x, y))
			level += 2;
		/* open terrain! */
		else
			level -= 1;
	}

	/*LOG(llevDebug, "DEBUG: hideability(): hideability of %s is %d\n", ob->name, level);*/
	return level;
#endif

	return 0;
}

/**
 * For hidden creatures - a chance of becoming 'unhidden'
 * every time they move - as we subtract off 'invisibility'
 * AND, for players, if they move into a ridiculously unhideable
 * spot (surrounded by clear terrain in broad daylight). -b.t.
 * @param op Object that is doing the hidden move */
void do_hidden_move(object *op)
{
	int hide = 0, num = random_roll(0, 19, op, PREFER_LOW);

	if (!op || !op->map)
		return;

	/* its *extremely* hard to run and sneak/hide at the same time! */
	if (op->type == PLAYER && CONTR(op)->run_on)
	{
		if (num >= SK_level(op))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You ran too much! You are no longer hidden!");
			make_visible(op);
			return;
		}
		else
			num += 20;
	}
	num += op->map->difficulty;
	/* modify by terrain hidden level */
	hide = hideability(op);
	num -= hide;

	if (op->type == PLAYER && hide < -10)
	{
		make_visible(op);
		new_draw_info(NDI_UNIQUE, 0, op, "You moved out of hiding! You are visible!");
	}
}

/**
 * Determine if object is standing near a hostile creature.
 * @param who The object to check
 * @return 1 if the object is standing near hostile creature, 0 if not. */
int stand_near_hostile(object *who)
{
	object *tmp = NULL;
	mapstruct *m;
	int i, xt, yt, friendly = 0, player = 0;

	if (!who)
		return 0;

	if (who->type == PLAYER)
		player = 1;
	else
		friendly = QUERY_FLAG(who, FLAG_FRIENDLY);

	/* search adjacent squares */
	for (i = 1; i < 9; i++)
	{
		xt = who->x + freearr_x[i];
		yt = who->y + freearr_y[i];

		if (!(m = out_of_map(who->map, &xt, &yt)))
			continue;

		for (tmp = get_map_ob(m, xt, yt); tmp; tmp = tmp->above)
		{
			if ((player || friendly) && QUERY_FLAG(tmp, FLAG_MONSTER) && !QUERY_FLAG(tmp, FLAG_UNAGGRESSIVE))
				return 1;
			else if (tmp->type == PLAYER)
				return 1;
		}
	}
	return 0;
}

/**
 * Routine for both players and monsters. We call this when
 * there is a possibility for our action distrubing our hiding
 * place or invisiblity spell. Artifact invisiblity is not
 * affected by this. Currently unused.
 * @param op Object to check
 * @return Always returns 0. */
int action_makes_visible(object *op)
{
	(void) op;

#if 0
	if (QUERY_FLAG(op, FLAG_IS_INVISIBLE) && QUERY_FLAG(op, FLAG_ALIVE))
	{
		if (!QUERY_FLAG(op, FLAG_SEE_INVISIBLE))
			return 0;
		else if (op->hide)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You become %!", op->hide ? "unhidden" : "visible");
			return 1;
		}
		else if (CONTR(op) && !CONTR(op)->shoottype == range_magic)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Your invisibility spell is broken!");
			return 1;
		}
	}
#endif

	return 0;
}

/**
 * Test for PVP area.
 * if only one opject is given, it test for it.
 * if 2 objects given, both player must be in pvp or
 * the function fails.
 * this function use map and x/y from the player object -
 * be sure player are valid and on map.
 * @param attacker
 * @param victim
 * @return 1 if PVP is possible, 0 if not */
int pvp_area(object *attacker, object* victim)
{
	/* No attacking of party members. */
	if (attacker && victim && attacker->type == PLAYER && victim->type == PLAYER && CONTR(attacker)->party_number != -1 && CONTR(victim)->party_number != -1 && CONTR(attacker)->party_number == CONTR(victim)->party_number)
	{
		return 0;
	}

	/* If both are the same, this is probably a firestorm from attacker or something. Don't want to kill ourselves! */
	if (attacker && victim && attacker == victim)
	{
		return 0;
	}

	if (attacker && attacker->map)
	{
		if (!(attacker->map->map_flags & MAP_FLAG_PVP) && !(GET_MAP_FLAGS(attacker->map, attacker->x, attacker->y) & P_IS_PVP))
		{
			return 0;
		}
	}

	if (victim && victim->map)
	{
		if (!(victim->map->map_flags & MAP_FLAG_PVP) && !(GET_MAP_FLAGS(victim->map, victim->x, victim->y) & P_IS_PVP))
		{
			return 0;
		}
	}

	return 1;
}

/**
 * When a dragon player gains a new stage of evolution,
 * he gets some treasure.
 * @param who The dragon player
 * @param atnr The attack number of the ability focus
 * @param level Ability level */
void dragon_ability_gain(object *who, int atnr, int level)
{
	/* treasurelist */
	treasurelist *trlist = NULL;
	/* treasure */
	treasure *tr;
	/* tmp. object */
	object *tmp;
	/* treasure object */
	object *item;
	/* tmp. string buffer */
	char buf[MAX_BUF];
	int i = 0, j = 0;

	/* get the appropriate treasurelist */
	if (atnr == ATNR_FIRE)
		trlist = find_treasurelist("dragon_ability_fire");
	else if (atnr == ATNR_COLD)
		trlist = find_treasurelist("dragon_ability_cold");
	else if (atnr == ATNR_ELECTRICITY)
		trlist = find_treasurelist("dragon_ability_elec");
	else if (atnr == ATNR_POISON)
		trlist = find_treasurelist("dragon_ability_poison");

	if (trlist == NULL || who->type != PLAYER)
		return;

	for (i = 0, tr = trlist->items; tr != NULL && i < level - 1; tr = tr->next, i++);

	if (tr == NULL || tr->item == NULL)
	{
		/*printf("-> no more treasure for %s\n", change_resist_msg[atnr]);*/
		return;
	}

	/* everything seems okay - now bring on the gift: */
	item = &(tr->item->clone);

	/* grant direct spell */
	if (item->type == SPELLBOOK)
	{
		int spell = look_up_spell_name(item->slaying);

		if (spell < 0 || check_spell_known(who, spell))
			return;

		if (IS_SYS_INVISIBLE(item))
		{
			sprintf(buf, "You gained the ability of %s", spells[spell].name);
			new_draw_info(NDI_UNIQUE | NDI_BLUE, 0, who, buf);
			do_learn_spell(who, spell, 0);
			return;
		}
	}
	else if (item->type == SKILL)
	{
		if (strcmp(item->title, "clawing") == 0 && change_skill(who, SK_CLAWING))
		{
			/* adding new attacktypes to the clawing skill */
			/* clawing skill object */
			tmp = who->chosen_skill;

			if (tmp->type == SKILL && strcmp(tmp->name, "clawing") == 0 && !(tmp->attacktype & item->attacktype))
			{
				/* always add physical if there's none */
				if (tmp->attacktype == 0)
					tmp->attacktype = AT_PHYSICAL;

				/* we add the new attacktype to the clawing ability */
				tmp->attacktype += item->attacktype;

				if (item->msg != NULL)
					new_draw_info(NDI_UNIQUE | NDI_BLUE, 0, who, item->msg);
			}
		}
	}
	else if (item->type == FORCE)
	{
		/* forces in the treasurelist can alter the player's stats */
		object *skin;
		/* first get the dragon skin force */
		for (skin = who->inv; skin != NULL && strcmp(skin->arch->name, "dragon_skin_force") != 0; skin = skin->below);

		if (skin == NULL)
			return;

		/* adding new spellpath attunements */
		if (item->path_attuned > 0 && !(skin->path_attuned & item->path_attuned))
		{
			/* add attunement to skin */
			skin->path_attuned += item->path_attuned;

			/* print message */
			sprintf(buf, "You feel attuned to ");
			for (i = 0, j = 0; i<NRSPELLPATHS; i++)
			{
				if (item->path_attuned & (1 << i))
				{
					if (j)
						strcat(buf, " and ");
					else
						j = 1;
					strcat(buf, spellpathnames[i]);
				}
			}
			strcat(buf, ".");
			new_draw_info(NDI_UNIQUE | NDI_BLUE, 0, who, buf);
		}

		/* evtl. adding flags: */
		if (QUERY_FLAG(item, FLAG_XRAYS))
			SET_FLAG(skin, FLAG_XRAYS);

		if (QUERY_FLAG(item, FLAG_STEALTH))
			SET_FLAG(skin, FLAG_STEALTH);

		if (QUERY_FLAG(item, FLAG_SEE_IN_DARK))
			SET_FLAG(skin, FLAG_SEE_IN_DARK);

		/* print message if there is one */
		if (item->msg != NULL)
			new_draw_info(NDI_UNIQUE | NDI_BLUE, 0, who, item->msg);
	}
	else
	{
		/* generate misc. treasure */
		tmp = arch_to_object(tr->item);
		sprintf(buf, "You gained %s", query_short_name(tmp, NULL));
		new_draw_info(NDI_UNIQUE | NDI_BLUE, 0, who, buf);
		tmp = insert_ob_in_ob(tmp, who);
		if (who->type == PLAYER)
			esrv_send_item(who, tmp);
	}
}

/**
 * Extended find arrow version, using tag and containers.
 * Find an arrow in the inventory and after that in the
 * right type container (quiver).
 * @param op Object
 * @param type Type of the ammunition (arrows, bolts, etc)
 * @param tag Firemode tag
 * @return Pointer to the arrow, NULL if not found */
static object *find_arrow_ext(object *op, const char *type, int tag)
{
	object *tmp = NULL;

	if (tag == -2)
	{
		for (op = op->inv; op; op = op->below)
		{
			if (!tmp && op->type == CONTAINER && op->race == type && QUERY_FLAG(op, FLAG_APPLIED))
				tmp = find_arrow_ext(op, type, -2);
			else if (op->type == ARROW && op->race == type)
				return op;
		}

		return tmp;
	}
	else
	{
		if (tag == -1)
			return tmp;

		for (op = op->inv; op; op = op->below)
		{
			if (op->count == (tag_t) tag)
			{
				/* the simple task: we have a arrow marked */
				if (op->race == type && op->type == ARROW)
					return op;

				/* we have container marked as missile source. Skip search when there is
				 * nothing in. Use the standard search now */
				/* because we don't want container in container, we don't care abvout applied */
				if (op->race == type && op->type == CONTAINER)
				{
					tmp = find_arrow_ext(op, type, -2);
					return tmp;
				}
			}
		}
		return tmp;
	}
}
