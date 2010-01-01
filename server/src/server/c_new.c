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
 * This file deals with administrative commands from the client. */

#include <global.h>
#include <sproto.h>

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
	int dir = 0;
	CONTR(op)->run_on = 1;

	if (params)
	{
		dir = atoi(params);

		/* Check the direction */
		if (dir > 9)
		{
			dir = 0;
		}
		else if (dir < 0)
		{
			dir = 0;
		}
	}

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
	int aim_self_flag = FALSE;
	char tmp[256];

	if (!pl->ob->map)
	{
		return;
	}

	tmp[0] = BINARY_CMD_TARGET;
	tmp[1] = pl->combat_mode;
	/* Color mode */
	tmp[2] = 0;

	pl->ob->enemy = NULL;
	pl->ob->enemy_count = 0;
	/* Target still legal? */
	/* thats we self */
	if (!pl->target_object || !OBJECT_ACTIVE(pl->target_object) || pl->target_object == pl->ob)
	{
		aim_self_flag = TRUE;
	}
	else if (pl->target_object_count == pl->target_object->count)
	{
		/* ok, a last check... i put it here to have clear code:
		 * perhaps we have legal issues why we can't aim or attack
		 * our target anymore... invisible & stuff are handled here.
		 * stuff like a out of pvp area moved player are handled different.
		 * we HOLD the target - perhaps the guy moved back.
		 * this special stuff is handled deeper in attack() functions. */
		if (QUERY_FLAG(pl->target_object, FLAG_SYS_OBJECT) || (QUERY_FLAG(pl->target_object, FLAG_IS_INVISIBLE) && !QUERY_FLAG(pl->ob, FLAG_SEE_INVISIBLE)))
		{
			aim_self_flag = TRUE;
		}
		else
		{
			/* friend */
			if ((pl->target_object->type == PLAYER && !pvp_area(pl->ob, pl->target_object)) || (QUERY_FLAG(pl->target_object, FLAG_FRIENDLY) && pl->target_object->type != PLAYER))
			{
				tmp[3] = 2;
			}
			/* enemy */
			else
			{
				tmp[3] = 1;
				pl->ob->enemy = pl->target_object;
				pl->ob->enemy_count = pl->target_object_count;
			}

			if (pl->target_object->name)
			{
				strcpy(tmp + 4, pl->target_object->name);
			}
			else
			{
				strcpy(tmp + 4, "(null)");
			}
		}
	}
	else
	{
		aim_self_flag = TRUE;
	}

	/* ok... at last, target self */
	if (aim_self_flag)
	{
		/* self */
		tmp[3] = 0;
		strcpy(tmp + 4, pl->ob->name);
		pl->target_object = pl->ob;
		pl->target_object_count = 0;
		pl->target_map_pos = 0;
	}

	/* now we have a target - lets calculate the color code.
	 * we can do it easy and send the real level to client and
	 * let calc it there but this will allow to spoil that
	 * data on client side. */
	/* target is lower */
	if (pl->target_object->level < level_color[pl->ob->level].yellow)
	{
		/* if < the green border value, the mob is grey */
		if (pl->target_object->level < level_color[pl->ob->level].green)
		{
			tmp[2] = NDI_GREY;
		}
		/* calc green or blue */
		else
		{
			if (pl->target_object->level < level_color[pl->ob->level].blue)
			{
				tmp[2] = NDI_GREEN;
			}
			else
			{
				tmp[2] = NDI_BLUE;
			}
		}

	}
	/* target is higher or as yellow min. range */
	else
	{
		if (pl->target_object->level >= level_color[pl->ob->level].purple)
		{
			tmp[2] = NDI_PURPLE;
		}
		else if (pl->target_object->level >= level_color[pl->ob->level].red)
		{
			tmp[2] = NDI_RED;
		}
		else if (pl->target_object->level >= level_color[pl->ob->level].orange)
		{
			tmp[2] = NDI_ORANGE;
		}
		else
		{
			tmp[2] = NDI_YELLOW;
		}
	}

	/* Some nice extra info for DMs */
	if (QUERY_FLAG(pl->ob, FLAG_WIZ))
	{
		char buf[64];

		snprintf(buf, sizeof(buf), " (lvl %d)", pl->target_object->level);
		strcat(tmp + 4, buf);
	}

	Write_String_To_Socket(&pl->socket, BINARY_CMD_TARGET, tmp, strlen(tmp + 4) + 4);
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

	if (!op || !op->map || op->type != PLAYER || !CONTR(op) || !params || params[0] == 0)
	{
		return 1;
	}

	/* !x y = mouse map target */
	if (params[0] == '!')
	{
		int xstart, ystart;
		char *ctmp;

		xstart = atoi(params + 1);

		ctmp = strchr(params + 1, ' ');

		/* bad format.. skip */
		if (!ctmp)
		{
			return 0;
		}

		ystart = atoi(ctmp + 1);

		for (n = 0; n < SIZEOFFREE; n++)
		{
			int xx, yy;

			/* thats the trick: we get  op map pos, but we have 2 offsets:
			 * the offset from the client mouse click - can be
			 * +- CONTR(op)->socket.mapx/2 - and the freearr_x/y offset for
			 * the search. */
			xt = op->x + (xx = freearr_x[n] + xstart);
			yt = op->y + (yy = freearr_y[n] + ystart);

			if (xx < -(int) (CONTR(op)->socket.mapx_2) || xx > (int) (CONTR(op)->socket.mapx_2) || yy < -(int) (CONTR(op)->socket.mapy_2) || yy > (int) (CONTR(op)->socket.mapy_2))
			{
				continue;
			}

			block = CONTR(op)->blocked_los[xx + CONTR(op)->socket.mapx_2][yy + CONTR(op)->socket.mapy_2];

			if (block > BLOCKED_LOS_BLOCKSVIEW || !(m = get_map_from_coord(op->map, &xt, &yt)))
			{
				continue;
			}

			/* we can have more as one possible target
			 * on a square - but i try this first without
			 * handle it. */
			for (tmp = get_map_ob(m, xt, yt); tmp != NULL; tmp = tmp->above)
			{
				/* this is a possible target */
				/* ensure we have head */
				tmp->head != NULL ? (head = tmp->head) : (head = tmp);

				if ((QUERY_FLAG(head, FLAG_MONSTER) || QUERY_FLAG(head, FLAG_FRIENDLY)) || (head->type == PLAYER && pvp_area(op, head)))
				{
					/* this can happen when our old target has moved to next position */
					if (head == CONTR(op)->target_object || head == op || QUERY_FLAG(head, FLAG_SYS_OBJECT) || (QUERY_FLAG(head, FLAG_IS_INVISIBLE) && !QUERY_FLAG(op, FLAG_SEE_INVISIBLE)))
					{
						continue;
					}

					CONTR(op)->target_object = head;
					CONTR(op)->target_object_count = head->count;
					CONTR(op)->target_map_pos = n;
					goto found_target;
				}
			}
		}
	}
	else if (params[0] == '0')
	{
		/* if our target before was a non enemy, start new search
		 * if it was an enemy, use old value. */
		n = 0;
		nt = -1;

		/* lets search for enemy object! */
		if (CONTR(op)->target_object && OBJECT_ACTIVE(CONTR(op)->target_object) && CONTR(op)->target_object_count == CONTR(op)->target_object->count && !QUERY_FLAG(CONTR(op)->target_object, FLAG_FRIENDLY))
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
			for (tmp = get_map_ob(m, xt, yt); tmp != NULL; tmp = tmp->above)
			{
				/* this is a possible target */
				/* ensure we have head */
				tmp->head != NULL ? (head = tmp->head) : (head = tmp);

				if ((QUERY_FLAG(head, FLAG_MONSTER) && !QUERY_FLAG(head, FLAG_FRIENDLY)) || (head->type == PLAYER && pvp_area(op, head)))
				{
					/* this can happen when our old target has moved to next position */
					if (head == CONTR(op)->target_object || head == op || QUERY_FLAG(head, FLAG_SYS_OBJECT) || (QUERY_FLAG(head, FLAG_IS_INVISIBLE) && !QUERY_FLAG(op, FLAG_SEE_INVISIBLE)))
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
		if (CONTR(op)->target_object && OBJECT_ACTIVE(CONTR(op)->target_object) && CONTR(op)->target_object_count == CONTR(op)->target_object->count && !QUERY_FLAG(CONTR(op)->target_object, FLAG_FRIENDLY))
		{
			CONTR(op)->target_object = op;
			CONTR(op)->target_object_count = op->count;
			CONTR(op)->target_map_pos = 0;
		}
		/* ok - search for a friendly object now! */
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
			else if (OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && (QUERY_FLAG(CONTR(op)->target_object, FLAG_FRIENDLY ) || CONTR(op)->target_object->type == PLAYER))
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
					tmp = get_map_ob(m, xt, yt);
				}

				for (get_ob_flag = 1; tmp != NULL; tmp = tmp->above)
				{
					/* this is a possible target */
					/* ensure we have head */
					tmp->head != NULL ? (head = tmp->head) : (head = tmp);

					if ((QUERY_FLAG(head, FLAG_MONSTER) && QUERY_FLAG(head, FLAG_FRIENDLY)) || (head->type == PLAYER && !pvp_area(op, head)))
					{
						/* this can happen when our old target has moved to next position
						 * i have no tmp == op here to allow self targeting in the friendly chain */
						if (head == CONTR(op)->target_object || QUERY_FLAG(head, FLAG_SYS_OBJECT) || (QUERY_FLAG(head, FLAG_IS_INVISIBLE) && !QUERY_FLAG(op, FLAG_SEE_INVISIBLE)))
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
	/* TODO: ok... try to use params as a name */
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
	}
	else
	{
		enter_exit(op, NULL);
	}
}

typedef struct _new_char_template
{
	char *name;
	int max_p;
	int min_Str;
	int max_Str;
	int min_Dex;
	int max_Dex;
	int min_Con;
	int max_Con;
	int min_Int;
	int max_Int;
	int min_Wis;
	int max_Wis;
	int min_Pow;
	int max_Pow;
	int min_Cha;
	int max_Cha;
}_new_char_template;

static _new_char_template new_char_template[] =
{
	{"human_male", 5, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14},
	{"human_female", 5, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14},
	{"half_elf_male", 5, 12, 14, 13, 15, 11, 13, 12, 14, 11, 13, 13, 15, 12, 14},
	{"half_elf_female", 5, 12, 14, 13, 15, 11, 13, 12, 14, 11, 13, 13, 15, 12, 14},
	{NULL, 5, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14, 12, 14}
};

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
	archetype *p_arch = NULL;
	const char *name_tmp = NULL;
	object *op = pl->ob;
	int x = pl->ob->x, y = pl->ob->y;
	int stats[7], i, v;
	char name[HUGE_BUF] = "";

	if (CONTR(op)->state != ST_ROLL_STAT)
	{
		LOG(llevDebug, "CRACK: command_new_char(): %s does not have state ST_ROLL_STAT.\n", query_name(pl->ob, NULL));
		pl->socket.status = Ns_Dead;
		return;
	}

	if (!params || len > MAX_BUF)
	{
		/* Kill socket */
		pl->socket.status = Ns_Dead;
		return;
	}

	sscanf(params, "%s %d %d %d %d %d %d %d\n", name, &stats[0], &stats[1], &stats[2], &stats[3], &stats[4], &stats[5], &stats[6]);

	/* now: we have to verify every *bit* of what the client has sent us */

	/* invalid player arch? */
	if (!(p_arch = find_archetype(name)) || p_arch->clone.type != PLAYER)
	{
		LOG(llevSystem, "CRACK: %s: Invalid player arch!\n", query_name(pl->ob, NULL));
		pl->socket.status = Ns_Dead;
		return;
	}

	LOG(llevDebug, "NewChar: %s:: ARCH: %s (%d %d %d %d %d %d %d)\n", query_name(pl->ob, NULL), name, stats[0], stats[1], stats[2], stats[3], stats[4], stats[5], stats[6]);

	for (i = 0; new_char_template[i].name != NULL; i++)
	{
		if (!strcmp(name, new_char_template[i].name))
		{
			break;
		}
	}

	if (!new_char_template[i].name)
	{
		LOG(llevDebug, "BUG:: %s: NewChar %s not in def table!\n", query_name(pl->ob, NULL), name);
		/* Kill socket */
		pl->socket.status = Ns_Dead;
		return;
	}

	v = new_char_template[i].min_Str + new_char_template[i].min_Dex + new_char_template[i].min_Con + new_char_template[i].min_Int + new_char_template[i].min_Wis + new_char_template[i].min_Pow + new_char_template[i].min_Cha + new_char_template[i].max_p;

	/* All bonus values put on the player? */
	if (v != (stats[0] + stats[1] + stats[2] + stats[3] + stats[4] + stats[5] + stats[6])
			|| stats[0] < new_char_template[i].min_Str || stats[0] > new_char_template[i].max_Str
			|| stats[1] < new_char_template[i].min_Dex || stats[1] > new_char_template[i].max_Dex
			|| stats[2] < new_char_template[i].min_Con || stats[2] > new_char_template[i].max_Con
			|| stats[3] < new_char_template[i].min_Int || stats[3] > new_char_template[i].max_Int
			|| stats[4] < new_char_template[i].min_Wis || stats[4] > new_char_template[i].max_Wis
			|| stats[5] < new_char_template[i].min_Pow || stats[5] > new_char_template[i].max_Pow
			|| stats[6] < new_char_template[i].min_Cha || stats[6] > new_char_template[i].max_Cha)
	{
		LOG(llevDebug, "CRACK: %s: Tried to crack NewChar! (%d - %d)\n", query_name(pl->ob, NULL), i, stats[0] + stats[1] + stats[2] + stats[3] + stats[4] + stats[5] + stats[6]);
		pl->socket.status = Ns_Dead;
		return;
	}

	/* the stats of a player are saved in pl struct and copied to the object */

	/* need to copy the name to new arch */
	FREE_AND_ADD_REF_HASH(name_tmp, op->name);
	copy_object(&p_arch->clone, op);
	op->custom_attrset = pl;
	pl->ob = op;
	CONTR(op)->last_value = -1;
	FREE_AND_CLEAR_HASH2(op->name);
	op->name = name_tmp;
	op->x = x;
	op->y = y;
	/* So player faces south */
	SET_ANIMATION(op, 4 * (NUM_ANIMATIONS(op) / NUM_FACINGS(op)));

	pl->orig_stats.Str = stats[0];
	pl->orig_stats.Dex = stats[1];
	pl->orig_stats.Con = stats[2];
	pl->orig_stats.Int = stats[3];
	pl->orig_stats.Wis = stats[4];
	pl->orig_stats.Pow = stats[5];
	pl->orig_stats.Cha = stats[6];

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);
	/* this must before then initial items are given */
	esrv_new_player(CONTR(op), op->weight + op->carrying);

	/* Trigger the global BORN event */
	trigger_global_event(EVENT_BORN, op, NULL);

	/* Trigger the global LOGIN event */
	trigger_global_event(EVENT_LOGIN, CONTR(op), CONTR(op)->socket.host);

	CONTR(op)->state = ST_PLAYING;
	FREE_AND_CLEAR_HASH2(op->msg);

#ifdef AUTOSAVE
	CONTR(op)->last_save_tick = pticks;
#endif

	display_motd(op);

	if (!CONTR(op)->dm_stealth)
	{
		new_draw_info_format(NDI_UNIQUE | NDI_ALL, 5, op, "%s entered the game.", op->name);

		if (dm_list)
		{
			player *pl_tmp;
			int players;
			objectlink *tmp_dm_list;

			for (pl_tmp = first_player, players = 0; pl_tmp != NULL; pl_tmp = pl_tmp->next, players++)
			{
			}

			for (tmp_dm_list = dm_list; tmp_dm_list != NULL; tmp_dm_list = tmp_dm_list->next)
			{
				new_draw_info_format(NDI_UNIQUE, 0, tmp_dm_list->objlink.ob, "DM: %d players now playing.", players);
			}
		}
	}

	CLEAR_FLAG(op, FLAG_WIZ);
	(void) init_player_exp(op);
	give_initial_items(op, op->randomitems);
	link_player_skills(op);
	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
	/* force send of skill exp data to client */
	CONTR(op)->last_stats.exp = 1;
	/* THATS our first fix_player() when we create a new char
	 * add this time, hp and sp will be set */
	fix_player(op);
	esrv_update_item(UPD_FACE, op, op);
	esrv_send_inventory(op, op);

	/* NOW we set our new char in the right map - we have a 100% right init player */
	set_first_map(op);
	SET_FLAG(op, FLAG_FRIENDLY);
	add_friendly_object(op);

	CONTR(op)->socket.update_tile = 0;
	CONTR(op)->socket.look_position = 0;
	CONTR(op)->socket.ext_title_flag = 1;
	esrv_new_player(CONTR(op), op->weight + op->carrying);
	send_skilllist_cmd(op, NULL, SPLIST_MODE_ADD);
	send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);
}

/**
 * Request a face command.
 * @param params Parameters.
 * @param len Length.
 * @param pl Player. */
void command_face_request(char *params, int len, player *pl)
{
	int i, count;

	(void) len;

	if (!params)
	{
		return;
	}

	count = *(uint8 *) params;

	for (i = 0; i < count; i++)
	{
		if (esrv_send_face(&pl->socket, *((short *) (params + 1) + i), 0) == SEND_FACE_OUT_OF_BOUNDS)
		{
			new_draw_info_format(NDI_UNIQUE | NDI_RED, 0, pl->ob, "CLIENT ERROR: Your client requested bad face (#%d). Connection closed!", *((short*)(params + 1) + i));

			LOG(llevInfo, "CLIENT BUG: command_face_request (%d) out of bounds. Player: %s. Close connection.\n", *((short*)(params + 1) + i), pl->ob ? pl->ob->name : "(->ob <no name>)");

			/* Kill socket */
			pl->socket.status = Ns_Dead;
			return;
		}
	}
}

void command_fire(char *params, int len, player *pl)
{
	int dir = 0, type, tag1, tag2;
	object *op = pl->ob;

	(void) len;

	if (!params)
	{
		return;
	}

	CONTR(op)->fire_on = 1;

	sscanf(params, "%d %d %d %d", &dir, &type, &tag1, &tag2);

	if (type == FIRE_MODE_SPELL)
	{
		char *tmp;
		tag2 = -1;
		tmp = strchr(params, ' ');
		tmp = strchr(tmp + 1, ' ');
		tmp = strchr(tmp + 1, ' ');

		if (strlen(tmp + 1) > 60)
		{
			LOG(llevDebug, "DEBUG: Player %s has sent too long fire command: %s\n", query_name(pl->ob, NULL), tmp + 1);
			return;
		}

		strncpy(CONTR(op)->firemode_name, tmp + 1, 60);

		if (!fire_cast_spell(op, CONTR(op)->firemode_name))
		{
			CONTR(op)->fire_on = 0;
			/* marks no client fire action */
			CONTR(op)->firemode_type = -1;
			return;
		}
	}
	else if (type == FIRE_MODE_SKILL)
	{
		char *tmp;

		tag2 = -1;
		tmp = strchr(params, ' ');
		tmp = strchr(tmp + 1, ' ');
		tmp = strchr(tmp + 1, ' ');

		if (strlen(tmp + 1) > 60)
		{
			LOG(llevDebug, "DEBUG: Player %s has sent too long fire command: %s\n", query_name(pl->ob, NULL), tmp + 1);
			return;
		}

		strncpy(CONTR(op)->firemode_name,tmp + 1, 60);
	}

	/* only here will this value be set */
	CONTR(op)->firemode_type = type;
	CONTR(op)->firemode_tag1 = tag1;
	CONTR(op)->firemode_tag2 = tag2;

	move_player(op, dir);
	CONTR(op)->fire_on = 0;
	/* marks no client fire action */
	CONTR(op)->firemode_type = -1;
}

/**
 * Sends mapstats command to the client, after the player has entered the map.
 *
 * Command sends map width, map height, map name, map music, etc.
 * @param op Player object.
 * @param map Map structure. */
void send_mapstats_cmd(object *op, struct mapdef *map)
{
	char tmp[HUGE_BUF];

	/* player: remember this is the map the client knows */
	CONTR(op)->last_update = map;
	snprintf(tmp, sizeof(tmp), "X%d %d %d %d %s %s", map->width, map->height, op->x, op->y, map->bg_music ? map->bg_music : "no_music", map->name);
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_MAPSTATS, tmp, strlen(tmp));
}

/**
 * Send spell list command to the client.
 * @param op Player object.
 * @param spellname If specified, send only this spell name.
 * Otherwise send all spells the player knows.
 * @param mode Mode. */
void send_spelllist_cmd(object *op, char *spellname, int mode)
{
	/* we should careful set a big enough buffer here */
	char tmp[HUGE_BUF * 4];
	char cost[MAX_BUF];
	object *old_skill = op->chosen_skill, *spell_skill = CONTR(op)->skill_ptr[SK_SPELL_CASTING], *prayer_skill = CONTR(op)->skill_ptr[SK_PRAYING];

	snprintf(tmp, sizeof(tmp), "X%d ", mode);

	/* Send single name */
	if (spellname)
	{
		int spnum = look_up_spell_name(spellname);

		strncat(tmp, "/", sizeof(tmp) - strlen(tmp) - 1);
		strncat(tmp, spellname, sizeof(tmp) - strlen(tmp) - 1);

		if ((spells[spnum].type == SPELL_TYPE_PRIEST && prayer_skill) || (spells[spnum].type == SPELL_TYPE_WIZARD && spell_skill))
		{
			op->chosen_skill = spells[spnum].type == SPELL_TYPE_PRIEST ? prayer_skill : spell_skill;
			strncat(tmp, ":", sizeof(tmp) - strlen(tmp) - 1);
			snprintf(cost, sizeof(cost), "%d", SP_level_spellpoint_cost(op, spnum));
			strncat(tmp, cost, sizeof(tmp) - strlen(tmp) - 1);
		}
	}
	else
	{
		int i, spnum;

		for (i = 0; i < (QUERY_FLAG(op, FLAG_WIZ) ? NROFREALSPELLS : CONTR(op)->nrofknownspells); i++)
		{
			if (QUERY_FLAG(op, FLAG_WIZ))
			{
				spnum = i;
			}
			else
			{
				spnum = CONTR(op)->known_spells[i];
			}

			strncat(tmp, "/", sizeof(tmp) - strlen(tmp) - 1);
			strncat(tmp, spells[spnum].name, sizeof(tmp) - strlen(tmp) - 1);

			if ((spells[spnum].type == SPELL_TYPE_PRIEST && prayer_skill) || (spells[spnum].type == SPELL_TYPE_WIZARD && spell_skill))
			{
				op->chosen_skill = spells[spnum].type == SPELL_TYPE_PRIEST ? prayer_skill : spell_skill;
				strncat(tmp, ":", sizeof(tmp) - strlen(tmp) - 1);
				snprintf(cost, sizeof(cost), "%d", SP_level_spellpoint_cost(op, spnum));
				strncat(tmp, cost, sizeof(tmp) - strlen(tmp) - 1);
			}
		}
	}

	op->chosen_skill = old_skill;
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_SPELL_LIST, tmp, strlen(tmp));
}

/**
 * Send skill list to the client.
 * @param op Player object.
 * @param skillp Skill object.
 * @param mode Mode. */
void send_skilllist_cmd(object *op, object *skillp, int mode)
{
	object *tmp2;
	char buf[256];
	/* we should careful set a big enough buffer here */
	char tmp[HUGE_BUF * 4];

	if (skillp)
	{
		/* Normal skills */
		if (skillp->last_eat == 1)
		{
			snprintf(tmp, sizeof(tmp), "X%d /%s|%d|%d", mode, skillp->name, skillp->level, skillp->stats.exp);
		}
		/* "buy level" skills */
		else if (skillp->last_eat == 2)
		{
			snprintf(tmp, sizeof(tmp), "X%d /%s|%d|-2", mode, skillp->name, skillp->level);
		}
		/* No level skills */
		else
		{
			snprintf(tmp, sizeof(tmp), "X%d /%s|%d|-1", mode, skillp->name, skillp->level);
		}
	}
	else
	{
		snprintf(tmp, sizeof(tmp), "X%d ", mode);

		for (tmp2 = op->inv; tmp2; tmp2 = tmp2->below)
		{
			if (tmp2->type == SKILL && IS_SYS_INVISIBLE(tmp2))
			{
				if (tmp2->last_eat == 1)
				{
					snprintf(buf, sizeof(buf), "/%s|%d|%d", tmp2->name, tmp2->level, tmp2->stats.exp);
				}
				else if (tmp2->last_eat == 2)
				{
					snprintf(buf, sizeof(buf), "/%s|%d|-2", tmp2->name, tmp2->level);
				}
				else
				{
					snprintf(buf, sizeof(buf), "/%s|%d|-1", tmp2->name, tmp2->level);
				}

				strncat(tmp, buf, sizeof(tmp) - strlen(tmp) - 1);
			}
		}
	}

	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_SKILL_LIST, tmp, strlen(tmp));
}

/**
 * Send skill ready command.
 * @param op Player object.
 * @param skillname Name of skill to ready. */
void send_ready_skill(object *op, char *skillname)
{
	/* we should careful set a big enough buffer here */
	char tmp[256];

	snprintf(tmp, sizeof(tmp), "X%s", skillname);
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_SKILLRDY, tmp, strlen(tmp));
}

/**
 * Send to client the golem face and name.
 * @param golem Golem object (will grab golem owner from this).
 * @param mode Mode (release or new golem). */
void send_golem_control(object *golem, int mode)
{
	/* we should careful set a big enough buffer here */
	char tmp[256];

	if (mode == GOLEM_CTR_RELEASE)
	{
		snprintf(tmp, sizeof(tmp), "X%d %d %s", mode, 0, golem->name);
	}
	else
	{
		snprintf(tmp, sizeof(tmp), "X%d %d %s", mode, golem->face->number, golem->name);
	}

	Write_String_To_Socket(&CONTR(golem->owner)->socket, BINARY_CMD_GOLEMCMD, tmp, strlen(tmp));
}

/**
 * Generate player's extended name from race, gender, guild, etc.
 * @param pl The player. */
void generate_ext_title(player *pl)
{
	object *walk;
	char *gender;
	char prof[32] = "";
	char title[32] = "";
	char rank[32] = "";
	char align[32] = "";

	/* collect all information from the force objects. Just walk one time through them*/
	for (walk = pl->ob->inv; walk != NULL; walk = walk->below)
	{
		if (!walk->name || !walk->arch->name)
		{
			LOG(llevDebug, "BUG?: object in %s doesn't have name/archname! (%s:%s)\n", pl->ob->name, walk->name ? walk->name : "<null>", walk->arch->name ? walk->arch->name : "<null>");
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

	if (QUERY_FLAG(pl->ob, FLAG_IS_MALE))
	{
		gender = QUERY_FLAG(pl->ob, FLAG_IS_FEMALE) ? "hermaphrodite" : "male";
	}
	else if (QUERY_FLAG(pl->ob, FLAG_IS_FEMALE))
	{
		gender = "female";
	}
	else
	{
		gender = "neuter";
	}

	strcpy(pl->quick_name, rank);
	strcat(pl->quick_name, pl->ob->name);

	if (QUERY_FLAG(pl->ob, FLAG_WIZ))
	{
		strcat(pl->quick_name, " [WIZ]");
	}

	if (pl->shop_items && QUERY_FLAG(pl->ob, FLAG_PLAYER_SHOP))
	{
		strcat(pl->quick_name, " [SHOP]");
	}

	snprintf(pl->ext_title, sizeof(pl->ext_title), "%s\n%s %s%s%s\n%s\n%s\n%s\n%s\n%c\n", rank, pl->ob->name, title, QUERY_FLAG(pl->ob, FLAG_WIZ) ? (strcmp(title, "") ? " [WIZ] " : "[WIZ] ") : "", pl->afk ? (strcmp(title, "") ? " [AFK]" : "[AFK]") : "", pl->ob->race, prof, align, determine_god(pl->ob), *gender);
}
