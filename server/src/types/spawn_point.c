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
 * Handles code for @ref SPAWN_POINT "spawn points". */

#include <global.h>

/**
 * Actually spawn the monster.
 * @param monster Monster to generate.
 * @param spawn_point Spawn point.
 * @param range Maximum range the monster can be spawned away.
 * @return The generated monster, NULL on failure. */
static object *spawn_monster(object *monster, object *spawn_point_ob, int range)
{
	int i;
	object *op;

	i = find_first_free_spot2(monster->arch, spawn_point_ob->map, spawn_point_ob->x, spawn_point_ob->y, 0, range);

	if (i == -1)
	{
		return NULL;
	}

	op = get_object();
	monster->type = MONSTER;
	copy_object(monster, op, 0);
	monster->type = SPAWN_POINT_MOB;

	op->x = spawn_point_ob->x + freearr_x[i];
	op->y = spawn_point_ob->y + freearr_y[i];

	if (op->item_condition)
	{
		int level = MAX(1, MIN(op->level, MAXLEVEL)), min, max, diff = spawn_point_ob->map->difficulty;

		switch (op->item_condition)
		{
			case 1:
				min = level_color[diff].green;
				max = level_color[diff].blue - 1;
				break;

			case 2:
				min = level_color[diff].blue;
				max = level_color[diff].yellow - 1;
				break;

			case 3:
				min = level_color[diff].yellow;
				max = level_color[diff].orange - 1;
				break;

			case 4:
				min = level_color[diff].orange;
				max = level_color[diff].red - 1;
				break;

			case 5:
				min = level_color[diff].red;
				max = level_color[diff].purple - 1;
				break;

			case 6:
				min = level_color[diff].purple;
				max = min + 1;
				break;

			default:
				min = level;
				max = min;
		}

		op->level = rndm(MAX(level, MIN(min, MAXLEVEL)), MAX(level, MIN(max, MAXLEVEL)));
	}

	if (op->randomitems)
	{
		create_treasure(op->randomitems, op, 0, op->level ? op->level : spawn_point_ob->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
	}

	return op;
}

/**
 * Check whether monster can be spawned, depending on the spawn point
 * map's darkness and other factors.
 * @param map Spawn point's map.
 * @param monster The monster to check.
 * @param darkness Darkness the monster may spawn int.
 * @return 1 if the monster can be spawned, 0 otherwise. */
static inline int spawn_point_can_spawn(mapstruct *map, object *monster, int darkness)
{
	shstr *spawn_time;

	if (!map)
	{
		return 0;
	}

	/* Check darkness. */
	if (darkness)
	{
		int map_darkness;

		/* Figure out the map's darkness. */
		if (MAP_OUTDOORS(map))
		{
			map_darkness = world_darkness;
		}
		else if (MAP_DARKNESS(map) == -1)
		{
			map_darkness = MAX_DARKNESS;
		}
		else
		{
			map_darkness = MAP_DARKNESS(map);
		}

		if (darkness < 0)
		{
			if (map_darkness > -darkness)
			{
				return 0;
			}
		}
		else
		{
			if (map_darkness < darkness)
			{
				return 0;
			}
		}
	}

	spawn_time = object_get_value(monster, "spawn_time");

	/* Check if the time is right for the monster to be spawned. */
	if (spawn_time)
	{
		int hour, minute, hour2, minute2;

		if (sscanf(spawn_time, "%d:%d - %d:%d", &hour, &minute, &hour2, &minute2) == 4)
		{
			timeofday_t tod;

			get_tod(&tod);

			/* Same day. */
			if (hour <= hour2)
			{
				if (tod.hour < hour || tod.hour > hour2)
				{
					return 0;
				}
			}
			/* Overnight. */
			else
			{
				if (tod.hour < hour && tod.hour > hour2)
				{
					return 0;
				}
			}

			/* Check minutes. */
			if ((tod.hour == hour && tod.minute < minute) || (tod.hour == hour2 && tod.minute > minute2))
			{
				return 0;
			}

			return 1;
		}
		else
		{
			LOG(llevBug, "spawn_point_can_spawn(): Syntax error in spawn_time attribute: %s\n", spawn_time);
		}
	}

	return 1;
}

/**
 * Insert a copy of all items the spawn point monster has to the new monster.
 * Takes care about random drop objects.
 *
 * This will recursively call itself if the item to put to the new
 * monster has an inventory.
 *
 * Usually these items are put from the map maker inside the spawn
 * monster inventory.
 * Remember that these are additional items to the treasures list ones.
 * @param op The spawn point object
 * @param monster The object where to put the copy of items to
 * @param tmp The inventory pointer where we are copying the items from */
static void insert_spawn_monster_loot(object *op, object *monster, object *tmp)
{
	object *tmp2, *next, *next2, *item;

	for (; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->type == RANDOM_DROP)
		{
			if (!tmp->weight_limit || rndm_chance(tmp->weight_limit))
			{
				for (tmp2 = tmp->inv; tmp2; tmp2 = next2)
				{
					next2 = tmp2->below;

					if (tmp2->type == RANDOM_DROP)
					{
						LOG(llevDebug, "RANDOM_DROP not allowed inside another RANDOM_DROP. Monster: >%s< map: %s (x: %d, y: %d)\n", query_name(monster, NULL), op->map ? op->map->path : "null", op->x, op->y);
					}
					else
					{
						item = get_object();
						copy_object(tmp2, item, 0);
						insert_ob_in_ob(item, monster);

						if (tmp2->inv)
						{
							insert_spawn_monster_loot(op, item, tmp2->inv);
						}
					}
				}
			}
		}
		else
		{
			item = get_object();
			copy_object(tmp, item, 0);
			insert_ob_in_ob(item, monster);

			if (tmp->inv)
			{
				insert_spawn_monster_loot(op, item, tmp->inv);
			}
		}
	}
}

/**
 * Main spawn point processing function.
 * @param op Spawn point to process. */
void spawn_point(object *op)
{
	int rmt;
	object *tmp, *monster, *next;

	if (op->enemy)
	{
		if (OBJECT_VALID(op->enemy, op->enemy_count))
		{
			if (!spawn_point_can_spawn(op->map, op->enemy, op->last_eat))
			{
				remove_ob(op->enemy);
				object_destroy(op->enemy);
			}
			else
			{
				return;
			}
		}

		/* Spawn point has nothing spawned */
		op->enemy = NULL;
	}

	/* A set sp value will override the spawn chance, which is particularly
	 * useful when saving/loading maps. */
	if (op->stats.sp == -1)
	{
		if (op->last_grace <= -1)
		{
			return;
		}

		if (op->last_grace && !rndm_chance(op->last_grace))
		{
			return;
		}

		op->stats.sp = rndm(1, SPAWN_RANDOM_RANGE) - 1;
	}

	/* Spawn point without inventory! */
	if (!op->inv)
	{
		LOG(llevBug, "Spawn point without inventory! --> map %s (x: %d, y: %d)\n", op->map ? (op->map->path ? op->map->path : ">no path<") : ">no map<", op->x, op->y);
		remove_ob(op);
		object_destroy(op);
		return;
	}

	for (rmt = 0, monster = NULL, tmp = op->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->type == BEACON)
		{
			continue;
		}

		if (tmp->type != SPAWN_POINT_MOB)
		{
			LOG(llevBug, "Spawn point in map %s (x: %d, y: %d) with wrong type object (%d) in inv: %s\n", op->map ? op->map->path : "<no map>", op->x, op->y, tmp->type, query_name(tmp, NULL));
		}
		else if ((int) tmp->enemy_count <= op->stats.sp && (int) tmp->enemy_count >= rmt)
		{
			if (!spawn_point_can_spawn(op->map, tmp, tmp->last_eat))
			{
				continue;
			}

			rmt = (int) tmp->enemy_count;
			monster = tmp;
		}
	}

	rmt = op->stats.sp;
	op->stats.sp = -1;

	if (!monster)
	{
		return;
	}

	/* Quick save the default monster inventory */
	tmp = monster->inv;

	if (!(monster = spawn_monster(monster, op, op->last_heal)))
	{
		return;
	}

	/* Setup special monster -> spawn point values */
	op->last_eat = 0;

	/* Darkness controlled spawns */
	if (monster->last_eat)
	{
		op->last_eat = monster->last_eat;
		monster->last_eat = 0;
	}

	insert_spawn_monster_loot(op, monster, tmp);

	op->last_sp = rmt;

	/* Chain the monster to our spawn point */
	op->enemy = monster;
	op->enemy_count = monster->count;

	/* Create spawn info */
	tmp = arch_to_object(op->other_arch);
	/* Chain spawn point to our monster */
	tmp->owner = op;
	tmp->ownercount = op->count;
	/* And put it inside the monster */
	insert_ob_in_ob(tmp, monster);

	insert_ob_in_map(monster, op->map, op, 0);

	SET_MULTI_FLAG(monster, FLAG_SPAWN_MOB);
	fix_monster(monster);

	if (QUERY_FLAG(monster, FLAG_ANIMATE))
	{
		animate_object(monster, 0);
	}
}
