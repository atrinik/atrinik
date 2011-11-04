/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * This file deals with administrative commands from the client. */

#include <global.h>

#define MAP_POS_X 0
#define MAP_POS_Y 1

static int map_pos_array[][2] =
{
	{0,0},

	{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},

	{0,-2},{1,-2},{2,-2},{2,-1},{2,0},{2,1},{2,2},{1,2},{0,2},{-1,2},{-2,2},{-2,1},
	{-2,0},{-2,-1},{-2,-2},{-1,-2},

	{0,-3},{1,-3},{2,-3},
	{3,-3},{3,-2},{3,-1},{3,0},{3,1},{3,2},
	{3,3},{2,3},{1,3},{0,3},{-1,3},{-2,3},
	{-3,3},{-3,2},{-3,1},{-3,0},{-3,-1},{-3,-2},
	{-3,-3},{-2,-3},{-1,-3},

	{0,-4},{1,-4},{2,-4},{3,-4},
	{4,-4},{4,-3},{4,-2},{4,-1},{4,0},{4,1},{4,2},{4,3},
	{4,4},{3,4},{2,4},{1,4},{0,4},{-1,4},{-2,4},{-3,4},
	{-4,4},{-4,3},{-4,2},{-4,1},{-4,0},{-4,-1},{-4,-2},{-4,-3},
	{-4,-4},{-3,-4},{-2,-4},{-1,-4},

	{0,-5},{1,-5},{2,-5},{3,-5},{4,-5},
	{5,-5},{5,-4},{5,-3},{5,-2},{5,-1},{5,0},{5,1},{5,2},{5,3},{5,4},
	{5,5},{4,5},{3,5},{2,5},{1,5},{0,5},{-1,5},{-2,5},{-3,5},{-4,5},
	{-5,5},{-5,4},{-5,3},{-5,2},{-5,1},{-5,0},{-5,-1},{-5,-2},{-5,-3},{-5,-4},
	{-5,-5},{-4,-5},{-3,-5},{-2,-5}, {-1,-5},

	{0,-6},{1,-6},{2,-6},{3,-6},{4,-6},{5,-6},
	{6,-6},{6,-5},{6,-4},{6,-3},{6,-2},{6,-1},{6,0},{6,1},{6,2},{6,3},{6,4},{6,5},
	{6,6},{5,6},{4,6},{3,6},{2,6},{1,6},{0,6},{-1,6},{-2,6},{-3,6},{-4,6},{-5,6},
	{-6,6},{-6,5},{-6,4},{-6,3},{-6,2},{-6,1},{-6,0},{-6,-1},{-6,-2},{-6,-3},{-6,-4},{-6,-5},
	{-6,-6},{-5,-6},{-4,-6},{-3,-6},{-2,-6},{-1,-6},

	{0,-7},{1,-7},{2,-7},{3,-7},{4,-7},{5,-7},{6,-7},
	{7,-7},{7,-6},{7,-5},{7,-4},{7,-3},{7,-2},{7,-1},{7,0},{7,1},{7,2},{7,3},{7,4},{7,5},{7,6},
	{7,7},{6,7},{5,7},{4,7},{3,7},{2,7},{1,7},{0,7},{-1,7},{-2,7},{-3,7},{-4,7},{-5,7},{-6,7},
	{-7,7},{-7,6},{-7,5},{-7,4},{-7,3},{-7,2},{-7,1},{-7,0},{-7,-1},{-7,-2},{-7,-3},{-7,-4},{-7,-5},{-7,-6},
	{-7,-7},{-6,-7},{-5,-7},{-4,-7},{-3,-7},{-2,-7},{-1,-7},

	{0,-8},{1,-8},{2,-8},{3,-8},{4,-8},{5,-8},{6,-8},{7,-8},
	{8,-8},{8,-7},{8,-6},{8,-5},{8,-4},{8,-3},{8,-2},{8,-1},{8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,6},{8,7},
	{8,8},{7,8},{6,8},{5,8},{4,8},{3,8},{2,8},{1,8},{0,8},{-1,8},{-2,8},{-3,8},{-4,8},{-5,8},{-6,8},{-7,8},
	{-8,8},{-8,7},{-8,6},{-8,5},{-8,4},{-8,3},{-8,2},{-8,1},{-8,0},{-8,-1},{-8,-2},{-8,-3},{-8,-4},{-8,-5},{-8,-6},{-8,-7},
	{-8,-8},{-7,-8},{-6,-8},{-5,-8},{-4,-8},{-3,-8},{-2,-8},{-1,-8}
};

#define NROF_MAP_NODE (sizeof(map_pos_array) / (sizeof(int) * 2))

/**
 * Run command. Used to make your character run in specified direction.
 * @param op Object requesting this.
 * @param params Command parameters. */
int command_run(object *op, char *params)
{
	int dir;

	if (!params)
	{
		CONTR(op)->run_on = 1;
		return 0;
	}

	dir = atoi(params);

	if (dir <= 0 || dir > 9)
	{
		draw_info(COLOR_WHITE, op, "Can't run into a non-adjacent square.");
		return 0;
	}

	CONTR(op)->run_on = 1;

	if (dir == 9)
	{
		dir = absdir(op->facing - 4);
	}

	return move_player(op, dir);
}

/**
 * Command to stop running.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_run_stop(object *op, char *params)
{
	(void) params;

	CONTR(op)->run_on = 0;
	return 1;
}

/**
 * Send target command, calculate the target's color level, etc.
 * @param pl Player requesting this. */
void send_target_command(player *pl)
{
	SockList sl;
	unsigned char sockbuf[HUGE_BUF];

	if (!pl->ob->map)
	{
		return;
	}

	sl.buf = sockbuf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_TARGET);
	SockList_AddChar(&sl, pl->combat_mode);

	pl->ob->enemy = NULL;
	pl->ob->enemy_count = 0;

	if (!pl->target_object || pl->target_object == pl->ob || !OBJECT_VALID(pl->target_object, pl->target_object_count) || IS_INVISIBLE(pl->target_object, pl->ob))
	{
		SockList_AddChar(&sl, CMD_TARGET_SELF);
		SockList_AddString(&sl, COLOR_YELLOW);
		SockList_AddString(&sl, pl->ob->name);

		pl->target_object = pl->ob;
		pl->target_object_count = 0;
		pl->target_map_pos = 0;
	}
	else
	{
		const char *color;

		if (is_friend_of(pl->ob, pl->target_object))
		{
			SockList_AddChar(&sl, CMD_TARGET_FRIEND);
		}
		else
		{
			SockList_AddChar(&sl, CMD_TARGET_ENEMY);

			pl->ob->enemy = pl->target_object;
			pl->ob->enemy_count = pl->target_object_count;
		}

		if (pl->target_object->level < level_color[pl->ob->level].yellow)
		{
			if (pl->target_object->level < level_color[pl->ob->level].green)
			{
				color = COLOR_GRAY;
			}
			else
			{
				if (pl->target_object->level < level_color[pl->ob->level].blue)
				{
					color = COLOR_GREEN;
				}
				else
				{
					color = COLOR_BLUE;
				}
			}
		}
		else
		{
			if (pl->target_object->level >= level_color[pl->ob->level].purple)
			{
				color = COLOR_PURPLE;
			}
			else if (pl->target_object->level >= level_color[pl->ob->level].red)
			{
				color = COLOR_RED;
			}
			else if (pl->target_object->level >= level_color[pl->ob->level].orange)
			{
				color = COLOR_ORANGE;
			}
			else
			{
				color = COLOR_YELLOW;
			}
		}

		SockList_AddString(&sl, color);

		if (QUERY_FLAG(pl->ob, FLAG_WIZ))
		{
			char buf[MAX_BUF];

			snprintf(buf, sizeof(buf), "%s (lvl %d)", pl->target_object->name, pl->target_object->level);
			SockList_AddString(&sl, buf);
		}
		else
		{
			SockList_AddString(&sl, pl->target_object->name);
		}
	}

	Send_With_Handling(&pl->socket, &sl);
}

/**
 * Turn combat mode on/off.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_combat(object *op, char *params)
{
	(void) params;

	if (!op || !op->map || op->type != PLAYER || !CONTR(op))
	{
		return 1;
	}

	if (CONTR(op)->combat_mode)
	{
		CONTR(op)->combat_mode = 0;
	}
	else
	{
		CONTR(op)->combat_mode = 1;
		CONTR(op)->praying = 0;
	}

	send_target_command(CONTR(op));
	return 1;
}

/**
 * Look for a target.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_target(object *op, char *params)
{
	mapstruct *m;
	object *tmp = NULL, *head;
	int jump_in, jump_in_n = 0, get_ob_flag;
	int n, nt, xt, yt, block;

	if (!op || !op->map || op->type != PLAYER || !CONTR(op) || !params || params[0] == '\0')
	{
		return 1;
	}

	/* !x y = mouse map target */
	if (params[0] == '!')
	{
		int x, y, i;

		/* Try to get the x/y for the target. */
		if (sscanf(params + 1, "%d %d", &x, &y) != 2)
		{
			return 0;
		}

		/* Validate the passed x/y. */
		if (x < 0 || x >= CONTR(op)->socket.mapx || y < 0 || y >= CONTR(op)->socket.mapy)
		{
			return 0;
		}

		for (i = 0; i <= SIZEOFFREE1; i++)
		{
			/* Check whether we are still in range of the player's
			 * viewport, and whether the player can see the square. */
			if (x + freearr_x[i] < 0 || x + freearr_x[i] >= CONTR(op)->socket.mapx || y + freearr_y[i] < 0 || y + freearr_y[i] >= CONTR(op)->socket.mapy || CONTR(op)->blocked_los[x + freearr_x[i]][y + freearr_y[i]] > BLOCKED_LOS_BLOCKSVIEW)
			{
				continue;
			}

			/* The x/y we got above is from the client's map, so 0,0 is
			 * actually topmost (northwest) corner of the map in the client,
			 * and not 0,0 of the actual map, so we need to transform it to
			 * actual map coordinates. */
			xt = op->x + (x - CONTR(op)->socket.mapx_2) + freearr_x[i];
			yt = op->y + (y - CONTR(op)->socket.mapy_2) + freearr_y[i];
			m = get_map_from_coord(op->map, &xt, &yt);

			/* Invalid x/y. */
			if (!m)
			{
				continue;
			}

			/* Nothing alive on this spot. */
			if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER)))
			{
				continue;
			}

			/* Try to find an alive object here. */
			for (tmp = GET_MAP_OB_LAYER(m, xt, yt, LAYER_LIVING, 0); tmp && tmp->layer == LAYER_LIVING; tmp = tmp->above)
			{
				head = HEAD(tmp);

				if (!IS_LIVE(head) || head == CONTR(op)->target_object || head == op || IS_INVISIBLE(head, op) || OBJECT_IS_HIDDEN(op, head))
				{
					continue;
				}

				CONTR(op)->target_object = head;
				CONTR(op)->target_object_count = head->count;
				CONTR(op)->target_map_pos = i;
				send_target_command(CONTR(op));
				return 1;
			}
		}

		return 0;
	}
	else if (params[0] == '0')
	{
		/* if our target before was a non enemy, start new search
		 * if it was an enemy, use old value. */
		n = 0;
		nt = -1;

		/* lets search for enemy object! */
		if (CONTR(op)->target_object && OBJECT_ACTIVE(CONTR(op)->target_object) && CONTR(op)->target_object_count == CONTR(op)->target_object->count && !is_friend_of(op, CONTR(op)->target_object))
		{
			n = CONTR(op)->target_map_pos;
		}
		else
		{
			CONTR(op)->target_object = NULL;
		}

		for (; n < (int) NROF_MAP_NODE && n != nt; n++)
		{
			int xx, yy;

			if (nt == -1)
			{
				nt = n;
			}

			xt = op->x + (xx = map_pos_array[n][MAP_POS_X]);
			yt = op->y + (yy = map_pos_array[n][MAP_POS_Y]);
			block = CONTR(op)->blocked_los[xx + CONTR(op)->socket.mapx_2][yy + CONTR(op)->socket.mapy_2];

			if (block > BLOCKED_LOS_BLOCKSVIEW || !(m = get_map_from_coord(op->map, &xt, &yt)))
			{
				if ((n + 1) == NROF_MAP_NODE)
				{
					n = -1;
				}

				continue;
			}

			/* we can have more as one possible target
			 * on a square - but i try this first without
			 * handle it. */
			for (tmp = GET_MAP_OB(m, xt, yt); tmp != NULL; tmp = tmp->above)
			{
				/* this is a possible target */
				/* ensure we have head */
				tmp->head != NULL ? (head = tmp->head) : (head = tmp);

				if (IS_LIVE(head) && !is_friend_of(op, head))
				{
					/* this can happen when our old target has moved to next position */
					if (head == CONTR(op)->target_object || head == op || QUERY_FLAG(head, FLAG_SYS_OBJECT) || (QUERY_FLAG(head, FLAG_IS_INVISIBLE) && !QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) || OBJECT_IS_HIDDEN(op, head))
					{
						continue;
					}

					CONTR(op)->target_object = head;
					CONTR(op)->target_object_count = head->count;
					CONTR(op)->target_map_pos = n;
					goto found_target;
				}
			}

			/* force a full loop */
			if ((n + 1) == NROF_MAP_NODE)
			{
				n = -1;
			}
		}
	}
	/* friend */
	else if (params[0] == '1')
	{
		/* if /target friend but old target was enemy - target self first */
		if (CONTR(op)->target_object && OBJECT_ACTIVE(CONTR(op)->target_object) && CONTR(op)->target_object_count == CONTR(op)->target_object->count && !is_friend_of(op, CONTR(op)->target_object))
		{
			CONTR(op)->target_object = op;
			CONTR(op)->target_object_count = op->count;
			CONTR(op)->target_map_pos = 0;
		}
		/* OK - search for a friendly object now! */
		else
		{
			/* if our target before was a non enemy, start new search
			 * if it was an enemy, use old value. */
			n = 0;
			nt = -1;

			/* lets search for last friendly object position! */
			if (CONTR(op)->target_object == op)
			{
				get_ob_flag = 0;
				jump_in = 1;
				jump_in_n = n;
				tmp = op->above;
			}
			else if (OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && is_friend_of(op, CONTR(op)->target_object))
			{
				get_ob_flag = 0;
				jump_in = 1;
				n = CONTR(op)->target_map_pos;
				jump_in_n = n;
				tmp = CONTR(op)->target_object->above;
			}
			else
			{
				n = 1;
				CONTR(op)->target_object = NULL;
				jump_in = 0;
				get_ob_flag = 1;
			}

			for (; n < (int) NROF_MAP_NODE && n != nt; n++)
			{
				int xx, yy;

				if (nt == -1)
				{
					nt = n;
				}

dirty_jump_in1:
				xt = op->x + (xx = map_pos_array[n][MAP_POS_X]);
				yt = op->y + (yy = map_pos_array[n][MAP_POS_Y]);
				block = CONTR(op)->blocked_los[xx + CONTR(op)->socket.mapx_2][yy + CONTR(op)->socket.mapy_2];

				if (block > BLOCKED_LOS_BLOCKSVIEW || !(m = get_map_from_coord(op->map, &xt, &yt)))
				{
					if ((n + 1) == NROF_MAP_NODE)
					{
						n = -1;
					}

					continue;
				}

				/* we can have more as one possible target
				 * on a square - but i try this first without
				 * handle it. */
				if (get_ob_flag)
				{
					tmp = GET_MAP_OB(m, xt, yt);
				}

				for (get_ob_flag = 1; tmp != NULL; tmp = tmp->above)
				{
					/* this is a possible target */
					/* ensure we have head */
					tmp->head != NULL ? (head = tmp->head) : (head = tmp);

					if (IS_LIVE(head) && is_friend_of(op, head))
					{
						/* this can happen when our old target has moved to next position
						 * i have no tmp == op here to allow self targeting in the friendly chain */
						if (head == CONTR(op)->target_object || QUERY_FLAG(head, FLAG_SYS_OBJECT) || (QUERY_FLAG(head, FLAG_IS_INVISIBLE) && !QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) || OBJECT_IS_HIDDEN(op, head))
						{
							continue;
						}

						CONTR(op)->target_object = head;
						CONTR(op)->target_object_count = head->count;
						CONTR(op)->target_map_pos = n;
						goto found_target;
					}
				}

				/* force a full loop */
				if ((n + 1) == NROF_MAP_NODE)
				{
					n = -1;
				}
			}

			/* force another dirty jump ;) */
			if (jump_in)
			{
				n = jump_in_n;
				jump_in = 0;

				if ((n + 1) == NROF_MAP_NODE)
				{
					nt = -1;
				}
				else
				{
					nt = n;
				}

				goto dirty_jump_in1;
			}
		}
	}
	/* self */
	else if (params[0] == '2')
	{
		CONTR(op)->target_object = op;
		CONTR(op)->target_object_count = op->count;
		CONTR(op)->target_map_pos = 0;
	}
	/* TODO: OK... try to use params as a name */
	else
	{
		/* still not sure we need this.. perhaps for groups? */
		/* dummy */
		CONTR(op)->target_object = NULL;
	}

found_target:

	send_target_command(CONTR(op));
	return 1;
}

/**
 * This loads the first map an puts the player on it.
 * @param op The player object. */
static void set_first_map(object *op)
{
	object *current;

	strcpy(CONTR(op)->maplevel, first_map_path);
	op->x = -1;
	op->y = -1;

	if (!strcmp(first_map_path, "/tutorial"))
	{
		current = get_object();
		FREE_AND_COPY_HASH(EXIT_PATH(current), first_map_path);
		EXIT_X(current) = 1;
		EXIT_Y(current) = 1;
		current->last_eat = MAP_PLAYER_MAP;
		enter_exit(op, current);
		/* Update save bed position, so if we die, we don't end up in
		 * the public version of the map. */
		strncpy(CONTR(op)->savebed_map, CONTR(op)->maplevel, sizeof(CONTR(op)->savebed_map) - 1);
	}
	else
	{
		enter_exit(op, NULL);
	}

	/* Update save bed X/Y in any case. */
	CONTR(op)->bed_x = op->x;
	CONTR(op)->bed_y = op->y;
}

/**
 * Information about a character the player may choose. */
typedef struct new_char_struct
{
	/** Archetype of the player. */
	char arch[MAX_BUF];

	/**
	 * Maximum number of points the player can allocate to their
	 * character's stats. */
	int points_max;

	/** Base values of stats for this character. */
	int stats_base[NUM_STATS];

	/** Minimum values of stats for this character. */
	int stats_min[NUM_STATS];

	/** Maximum values of stats for this character. */
	int stats_max[NUM_STATS];
} new_char_struct;

/** All the loaded characters. */
static new_char_struct *new_chars = NULL;
/** Number of ::new_chars. */
static size_t num_new_chars = 0;

/**
 * Initialize ::new_chars by reading server_settings file. */
void new_chars_init(void)
{
	char filename[HUGE_BUF], buf[HUGE_BUF];
	FILE *fp;
	size_t added = 0, i;

	/* Open the server_settings file. */
	snprintf(filename, sizeof(filename), "%s/server_settings", settings.localdir);
	fp = fopen(filename, "r");

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		/* New race; added keeps track of how many archetypes have been
		 * added since the last new. */
		if (!strncmp(buf, "char ", 5))
		{
			added = 0;
		}
		/* Add new archetype for this race. */
		else if (!strncmp(buf, "gender ", 7))
		{
			char gender[MAX_BUF], arch[MAX_BUF], face[MAX_BUF];

			/* Parse the line. */
			if (sscanf(buf + 7, "%s %s %s", gender, arch, face) != 3)
			{
				LOG(llevError, "Bogus line in %s: %s\n", filename, buf);
			}

			new_chars = realloc(new_chars, sizeof(*new_chars) * (num_new_chars + 1));
			strncpy(new_chars[num_new_chars].arch, arch, sizeof(new_chars[num_new_chars].arch) - 1);
			new_chars[num_new_chars].arch[sizeof(new_chars[num_new_chars].arch) - 1] = '\0';
			num_new_chars++;
			added++;
		}
		/* Data that applies to any gender archetype of this race. */
		else if (!strncmp(buf, "points_max ", 11) || !strncmp(buf, "stats_", 6))
		{
			/* Start from the end of the array. */
			for (i = num_new_chars - 1; ; i--)
			{
				if (!strncmp(buf, "points_max ", 11))
				{
					new_chars[i].points_max = atoi(buf + 11);
				}
				else if (!strncmp(buf, "stats_base ", 11) && sscanf(buf + 11, "%d %d %d %d %d %d %d", &new_chars[i].stats_base[STR], &new_chars[i].stats_base[DEX], &new_chars[i].stats_base[CON], &new_chars[i].stats_base[INT], &new_chars[i].stats_base[WIS], &new_chars[i].stats_base[POW], &new_chars[i].stats_base[CHA]) != NUM_STATS)
				{
					LOG(llevError, "Bogus line in %s: %s\n", filename, buf);
				}
				else if (!strncmp(buf, "stats_min ", 10) && sscanf(buf + 10, "%d %d %d %d %d %d %d", &new_chars[i].stats_min[STR], &new_chars[i].stats_min[DEX], &new_chars[i].stats_min[CON], &new_chars[i].stats_min[INT], &new_chars[i].stats_min[WIS], &new_chars[i].stats_min[POW], &new_chars[i].stats_min[CHA]) != NUM_STATS)
				{
					LOG(llevError, "Bogus line in %s: %s\n", filename, buf);
				}
				else if (!strncmp(buf, "stats_max ", 10) && sscanf(buf + 10, "%d %d %d %d %d %d %d", &new_chars[i].stats_max[STR], &new_chars[i].stats_max[DEX], &new_chars[i].stats_max[CON], &new_chars[i].stats_max[INT], &new_chars[i].stats_max[WIS], &new_chars[i].stats_max[POW], &new_chars[i].stats_max[CHA]) != NUM_STATS)
				{
					LOG(llevError, "Bogus line in %s: %s\n", filename, buf);
				}

				/* Check if we have reached the total number of gender
				 * archetypes added for this race. */
				if (i == num_new_chars - added)
				{
					break;
				}
			}
		}
	}

	fclose(fp);
}

/**
 * Client sent us a new char creation.
 *
 * At this point we know the player's name and the password but nothing
 * about his (player char) base arch.
 *
 * This command tells us which the player has selected and how he has
 * setup the stats.
 *
 * If <b>anything</b> is not correct here, we kill this socket.
 * @param params Parameters.
 * @param len Length.
 * @param pl Player structure. */
void command_new_char(char *params, int len, player *pl)
{
	archetype *player_arch;
	const char *name_tmp = NULL;
	object *op = pl->ob;
	int x = pl->ob->x, y = pl->ob->y;
	int stats[NUM_STATS];
	size_t i, j;
	char arch[HUGE_BUF] = "";

	/* Ignore the command if the player is already playing. */
	if (pl->state == ST_PLAYING)
	{
		return;
	}

	/* Incorrect state... */
	if (pl->state != ST_ROLL_STAT)
	{
		LOG(llevSystem, "command_new_char(): %s does not have state ST_ROLL_STAT.\n", pl->ob->name);
		pl->socket.status = Ns_Dead;
		return;
	}

	/* Make sure there is some data to process for this command, and
	 * actually process the data. */
	if (!params || !len || len > MAX_BUF || sscanf(params, "%s %d %d %d %d %d %d %d\n", arch, &stats[STR], &stats[DEX], &stats[CON], &stats[INT], &stats[WIS], &stats[POW], &stats[CHA]) != 8)
	{
		pl->socket.status = Ns_Dead;
		return;
	}

	player_arch = find_archetype(arch);

	/* Invalid player arch? */
	if (!player_arch || player_arch->clone.type != PLAYER)
	{
		LOG(llevSystem, "command_new_char(): %s tried to make a character with invalid player arch.\n", pl->ob->name);
		pl->socket.status = Ns_Dead;
		return;
	}

	LOG(llevInfo, "NewChar: %s: ARCH: %s (%d %d %d %d %d %d %d)\n", pl->ob->name, arch, stats[STR], stats[DEX], stats[CON], stats[INT], stats[WIS], stats[POW], stats[CHA]);

	for (i = 0; i < num_new_chars; i++)
	{
		if (!strcmp(arch, new_chars[i].arch))
		{
			break;
		}
	}

	if (i == num_new_chars)
	{
		LOG(llevSystem, "command_new_char(): %s tried to make a character with valid player arch (%s), but the arch is not defined in server_settings file.\n", pl->ob->name, arch);
		pl->socket.status = Ns_Dead;
		return;
	}

	/* Ensure all stat points have been allocated. */
	if (stats[STR] + stats[DEX] + stats[CON] + stats[INT] + stats[WIS] + stats[POW] + stats[CHA] != new_chars[i].stats_min[STR] + new_chars[i].stats_min[DEX] + new_chars[i].stats_min[CON] + new_chars[i].stats_min[INT] + new_chars[i].stats_min[WIS] + new_chars[i].stats_min[POW] + new_chars[i].stats_min[CHA] + new_chars[i].points_max)
	{
		LOG(llevSystem, "command_new_char(): %s didn't allocate all stat points (player arch: %s) (stats: %d, %d, %d, %d, %d, %d, %d).\n", pl->ob->name, arch, stats[STR], stats[DEX], stats[CON], stats[INT], stats[WIS], stats[POW], stats[CHA]);
		pl->socket.status = Ns_Dead;
		return;
	}

	/* Make sure all the stats are in a valid range. */
	for (j = 0; j < NUM_STATS; j++)
	{
		if (stats[j] < new_chars[i].stats_min[j])
		{
			LOG(llevSystem, "command_new_char(): %s tried to allocate too few points to %s (min: %d).", pl->ob->name, statname[j], new_chars[i].stats_min[j]);
			pl->socket.status = Ns_Dead;
			return;
		}
		else if (stats[j] > new_chars[i].stats_max[j])
		{
			LOG(llevSystem, "command_new_char(): %s tried to allocate too many points to %s (max: %d).", pl->ob->name, statname[j], new_chars[i].stats_max[j]);
			pl->socket.status = Ns_Dead;
			return;
		}
	}

	FREE_AND_ADD_REF_HASH(name_tmp, op->name);
	copy_object(&player_arch->clone, op, 0);
	op->custom_attrset = pl;
	pl->ob = op;
	FREE_AND_CLEAR_HASH2(op->name);
	op->name = name_tmp;
	op->x = x;
	op->y = y;
	/* So the player faces east. */
	op->direction = op->anim_last_facing = op->anim_last_facing_last = op->facing = 3;
	/* We assume that players always have a valid animation. */
	SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);

	pl->orig_stats.Str = stats[STR];
	pl->orig_stats.Dex = stats[DEX];
	pl->orig_stats.Con = stats[CON];
	pl->orig_stats.Int = stats[INT];
	pl->orig_stats.Wis = stats[WIS];
	pl->orig_stats.Pow = stats[POW];
	pl->orig_stats.Cha = stats[CHA];

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);
	/* This must before then initial items are given. */
	esrv_new_player(CONTR(op), op->weight + op->carrying);

	/* Trigger the global BORN event */
	trigger_global_event(GEVENT_BORN, op, NULL);

	/* Trigger the global LOGIN event */
	trigger_global_event(GEVENT_LOGIN, CONTR(op), CONTR(op)->socket.host);

	CONTR(op)->state = ST_PLAYING;
	FREE_AND_CLEAR_HASH2(op->msg);

#ifdef AUTOSAVE
	CONTR(op)->last_save_tick = pticks;
#endif

	display_motd(op);

	if (!CONTR(op)->dm_stealth)
	{
		draw_info_flags_format(NDI_ALL, COLOR_DK_ORANGE, op, "%s entered the game.", op->name);
	}

	CLEAR_FLAG(op, FLAG_WIZ);
	init_player_exp(op);
	give_initial_items(op, op->randomitems);
	link_player_skills(op);
	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
	/* Force sending of skill exp data to client */
	CONTR(op)->last_stats.exp = 1;
	fix_player(op);
	esrv_update_item(UPD_FACE, op);
	esrv_send_inventory(op, op);

	set_first_map(op);
	SET_FLAG(op, FLAG_FRIENDLY);

	CONTR(op)->socket.update_tile = 0;
	CONTR(op)->socket.look_position = 0;
	CONTR(op)->socket.ext_title_flag = 1;
	esrv_new_player(CONTR(op), op->weight + op->carrying);
	send_skilllist_cmd(op, NULL, SPLIST_MODE_ADD);
	send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);
}

/**
 * Determine spell's path for spell list sending.
 * @param op Object.
 * @param spell_number Spell ID.
 * @retval d Player is denied from casting the spell.
 * @retval a Player is attuned to the spell.
 * @retval r Player is repelled from the spell.
 * @return If none of the above, a space character is returned. */
static char spelllist_determine_path(object *op, int spell_number)
{
	uint32 path = spells[spell_number].path;

	if ((op->path_denied & path))
	{
		return 'd';
	}

	if ((op->path_attuned & path) && !(op->path_repelled & path))
	{
		return 'a';
	}

	if ((op->path_repelled & path) && !(op->path_attuned & path))
	{
		return 'r';
	}

	return ' ';
}

/**
 * Helper function for send_spelllist_cmd(), adds one spell to buffer
 * which is then sent to the client as the spell list command.
 * @param op Object.
 * @param spell_number ID of the spell to add.
 * @param sb StringBuffer instance to add to. */
static void add_spell_to_spelllist(object *op, int spell_number, StringBuffer *sb)
{
	int cost = 0;

	/* Determine cost of the spell */
	if (spells[spell_number].type == SPELL_TYPE_PRIEST && CONTR(op)->skill_ptr[SK_PRAYING])
	{
		cost = SP_level_spellpoint_cost(op, spell_number, CONTR(op)->skill_ptr[SK_PRAYING]->level);
	}
	else if (spells[spell_number].type == SPELL_TYPE_WIZARD && CONTR(op)->skill_ptr[SK_SPELL_CASTING])
	{
		cost = SP_level_spellpoint_cost(op, spell_number, CONTR(op)->skill_ptr[SK_SPELL_CASTING]->level);
	}

	stringbuffer_append_printf(sb, "/%s:%d:%c", spells[spell_number].name, cost, spelllist_determine_path(op, spell_number));
}

/**
 * Send spell list command to the client.
 * @param op Player.
 * @param spellname If specified, send only this spell name. Otherwise
 * send all spells the player knows.
 * @param mode One of @ref spelllist_modes. */
void send_spelllist_cmd(object *op, const char *spellname, int mode)
{
	StringBuffer *sb = stringbuffer_new();
	char *cp;
	size_t cp_len;

	stringbuffer_append_printf(sb, "X%d ", mode);

	/* Send single name */
	if (spellname)
	{
		add_spell_to_spelllist(op, look_up_spell_name(spellname), sb);
	}
	/* Send all. If the player is a wizard, send all spells in the game. */
	else
	{
		int i, spnum, num_spells = QUERY_FLAG(op, FLAG_WIZ) ? NROFREALSPELLS : CONTR(op)->nrofknownspells;

		for (i = 0; i < num_spells; i++)
		{
			if (QUERY_FLAG(op, FLAG_WIZ))
			{
				spnum = i;
			}
			else
			{
				spnum = CONTR(op)->known_spells[i];
			}

			add_spell_to_spelllist(op, spnum, sb);
		}
	}

	cp_len = sb->pos;
	cp = stringbuffer_finish(sb);
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_SPELL_LIST, cp, cp_len);
	free(cp);
}

/**
 * Send skill list to the client.
 * @param op Object.
 * @param skillp Skill object; if not NULL, will only send this skill.
 * @param mode One of @ref spelllist_modes. */
void send_skilllist_cmd(object *op, object *skillp, int mode)
{
	StringBuffer *sb = stringbuffer_new();
	char *cp;
	size_t cp_len;

	stringbuffer_append_printf(sb, "X%d ", mode);

	if (skillp)
	{
		add_skill_to_skilllist(skillp, sb);
	}
	else
	{
		int i;

		for (i = 0; i < NROFSKILLS; i++)
		{
			if (CONTR(op)->skill_ptr[i])
			{
				add_skill_to_skilllist(CONTR(op)->skill_ptr[i], sb);
			}
		}
	}

	cp_len = sb->pos;
	cp = stringbuffer_finish(sb);
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_SKILL_LIST, cp, cp_len);
	free(cp);
}

/**
 * Send skill ready command.
 * @param op Object.
 * @param skillname Name of skill to ready. */
void send_ready_skill(object *op, const char *skillname)
{
	char tmp[MAX_BUF];

	snprintf(tmp, sizeof(tmp), "X%s", skillname);
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_SKILLRDY, tmp, strlen(tmp));
}

/**
 * Generate player's extended name from race, gender, guild, etc.
 * @param pl The player. */
void generate_ext_title(player *pl)
{
	object *walk;
	char prof[32] = "";
	char title[32] = "";
	char rank[32] = "";
	char align[32] = "";
	char race[MAX_BUF];
	char name[MAX_BUF];
	shstr *godname;

	for (walk = pl->ob->inv; walk; walk = walk->below)
	{
		if (!walk->name || !walk->arch->name)
		{
			LOG(llevDebug, "Object in %s doesn't have name/archname! (%s:%s)\n", pl->ob->name, STRING_SAFE(walk->name), STRING_SAFE(walk->arch->name));
			continue;
		}

		if (walk->name == shstr_cons.GUILD_FORCE && walk->arch->name == shstr_cons.guild_force)
		{
			if (walk->slaying)
			{
				strcpy(prof, walk->slaying);
			}

			if (walk->title)
			{
				strcpy(title, " the ");
				strcat(title, walk->title);
			}
		}
		else if (walk->name == shstr_cons.RANK_FORCE && walk->arch->name == shstr_cons.rank_force)
		{
			if (walk->title)
			{
				strcpy(rank, walk->title);
				strcat(rank, " ");
			}
		}
		else if (walk->name == shstr_cons.ALIGNMENT_FORCE && walk->arch->name == shstr_cons.alignment_force)
		{
			if (walk->title)
			{
				strcpy(align, walk->title);
			}
		}
	}

	strcpy(pl->quick_name, rank);
	strcat(pl->quick_name, pl->ob->name);

	if (QUERY_FLAG(pl->ob, FLAG_WIZ))
	{
		strcat(pl->quick_name, " [WIZ]");
	}

	snprintf(name, sizeof(name), "%s%s%s", rank, pl->ob->name, title);

	if (QUERY_FLAG(pl->ob, FLAG_WIZ))
	{
		strncat(name, " [WIZ]", sizeof(name) - strlen(name) - 1);
	}

	if (pl->afk)
	{
		strncat(name, " [AFK]", sizeof(name) - strlen(name) - 1);
	}

	snprintf(pl->ext_title, sizeof(pl->ext_title), "%s\n%s %s %s\n%s", name, gender_noun[object_get_gender(pl->ob)], player_get_race_class(pl->ob, race, sizeof(race)), prof, align);

	godname = determine_god(pl->ob);

	if (godname)
	{
		strncat(pl->ext_title, " follower of ", sizeof(pl->ext_title) - strlen(pl->ext_title) - 1);
		strncat(pl->ext_title, godname, sizeof(pl->ext_title) - strlen(pl->ext_title) - 1);
	}
}
