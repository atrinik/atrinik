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
 * Handles code for @ref SPAWN_POINT "spawn points". */

#include <global.h>

/* drop a monster on the map, by copying a monster object or
 * monster object head. Add treasures. */
static object *spawn_monster(object *gen, object *orig, int range)
{
	int i;
	object *op, *head = NULL, *prev = NULL, *ret = NULL;
	archetype *at = gen->arch;

	i = find_first_free_spot2(at, orig->map, orig->x, orig->y, 0, range);

	if (i == -1)
		return NULL;

	while (at != NULL)
	{
		op = get_object();
		/* copy single/head from spawn inventory */
		if (head == NULL)
		{
			gen->type = MONSTER;
			copy_object(gen, op);
			gen->type = SPAWN_POINT_MOB;
			ret = op;
		}
		/* but the tails for multi arch from the clones */
		else
		{
			copy_object(&at->clone, op);
		}
		op->x = orig->x + freearr_x[i] + at->clone.x;
		op->y = orig->y + freearr_y[i] + at->clone.y;
		op->map = orig->map;

		if (head != NULL)
			op->head = head, prev->more = op;

		if (OBJECT_FREE(op))
			return NULL;

		if (op->randomitems != NULL)
			create_treasure(op->randomitems, op, 0, op->level ? op->level : orig->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

		if (head == NULL)
			head = op;

		prev = op;
		at = at->more;
	}

	/* return object ptr to our spawn */
	return ret;
}

/* check the current darkness on this map allows to spawn
 * 0: not allowed, 1: allowed */
static inline int spawn_point_darkness(object *spoint, int darkness)
{
	int map_light;

	if (!spoint->map)
		return 0;

	/* outdoor map */
	if (MAP_OUTDOORS(spoint->map))
		map_light = world_darkness;
	else
	{
		if (MAP_DARKNESS(spoint->map) == -1)
			map_light = MAX_DARKNESS;
		else
			map_light = MAP_DARKNESS(spoint->map);
	}

	if (darkness < 0)
	{
		if (map_light < -darkness)
			return 1;
	}
	else
	{
		if (map_light > darkness)
			return 1;
	}
	return 0;
}

/**
 * Insert a copy of all items the spawn point mob has to the new monster.
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

		if (tmp->type == TYPE_RANDOM_DROP)
		{
			/* skip this container - drop the ->inv */
			if (!tmp->weight_limit || !(RANDOM() % (tmp->weight_limit + 1)))
			{
				for (tmp2 = tmp->inv; tmp2; tmp2 = next2)
				{
					next2 = tmp2->below;

					if (tmp2->type == TYPE_RANDOM_DROP)
					{
						LOG(llevDebug,"DEBUG:: Spawn:: RANDOM_DROP (102) not allowed inside RANDOM_DROP.mob:>%s< map:%s (%d,%d)\n", query_name(monster, NULL), op->map ? op->map->path : "BUG: S-Point without map!", op->x, op->y);
					}
					else
					{
						item = get_object();
						copy_object(tmp2, item);
						/* and put it in the mob */
						insert_ob_in_ob(item, monster);

						if (tmp2->inv)
						{
							insert_spawn_monster_loot(op, item, tmp2->inv);
						}
					}
				}
			}
		}
		/* remember this can be sys_objects too! */
		else
		{
			item = get_object();
			copy_object(tmp, item);
			/* and put it in the mob */
			insert_ob_in_ob(item, monster);

			if (tmp->inv)
			{
				insert_spawn_monster_loot(op, item, tmp->inv);
			}
		}
	}
}


/* central spawn point function.
 * Control, generate or remove the generated object. */
void spawn_point(object *op)
{
	int rmt;
	object *tmp, *mob, *next;

	if (op->enemy)
	{
		/* all ok, our spawn have fun */
		if (OBJECT_VALID(op->enemy, op->enemy_count))
		{
			/* check darkness if needed */
			if (op->last_eat)
			{
				/* 1 = darkness is ok */
				if (spawn_point_darkness(op, op->last_eat))
					return;

				/* darkness has changed - now remove the spawned monster */
				remove_ob(op->enemy);
				check_walk_off(op->enemy, NULL, MOVE_APPLY_VANISHED);
			}
			else
				return;
		}
		/* spawn point has nothing spawned */
		op->enemy = NULL;
	}

	/* a set sp value will override the spawn chance.
	* with that "trick" we force for map loading the
	* default spawns of the map because sp is 0 as default. */
	/*LOG(-1,"SPAWN...(%d,%d)",op->x, op->y);*/
	if (op->stats.sp == -1)
	{
		int gg;
		/* now lets decide we will have a spawn event */
		/* never */
		if (op->last_grace <= -1)
		{
			/*LOG(-1," closed (-1)\n");*/
			return;
		}
		/* if >0 and random%x is NOT null ... */
		if (op->last_grace && (gg = (RANDOM() % (op->last_grace + 1))))
		{
			/*LOG(-1," chance: %d (%d)\n",gg,op->last_grace);*/
			return;
		}

		op->stats.sp = (RANDOM() % SPAWN_RANDOM_RANGE);
	}
	/*LOG(-1," hit!: %d\n",op->stats.sp);*/

	/* spawn point without inventory! */
	if (!op->inv)
	{
		LOG(llevBug, "BUG: Spawn point without inventory!! --> map %s (%d,%d)\n", op->map ? (op->map->path ? op->map->path : ">no path<") : ">no map<", op->x, op->y);
		/* kill this spawn point - its useless and need to fixed from the map maker/generator */
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}
	/* now we move through the spawn point inventory and
	 * get the mob with a number under this value AND nearest. */
	for (rmt = 0, mob = NULL, tmp = op->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->type != SPAWN_POINT_MOB)
			LOG(llevBug, "BUG: spawn point in map %s (%d,%d) with wrong type object (%d) in inv: %s\n", op->map ? op->map->path : "<no map>", op->x, op->y, tmp->type, query_name(tmp, NULL));
		else if ((int)tmp->enemy_count <= op->stats.sp && (int)tmp->enemy_count >= rmt)
		{
			/* we have a possible hit - control special settings now */
			/* darkness */
			if (tmp->last_eat)
			{
				/* 1: darkness on map of spawn point is ok */
				if (!spawn_point_darkness(op, tmp->last_eat))
					continue;
			}

			rmt = (int)tmp->enemy_count;
			mob = tmp;
		}
		/*LOG(llevInfo,"inv -> %s (%d :: %d - %f)\n", tmp->name, op->stats.sp, tmp->enemy_count, tmp->speed_left);*/
	}

	/* we try only ONE time a respawn of a pre setting - so careful! */
	rmt = op->stats.sp;
	op->stats.sp = -1;
	/* well, this time we spawn nothing */
	if (!mob)
		return;

	/* quick save the def mob inventory */
	tmp = mob->inv;
	/* that happens when we have no free spot....*/
	if (!(mob = spawn_monster(mob, op, op->last_heal)))
		return;

	/* setup special monster -> spawn point values */
	op->last_eat = 0;

	/* darkness controled spawns */
	if (mob->last_eat)
	{
		op->last_eat = mob->last_eat;
		mob->last_eat = 0;
	}

	insert_spawn_monster_loot(op, mob, tmp);

	/* this is the last rand() for what we have spawned! */
	op->last_sp = rmt;

	/* chain the mob to our spawn point */
	op->enemy = mob;
	op->enemy_count = mob->count;

	/* perhaps we have later more unique mob setting - then we can store it here too. */

	/* create spawn info */
	tmp = arch_to_object(op->other_arch);
	/* chain spawn point to our mob */
	tmp->owner = op;
	/* and put it in the mob */
	insert_ob_in_ob(tmp, mob);

	/* FINISH: now mark our mob as a spawn */
	SET_MULTI_FLAG(mob, FLAG_SPAWN_MOB);
	/* fix all the values and add in possible abilities or forces ... */
	fix_monster(mob);

	/* *now* all is done - *now* put it on map */
	if (!insert_ob_in_map(mob, mob->map, op, 0))
		return;
}
