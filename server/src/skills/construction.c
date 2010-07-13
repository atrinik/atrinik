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
 * Handles code for the construction skill. */

#include <global.h>

/**
 * Check if objects on a square interfere with building.
 * @param m Map we're building on.
 * @param new_item Item the player is trying to build.
 * @param x X coordinate where to build.
 * @param y Y coordinate where to build.
 * @return 1 if 'new_item' can be built on the spot, 0 otherwise. */
static int can_build_over(mapstruct *m, object *new_item, int x, int y)
{
	object *tmp;

	for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above)
	{
		tmp = HEAD(tmp);

		if (QUERY_FLAG(tmp, FLAG_IS_BUILDABLE))
		{
			continue;
		}

		switch (new_item->type)
		{
			case SIGN:
				/* Allow signs to be built on books. */
				if (tmp->type != BOOK)
				{
					return 0;
				}

				break;

			default:
				return 0;
		}
	}

	/* If item being built is multi-tile, need to check other parts too. */
	if (new_item->more)
	{
		return can_build_over(m, new_item->more, x + new_item->more->arch->clone.x - new_item->arch->clone.x, y + new_item->more->arch->clone.y - new_item->arch->clone.y);
	}

	return 1;
}

/**
 * Find wall on the specified square.
 * @param m Map where to look.
 * @param x X where to look.
 * @param y Y where to look.
 * @return The wall if found, NULL otherwise. */
static object *get_wall(mapstruct *m, int x, int y)
{
	object *tmp;

	for (tmp = GET_MAP_OB_LAYER(m, x, y, 4); tmp && tmp->layer == 5; tmp = tmp->above)
	{
		if (tmp->type == WALL)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Floor building function.
 * @param op Player building.
 * @param new_floor New floor object
 * @param x X where to build.
 * @param y Y where to build.
 * @return 1 if the floor was built, 0 otherwise. */
static int builder_floor(object *op, object *new_floor, int x, int y)
{
	object *tmp;

	for (tmp = GET_MAP_OB_LAYER(op->map, x, y, 0); tmp && tmp->layer == 1; tmp = tmp->above)
	{
		if (tmp->type == FLOOR || QUERY_FLAG(tmp, FLAG_IS_FLOOR))
		{
			if (tmp->arch == new_floor->arch)
			{
				new_draw_info(NDI_UNIQUE, op, "You feel too lazy to redo the exact same floor.");
				return 0;
			}

			remove_ob(tmp);
			break;
		}
	}

	SET_FLAG(new_floor, FLAG_IS_FLOOR);
	new_floor->type = FLOOR;
	new_floor->x = x;
	new_floor->y = y;
	insert_ob_in_map(new_floor, op->map, NULL, 0);
	new_draw_info(NDI_UNIQUE, op, "You change the floor to better suit your tastes.");

	return 1;
}

/**
 * Build an item like sign, wall mask (fireplace), etc.
 * @param op Player building.
 * @param new_item What to build.
 * @param x X where to build.
 * @param y Y where to build.
 * @return 1 if the item was built, 0 otherwise. */
static int builder_item(object *op, object *new_item, int x, int y)
{
	object *floor;
	int w = wall_blocked(op->map, x, y);

	/* If it's not a wallmask, don't allow building on top of blocked squares. */
	if (new_item->type != WALL && w)
	{
		new_draw_info_format(NDI_UNIQUE, op, "Something is blocking you from building the %s on that square.", query_name(new_item, NULL));
		return 0;
	}
	/* Wallmask, only allow building on top of blocked square that only
	 * contains a wall. */
	else if (new_item->type == WALL)
	{
		object *wall = get_wall(op->map, x, y);

		if (!w || !wall)
		{
			new_draw_info_format(NDI_UNIQUE, op, "The %s can only be built on top of a wall.", query_name(new_item, NULL));
			return 0;
		}
		else if (wall->above && wall->above->type == WALL)
		{
			new_draw_info_format(NDI_UNIQUE, op, "You first need to remove the %s before building on top of that wall again.", query_name(wall->above, NULL));
			return 0;
		}
	}

	/* Only allow building if there is a floor. */
	for (floor = GET_MAP_OB_LAYER(op->map, x, y, 0); floor && floor->layer == 1; floor = floor->above)
	{
		if (floor->type == FLOOR || QUERY_FLAG(floor, FLAG_IS_FLOOR))
		{
			break;
		}
	}

	if (!floor)
	{
		new_draw_info(NDI_UNIQUE, op, "This square has no floor, you can't build here.");
		return 0;
	}

	/* For signs, take the msg from a book on the square. */
	if (new_item->type == SIGN)
	{
		object *book;

		for (book = GET_MAP_OB(op->map, x, y); book; book = book->above)
		{
			if (book->type == BOOK)
			{
				break;
			}
		}

		if (!book || !book->msg)
		{
			new_draw_info(NDI_UNIQUE, op, "You need to put a book with the message.");
			return 0;
		}

		/* If the book has a custom name, copy it to the sign's name. */
		if (book->custom_name)
		{
			FREE_AND_COPY_HASH(new_item->name, book->custom_name);
		}

		/* Copy the message and remove the book. */
		FREE_AND_COPY_HASH(new_item->msg, book->msg);
		remove_ob(book);
	}

	/* If the item is turnable, adjust direction. */
	if (QUERY_FLAG(new_item, FLAG_IS_TURNABLE) && op->facing)
	{
		new_item->direction = op->facing;
		SET_ANIMATION(new_item, (NUM_ANIMATIONS(new_item) / NUM_FACINGS(new_item)) * new_item->direction);
	}

	SET_FLAG(new_item, FLAG_NO_PICK);
	new_item->x = x;
	new_item->y = y;
	insert_ob_in_map(new_item, op->map, NULL, 0);
	new_draw_info_format(NDI_UNIQUE, op, "You build the %s.", query_name(new_item, NULL));

	return 1;
}

/**
 * Split wall's name from the orientation.
 * @param wall Wall to get the name with orientation from.
 * @param wall_name Where the wall's name will go.
 * @param wall_name_size Size of 'wall_name'.
 * @param orientation Where the wall's orientation will go.
 * @param orientation_size Size of 'orientation'.
 * @return 1 on success, 0 on failure. */
static int wall_split_orientation(const object *wall, char *wall_name, size_t wall_name_size, char *orientation, size_t orientation_size)
{
	int l;

	strncpy(wall_name, wall->arch->name, wall_name_size - 1);

	/* Extract the wall name, which is the text up to the last '_'. */
	for (l = wall_name_size; l >= 0; l--)
	{
		if (wall_name[l] == '_')
		{
			/* Copy over orientation. */
			strncpy(orientation, wall_name + l, orientation_size - 1);
			wall_name[l] = '\0';
			return 1;
		}
	}

	return 0;
}

/**
 * Fixes walls around specified spot
 *
 * Basically it ensures the correct wall is put where needed.
 * @note x and y must be valid map coordinates.
 * @param map Map where to fix.
 * @param x X where to fix.
 * @param y Y where to fix. */
static void fix_walls(mapstruct *map, int x, int y)
{
	int connect = 0;
	object *wall;
	char wall_name[MAX_BUF], orientation[MAX_BUF];
	uint32 old_flags[NUM_FLAGS_32];
	archetype *new_arch;
	int flag;

	/* First, find the wall on that spot */
	wall = get_wall(map, x, y);

	if (!wall || !wall_split_orientation(wall, wall_name, sizeof(wall_name), orientation, sizeof(orientation)))
	{
		return;
	}

	connect = 0;

	if (x > 0 && get_wall(map, x - 1, y))
	{
		connect |= 1;
	}

	if (x < MAP_WIDTH(map) - 1 && get_wall(map, x + 1, y))
	{
		connect |= 2;
	}

	if (y > 0 && get_wall(map, x, y - 1))
	{
		connect |= 4;
	}

	if (y < MAP_HEIGHT(map) - 1 && get_wall(map, x, y + 1))
	{
		connect |= 8;
	}

	switch (connect)
	{
		case 0:
			return;

		case 10:
		case 8:
		case 2:
			strncat(wall_name, "_8", sizeof(wall_name) - strlen(wall_name) - 1);
			break;

		case 11:
		case 9:
		case 3:
		case 1:
			strncat(wall_name, "_1", sizeof(wall_name) - strlen(wall_name) - 1);
			break;

		case 12:
		case 4:
		case 14:
		case 6:
			strncat(wall_name, "_3", sizeof(wall_name) - strlen(wall_name) - 1);
			break;

		case 5:
		case 7:
		case 13:
		case 15:
			strncat(wall_name, "_4", sizeof(wall_name) - strlen(wall_name) - 1);
			break;
	}

	/* No need to change anything if the old and new names are identical. */
	if (!strncmp(wall_name, wall->arch->name, sizeof(wall_name)))
	{
		return;
	}

	/* Before anything, make sure the archetype does in fact exist... */
	new_arch = find_archetype(wall_name);

	if (!new_arch)
	{
		return;
	}

	/* Now delete current wall, and insert new one
	 * We save flags to avoid any trouble with buildable/non buildable, etc. */
	for (flag = 0; flag < NUM_FLAGS_32; flag++)
	{
		old_flags[flag] = wall->flags[flag];
	}

	remove_ob(wall);

	wall = arch_to_object(new_arch);
	wall->type = WALL;
	wall->x = x;
	wall->y = y;
	insert_ob_in_map(wall, map, NULL, 0);

	for (flag = 0; flag < NUM_FLAGS_32; flag++)
	{
		wall->flags[flag] = old_flags[flag];
	}
}

/**
 * Build a wall.
 * @param op Player building.
 * @param new_wall New wall object.
 * @param x X where to build.
 * @param y Y where to build.
 * @return 1 if the wall was built, 0 otherwise. */
static int builder_wall(object *op, object *new_wall, int x, int y)
{
	object *wall = get_wall(op->map, x, y);

	if (wall)
	{
		char wall_name[MAX_BUF], orientation[MAX_BUF];

		if (!wall_split_orientation(wall, wall_name, sizeof(wall_name), orientation, sizeof(orientation)))
		{
			new_draw_info(NDI_UNIQUE, op, "You don't see a way to redecorate that wall.");
			return 0;
		}

		if (!strncmp(wall->arch->name, wall_name, strlen(wall_name)))
		{
			new_draw_info(NDI_UNIQUE, op, "You feel too lazy to redo the exact same wall.");
			return 0;
		}
	}

	new_wall->type = WALL;

	/* If existing wall, replace it, no need to fix other walls */
	if (wall)
	{
		remove_ob(wall);
		new_wall->x = x;
		new_wall->y = y;
		insert_ob_in_map(new_wall, op->map, NULL, 0);
		fix_walls(op->map, x, y);
		new_draw_info(NDI_UNIQUE, op, "You redecorate the wall to better suit your tastes.");
	}
	/* Else insert new wall and fix all walls around */
	else
	{
		int xt, yt;

		new_wall->x = x;
		new_wall->y = y;
		insert_ob_in_map(new_wall, op->map, NULL, 0);

		for (xt = x - 1; xt <= x + 1; xt++)
		{
			for (yt = y - 1; yt <= y + 1; yt++)
			{
				if (OUT_OF_REAL_MAP(op->map, xt, yt))
				{
					continue;
				}

				fix_walls(op->map, xt, yt);
			}
		}

		new_draw_info(NDI_UNIQUE, op, "You build a wall.");
	}

	return 1;
}

/**
 * Window building function.
 * @param op Player building.
 * @param x X where to build.
 * @param y Y where to build.
 * @return 1 if the window was built, 0 otherwise. */
static int builder_window(object *op, int x, int y)
{
	object *wall;
	char wall_name[MAX_BUF], orientation[MAX_BUF];
	archetype *new_arch;
	object *window;
	uint32 old_flags[NUM_FLAGS_32];
	int flag;

	wall = get_wall(op->map, x, y);

	if (!wall)
	{
		new_draw_info(NDI_UNIQUE, op, "There is no wall there.");
		return 0;
	}

	if (!wall_split_orientation(wall, wall_name, sizeof(wall_name), orientation, sizeof(orientation)))
	{
		new_draw_info(NDI_UNIQUE, op, "You don't see a way to build a window in that wall.");
		return 0;
	}
	else if (!strcmp(orientation, "_1") && !strcmp(orientation, "_3"))
	{
		new_draw_info(NDI_UNIQUE, op, "You cannot build a window in that wall.");
		return 0;
	}

	strncat(wall_name, "_w", sizeof(wall_name) - strlen(wall_name) - 1);
	strncat(wall_name, orientation, sizeof(wall_name) - strlen(wall_name) - 1);

	new_arch = find_archetype(wall_name);

	/* That type of wall doesn't have corresponding window archetype. */
	if (!new_arch)
	{
		new_draw_info(NDI_UNIQUE, op, "You cannot build a window in that wall.");
		return 0;
	}

	/* Now delete current wall, and insert new one with a window.
	 * We save flags to avoid any trouble with buildable/non buildable, etc. */
	for (flag = 0; flag < NUM_FLAGS_32; flag++)
	{
		old_flags[flag] = wall->flags[flag];
	}

	remove_ob(wall);

	window = arch_to_object(new_arch);
	window->type = WALL;
	window->x = x;
	window->y = y;
	insert_ob_in_map(window, op->map, NULL, 0);

	for (flag = 0; flag < NUM_FLAGS_32; flag++)
	{
		window->flags[flag] = old_flags[flag];
	}

	new_draw_info(NDI_UNIQUE, op, "You build a window in the wall.");
	return 1;
}

/**
 * Generic function for handling the construction builder skill item.
 * @param op Player building.
 * @param x X where to build.
 * @param y Y where to build. */
static void construction_builder(object *op, int x, int y)
{
	object *material, *new_item;
	archetype *new_arch;
	int built = 0;

	material = find_marked_object(op);

	if (!material)
	{
		new_draw_info(NDI_UNIQUE, op, "You need to mark raw materials to use.");
		return;
	}

	if (material->type != MATERIAL)
	{
		new_draw_info(NDI_UNIQUE, op, "You can't use the marked item to build.");
		return;
	}

	/* Create a new object from the raw materials. */
	new_arch = find_archetype(material->slaying);

	if (!new_arch)
	{
		LOG(llevBug, "construction_builder(): Unable to find archetype %s.\n", material->slaying);
		new_draw_info(NDI_UNIQUE, op, "You can't use this strange material.");
		return;
	}

	new_item = arch_to_object(new_arch);
	SET_FLAG(new_item, FLAG_IS_BUILDABLE);

	if (!can_build_over(op->map, new_item, x, y))
	{
		new_draw_info(NDI_UNIQUE, op, "You can't build there.");
		return;
	}

	/* Insert the new object in the map. */
	switch (material->sub_type)
	{
		case ST_MAT_FLOOR:
			built = builder_floor(op, new_item, x, y);
			break;

		case ST_MAT_WALL:
			built = builder_wall(op, new_item, x, y);
			break;

		case ST_MAT_ITEM:
			built = builder_item(op, new_item, x, y);
			break;

		case ST_MAT_WIN:
			built = builder_window(op, x, y);
			break;

		default:
			LOG(llevBug, "BUG: construction_builder(): Invalid material subtype %d.\n", material->sub_type);
			new_draw_info(NDI_UNIQUE, op, "Don't know how to apply this material, sorry.");
			break;
	}

	if (built)
	{
		decrease_ob(material);
	}
}

/**
 * Item remover.
 *
 * Removes first buildable item except floor.
 * @param op Player removing an item.
 * @param x X Where to remove.
 * @param y Y where to remove. */
static void construction_destroyer(object *op, int x, int y)
{
	object *item;

	for (item = GET_MAP_OB_LAST(op->map, x, y); item; item = item->below)
	{
		if (item->type != FLOOR && QUERY_FLAG(item, FLAG_IS_BUILDABLE))
		{
			break;
		}
	}

	if (!item)
	{
		new_draw_info(NDI_UNIQUE, op, "Nothing to remove.");
		return;
	}

	switch (item->type)
	{
		case CONTAINER:
			/* Do not allow destroying containers with inventory,
			 * otherwise fall through. */
			if (item->inv)
			{
				new_draw_info_format(NDI_UNIQUE, op, "You cannot remove the %s, since it contains items.", query_name(item, NULL));
				return;
			}

		case WALL:
		case DOOR:
		case SIGN:
			remove_ob(item);

			/* Fix walls around the one that was removed. */
			if (item->type == WALL)
			{
				int xt, yt;

				for (xt = x - 1; xt <= x + 1; xt++)
				{
					for (yt = y - 1; yt <= y + 1; yt++)
					{
						if (!OUT_OF_REAL_MAP(op->map, xt, yt))
						{
							fix_walls(op->map, xt, yt);
						}
					}
				}
			}

			new_draw_info_format(NDI_UNIQUE, op, "You remove the %s.", query_name(item, NULL));
			break;
	}
}

/**
 * Main point of construction skill. From here, we decide what to do
 * with skill item we have equipped.
 * @param op Player. */
void construction_do(object *op, int dir)
{
	object *skill_item, *floor, *tmp;
	int x, y;

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: construction_do(): Construction can only be used by players.\n");
		return;
	}

	skill_item = CONTR(op)->equipment[PLAYER_EQUIP_SKILL_ITEM];

	if (!skill_item)
	{
		new_draw_info(NDI_UNIQUE, op, "You need to apply a skill item to use this skill.");
		return;
	}

	if (skill_item->stats.sp != SK_CONSTRUCTION)
	{
		new_draw_info_format(NDI_UNIQUE, op, "The %s cannot be used with the construction skill.", query_name(skill_item, NULL));
		return;
	}

	if (!dir)
	{
		new_draw_info(NDI_UNIQUE, op, "You can't build or destroy under yourself.");
		return;
	}

	x = op->x + freearr_x[dir];
	y = op->y + freearr_y[dir];

	if ((1 > x) || (1 > y) || ((MAP_WIDTH(op->map) - 2) < x) || ((MAP_HEIGHT(op->map) - 2) < y))
	{
		new_draw_info(NDI_UNIQUE, op, "Can't build on map edge.");
		return;
	}

	/* Check specified square
	 * The square must have only buildable items
	 * Exception: marking runes are all right,
	 * since they are used for special things like connecting doors / buttons */
	floor = GET_MAP_OB(op->map, x, y);

	if (!floor)
	{
		LOG(llevBug, "construction_do(): Undefined square on map %s (%d, %d)\n", op->map->path, x, y);
		new_draw_info(NDI_UNIQUE, op, "You'd better not build here, it looks weird.");
		return;
	}

	if (skill_item->sub_type != ST_BD_BUILD)
	{
		for (tmp = floor; tmp; tmp = tmp->above)
		{
			if (!QUERY_FLAG(tmp, FLAG_IS_BUILDABLE))
			{
				new_draw_info(NDI_UNIQUE, op, "You can't build there.");
				return;
			}
		}
	}

	/* Prevent players without destroyer from getting themselves stuck. */
	if (skill_item->sub_type != ST_BD_REMOVE)
	{
		int found_destroyer = 0;

		for (tmp = op->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == SKILL_ITEM && tmp->sub_type == ST_BD_REMOVE)
			{
				found_destroyer = 1;
				break;
			}
		}

		if (!found_destroyer)
		{
			new_draw_info(NDI_UNIQUE, op, "You cannot build without having a destroyer at hand.");
			return;
		}
	}

	switch (skill_item->sub_type)
	{
		case ST_BD_REMOVE:
			construction_destroyer(op, x, y);
			break;

		case ST_BD_BUILD:
			construction_builder(op, x, y);
			break;

		default:
			LOG(llevBug, "BUG: Skill item %s has invalid subtype.\n", query_name(skill_item, NULL));
			new_draw_info(NDI_UNIQUE, op, "Don't know how to apply this tool, sorry.");
			break;
	}
}
