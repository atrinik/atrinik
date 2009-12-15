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
 * Object related code. */

#include <global.h>

#ifndef WIN32
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#endif

#include <skillist.h>
#include <loader.h>
#include <sproto.h>

/**
 * List of objects that have been removed during the last server
 * timestep. */
struct mempool_chunk *removed_objects;

/* List of active objects that need to be processed */
object *active_objects;

/* see walk_off/walk_on functions  */
static int static_walk_semaphore = FALSE;

/* container for objects without real maps or envs */
object void_container;

/**
 * Basically, this is a table of directions, and what directions one
 * could go to go back to us. Eg, entry 15 below is 4, 14, 16. This
 * basically means that if direction is 15, then it could either go
 * direction 4, 14, or 16 to get back to where we are. */
static const int reduction_dir[SIZEOFFREE][3] =
{
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{8, 1, 2},
	{1, 2, -1},
	{2, 10, 12},
	{2, 3, -1},
	{2, 3, 4},
	{3, 4, -1},
	{4, 14, 16},
	{5, 4, -1},
	{4, 5, 6},
	{6, 5, -1},
	{6, 20, 18},
	{7, 6, -1},
	{6, 7, 8},
	{7, 8, -1},
	{8, 22, 24},
	{8, 1, -1},
	{24, 9, 10},
	{9, 10, -1},
	{10, 11, -1},
	{27, 11, 29},
	{11, 12, -1},
	{12, 13, -1},
	{12, 13, 14},
	{13, 14, -1},
	{14, 15, -1},
	{33, 15, 35},
	{16, 15, -1},
	{17, 16, -1},
	{18, 17, 16},
	{18, 17, -1},
	{18, 19, -1},
	{41, 19, 39},
	{19, 20, -1},
	{20, 21, -1},
	{20, 21, 22},
	{21, 22, -1},
	{23, 22, -1},
	{45, 47, 23},
	{23, 24, -1},
	{24, 9, -1}
};

/** Material types */
materialtype material[NROFMATERIALS] =
{
	{"paper",          {15, 10, 17, 9, 5, 7,13, 0, 20, 15, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"metal",          {2, 12, 3, 12, 2,10, 7, 0, 20, 15, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"crystal",        {14, 11, 8, 3, 10, 5, 1, 0, 20, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"leather",        {5, 10, 10, 3, 3, 10,10, 0, 20, 15, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"wood",           {10, 11, 13, 2, 2, 10, 9, 0, 20, 15, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"organics",       {3, 12, 9, 11, 3, 10, 9, 0, 20, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"stone",          {2, 5, 2, 2, 2, 2, 1, 0, 20, 15, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"cloth",          {14, 11, 13, 4, 4, 5, 10, 0, 20, 15, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"magic material", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"liquid",         {0, 8, 9, 6, 17, 0, 15, 0, 20, 15, 12, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"soft metal",     {6, 12, 6, 14, 2, 10, 1, 0, 20, 15, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"bone",           {10, 9, 4, 5, 3, 10, 10, 0, 20, 15, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0}},
	{"ice",            {14, 11, 16, 5, 0, 5, 6, 0, 20, 15, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0}}
};

#define NUM_MATERIALS_REAL NROFMATERIALS * NROFMATERIALS_REAL + 1

/**
 * Real material types. This array is initialized by
 * init_materials(). */
material_real_struct material_real[NUM_MATERIALS_REAL] = {};

static void dump_object2(object *op);
static void sub_weight(object *op, sint32 weight);
static void remove_ob_inv(object *op);
static void add_weight(object *op, sint32 weight);

/**
 * Initialize materials from file. */
void init_materials()
{
	int i;
	char filename[MAX_BUF], buf[MAX_BUF];
	FILE *fp;

	/* First initialize default values to the array */
	for (i = 0; i < NUM_MATERIALS_REAL; i++)
	{
		material_real[i].name[0] = '\0';
		material_real[i].tearing = 100;
		material_real[i].quality = 100;
		material_real[i].type = M_NONE;
		material_real[i].def_race = RACE_TYPE_NONE;
	}

	snprintf(filename, sizeof(filename), "%s/materials", settings.datadir);

	if (!(fp = fopen(filename, "r")))
	{
		LOG(llevBug, "BUG: Could not open materials file: %s\n", filename);
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		if (sscanf(buf, "material_real %d\n", &i))
		{
			int def_race = RACE_TYPE_NONE, type = M_NONE, quality = 100, tearing = 100;
			char name[MAX_BUF];

			if (i > NUM_MATERIALS_REAL)
			{
				LOG(llevError, "ERROR: Materials file contains declaration for material #%d but it doesn't exist.\n", i);
			}

			name[0] = '\0';

			while (fgets(buf, sizeof(buf), fp))
			{
				if (strcmp(buf, "end\n") == 0)
				{
					break;
				}

				if (!sscanf(buf, "tearing %d\n", &tearing) && !sscanf(buf, "quality %d\n", &quality) && !sscanf(buf, "type %d\n", &type) && !sscanf(buf, "def_race %d\n", &def_race) && !sscanf(buf, "name %[^\n]", name))
				{
					LOG(llevError, "ERROR: Bogus line in materials file: %s\n", buf);
				}
			}

			if (name[0] != '\0')
			{
				snprintf(material_real[i].name, sizeof(material_real[i].name), "%s ", name);
			}

			material_real[i].tearing = tearing;
			material_real[i].quality = quality;
			material_real[i].type = type;
			material_real[i].def_race = def_race;
		}
		else
		{
			LOG(llevError, "ERROR: Bogus line in materials file: %s\n", buf);
		}
	}
}

int freearr_x[SIZEOFFREE] =
{
	0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1,
	0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1
};

int freearr_y[SIZEOFFREE] =
{
	0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2,
	-3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3
};

int maxfree[SIZEOFFREE] =
{
	0, 9, 10, 13, 14, 17, 18, 21, 22, 25, 26, 27, 30, 31, 32, 33, 36, 37, 39, 39, 42, 43, 44, 45,
	48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49
};

int freedir[SIZEOFFREE]=
{
	0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 2, 2, 3, 4, 4, 4, 5, 6, 6, 6, 7, 8, 8, 8,
	1, 2, 2, 2, 2, 2, 3, 4, 4, 4, 4, 4, 5, 6, 6, 6, 6, 6, 7, 8, 8, 8, 8, 8
};

/**
 * Put an object in the list of removal candidates.
 *
 * If the object has still FLAG_REMOVED set at the end of the server
 * timestep it will be freed.
 * @param ob The object */
void mark_object_removed(object *ob)
{
	struct mempool_chunk *mem = MEM_POOLDATA(ob);

	if (OBJECT_FREE(ob))
		LOG(llevBug, "BUG: mark_object_removed() called for free object\n");

	SET_FLAG(ob, FLAG_REMOVED);

	/* Don't mark objects twice */
	if (mem->next != NULL)
		return;

	mem->next = removed_objects;
	removed_objects = mem;
}

/**
 * Go through all objects in the removed list and free the forgotten ones */
void object_gc()
{
	struct mempool_chunk *current, *next;
	object *ob;

	while ((next = removed_objects) != &end_marker)
	{
		/* destroy_object() may free some more objects (inventory items) */
		removed_objects = &end_marker;
		while (next != &end_marker)
		{
			current = next;
			next = current->next;
			current->next = NULL;

			ob = (object *)MEM_USERDATA(current);

			if (QUERY_FLAG(ob, FLAG_REMOVED))
			{
				if (OBJECT_FREE(ob))
					LOG(llevBug, "BUG: Freed object in remove list: %s\n", STRING_OBJ_NAME(ob));
				else
					return_poolchunk(ob, pool_object);
			}
		}
	}
}

/**
 * Compares value lists.
 * @param wants What to search.
 * @param has Where to search.
 * @return 1 if every key_values in wants has a partner with the same
 * value in has. */
static int compare_ob_value_lists_one(const object *wants, const object *has)
{
	key_value *wants_field;

	/* For each field in wants. */
	for (wants_field = wants->key_values; wants_field; wants_field = wants_field->next)
	{
		key_value *has_field = get_ob_key_link(has, wants_field->key);

		if (has_field == NULL)
		{
			return 0;
		}

		/* Found the matching field. */
		if (has_field->value != wants_field->value)
		{
			return 0;
		}
	}

	return 1;
}

/**
 * Compares two object lists.
 * @param ob1 Object to compare.
 * @param ob2 Object to compare.
 * @return 1 if ob1 has the same key_values as ob2. */
static int compare_ob_value_lists(const object *ob1, const object *ob2)
{
	return compare_ob_value_lists_one(ob1, ob2) && compare_ob_value_lists_one(ob2, ob1);
}

/**
 * Moved this out of define.h and in here, since this is the only file
 * it is used in. Also, make it an inline function for cleaner
 * design.
 *
 * Examines two objects given to, and returns 1 if they can be merged
 * together.
 *
 * Check nrof variable <b>before</b> calling this.
 *
 * Improvements made with merge:  Better checking on potion, and also
 * check weight
 *
 * @note This function appears a lot longer than the macro it replaces
 * (mostly for clarity). A decent compiler should hopefully reduce this
 * to the same efficiency.
 * @param ob1 The first object
 * @param ob2 The second object
 * @return 1 if the two object can merge, 0 otherwise */
int CAN_MERGE(object *ob1, object *ob2)
{
	if (!QUERY_FLAG(ob1, FLAG_CAN_STACK))
	{
		return 0;
	}

	/* just some quick hack */
	if (ob1->type == MONEY && ob1->type == ob2->type && ob1->arch == ob2->arch)
	{
		return 1;
	}

	/* Gecko: Moved out special handling of event obejct nrof */
	/* important: don't merge objects with glow_radius set - or we come
	 * in heavy side effect situations. Because we really not know what
	 * our calling function will do after this merge (and the calling function
	 * then must first find out a merge has happen or not). The sense of stacks
	 * are to store inactive items. Because glow_radius items can be active even
	 * when not apllied, merging is simply wrong here. MT. */
	if (((!ob1->nrof || !ob2->nrof) && ob1->type != TYPE_EVENT_OBJECT) || ob1->glow_radius || ob2->glow_radius)
	{
		return 0;
	}

	/* just a brain dead long check for things NEVER NEVER should be different
	 * this is true under all circumstances for all objects. */
	if (ob1->type != ob2->type || ob1 == ob2 || ob1->arch != ob2->arch || ob1->sub_type1 != ob2->sub_type1 || ob1->material != ob2->material || ob1->material_real != ob2->material_real || ob1->magic != ob2->magic || ob1->item_quality != ob2->item_quality || ob1->item_condition != ob2->item_condition || ob1->item_race != ob2->item_race || ob1->speed != ob2->speed || ob1->value !=ob2->value || ob1->weight != ob2->weight)
	{
		return 0;
	}

	/* Gecko: added bad special check for event objects
	 * Idea is: if inv is identical events only then go ahead and merge)
	 * This goes hand in hand with the event keeping addition in get_split_ob() */
	if (ob1->inv || ob2->inv)
	{
		object *tmp1, *tmp2;

		if (!ob1->inv || !ob2->inv)
		{
			return 0;
		}

		/* Check that all inv objects are event objects */
		for (tmp1 = ob1->inv, tmp2 = ob2->inv; tmp1 && tmp2; tmp1 = tmp1->below, tmp2 = tmp2->below)
		{
			if (tmp1->type != TYPE_EVENT_OBJECT || tmp2->type != TYPE_EVENT_OBJECT)
			{
				return 0;
			}
		}

		/* Same number of events */
		if (tmp1 || tmp2)
		{
			return 0;
		}

		for (tmp1 = ob1->inv; tmp1; tmp1 = tmp1->below)
		{
			for (tmp2 = ob2->inv; tmp2; tmp2 = tmp2->below)
			{
				if (CAN_MERGE(tmp1, tmp2))
				{
					break;
				}
			}

			/* Couldn't find something to merge event from ob1 with? */
			if (!tmp2)
			{
				return 0;
			}
		}
	}

	/* Check the refcount pointer */
	if (ob1->name != ob2->name || ob1->title != ob2->title || ob1->race != ob2->race || ob1->slaying != ob2->slaying || ob1->msg != ob2->msg)
	{
		return 0;
	}

	/* Compare the static arrays/structs */
	if ((memcmp(&ob1->stats, &ob2->stats, sizeof(living)) != 0) || (memcmp(&ob1->resist, &ob2->resist, sizeof(ob1->resist)) != 0) || (memcmp(&ob1->attack, &ob2->attack, sizeof(ob1->attack)) != 0) || (memcmp(&ob1->protection, &ob2->protection, sizeof(ob1->protection)) != 0))
	{
		return 0;
	}

	/* Ignore REMOVED and BEEN_APPLIED */
	if (ob1->randomitems != ob2->randomitems || ob1->other_arch != ob2->other_arch || (ob1->flags[0] | 0x300) != (ob2->flags[0] | 0x300) || ob1->flags[1] != ob2->flags[1] || ob1->flags[2] != ob2->flags[2] || ob1->flags[3] != ob2->flags[3] || ob1->path_attuned != ob2->path_attuned || ob1->path_repelled != ob2->path_repelled || ob1->path_denied != ob2->path_denied || ob1->terrain_type != ob2->terrain_type || ob1->terrain_flag != ob2->terrain_flag || ob1->weapon_speed != ob2->weapon_speed || ob1->magic != ob2->magic || ob1->item_level != ob2->item_level || ob1->item_skill != ob2->item_skill || ob1->glow_radius != ob2->glow_radius  || ob1->level != ob2->level)
	{
		return 0;
	}

	/* Face can be difficult - but inv_face should never be different or obj is different! */
	if (ob1->face != ob2->face || ob1->inv_face != ob2->inv_face || ob1->animation_id != ob2->animation_id || ob1->inv_animation_id != ob2->inv_animation_id)
	{
		return 0;
	}

	/* Avoid merging empty containers. */
	if (ob1->type == CONTAINER)
	{
		return 0;
	}

	/* At least one of these has key_values. */
	if (ob1->key_values != NULL || ob2->key_values != NULL)
	{
		/* One has fields, but the other one doesn't. */
		if ((ob1->key_values == NULL) != (ob2->key_values == NULL))
		{
			return 0;
		}
		else
		{
			return compare_ob_value_lists(ob1, ob2);
		}
	}

	/* some stuff we should not need to test:
	 * carrying: because container merge isa big nono - and we tested ->inv before. better no double use here.
	 * weight_limit: same reason like carrying - add when we double use for stacking items
	 * last_heal;
	 * last_sp;
	 * last_grace;
	 * sint16 last_eat;
	 * will_apply;
	 * run_away;
	 * stealth;
	 * hide;
	 * move_type;
	 * layer;				this *can* be different for real same item - watch it
	 * anim_speed;			this can be interesting... */

	/* Can merge! */
	return 1;
}

/**
 * This function goes through all objects below and including top, and
 * merges op to the first matching object.
 * If top is NULL, it is calculated.
 * @param op The object
 * @param top The top object
 * @return Pointer to object if it succeded in the merge, NULL otherwise */
object *merge_ob(object *op, object *top)
{
	if (!op->nrof)
	{
		return 0;
	}

	if (top == NULL)
	{
		for (top = op; top != NULL && top->above != NULL; top = top->above);
	}

	for (; top != NULL; top = top->below)
	{
		if (top == op)
		{
			continue;
		}

		if (CAN_MERGE(op,top))
		{
			top->nrof += op->nrof;

			/* Don't want any adjustements now */
			op->weight = 0;

			/* this is right: no check off */
			remove_ob(op);

			return top;
		}
	}

	return NULL;
}

/**
 * Recursive function to calculate the weight an object is carrying.
 *
 * It goes through in figures out how much containers are carrying, and
 * sums it up.
 * @param op The object to calculate the weight for
 * @return The calculated weight */
signed long sum_weight(object *op)
{
	sint32 sum;
	object *inv;

	for (sum = 0, inv = op->inv; inv != NULL; inv = inv->below)
	{
		if (inv->inv)
		{
			sum_weight(inv);
		}

		sum += inv->carrying + (inv->nrof ? inv->weight * (int) inv->nrof : inv->weight);
	}

	/* because we avoid calculating for EVERY item in the loop above
	 * the weight adjustment for magic containers, we can run here in some
	 * rounding problems... in the worst case, we can remove a item from the
	 * container but we are not able to put it back because rounding.
	 * well, a small prize for saving *alot* of muls in player houses for example. */
	if (op->type == CONTAINER && op->weapon_speed != 1.0f)
	{
		sum = (sint32) ((float)sum * op->weapon_speed);
	}

	op->carrying = sum;

	return sum;
}

/**
 * Adds the specified weight to an object, and also updates how much the
 * environment(s) is/are carrying.
 * @param op The object
 * @param weight The weight to add */
static void add_weight(object *op, sint32 weight)
{
	while (op != NULL)
	{
		/* only *one* time magic can effect the weight of objects */
		if (op->type == CONTAINER && op->weapon_speed != 1.0f)
		{
			weight = (sint32) ((float) weight * op->weapon_speed);
		}

		op->carrying += weight;

		if (op->env && op->env->type == PLAYER)
		{
			esrv_update_item(UPD_WEIGHT, op->env, op);
		}

		op = op->env;
	}
}

/**
 * Recursively (outwards) subtracts a number from the weight of an object
 * (and what is carried by its environment(s)).
 * @param op The object
 * @param weight The weight to subtract */
static void sub_weight(object *op, sint32 weight)
{
	while (op != NULL)
	{
		/* only *one* time magic can effect the weight of objects */
		if (op->type == CONTAINER && op->weapon_speed != 1.0f)
		{
			weight = (sint32) ((float) weight * op->weapon_speed);
		}

		op->carrying -= weight;

		if (op->env && op->env->type == PLAYER)
		{
			esrv_update_item(UPD_WEIGHT, op->env, op);
		}

		op = op->env;
	}
}

/* Eneq(@csd.uu.se): Since we can have items buried in a character we need
 * a better check.  We basically keeping traversing up until we can't
 * or find a player. */

/* this function was wrong used in the past. Its only senseful for fix_player() - for
 * example we remove a active force from a player which was inserted in a special
 * force container (for example a exp object). For inventory views, we DON'T need
 * to update the item then! the player only sees his main inventory and *one* container.
 * is this object in a closed container, the player will never notice any change. */
object *is_player_inv(object *op)
{
	for (; op != NULL && op->type != PLAYER; op = op->env)
		if (op->env == op)
			op->env = NULL;

	return op;
}

/**
 * Used by: Server DM commands: dumpbelow, dump.
 *
 * Some error messages.
 *
 * The result of the dump is stored in the static global errmsg array.
 * @param op The object to dump */
static void dump_object2(object *op)
{
	char *cp, buf[MAX_BUF];

	if (op->arch != NULL)
	{
		snprintf(buf, sizeof(buf), "arch %s (%u)\n", op->arch->name ? op->arch->name : "(null)", op->count);
		strcat(errmsg, buf);

		if ((cp = get_ob_diff(op, &empty_archetype->clone)) != NULL)
		{
			strcat(errmsg, cp);
		}

		strcat(errmsg, "end\n");
	}
	else
	{
		strcat(errmsg, "Object ");

		if (op->name == NULL)
		{
			strcat(errmsg, "(null)");
		}
		else
		{
			strcat(errmsg, op->name);
		}

		strcat(errmsg, "\n");
		strcat(errmsg, "end\n");
	}
}

/**
 * Dumps an object. Returns output in the static global errmsg array.
 * @param op The object to dump. */
void dump_object(object *op)
{
	if (op == NULL)
	{
		strcpy(errmsg, "[NULL pointer]");

		return;
	}

	errmsg[0] = '\0';
	dump_object2(op);
}

/**
 * Dumps an object. Return the result into a string.
 *
 * @note No checking is done for the validity of the target string, so
 * you need to be sure that you allocated enough space for it.
 * @param op The object to dump
 * @param outstr Pointer to character where to store the dump. */
void dump_me(object *op, char *outstr)
{
	char *cp;

	if (op == NULL)
	{
		strcpy(outstr, "[NULL pointer]");

		return;
	}

	outstr[0] = '\0';

	if (op->arch != NULL)
	{
		strcat(outstr, "arch ");
		strcat(outstr, op->arch->name ? op->arch->name : "(null)");
		strcat(outstr, "\n");

		if ((cp = get_ob_diff(op, &empty_archetype->clone)) != NULL)
		{
			strcat(outstr, cp);
		}

		strcat(outstr, "end\n");
	}
	else
	{
		strcat(outstr, "Object ");

		if (op->name == NULL)
		{
			strcat(outstr, "(null)");
		}
		else
		{
			strcat(outstr, op->name);
		}

		strcat(outstr, "\n");
		strcat(outstr, "end\n");
	}
}

void free_all_object_data()
{
#ifdef MEMORY_DEBUG
	object *op, *next;

	for (op = free_objects; op != NULL; )
	{
		next = op->next;
		free(op);
		nrofallocobjects--;
		nroffreeobjects--;
		op = next;
	}
#endif

	LOG(llevDebug, "%d allocated objects, %d free objects\n", pool_object->nrof_allocated, pool_object->nrof_free);
}

/**
 * Returns the object which this object marks as being the owner.
 *
 * An ID scheme is used to avoid pointing to objects which have been
 * freed and are now reused. If this is detected, the owner is
 * set to NULL, and NULL is returned.
 *
 * (This scheme should be changed to a refcount scheme in the future)
 * @param op The object to get owner for
 * @return Owner of the object if any, NULL if no owner */
object *get_owner(object *op)
{
	if (!op || op->owner == NULL)
	{
		return NULL;
	}

	if (!OBJECT_FREE(op) && op->owner->count == op->ownercount)
	{
		return op->owner;
	}

	op->owner = NULL, op->ownercount = 0;

	return NULL;
}

/**
 * Clear pointer to owner of an object, including ownercount.
 * @param op The object to clear the owner for */
void clear_owner(object *op)
{
	if (!op)
	{
		return;
	}

#if 0
	if (op->owner && op->ownercount == op->owner->count)
		op->owner->refcount--;
#endif

	op->owner = NULL;
	op->ownercount = 0;
}

/**
 * Sets the owner of the first object to the second object.
 *
 * Also checkpoints a backup id scheme which detects freeing (and
 * reusage) of the owner object.
 * @param op The object to set the owner for
 * @param owner The owner
 * @see get_owner() */
static void set_owner_simple(object *op, object *owner)
{
	/* next line added to allow objects which own objects */
	/* Add a check for ownercounts in here, as I got into an endless loop
	 * with the fireball owning a poison cloud which then owned the
	 * fireball.  I believe that was caused by one of the objects getting
	 * freed and then another object replacing it.  Since the ownercounts
	 * didn't match, this check is valid and I believe that cause is valid. */
	while (owner->owner && owner != owner->owner && owner->ownercount == owner->owner->count)
	{
		owner = owner->owner;
	}

	/* IF the owner still has an owner, we did not resolve to a final owner.
	 * so lets not add to that. */
	if (owner->owner)
	{
		return;
	}

	op->owner = owner;

	op->ownercount = owner->count;
	/*owner->refcount++;*/
}

static void set_skill_pointers(object *op, object *chosen_skill, object *exp_obj)
{
	op->chosen_skill = chosen_skill;
	op->exp_obj = exp_obj;

	/* unfortunately, we can't allow summoned monsters skill use
	 * because we will need the chosen_skill field to pick the
	 * right skill/stat modifiers for calc_skill_exp(). See
	 * hit_player() in server/attack.c -b.t. */
	CLEAR_FLAG(op, FLAG_CAN_USE_SKILL);
	CLEAR_FLAG(op, FLAG_READY_SKILL);
}

/**
 * Sets the owner and sets the skill and exp pointers to owner's current
 * skill and experience objects.
 * @param op The object
 * @param owner The owner */
void set_owner(object *op, object *owner)
{
	if (owner == NULL || op == NULL)
	{
		return;
	}

	set_owner_simple(op, owner);

	if (owner->type == PLAYER && owner->chosen_skill)
	{
		set_skill_pointers(op, owner->chosen_skill, owner->chosen_skill->exp_obj);
	}
	else if (op->type != PLAYER)
	{
		CLEAR_FLAG(op, FLAG_READY_SKILL);
	}
}

/**
 * Set the owner to clone's current owner and set the skill and experience
 * objects to clone's objects (typically those objects that where the owner's
 * current skill and experience objects at the time when clone's owner was
 * set - not the owner's current skill and experience objects).
 *
 * Use this function if player created an object (e.g. fire bullet, swarm
 * spell), and this object creates further objects whose kills should be
 * accounted for the player's original skill, even if player has changed
 * skills meanwhile.
 * @param op The object
 * @param clone The clone */
void copy_owner(object *op, object *clone)
{
	object *owner = get_owner(clone);

	if (owner == NULL)
	{
		/* players don't have owners - they own themselves. Update
		 * as appropriate. */
		if (clone->type == PLAYER)
		{
			owner = clone;
		}
		else
		{
			return;
		}
	}

	set_owner_simple(op, owner);

	if (clone->chosen_skill)
	{
		set_skill_pointers(op, clone->chosen_skill, clone->exp_obj);
	}
	else if (op->type != PLAYER)
	{
		CLEAR_FLAG(op, FLAG_READY_SKILL);
	}
}

/**
 * Frees everything allocated by an object, and also initializes all
 * variables and flags to default settings.
 * @param op The object */
void initialize_object(object *op)
{
	/* the memset will clear all these values for us, but we need
	 * to reduce the refcount on them. */
	FREE_ONLY_HASH(op->name);
	FREE_ONLY_HASH(op->title);
	FREE_ONLY_HASH(op->race);
	FREE_ONLY_HASH(op->slaying);
	FREE_ONLY_HASH(op->msg);

	/* Using this memset is a lot easier (and probably faster)
	 * than explicitly clearing the fields. */
	memset(op, 0, sizeof(object));

	/* Set some values that should not be 0 by default */
	/* control the facings 25 animations */
	op->anim_enemy_dir = -1;
	/* the same for movement */
	op->anim_moving_dir = -1;
	op->anim_enemy_dir_last = -1;
	op->anim_moving_dir_last = -1;
	op->anim_last_facing = 4;
	op->anim_last_facing_last = -1;

	op->face = blank_face;
	op->attacked_by_count= -1;

	/* give the object a new (unique) count tag */
	op->count= ++ob_count;
}

/**
 * Copy object first frees everything allocated by the second object,
 * and then copies the contends of the first object into the second
 * object, allocating what needs to be allocated.
 * @param op2
 * @param op  */
void copy_object(object *op2, object *op)
{
	int is_removed = QUERY_FLAG(op, FLAG_REMOVED);

	FREE_ONLY_HASH(op->name);
	FREE_ONLY_HASH(op->title);
	FREE_ONLY_HASH(op->race);
	FREE_ONLY_HASH(op->slaying);
	FREE_ONLY_HASH(op->msg);

	free_key_values(op);

	(void) memcpy((void *)((char *) op + offsetof(object, name)), (void *)((char *) op2 + offsetof(object, name)), sizeof(object) - offsetof(object, name));

	if (is_removed)
	{
		SET_FLAG(op, FLAG_REMOVED);
	}

	ADD_REF_NOT_NULL_HASH(op->name);
	ADD_REF_NOT_NULL_HASH(op->title);
	ADD_REF_NOT_NULL_HASH(op->race);
	ADD_REF_NOT_NULL_HASH(op->slaying);
	ADD_REF_NOT_NULL_HASH(op->msg);

	if (QUERY_FLAG(op, FLAG_IDENTIFIED))
	{
		SET_FLAG(op, FLAG_KNOWN_MAGICAL);
		SET_FLAG(op, FLAG_KNOWN_CURSED);
	}

	/* Only alter speed_left when we sure we have not done it before */
	if (op->speed < 0 && op->speed_left == op->arch->clone.speed_left)
	{
		op->speed_left += (RANDOM() % 90) / 100.0f;
	}

	/* Copy over key_values, if any. */
	if (op2->key_values)
	{
		key_value *tail = NULL, *i;

		op->key_values = NULL;

		for (i = op2->key_values; i; i = i->next)
		{
			key_value *new_link = malloc(sizeof(key_value));

			new_link->next = NULL;
			new_link->key = add_refcount(i->key);

			if (i->value)
			{
				new_link->value = add_refcount(i->value);
			}
			else
			{
				new_link->value = NULL;
			}

			/* Try and be clever here, too. */
			if (op->key_values == NULL)
			{
				op->key_values = new_link;
				tail = new_link;
			}
			else
			{
				tail->next = new_link;
				tail = new_link;
			}
		}
	}

	update_ob_speed(op);
}

/**
 * Same as copy_object(), but not touching the active list.
 * @param op2
 * @param op  */
void copy_object_data(object *op2, object *op)
{
	int is_removed = QUERY_FLAG(op, FLAG_REMOVED);

	FREE_ONLY_HASH(op->name);
	FREE_ONLY_HASH(op->title);
	FREE_ONLY_HASH(op->race);
	FREE_ONLY_HASH(op->slaying);
	FREE_ONLY_HASH(op->msg);

    free_key_values(op);

	(void) memcpy((void *)((char *) op + offsetof(object, name)), (void *)((char *) op2 + offsetof(object, name)), sizeof(object) - offsetof(object, name));

	if (is_removed)
	{
		SET_FLAG(op, FLAG_REMOVED);
	}

	ADD_REF_NOT_NULL_HASH(op->name);
	ADD_REF_NOT_NULL_HASH(op->title);
	ADD_REF_NOT_NULL_HASH(op->race);
	ADD_REF_NOT_NULL_HASH(op->slaying);
	ADD_REF_NOT_NULL_HASH(op->msg);

	if (QUERY_FLAG(op, FLAG_IDENTIFIED))
	{
		SET_FLAG(op, FLAG_KNOWN_MAGICAL);
		SET_FLAG(op, FLAG_KNOWN_CURSED);
	}

	/* Copy over key_values, if any. */
	if (op2->key_values)
	{
		key_value *tail = NULL, *i;

		op->key_values = NULL;

		for (i = op2->key_values; i; i = i->next)
		{
			key_value *new_link = malloc(sizeof(key_value));

			new_link->next = NULL;
			new_link->key = add_refcount(i->key);

			if (i->value)
			{
				new_link->value = add_refcount(i->value);
			}
			else
			{
				new_link->value = NULL;
			}

			/* Try and be clever here, too. */
			if (op->key_values == NULL)
			{
				op->key_values = new_link;
				tail = new_link;
			}
			else
			{
				tail->next = new_link;
				tail = new_link;
			}
		}
	}
}

/**
 * Grabs an object from the list of unused objects, makes sure it is
 * initialized, and returns it.
 *
 * If there are no free objects, expand_objects() is called to get more.
 * @return The new object. */
object *get_object()
{
	object *new_obj = (object *) get_poolchunk(pool_object);

	mark_object_removed(new_obj);

	return new_obj;
}

/**
 * If an object with the IS_TURNABLE() flag needs to be turned due
 * to the closest player being on the other side, this function can
 * be called to update the face variable, <u>and</u> how it looks on the
 * map.
 * @param op The object to update
 */
void update_turn_face(object *op)
{
	if (!QUERY_FLAG(op, FLAG_IS_TURNABLE) || op->arch == NULL)
	{
		return;
	}

	SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
	update_object(op, UP_OBJ_FACE);
}

/**
 * Updates the speed of an object. If the speed changes from 0 to another
 * value, or vice versa, then add/remove the object from the active list.
 * This function needs to be called whenever the speed of an object changes.
 * @param op The object */
void update_ob_speed(object *op)
{
	extern int arch_init;

	/* No reason putting the archetypes objects on the speed list,
	 * since they never really need to be updated. */
	if (OBJECT_FREE(op) && op->speed)
	{
		dump_object(op);
		LOG(llevBug, "BUG: Object %s is freed but has speed.\n:%s\n", op->name, errmsg);
		op->speed = 0;
	}

	if (arch_init)
	{
		return;
	}

	/* These are special case objects - they have speed set, but should not be put
	 * on the active list. */
	if (op->type == SPAWN_POINT_MOB)
	{
		return;
	}

	if (FABS(op->speed) > MIN_ACTIVE_SPEED)
	{
		/* If already on active list, don't do anything */
		if (op->active_next || op->active_prev || op == active_objects)
		{
			return;
		}

		/* process_events() expects us to insert the object at the beginning
		 * of the list. */
		/*LOG(-1, "SPEED: add object to speed list: %s (%d,%d)\n", query_name(op), op->x, op->y);*/
		op->active_next = active_objects;

		if (op->active_next != NULL)
		{
			op->active_next->active_prev = op;
		}

		active_objects = op;
		op->active_prev = NULL;
	}
	else
	{
		/* If not on the active list, nothing needs to be done */
		if (!op->active_next && !op->active_prev && op != active_objects)
		{
			return;
		}

		/*LOG(-1, "SPEED: remove object from speed list: %s (%d,%d)\n", query_name(op), op->x, op->y);*/

		if (op->active_prev == NULL)
		{
			active_objects = op->active_next;

			if (op->active_next != NULL)
			{
				op->active_next->active_prev = NULL;
			}
		}
		else
		{
			op->active_prev->active_next = op->active_next;

			if (op->active_next)
			{
				op->active_next->active_prev = op->active_prev;
			}
		}

		op->active_next = NULL;
		op->active_prev = NULL;
	}
}

/**
 * OLD NOTES
 * update_object() updates the array which represents the map.
 * It takes into account invisible objects (and represent squares covered
 * by invisible objects by whatever is below them (unless it's another
 * invisible object, etc...)
 * If the object being updated is beneath a player, the look-window
 * of that player is updated (this might be a suboptimal way of
 * updating that window, though, since update_object() is called _often_)
 *
 * action is a hint of what the caller believes need to be done.
 * For example, if the only thing that has changed is the face (due to
 * an animation), we don't need to call update_position until that actually
 * comes into view of a player.  OTOH, many other things, like addition/removal
 * of walls or living creatures may need us to update the flags now.
 * current action are:
 * UP_OBJ_INSERT: op was inserted
 * UP_OBJ_REMOVE: op was removed
 * UP_OBJ_CHANGE: object has somehow changed.  In this case, we always update
 *  as that is easier than trying to look at what may have changed.
 * UP_OBJ_FACE: only the objects face has changed.
 *
 * I want use this function as one and only way to decide what we update
 * in a tile and how - and what not. The smarter this function is, the
 * better. This function MUST be called now from everything what does a
 * noticeable change to an object. We can pre-decide whether it's needed
 * to call but normally we must call this.
 * @param op
 * @param action  */
void update_object(object *op, int action)
{
	MapSpace *msp;
	int flags, newflags;

	/*LOG(-1, "update_object: %s (%d,%d) - action %x\n", op->name, op->x, op->y, action);*/
	if (op == NULL)
	{
		/* this should never happen */
		LOG(llevError, "ERROR: update_object() called for NULL object.\n");
		return;
	}

	if (op->env != NULL || !op->map || op->map->in_memory == MAP_SAVING)
		return;

	/* make sure the object is within map boundaries */
#if 0
	if (op->x < 0 || op->x >= MAP_WIDTH(op->map) || op->y < 0 || op->y >= MAP_HEIGHT(op->map))
	{
		LOG(llevError, "ERROR: update_object() called for object out of map!\n");
		return;
	}
#endif

	/* no need to change anything except the map update counter */
	if (action == UP_OBJ_FACE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_FACE - %s\n", query_name(op));
#endif
		INC_MAP_UPDATE_COUNTER(op->map, op->x, op->y);
		return;
	}

	msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);
	newflags = msp->flags;
	flags = newflags & ~P_NEED_UPDATE;

	/* always resort layer - but not always flags */
	if (action == UP_OBJ_INSERT)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_INS - %s\n", query_name(op));
#endif
		/* force layer rebuild */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;

		/* handle lightning system */
		if (op->glow_radius)
			adjust_light_source(op->map, op->x, op->y, op->glow_radius);

		/* this is handled a bit more complex, we must always loop the flags! */
		if (QUERY_FLAG(op, FLAG_NO_PASS) || QUERY_FLAG(op, FLAG_PASS_THRU))
			newflags |= P_FLAGS_UPDATE;
		/* floors define our node - force a update */
		else if (QUERY_FLAG(op, FLAG_IS_FLOOR))
		{
			newflags |= P_FLAGS_UPDATE;
			msp->light_value += op->last_sp;
		}
		/* ok, we don't have to use flag loop - we can set it by hand! */
		else
		{
			if (op->type == CHECK_INV)
				newflags |= P_CHECK_INV;
			else if (op->type == MAGIC_EAR)
				newflags|= P_MAGIC_EAR;

			if (QUERY_FLAG(op, FLAG_ALIVE))
				newflags |= P_IS_ALIVE;

			if (QUERY_FLAG(op, FLAG_IS_PLAYER))
				newflags |= P_IS_PLAYER;

			if (QUERY_FLAG(op, FLAG_PLAYER_ONLY))
				newflags |= P_PLAYER_ONLY;

			if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
				newflags |= P_BLOCKSVIEW;

			if (QUERY_FLAG(op, FLAG_NO_MAGIC))
				newflags |= P_NO_MAGIC;

			if (QUERY_FLAG(op, FLAG_NO_CLERIC))
				newflags |= P_NO_CLERIC;

			if (QUERY_FLAG(op, FLAG_WALK_ON))
				newflags |= P_WALK_ON;

			if (QUERY_FLAG(op, FLAG_FLY_ON))
				newflags |= P_FLY_ON;

			if (QUERY_FLAG(op, FLAG_WALK_OFF))
				newflags |= P_WALK_OFF;

			if (QUERY_FLAG(op, FLAG_FLY_OFF))
				newflags |= P_FLY_OFF;

			if (QUERY_FLAG(op, FLAG_DOOR_CLOSED))
				newflags |= P_DOOR_CLOSED;

			if (QUERY_FLAG(op, FLAG_CAN_REFL_SPELL))
				newflags |= P_REFL_SPELLS;

			if (QUERY_FLAG(op, FLAG_CAN_REFL_MISSILE))
				newflags |= P_REFL_MISSILE;
		}
	}
	else if (action == UP_OBJ_REMOVE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug,"UO_REM - %s\n", query_name(op));
#endif
		/* force layer rebuild */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;

		/* we don't handle floor tile light/darkness setting here -
		 * we assume we don't remove a floor tile ever before dropping
		 * the map. */

		/* handle lightning system */
		if (op->glow_radius)
			adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));

		/* we must rebuild the flags when one of this flags is touched from our object */
		if (QUERY_FLAG(op, FLAG_ALIVE) || QUERY_FLAG(op, FLAG_IS_PLAYER) || QUERY_FLAG(op, FLAG_BLOCKSVIEW) || QUERY_FLAG(op, FLAG_DOOR_CLOSED) || QUERY_FLAG(op, FLAG_PASS_THRU) || QUERY_FLAG(op, FLAG_NO_PASS) || QUERY_FLAG(op, FLAG_PLAYER_ONLY) || QUERY_FLAG(op, FLAG_NO_MAGIC) || QUERY_FLAG(op, FLAG_NO_CLERIC) || QUERY_FLAG(op, FLAG_WALK_ON) || QUERY_FLAG(op, FLAG_FLY_ON) || QUERY_FLAG(op, FLAG_WALK_OFF) || QUERY_FLAG(op, FLAG_FLY_OFF) || QUERY_FLAG(op, FLAG_CAN_REFL_SPELL) || QUERY_FLAG(op, FLAG_CAN_REFL_MISSILE) || QUERY_FLAG(op,	FLAG_IS_FLOOR) || op->type == CHECK_INV || op->type == MAGIC_EAR )
			newflags |= P_FLAGS_UPDATE;
	}
	else if (action == UP_OBJ_FLAGS)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_FLAGS - %s\n", query_name(op));
#endif
		/* force flags rebuild but no tile counter*/
		newflags |= P_FLAGS_UPDATE;
	}
	else if (action == UP_OBJ_FLAGFACE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_FLAGFACE - %s\n", query_name(op));
#endif
		/* force flags rebuild */
		newflags |= P_FLAGS_UPDATE;
		msp->update_tile++;
	}
	else if (action == UP_OBJ_LAYER)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_LAYER - %s\n", query_name(op));
#endif
		/* rebuild layers - most common when we change visibility of the object */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;
	}
	else if (action == UP_OBJ_ALL)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_ALL - %s\n", query_name(op));
#endif
		/* force full tile update */
		newflags |= (P_FLAGS_UPDATE | P_NEED_UPDATE);
		msp->update_tile++;
	}
	else
	{
		LOG(llevError, "ERROR: update_object called with invalid action: %d\n", action);
		return;
	}

	if (flags != newflags)
	{
		/* rebuild flags */
		if (newflags & (P_FLAGS_UPDATE))
		{
			msp->flags |= (newflags | P_NO_ERROR | P_FLAGS_ONLY);
			update_position(op->map, op->x, op->y);
		}
		else
			msp->flags |= newflags;
	}

	if (op->more != NULL)
		update_object(op->more, action);
}

/**
 * Drops the inventory of ob into ob's current environment.
 *
 * Makes some decisions whether to actually drop or not, and/or to
 * create a corpse for the stuff.
 * @param ob The object to drop the inventory for. */
void drop_ob_inv(object *ob)
{
	object *corpse = NULL;
	object *enemy = NULL;
	object *tmp_op = NULL;
	object *tmp = NULL;

	/* We don't handle players here */
	if (ob->type == PLAYER)
	{
		LOG(llevBug, "BUG: drop_ob_inv() - try to drop items of %s\n", ob->name);

		return;
	}

	/* TODO */
	if (ob->env == NULL && (ob->map == NULL || ob->map->in_memory != MAP_IN_MEMORY))
	{
		LOG(llevDebug, "BUG: drop_ob_inv() - can't drop inventory of objects not in map yet: %s (%x)\n", ob->name, ob->map);

		return;
	}

	/* Create race corpse and/or drop stuff to floor */
	if (ob->enemy && ob->enemy->type == PLAYER)
	{
		enemy = ob->enemy;
	}
	else
	{
		enemy = get_owner(ob->enemy);
	}

	if ((QUERY_FLAG(ob, FLAG_CORPSE) && !QUERY_FLAG(ob, FLAG_STARTEQUIP)) || QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
	{
		racelink *race_corpse = find_racelink(ob->race);

		if (race_corpse)
		{
			corpse = arch_to_object(race_corpse->corpse);
			corpse->x = ob->x;
			corpse->y = ob->y;
			corpse->map = ob->map;
			corpse->weight = ob->weight;
		}
	}

	tmp_op = ob->inv;

	while (tmp_op != NULL)
	{
		tmp = tmp_op->below;
		/* Inv-no check off / This will be destroyed in next loop of object_gc() */
		remove_ob(tmp_op);
		/* if we recall spawn mobs, we don't want drop their items as free.
		 * So, marking the mob itself with "FLAG_STARTEQUIP" will kill
		 * all inventory and not dropping it on the map.
		 * This also happens when a player slays a to low mob/non exp mob.
		 * Don't drop any sys_object in inventory... I can't think about
		 * any use... when we do it, a disease needle for example
		 * is dropping his disease force and so on. */

		if (tmp_op->type == TYPE_QUEST_CONTAINER)
		{
			/* legal, non freed enemy */
			if (enemy && enemy->type == PLAYER && enemy->count == ob->enemy_count)
			{
				check_quest(enemy, tmp_op);
			}
		}
		else if (!(QUERY_FLAG(ob, FLAG_STARTEQUIP) || (tmp_op->type != RUNE && (QUERY_FLAG(tmp_op, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp_op, FLAG_STARTEQUIP) || QUERY_FLAG(tmp_op, FLAG_NO_DROP)))))
		{
			tmp_op->x = ob->x, tmp_op->y = ob->y;

			/* if we have a corpse put the item in it */
			if (corpse)
			{
				insert_ob_in_ob(tmp_op, corpse);
			}
			else
			{
				/* don't drop traps from a container to the floor.
				 * removing the container where a trap is applied will
				 * neutralize the trap too
				 * Also not drop it in env - be safe here */
				if (tmp_op->type != RUNE)
				{
					if (ob->env)
					{
						insert_ob_in_ob(tmp_op, ob->env);

						/* this should handle in future insert_ob_in_ob() */
						if (ob->env->type == PLAYER)
						{
							esrv_send_item(ob->env, tmp_op);
						}
						else if (ob->env->type == CONTAINER)
						{
							esrv_send_item(ob->env, tmp_op);
						}
					}
					/* Insert in same map as the env */
					else
					{
						insert_ob_in_map(tmp_op, ob->map, NULL, 0);
					}
				}
			}
		}
		tmp_op = tmp;
	}

	if (corpse)
	{
		/* drop the corpse when something is in OR corpse_forced is set */
		/* i changed this to drop corpse always even they have no items
		 * inside (player get confused when corpse don't drop. To avoid
		 * clear corpses, change below "||corpse " to "|| corpse->inv" */
		if (QUERY_FLAG(ob, FLAG_CORPSE_FORCED) || corpse)
		{
			/* ok... we have a corpse AND we insert something in.
			 * now check enemy and/or attacker to find a player.
			 * if there is one - personlize this corpse container.
			 * this gives the player the chance to grap this stuff first
			 * - and looter will be stopped. */
			if (enemy && enemy->type == PLAYER)
			{
				if (enemy->count == ob->enemy_count)
				{
					FREE_AND_ADD_REF_HASH(corpse->slaying, enemy->name);
				}
			}
			/* && no player */
			else if (QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
			{
				/* normallly only player drop corpse. But in some cases
				 * npc can do it too. Then its smart to remove that corpse fast.
				 * It will not harm anything because we never deal for NPC with
				 * bounty. */
				corpse->stats.food = 3;
			}

			/* change sub_type to mark this corpse */
			if (corpse->slaying)
			{
				if (CONTR(enemy)->party_number != -1)
				{
					corpse->stats.maxhp = CONTR(enemy)->party_number;
					corpse->sub_type1 = ST1_CONTAINER_CORPSE_party;
				}
				else
				{
					corpse->sub_type1 = ST1_CONTAINER_CORPSE_player;
				}
			}

			if (ob->env)
			{
				insert_ob_in_ob(corpse, ob->env);

				/* this should handle in future insert_ob_in_ob() */
				if (ob->env->type == PLAYER)
				{
					esrv_send_item(ob->env, corpse);
				}
				else if (ob->env->type == CONTAINER)
				{
					esrv_send_item(ob->env, corpse);
				}
			}
			else
			{
				insert_ob_in_map(corpse, ob->map, NULL, 0);
			}
		}
		/* disabled */
		else
		{
			/* if we are here, our corpse mob had something in inv but its nothing to drop */
			if (!QUERY_FLAG(corpse, FLAG_REMOVED))
			{
				/* no check off - not put in the map here */
				remove_ob(corpse);
			}
		}
	}
}

/**
 * Frees everything allocated by an object, removes it from the list of
 * used objects, and puts it on the list of free objects. This function
 * is called automatically to free unused objects (it is called from
 * return_poolchunk() during garbage collection in object_gc() ).
 *
 * The object must have been removed by remove_ob() first for
 * this function to succeed.
 * @param ob The object to destroy. */
void destroy_object(object *ob)
{
	if (OBJECT_FREE(ob))
	{
		dump_object(ob);
		LOG(llevBug, "BUG: Trying to destroy freed object.\n%s\n", errmsg);

		return;
	}

	if (!QUERY_FLAG(ob, FLAG_REMOVED))
	{
		dump_object(ob);
		LOG(llevBug, "BUG: Destroy object called with non removed object\n:%s\n", errmsg);
	}

	free_key_values(ob);

	/* This should be very rare... */
	if (QUERY_FLAG(ob, FLAG_IS_LINKED))
	{
		remove_button_link(ob);
	}

	if (QUERY_FLAG(ob, FLAG_FRIENDLY))
	{
		remove_friendly_object(ob);
	}

	if (ob->type == CONTAINER && ob->attacked_by)
	{
		container_unlink(NULL, ob);
	}

	/* Make sure to get rid of the inventory, too. It will be destroy()ed at the next gc */
	/* TODO: maybe destroy() it here too? */
	remove_ob_inv(ob);

	/* Remove object from the active list */
	ob->speed = 0;
	update_ob_speed(ob);
	/*LOG(llevDebug, "FO: a:%s %x >%s< (#%d)\n", ob->arch ? (ob->arch->name ? ob->arch->name : "") : "", ob->name, ob->name ? ob->name : "", ob->name ? query_refcount(ob->name) : 0);*/

	/* Free attached attrsets */
	if (ob->custom_attrset)
	{
		/*LOG(llevDebug, "destroy_object() custom attrset found in object %s (type %d)\n", STRING_OBJ_NAME(ob), ob->type);*/

		switch (ob->type)
		{
			case PLAYER:
			/* Players are changed into DEAD_OBJECTs when they logout */
			case DEAD_OBJECT:
				return_poolchunk(ob->custom_attrset, pool_player);
				break;

			default:
				LOG(llevBug, "BUG: destroy_object() custom attrset found in unsupported object %s (type %d)\n", STRING_OBJ_NAME(ob), ob->type);
		}

		ob->custom_attrset = NULL;
	}

	if (ob->type == BEACON)
	{
		beacon_remove(ob);
	}

	FREE_AND_CLEAR_HASH2(ob->name);
	FREE_AND_CLEAR_HASH2(ob->title);
	FREE_AND_CLEAR_HASH2(ob->race);
	FREE_AND_CLEAR_HASH2(ob->slaying);
	FREE_AND_CLEAR_HASH2(ob->msg);

	/* mark object as "do not use" and invalidate all references to it */
	ob->count = 0;
}

/**
 * Drop op's inventory on the floor and remove op from the map.
 * Used mainly for physical destruction of normal objects and mobs
 * @param op  */
void destruct_ob(object *op)
{
	if (op->inv)
	{
		drop_ob_inv(op);
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_DEFAULT);
}

/**
 * This function removes the object op from the linked list of objects
 * which it is currently tied to.  When this function is done, the
 * object will have no environment.  If the object previously had an
 * environment, the x and y coordinates will be updated to
 * the previous environment.
 *
 * if we want remove alot of players inventory items, set
 * FLAG_NO_FIX_PLAYER to the player first and call fix_player()
 * explicit then.
 * @param op  */
void remove_ob(object *op)
{
	MapSpace *msp;
	object *otmp;

	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		/*dump_object(op);*/
		LOG(llevBug, "BUG: Trying to remove removed object.:%s map:%s (%d,%d)\n", query_name(op, NULL), op->map ? (op->map->path ? op->map->path : "op->map->path == NULL") : "op->map == NULL", op->x, op->y);

		return;
	}

	/* check off is handled outside here */
	if (op->more != NULL)
	{
		remove_ob(op->more);
	}

	mark_object_removed(op);
	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);

	/* In this case, the object to be removed is in someones
	 * inventory. */
	if (op->env != NULL)
	{
		/* this is not enough... when we for example remove money from a pouch
		 * which is in a sack (which is itself in the player inv) then the weight
		 * of the sack is not calculated right. This is only a temporary effect but
		 * we need to fix it here a recursive ->env chain. */
		if (op->nrof)
		{
			sub_weight(op->env, op->weight * op->nrof);
		}
		else
		{
			sub_weight(op->env, op->weight + op->carrying);
		}

		/* NO_FIX_PLAYER is set when a great many changes are being
		 * made to players inventory.  If set, avoiding the call to save cpu time.
		 * the flag is set from outside... perhaps from a drop_all() function. */
		if ((otmp = is_player_inv(op->env)) != NULL && CONTR(otmp) && !QUERY_FLAG(otmp, FLAG_NO_FIX_PLAYER))
		{
			fix_player(otmp);
		}

		if (op->above != NULL)
		{
			op->above->below = op->below;
		}
		else
		{
			op->env->inv = op->below;
		}

		if (op->below != NULL)
		{
			op->below->above = op->above;
		}

		/* we set up values so that it could be inserted into
		 * the map, but we don't actually do that - it is up
		 * to the caller to decide what we want to do. */
		op->x = op->env->x, op->y = op->env->y;

#ifdef POSITION_DEBUG
		op->ox = op->x, op->oy = op->y;
#endif

		op->map = op->env->map;
		op->above = NULL, op->below = NULL;
		op->env = NULL;
		return;
	}

	/* If we get here, we are removing it from a map */
	if (!op->map)
	{
		LOG(llevBug, "BUG: remove_ob(): object %s (%s) not on map or env.\n", query_short_name(op, NULL), op->arch ? (op->arch->name ? op->arch->name : "<nor arch name!>") : "<no arch!>");

		return;
	}

	/* lets first unlink this object from map*/

	/* if this is the base layer object, we assign the next object to be it if it is from same layer type */
	msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

	if (op->layer)
	{
		if (GET_MAP_SPACE_LAYER(msp, op->layer - 1) == op)
		{
			/* well, don't kick the inv objects of this layer to normal layer */
			if (op->above && op->above->layer == op->layer && GET_MAP_SPACE_LAYER(msp, op->layer + 6) != op->above)
			{
				SET_MAP_SPACE_LAYER(msp, op->layer - 1, op->above);
			}
			else
			{
				SET_MAP_SPACE_LAYER(msp, op->layer - 1, NULL);
			}
		}
		/* inv layer? */
		else if (GET_MAP_SPACE_LAYER(msp, op->layer + 6) == op)
		{
			if (op->above && op->above->layer == op->layer)
			{
				SET_MAP_SPACE_LAYER(msp, op->layer + 6, op->above);
			}
			else
			{
				SET_MAP_SPACE_LAYER(msp, op->layer + 6, NULL);
			}
		}
	}

	/* link the object above us */
	if (op->above)
	{
		op->above->below = op->below;
	}
	/* assign below as last one */
	else
	{
		SET_MAP_SPACE_LAST(msp, op->below);
	}

	/* Relink the object below us, if there is one */
	if (op->below)
	{
		op->below->above = op->above;
	}
	/* first object goes on above it. */
	else
	{
		SET_MAP_SPACE_FIRST(msp, op->above);
	}

	op->above = NULL;
	op->below = NULL;

	/* this is triggered when a map is swaped out and the objects on it get removed too */
	if (op->map->in_memory == MAP_SAVING)
	{
		return;
	}

	/* we updated something here - mark this tile as changed! */
	msp->update_tile++;

	/* some player only stuff.
	 * we adjust the ->player map variable and the local map player chain. */
	if (op->type == PLAYER)
	{
		struct pl_player *pltemp = CONTR(op);

		/* now we remove us from the local map player chain */
		if (pltemp->map_below)
		{
			CONTR(pltemp->map_below)->map_above = pltemp->map_above;
		}
		else
		{
			op->map->player_first = pltemp->map_above;
		}

		if (pltemp->map_above)
		{
			CONTR(pltemp->map_above)->map_below = pltemp->map_below;
		}

		pltemp->map_below = pltemp->map_above = NULL;

		/* thats always true when touching the players map pos. */
		pltemp->update_los = 1;

		/* a open container NOT in our player inventory = unlink (close) when we move */
		if (pltemp->container && pltemp->container->env != op)
		{
			container_unlink(pltemp, NULL);
		}
	}

	update_object(op, UP_OBJ_REMOVE);

	op->env = NULL;
}

/**
 * Recursively delete and remove the inventory of an object.
 * @param op  */
static void remove_ob_inv(object *op)
{
	object *tmp, *tmp2;

	for (tmp = op->inv; tmp; tmp = tmp2)
	{
		/* save ptr, gets NULL in remove_ob */
		tmp2 = tmp->below;

		if (tmp->inv)
		{
			remove_ob_inv(tmp);
		}

		/* no map, no check off */
		remove_ob(tmp);
	}
}

/**
 * This function inserts the object in the two-way linked list
 * which represents what is on a map.
 * The second argument specifies the map, and the x and y variables
 * in the object about to be inserted specifies the position.
 *
 * originator: Player, monster or other object that caused 'op' to be inserted
 * into 'map'.  May be NULL.
 *
 * flag is a bitmask about special things to do (or not do) when this
 * function is called.  see the object.h file for the INS_ values.
 * Passing 0 for flag gives proper default values, so flag really only needs
 * to be set if special handling is needed.
 *
 * Return value:
 *   NULL if 'op' was destroyed
 *   just 'op' otherwise
 *   When a trap (like a trapdoor) has moved us here, op will returned true.
 *   The caller function must handle it and controlling ->map, ->x and ->y of op
 *
 * I reworked the FLY/MOVE_ON system - it should now very solid and faster. MT-2004.
 * Notice that the FLY/WALK_OFF stuff is removed from remove_ob() and must be called
 * explicit when we want make a "move/step" for a object which can trigger it.
 * @param op
 * @param m
 * @param originator
 * @param flag
 * @return  */
object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag)
{
	object *tmp = NULL, *top;
	MapSpace *mc;
	int x, y, lt, layer, layer_inv;

	/* some tests to check all is ok... some cpu ticks
	 * which tracks we have problems or not */
	if (OBJECT_FREE(op))
	{
		dump_object(op);
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert freed object %s in map %s!\n:%s\n", query_name(op, NULL), m->name, errmsg);
		return NULL;
	}

	if (m == NULL)
	{
		dump_object(op);
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert object %s in null-map!\n%s\n", query_name(op, NULL), errmsg);
		return NULL;
	}

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		dump_object(op);
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert non removed object %s in map %s.\n%s\n", query_name(op, NULL), m->name, errmsg);
		return NULL;
	}

	/* tail, but no INS_TAIL_MARKER: we had messed something outside! */
	if (op->head && !(flag & INS_TAIL_MARKER))
	{
		LOG(llevBug, "BUG: insert_ob_in_map(): inserting op->more WITHOUT INS_TAIL_MARKER! OB:%s (ARCH: %s) (MAP: %s (%d,%d))\n", query_name(op, NULL), op->arch->name, m->path, op->x, op->y);
		return NULL;
	}

	if (op->more)
	{
		if (insert_ob_in_map(op->more, op->more->map, originator, flag | INS_TAIL_MARKER) == NULL)
		{
			if (!op->head)
				LOG(llevBug, "BUG: insert_ob_in_map(): inserting op->more killed op %s in map %s\n", query_name(op, NULL), m->name);

			return NULL;
		}
	}

	CLEAR_FLAG(op, FLAG_REMOVED);

#ifdef POSITION_DEBUG
	op->ox = op->x;
	op->oy = op->y;
#endif

	/* this is now a key part of this function, because
	 * we adjust multi arches here when they cross map boarders! */
	x = op->x;
	y = op->y;
	op->map = m;

	if (!(m = out_of_map(m, &x, &y)))
	{
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert object %s outside the map %s (%d,%d).\n\n", query_name(op, NULL), op->map->path, op->x, op->y);
		return NULL;
	}

	/* x and y will only change when we change the map too - so check the map */
	if (op->map != m)
	{
		op->map = m;
		op->x = x;
		op->y = y;
	}

	/* hm, i not checked this, but is it not smarter to remove op instead and return? MT */
	if (op->nrof && !(flag & INS_NO_MERGE))
	{
		for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		{
			if (CAN_MERGE(op,tmp))
			{
				op->nrof+=tmp->nrof;
				/* a bit tricky remove_ob() without check off.
				 * technically, this happens: arrow x/y is falling on the stack
				 * of perhaps 10 arrows. IF a teleporter is called, the whole 10
				 * arrows are teleported.Thats a right effect. */
				remove_ob(tmp);
			}
		}
	}

	/* we need this for FLY/MOVE_ON/OFF */
	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	/* nothing on the floor can be applied */
	CLEAR_FLAG(op, FLAG_APPLIED);
	/* or locked */
	CLEAR_FLAG(op, FLAG_INV_LOCKED);

	/* map layer system */
	/* we don't test for sys object because we ALWAYS set the layer of a sys object
	 * to 0 when we load a sys_object (0 is default, but server loader will warn when
	 * we set a layer != 0). We will do the check in the arch load and
	 * in the map editor, so we don't need to mess with it anywhere at runtime.
	 * Note: even the inserting process is more complicate and more code as the crossfire
	 * on, we should speed up things alot - with more object more cpu time we will safe.
	 * Also, see that we don't need to access in the inserting or sorting the old objects.
	 * no FLAG_xx check or something - all can be done by the cpu in cache. */
	/* for fast access - we will not change the node here */
	mc = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

	/* so we have a non system object */
	if (op->layer)
	{
		layer = op->layer - 1;
		layer_inv = layer + 7;

		/* not invisible? */
		if (!QUERY_FLAG(op, FLAG_IS_INVISIBLE))
		{
			/* have we another object of this layer? */
			if ((top = GET_MAP_SPACE_LAYER(mc, layer)) == NULL && (top = GET_MAP_SPACE_LAYER(mc, layer_inv)) == NULL)
			{
				/* no, we are the first of this layer - lets search something above us we can chain with.*/
				for (lt = op->layer; lt < MAX_ARCH_LAYERS && (top = GET_MAP_SPACE_LAYER(mc, lt)) == NULL && (top = GET_MAP_SPACE_LAYER(mc, lt + 7)) == NULL; lt++);
			}

			/* now, if top != NULL, thats the object BEFORE we want chain. This can be
			 * the base layer, the inv base layer or a object from a upper layer.
			 * If it NULL, we are the only object in this tile OR
			 * ->last holds the object BEFORE ours. */
			/* we always go in front */
			SET_MAP_SPACE_LAYER(mc, layer, op);
			/* easy - we chain our object before this one */
			if (top)
			{
				if (top->below)
					top->below->above = op;
				/* if no object before, we are new starting object */
				else
					SET_MAP_SPACE_FIRST(mc, op);

				op->below = top->below;
				top->below = op;
				op->above = top;
			}
			/* we are first object here or one is before us  - chain to it */
			else
			{
				if ((top = GET_MAP_SPACE_LAST(mc)) != NULL)
				{
					top->above = op;
					op->below = top;

				}
				/* a virgin! we set first and last object */
				else
					SET_MAP_SPACE_FIRST(mc, op);

				SET_MAP_SPACE_LAST(mc, op);
			}
		}
		/* invisible object */
		else
		{
			int tmp_flag;

			/* check the layers */
			if ((top = GET_MAP_SPACE_LAYER(mc, layer_inv)) != NULL)
				tmp_flag = 1;
			else if ((top = GET_MAP_SPACE_LAYER(mc, layer)) != NULL)
			{
				/* in this case, we have 1 or more normal objects in this layer,
				 * so we must skip all of them. easiest way is to get a upper layer
				 * valid object. */
				for (lt = op->layer; lt < MAX_ARCH_LAYERS && (tmp = GET_MAP_SPACE_LAYER(mc, lt)) == NULL && (tmp = GET_MAP_SPACE_LAYER(mc, lt + 7)) == NULL; lt++);
				tmp_flag = 2;
			}
			else
			{
				/* no, we are the first of this layer - lets search something above us we can chain with.*/
				for (lt = op->layer; lt < MAX_ARCH_LAYERS && (top = GET_MAP_SPACE_LAYER(mc, lt)) == NULL && (top = GET_MAP_SPACE_LAYER(mc, lt + 7)) == NULL; lt++);
				tmp_flag = 3;
			}

			/* in all cases, we are the new inv base layer */
			SET_MAP_SPACE_LAYER(mc, layer_inv, op);
			/* easy - we chain our object before this one - well perhaps */
			if (top)
			{
				/* ok, now the tricky part.
				 * if top is set, this can be...
				 * - the inv layer of same layer id (and tmp_flag will be 1)
				 * - the normal layer of same layer id (tmp_flag == 2)
				 * - the inv OR normal layer of a upper layer (tmp_flag == 3)
				 * if tmp_flag = 1, its easy - just we get in front of top and use
				 * the same links.
				 * if tmp_flag = 2 AND tmp is set, tmp is the object we chain before.
				 * is tmp is NULL, we get ->last and chain after it.
				 * if tmp_flag = 3, we chain all times to top (before). */
				if (tmp_flag == 2)
				{
					if (tmp)
					{
						/* we can't be first, there is always one before us */
						tmp->below->above = op;
						op->below = tmp->below;
						/* and one after us... */
						tmp->below = op;
						op->above = tmp;
					}
					else
					{
						/* there is one before us, so this is always valid */
						tmp = GET_MAP_SPACE_LAST(mc);
						/* new last object */
						SET_MAP_SPACE_LAST(mc, op);
						op->below = tmp;
						tmp->above = op;
					}
				}
				/* tmp_flag 1 & tmp_flag 3 are done the same way */
				else
				{
					if (top->below)
						top->below->above = op;
					/* if no object before ,we are new starting object */
					else
						SET_MAP_SPACE_FIRST(mc, op);

					op->below = top->below;
					top->below = op;
					op->above = top;
				}
			}
			/* we are first object here or one is before us  - chain to it */
			else
			{
				/* there is something down we don't care what */
				if ((top = GET_MAP_SPACE_LAST(mc)) != NULL)
				{
					/* just chain to it */
					top->above = op;
					op->below = top;

				}
				/* a virgin! we set first and last object */
				else
					SET_MAP_SPACE_FIRST(mc, op);

				SET_MAP_SPACE_LAST(mc, op);
			}
		}
	}
	/* op->layer == 0 - lets just put this object in front of all others */
	else
	{
		/* is there some else? */
		if ((top = GET_MAP_SPACE_FIRST(mc)) != NULL)
		{
			/* easy chaining */
			top->below = op;
			op->above = top;
		}
		/* no ,we are last object too */
		else
			SET_MAP_SPACE_LAST(mc, op);

		SET_MAP_SPACE_FIRST(mc, op);
	}

	/* lets set some specials for our players
	 * we adjust the ->player map variable and the local
	 * map player chain. */
	if (op->type == PLAYER)
	{
		CONTR(op)->socket.update_tile = 0;
		/* thats always true when touching the players map pos. */
		CONTR(op)->update_los = 1;

		if (op->map->player_first)
		{
			CONTR(op->map->player_first)->map_below = op;
			CONTR(op)->map_above = op->map->player_first;
		}
		op->map->player_first = op;

	}

	/* we updated something here - mark this tile as changed! */
	mc->update_tile++;
	/* updates flags (blocked, alive, no magic, etc) for this map space */
	update_object(op, UP_OBJ_INSERT);

	/* check walk on/fly on flag if not canceld AND there is some to move on.
	 * Note: We are first inserting the WHOLE object/multi arch - then we check all
	 * part for traps. This ensures we don't must do nasty hacks with half inserted/removed
	 * objects - for example when we hit a teleporter trap.
	 * Check only for single tiles || or head but ALWAYS for heads. */
	if (!(flag & INS_NO_WALK_ON) && (mc->flags & (P_WALK_ON | P_FLY_ON) || op->more) && !op->head)
	{
		int event;

		/* we want reuse mc here... bad enough we need to check it double for multi arch */
		if (QUERY_FLAG(op, FLAG_FLY_ON))
		{
			/* we are flying but no fly event here */
			if (!(mc->flags & P_FLY_ON))
				goto check_walk_loop;
		}
		/* we are not flying - check walking only */
		else
		{
			if (!(mc->flags & P_WALK_ON))
				goto check_walk_loop;
		}

		if ((event = check_walk_on(op, originator, MOVE_APPLY_MOVE)))
		{
			/* don't return NULL - we are valid but we was moved */
			if (event == CHECK_WALK_MOVED)
				return op;
			/* CHECK_WALK_DESTROYED */
			else
				return NULL;
		}

		/* TODO: check event */
check_walk_loop:
		for (tmp = op->more; tmp != NULL; tmp = tmp->more)
		{
			mc = GET_MAP_SPACE_PTR(tmp->map, tmp->x, tmp->y);

			/* object is flying/levitating */
			/* trick: op is single tile OR always head! */
			if (QUERY_FLAG(op, FLAG_FLY_ON))
			{
				/* we are flying but no fly event here */
				if (!(mc->flags & P_FLY_ON))
					continue;
			}
			/* we are not flying - check walking only */
			else
			{
				if (!(mc->flags & P_WALK_ON))
					continue;
			}

			if ((event = check_walk_on(tmp, originator, MOVE_APPLY_MOVE)))
			{
				/* don't return NULL - we are valid but we was moved */
				if (event == CHECK_WALK_MOVED)
					return op;
				/* CHECK_WALK_DESTROYED */
				else
					return NULL;
			}
		}
	}

	return op;
}

/**
 * This function inserts an object in the map, but if it finds an object
 * of its own type, it'll remove that one first. op is the object to
 * insert it under:  supplies x and the map.
 * @param arch_string
 * @param op  */
void replace_insert_ob_in_map(char *arch_string, object *op)
{
	object *tmp;
	object *tmp1;

	/* first search for itself and remove any old instances */
	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		if (!strcmp(tmp->arch->name, arch_string))
		{
			/* no move off here... should be ok, this is a technical function */
			remove_ob(tmp);
			tmp->speed = 0;
			/* Remove it from active list */
			update_ob_speed(tmp);
		}
	}

	tmp1 = arch_to_object(find_archetype(arch_string));

	tmp1->x = op->x;
	tmp1->y = op->y;
	insert_ob_in_map(tmp1, op->map, op, 0);
}

/**
 * Splits up ob into two parts. The part which is returned contains nr
 * objects, and the remaining parts contains the rest (or is removed
 * and freed if that number is 0). On failure, NULL is returned, and the
 * reason put into the global static errmsg array.
 * @param orig_ob
 * @param nr
 * @return  */
object *get_split_ob(object *orig_ob, int nr)
{
	object *newob;
	object *tmp, *event;
	int is_removed = (QUERY_FLAG(orig_ob, FLAG_REMOVED) != 0);

	if ((int) orig_ob->nrof < nr)
	{
		sprintf(errmsg, "There are only %d %ss.", orig_ob->nrof ? orig_ob->nrof : 1, query_name(orig_ob, NULL));
		return NULL;
	}

	newob = get_object();
	copy_object(orig_ob, newob);

	/* Gecko: copy inventory (event objects) */
	for (tmp = orig_ob->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == TYPE_EVENT_OBJECT)
		{
			event = get_object();
			copy_object(tmp, event);
			insert_ob_in_ob(event, newob);
		}
	}

	/*    if(QUERY_FLAG(orig_ob, FLAG_UNPAID) && QUERY_FLAG(orig_ob, FLAG_NO_PICK))*/
	/*		;*/ /* clone objects .... */
	/*	else*/
	orig_ob->nrof -= nr;

	if (orig_ob->nrof < 1)
	{
		if (!is_removed)
			remove_ob(orig_ob);

		check_walk_off(orig_ob, NULL, MOVE_APPLY_VANISHED);
	}
	else if (!is_removed)
	{
		if (orig_ob->env != NULL)
			sub_weight(orig_ob->env, orig_ob->weight * nr);

		if (orig_ob->env == NULL && orig_ob->map->in_memory != MAP_IN_MEMORY)
		{
			strcpy(errmsg, "Tried to split object whose map is not in memory.");
			LOG(llevDebug, "Error, Tried to split object whose map is not in memory.\n");
			return NULL;
		}
	}

	newob->nrof = nr;
	return newob;
}

/**
 * Decreases a specified number from the amount of an object. If the
 * amount reaches 0, the object is subsequently removed and freed.
 *
 * Return value: 'op' if something is left, NULL if the amount reached 0
 * @param op
 * @param i
 * @return  */
object *decrease_ob_nr(object *op, int i)
{
	object *tmp;
	player *pl;

	/* objects with op->nrof require this check */
	if (i == 0)
		return op;

	if (i > (int)op->nrof)
		i = (int)op->nrof;

	if (QUERY_FLAG(op, FLAG_REMOVED))
		op->nrof -= i;
	else if (op->env != NULL)
	{
		/* is this object in the players inventory, or sub container
		 * therein? */
		tmp = is_player_inv(op->env);
		/* nope.  Is this a container the player has opened?
		 * If so, set tmp to that player.
		 * IMO, searching through all the players will mostly
		 * likely be quicker than following op->env to the map,
		 * and then searching the map for a player. */

		/* TODO: this is another nasty "search all players"...
		 * i skip this to rework as i do the container patch -
		 * but we can do this now much smarter !
		 * MT -08.02.04  */
		if (!tmp)
		{
			for (pl = first_player; pl; pl = pl->next)
				if (pl->container == op->env)
					break;

			if (pl)
				tmp = pl->ob;
			else
				tmp = NULL;
		}

		if (i < (int)op->nrof)
		{
			sub_weight (op->env, op->weight * i);
			op->nrof -= i;

			if (tmp)
			{
				esrv_send_item(tmp, op);
				esrv_update_item(UPD_WEIGHT, tmp, tmp);
			}
		}
		else
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			op->nrof = 0;

			if (tmp)
			{
				esrv_del_item(CONTR(tmp), op->count,op->env);
				esrv_update_item(UPD_WEIGHT, tmp, tmp);
			}
		}
	}
	else
	{
		object *above = op->above;

		if (i < (int)op->nrof)
			op->nrof -= i;
		else
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			op->nrof = 0;
		}

		/* Since we just removed op, op->above is null */
		for (tmp = above; tmp != NULL; tmp = tmp->above)
		{
			if (tmp->type == PLAYER)
			{
				if (op->nrof)
					esrv_send_item(tmp, op);
				else
					esrv_del_item(CONTR(tmp), op->count, op->env);
			}
		}
	}

	if (op->nrof)
		return op;
	else
		return NULL;
}

/**
 * This function inserts the object op in the linked list inside the
 * object environment.
 *
 * Eneq(@csd.uu.se): Altered insert_ob_in_ob to make things picked up enter
 * the inventory at the last position or next to other objects of the same
 * type.
 *
 * Frank: Now sorted by type, archetype and magic!
 *
 * The function returns now pointer to inserted item, and return value can
 * be != op, if items are merged. -Tero
 * @param op
 * @param where
 * @return  */
object *insert_ob_in_ob(object *op, object *where)
{
	object *tmp, *otmp;

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		dump_object(op);
		LOG(llevBug, "BUG: Trying to insert (ob) inserted object.\n%s\n", errmsg);
		return op;
	}

	if (where == NULL)
	{
		dump_object(op);
		LOG(llevBug, "BUG: Trying to put object in NULL.\n%s\n", errmsg);
		return op;
	}

	if (where->head)
	{
		LOG(llevBug, "BUG: Tried to insert object wrong part of multipart object.\n");
		where = where->head;
	}

	if (op->more)
	{
		LOG(llevError, "ERROR: Tried to insert multipart object %s (%d)\n", query_name(op, NULL), op->count);
		return op;
	}

	CLEAR_FLAG(op, FLAG_REMOVED);

	if (op->nrof)
	{
		for (tmp = where->inv; tmp != NULL; tmp = tmp->below)
		{
			if (CAN_MERGE(tmp, op))
			{
				/* return the original object and remove inserted object
				 * (client needs the original object) */
				tmp->nrof += op->nrof;

				/* Weight handling gets pretty funky.  Since we are adding to
				 * tmp->nrof, we need to increase the weight. */
				add_weight(where, op->weight * op->nrof);

				/* Make sure we get rid of the old object */
				SET_FLAG(op, FLAG_REMOVED);

				op = tmp;
				/* and fix old object's links (we will insert it further down)*/
				remove_ob(op);
				/* Just kidding about previous remove */
				CLEAR_FLAG(op, FLAG_REMOVED);
				break;
			}
		}

		/* I assume stackable objects have no inventory
		 * We add the weight - this object could have just been removed
		 * (if it was possible to merge).  calling remove_ob will subtract
		 * the weight, so we need to add it in again, since we actually do
		 * the linking below */
		add_weight(where, op->weight * op->nrof);
	}
	else
		add_weight(where, (op->weight + op->carrying));

	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	op->map = NULL;
	op->env = where;
	op->above = NULL;
	op->below = NULL;
	op->x = 0, op->y = 0;

#ifdef POSITION_DEBUG
	op->ox = 0, op->oy = 0;
#endif

	/* Client has no idea of ordering so lets not bother ordering it here.
	  * It sure simplifies this function... */
	if (where->inv==NULL)
		where->inv = op;
	else
	{
		op->below = where->inv;
		op->below->above = op;
		where->inv = op;
	}

	/* check for event object and set the owner object
	 * event flags. */
	if (op->type == TYPE_EVENT_OBJECT && op->sub_type1)
		where->event_flags |= (1U << (op->sub_type1 - 1));

	/* if player, adjust one drop items and fix player if not
	 * marked as no fix. */
	otmp = is_player_inv(where);
	if (otmp && CONTR(otmp) != NULL)
	{
		if (QUERY_FLAG(op, FLAG_ONE_DROP))
			SET_FLAG(op, FLAG_STARTEQUIP);

		if (!QUERY_FLAG(otmp, FLAG_NO_FIX_PLAYER))
			fix_player(otmp);
	}

	return op;
}

/**
 * Checks if any objects which has the WALK_ON() (or FLY_ON() if the
 * object is flying) flag set, will be auto-applied by the insertion
 * of the object into the map (applying is instantly done).
 * Any speed-modification due to SLOW_MOVE() of other present objects
 * will affect the speed_left of the object.
 *
 * originator: Player, monster or other object that caused 'op' to be inserted
 * into 'map'.  May be NULL.
 *
 * Return value: 1 if 'op' was destroyed, 0 otherwise.
 *
 * 4-21-95 added code to check if appropriate skill was readied - this will
 * permit faster movement by the player through this terrain. -b.t.
 *
 * MSW 2001-07-08: Check all objects on space, not just those below
 * object being inserted.  insert_ob_in_map may not put new objects
 * on top.
 * @param op
 * @param originator
 * @param flags
 * @return  */
int check_walk_on(object *op, object *originator, int flags)
{
	object *tmp;
	/* when TRUE, this function is root call for static_walk_semaphore setting */
	int local_walk_semaphore = FALSE;
	tag_t tag;
	int fly;

	if (QUERY_FLAG(op, FLAG_NO_APPLY))
		return 0;

	fly = QUERY_FLAG(op, FLAG_FLYING);

	if (fly)
		flags |= MOVE_APPLY_FLY_ON;
	else
		flags |= MOVE_APPLY_WALK_ON;

	tag = op->count;

	/* This flags ensures we notice when a moving event has appeared!
	 * Because the functions who set/clear the flag can be called recursive
	 * from this function and walk_off() we need a static, global semaphor
	 * like flag to ensure we don't clear the flag except in the mother call. */
	if (!static_walk_semaphore)
	{
		local_walk_semaphore = TRUE;
		static_walk_semaphore = TRUE;
		CLEAR_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	}

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		/* Can't apply yourself */
		if (tmp == op)
			continue;

		if (fly ? QUERY_FLAG(tmp, FLAG_FLY_ON) : QUERY_FLAG(tmp, FLAG_WALK_ON))
		{
			/* apply_func must handle multi arch parts....
			 * NOTE: move_apply() can be heavy recursive and recall
			 * this function too.*/
			move_apply(tmp, op, originator,flags);

			/* this means we got killed, removed or whatever! */
			if (was_destroyed(op, tag))
			{
				if (local_walk_semaphore)
					static_walk_semaphore = FALSE;

				return CHECK_WALK_DESTROYED;
			}

			/* and here a remove_ob() or insert_xx() was triggered - we MUST stop now */
			if (QUERY_FLAG(op, FLAG_OBJECT_WAS_MOVED))
			{
				if (local_walk_semaphore)
					static_walk_semaphore = FALSE;

				return CHECK_WALK_MOVED;
			}
		}
	}

	if (local_walk_semaphore)
		static_walk_semaphore = FALSE;

	return CHECK_WALK_OK;
}

/**
 * Different to check_walk_on() this must be called explicit and its
 * handles muti arches at once.
 * There are some flags notifiying move_apply() about the kind of event
 * we have.
 * @param op
 * @param originator
 * @param flags
 * @return  */
int check_walk_off(object *op, object *originator, int flags)
{
	MapSpace *mc;
	object *tmp, *part;
	/* when TRUE, this function is root call for static_walk_semaphore setting */
	int local_walk_semaphore = FALSE;
	int fly;
	tag_t tag;

	/* no map, no walk off - item can be in inventory and/or ... */
	if (!op || !op->map)
		return CHECK_WALK_OK;

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		LOG(llevBug, "BUG: check_walk_off: object %s is not removed when called\n", query_name(op, NULL));
		return CHECK_WALK_OK;
	}

	if (QUERY_FLAG(op, FLAG_NO_APPLY))
		return CHECK_WALK_OK;

	tag = op->count;
	fly = QUERY_FLAG(op, FLAG_FLYING);

	if (fly)
		flags |= MOVE_APPLY_FLY_OFF;
	else
		flags |= MOVE_APPLY_WALK_OFF;

	/* check single and multi arches */
	for (part = op; part; part = part->more)
	{
		mc = GET_MAP_SPACE_PTR(part->map, part->x, part->y);

		/* no event on this tile */
		if (!(mc->flags & (P_WALK_OFF | P_FLY_OFF)))
			continue;

		/* This flags ensures we notice when a moving event has appeared!
		 * Because the functions who set/clear the flag can be called recursive
		 * from this function and walk_off() we need a static, global semaphor
		 * like flag to ensure we don't clear the flag except in the mother call. */
		if (!static_walk_semaphore)
		{
			local_walk_semaphore = TRUE;
			static_walk_semaphore = TRUE;
			CLEAR_FLAG(op, FLAG_OBJECT_WAS_MOVED);
		}

		/* ok, check objects here... */
		for (tmp = mc->first; tmp != NULL; tmp = tmp->above)
		{
			/* its the ob part in this space... better not >1 part in same space of same arch */
			if (tmp == part)
				continue;

			/* event */
			if (fly ? QUERY_FLAG(tmp, FLAG_FLY_OFF) : QUERY_FLAG(tmp, FLAG_WALK_OFF))
			{
				move_apply(tmp, part, originator, flags);

				if (OBJECT_FREE(part) || tag != op->count)
				{
					if (local_walk_semaphore)
						static_walk_semaphore = FALSE;

					return CHECK_WALK_DESTROYED;
				}

				/* and here a insert_xx() was triggered - we MUST stop now */
				if (!QUERY_FLAG(part, FLAG_REMOVED) || QUERY_FLAG(part, FLAG_OBJECT_WAS_MOVED))
				{
					if (local_walk_semaphore)
						static_walk_semaphore = FALSE;

					return CHECK_WALK_MOVED;
				}
			}
		}

		if (local_walk_semaphore)
		{
			local_walk_semaphore = FALSE;
			static_walk_semaphore = FALSE;
		}

	}

	if (local_walk_semaphore)
		static_walk_semaphore = FALSE;

	return CHECK_WALK_OK;
}

/**
 * Searches for any objects with a matching archetype at the given map
 * and coordinates. The first matching object is returned, or NULL if none.
 * @param at
 * @param m
 * @param x
 * @param y
 * @return  */
object *present_arch(archetype *at, mapstruct *m, int x, int y)
{
	object *tmp;

	if (!(m = out_of_map(m, &x, &y)))
	{
		LOG(llevError, "ERROR: Present_arch called outside map.\n");
		return NULL;
	}

	for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		if (tmp->arch == at)
			return tmp;

	return NULL;
}

/**
 * Searches for any objects with a matching type variable at the given
 * map and coordinates. The first matching object is returned, or NULL if
 * none.
 * @param type
 * @param m
 * @param x
 * @param y
 * @return  */
object *present(unsigned char type, mapstruct *m, int x, int y)
{
	object *tmp;

	if (!(m = out_of_map(m, &x, &y)))
	{
		LOG(llevError, "ERROR: Present called outside map.\n");
		return NULL;
	}

	for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		if (tmp->type == type)
			return tmp;

	return NULL;
}

/**
 * Searches for any objects with a matching type variable in the
 * inventory of the given object.
 * The first matching object is returned, or NULL if none.
 * @param type
 * @param op
 * @return  */
object *present_in_ob(unsigned char type, object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		if (tmp->type == type)
			return tmp;

	return NULL;
}

/**
 * Searches for any objects with a matching archetype in the inventory of
 * the given object.
 * The first matching object is returned, or NULL if none.
 * @param at
 * @param op
 * @return  */
object *present_arch_in_ob(archetype *at, object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		if (tmp->arch == at)
			return tmp;

	return NULL;
}

/**
 * Sets the cheat flag (WAS_WIZ) in the object and in all it's inventory
 * (recursively).
 *
 * If checksums are used, a player will get set_cheat called for
 * him/her-self and all object carried by a call to this function.
 * @param op  */
void set_cheat(object *op)
{
	object *tmp;

	SET_FLAG(op, FLAG_WAS_WIZ);

	if (op->inv)
	{
		for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		{
			set_cheat(tmp);
		}
	}
}

/* find_free_spot(archetype, map, x, y, start, stop) */
/**
 * Will search for a spot at the given map and coordinates which will be
 * able to contain the given archetype. start and stop specifies how many
 * squares to search (see the freearr_x/y[] definition).
 *
 * It returns a random choice among the alternatives found.
 * start and stop are where to start relative to the free_arr array (1,9
 * does all 4 immediate directions).  This returns the index into the
 * array of the free spot, -1 if no spot available (dir 0 = x,y)
 *
 * @note this only checks to see if there is space for the head of the
 * object - if it is a multispace object, this should be called for all
 * pieces.
 * @param at
 * @param op
 * @param m
 * @param x
 * @param y
 * @param start
 * @param stop
 * @return  */
int find_free_spot(archetype *at, object *op, mapstruct *m, int x, int y, int start, int stop)
{
	int i, index = 0;
	static int altern[SIZEOFFREE];

	for (i = start; i < stop; i++)
	{
		if (!arch_blocked(at, op, m, x + freearr_x[i], y + freearr_y[i]))
		{
			altern[index++] = i;
		}
		else if (wall(m, x + freearr_x[i], y + freearr_y[i]) && maxfree[i] < stop)
		{
			stop = maxfree[i];
		}
	}

	if (!index)
	{
		return -1;
	}

	return altern[RANDOM() % index];
}


/**
 * works like find_free_spot(), but it will search max number of squares.
 * But it will return the first available spot, not a random choice.
 * Changed 0.93.2: Have it return -1 if there is no free spot available.
 * @param at
 * @param m
 * @param x
 * @param y
 * @return  */
int find_first_free_spot(archetype *at, mapstruct *m, int x, int y)
{
	int i;

	for (i = 0; i < SIZEOFFREE; i++)
	{
		if (!arch_blocked(at, NULL, m, x + freearr_x[i], y + freearr_y[i]))
		{
			return i;
		}
	}

	return -1;
}

int find_first_free_spot2(archetype *at, mapstruct *m, int x, int y, int start, int range)
{
	int i;
	for (i = start; i < range; i++)
	{
		if (!arch_blocked(at, NULL, m, x + freearr_x[i], y + freearr_y[i]))
			return i;
	}

	return -1;
}

/**
 * Will search some close squares in the given map at the given
 * coordinates for live objects.
 *
 * It will not considered the object given as exlude among possible
 * live objects.
 *
 * It returns the direction toward the first/closest live object if finds
 * any, otherwise 0.
 * @param m
 * @param x
 * @param y
 * @param exclude
 * @return  */
int find_dir(mapstruct *m, int x, int y, object *exclude)
{
	int i, xt, yt, max = SIZEOFFREE;
	mapstruct *mt;
	object *tmp;

	if (exclude && exclude->head)
	{
		exclude = exclude->head;
	}

	for (i = 1; i < max; i++)
	{
		xt = x + freearr_x[i];
		yt = y + freearr_y[i];

		if (wall(m, xt, yt))
		{
			max = maxfree[i];
		}
		else
		{
			if (!(mt = out_of_map(m, &xt, &yt)))
			{
				continue;
			}

			tmp = GET_MAP_OB(mt, xt, yt);

			while (tmp != NULL && ((tmp != NULL && !QUERY_FLAG(tmp, FLAG_MONSTER) && tmp->type != PLAYER) || (tmp == exclude || (tmp->head && tmp->head == exclude))))
			{
				tmp = tmp->above;
			}

			if (tmp != NULL)
			{
				return freedir[i];
			}
		}
	}

	return 0;
}

/**
 * Will return a direction in which an object which has subtracted the x
 * and y coordinates of another object, needs to travel toward it.
 * @param x
 * @param y
 * @return  */
int find_dir_2(int x, int y)
{
	int q;

	if (!y)
	{
		q = -300 * x;
	}
	else
	{
		q = x * 100 / y;
	}

	if (y > 0)
	{
		if (q < -242)
		{
			return 3;
		}

		if (q < -41)
		{
			return 2;
		}

		if (q < 41)
		{
			return 1;
		}

		if (q < 242)
		{
			return 8;
		}

		return 7;
	}

	if (q < -242)
	{
		return 7;
	}

	if (q < -41)
	{
		return 6;
	}

	if (q < 41)
	{
		return 5;
	}

	if (q < 242)
	{
		return 4;
	}

	return 3;
}

/**
 * Returns a number between 1 and 8, which represent the "absolute"
 * direction of a number (it actually takes care of "overflow" in
 * previous calculations of a direction).
 * @param d
 * @return  */
int absdir(int d)
{
	while (d < 1)
	{
		d += 8;
	}

	while (d > 8)
	{
		d -= 8;
	}

	return d;
}


/**
 * Returns how many 45-degrees differences there is
 * between two directions (which are expected to be absolute (see absdir() ))
 * @param dir1
 * @param dir2
 * @return  */
int dirdiff(int dir1, int dir2)
{
	int d;
	d = abs(dir1 - dir2);

	if (d > 4)
	{
		d = 8 - d;
	}

	return d;
}

/**
 * Get direction from one object to another.
 *
 * If the first object is a player, this will set the player's facing
 * direction to the returned direction.
 * @param op The first object
 * @param target The target object
 * @param range_vector Range vector pointer to use
 * @return The direction */
int get_dir_to_target(object *op, object *target, rv_vector *range_vector)
{
	int dir;

	get_rangevector(op, target, range_vector, 0);
	dir = range_vector->direction;

	if (op->type == PLAYER)
	{
		if (op->head)
		{
			op->head->anim_enemy_dir = dir;
			op->head->facing = dir;
		}
		else
		{
			op->anim_enemy_dir = dir;
			op->facing = dir;
		}
	}

	return dir;
}


/**
 * Finds out if an object is possible to be picked up by the picker.
 * Returnes 1 if it can be picked up, otherwise 0.
 *
 * Cf 0.91.3 - don't let WIZ's pick up anything - will likely cause
 * core dumps if they do.
 *
 * Add a check so we can't pick up invisible objects (0.93.8)
 * @param who
 * @param item
 * @return
 */
int can_pick(object *who, object *item)
{
	return ((who->type == PLAYER && QUERY_FLAG(item, FLAG_NO_PICK) && QUERY_FLAG(item, FLAG_UNPAID)) || (item->weight > 0 && !QUERY_FLAG(item, FLAG_NO_PICK) && (!IS_INVISIBLE(item, who) || QUERY_FLAG(who, FLAG_SEE_INVISIBLE)) && (who->type == PLAYER || item->weight < who->weight / 3)));
}

/**
 * create clone from object to another
 * @param asrc
 * @return  */
object *object_create_clone(object *asrc)
{
	object *dst = NULL, *tmp, *src, *part, *prev, *item;

	if (!asrc)
		return NULL;

	src = asrc;
	if (src->head)
		src = src->head;

	prev = NULL;
	for (part = src; part; part = part->more)
	{
		tmp = get_object();
		copy_object(part, tmp);
		tmp->x -= src->x;
		tmp->y -= src->y;

		if (!part->head)
		{
			dst = tmp;
			tmp->head = NULL;
		}
		else
			tmp->head = dst;

		tmp->more = NULL;

		if (prev)
			prev->more = tmp;

		prev = tmp;
	}

	/* copy inventory */
	for (item = src->inv; item; item = item->below)
	{
		insert_ob_in_ob(object_create_clone(item), dst);
	}

	return dst;
}

int was_destroyed(object *op, tag_t old_tag)
{
	/* checking for OBJECT_FREE isn't necessary, but makes this function more
	 * robust */
	/* Gecko: redefined "destroyed" a little broader: included removed objects.
	 * -> need to make sure this is never a problem with temporarily removed objects */
	return (QUERY_FLAG(op, FLAG_REMOVED) || (op->count != old_tag) || OBJECT_FREE(op));
}

/**
 * Creates an object using a string representing its content.
 *
 * Basically, we save the content of the string to a temp file, then call
 * load_object on it. I admit it is a highly inefficient way to make things,
 * but it was simple to make and allows reusing the load_object function.
 * Remember not to use load_object_str in a time-critical situation.
 * Also remember that multiparts objects are not supported for now.
 * @param obstr
 * @return  */
object* load_object_str(char *obstr)
{
	object *op;
	FILE *tempfile;
	void *mybuffer;
	char filename[MAX_BUF];

	sprintf(filename, "%s/cfloadobstr2044", settings.tmpdir);
	tempfile = fopen(filename, "w+");
	if (tempfile == NULL)
	{
		LOG(llevError, "ERROR: load_object_str(): Unable to access load object temp file\n");
		return NULL;
	}

	fprintf(tempfile, "%s", obstr);
	op = get_object();
	rewind(tempfile);
	mybuffer = create_loader_buffer(tempfile);
	load_object(tempfile, op, mybuffer, LO_REPEAT, 0);
	delete_loader_buffer(mybuffer);
	LOG(llevDebug, "load str completed, object=%s\n", query_name(op, NULL));
	fclose(tempfile);
	return op;
}

/**
 * Process object with FLAG_AUTO_APPLY.
 *
 * Basically creates treasure for objects like
 * @ref SHOP_FLOOR "shop floors" and @ref TREASURE "treasures".
 * @param op The object to process.
 * @return 1 if a new object was generated, 0 otherwise. */
int auto_apply(object *op)
{
	object *tmp = NULL, *tmp2;
	int i, level, a_chance;

	/* because auto_apply will be done only *one* time
	 * when a new, base map is loaded, we always clear
	 * the flag now. */
	CLEAR_FLAG(op, FLAG_AUTO_APPLY);

	switch (op->type)
	{
		case SHOP_FLOOR:
			if (op->randomitems == NULL)
			{
				return 0;
			}

			a_chance = op->randomitems->artifact_chance;

			/* If damned shop floor, force 0 artifact chance. */
			if (QUERY_FLAG(op, FLAG_DAMNED))
			{
				a_chance = 0;
			}

			do
			{
				/* let's give it 10 tries */
				i = 10;
				level = op->stats.exp ? (int) op->stats.exp : get_enviroment_level(op);

				while ((tmp = generate_treasure(op->randomitems, level, a_chance)) == NULL && --i)
				{
				}

				if (tmp == NULL)
				{
					return 0;
				}

				if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
				{
					tmp = NULL;
				}
			}
			while (!tmp);

			tmp->x = op->x, tmp->y = op->y;
			SET_FLAG(tmp, FLAG_UNPAID);

			/* If this shop floor doesn't have FLAG_CURSED, generate
			 * shop-clone items. */
			if (!QUERY_FLAG(op, FLAG_CURSED))
			{
				SET_FLAG(tmp, FLAG_NO_PICK);
			}

			insert_ob_in_map(tmp, op->map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
			identify(tmp);

			break;

		case TREASURE:
			level = op->stats.exp ? (int) op->stats.exp : get_enviroment_level(op);
			create_treasure(op->randomitems, op, op->map ? GT_ENVIRONMENT : 0, level, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

			/* If we generated on object and put it in this object inventory,
			 * move it to the parent object as the current object is about
			 * to disappear.  An example of this item is the random_* stuff
			 * that is put inside other objects. */
			for (tmp = op->inv; tmp; tmp = tmp2)
			{
				tmp2 = tmp->below;
				remove_ob(tmp);

				if (op->env)
				{
					insert_ob_in_ob(tmp, op->env);
				}
			}

			/* no move off needed */
			remove_ob(op);
			break;
	}

	return tmp ? 1 : 0;
}

/**
 * Recursive routine to see if we can find a path to a certain point.
 * @param m Map we're on
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param dir Direction we're going to. Must be less than SIZEOFFREE.
 * @return 1 if we can see a direct way to get it */
int can_see_monsterP(mapstruct *m, int x, int y, int dir)
{
	int dx, dy;

	/* Exit condition: invalid direction */
	if (dir < 0)
	{
		return 0;
	}

	dx = x + freearr_x[dir];
	dy = y + freearr_y[dir];

	if (!(m = out_of_map(m, &dx, &dy)))
	{
		return 0;
	}

	if (wall(m, dx, dy))
	{
		return 0;
	}

	/* Yes, can see. */
	if (dir < 9)
	{
		return 1;
	}

	return can_see_monsterP(m, x, y, reduction_dir[dir][0]) | can_see_monsterP(m, x, y, reduction_dir[dir][1]) | can_see_monsterP(m, x, y, reduction_dir[dir][2]);
}

/**
 * Zero the key_values on op, decrementing the shared-string refcounts
 * and freeing the links.
 * @param op Object to clear. */
void free_key_values(object *op)
{
	key_value *i, *next = NULL;

	if (op->key_values == NULL)
	{
		return;
	}

	for (i = op->key_values; i; i = next)
	{
		/* Store next *first*. */
		next = i->next;

		if (i->key)
		{
			FREE_AND_CLEAR_HASH(i->key);
		}

		if (i->value)
		{
			FREE_AND_CLEAR_HASH(i->value);
		}

		i->next = NULL;
		free(i);
	}

	op->key_values = NULL;
}

/**
 * Search for a field by key.
 * @param ob Object to search in.
 * @param key Key to search. Must be a shared string.
 * @return The link from the list if ob has a field named key, NULL
 * otherwise. */
key_value *get_ob_key_link(const object *ob, const char *key)
{
	key_value *link;

	for (link = ob->key_values; link; link = link->next)
	{
		if (link->key == key)
		{
			return link;
		}
	}

	return NULL;
}

/**
 * Get an extra value by key.
 * @param op Object we're considering.
 * @param key Key of which to retrieve the value. Doesn't need to be a
 * shared string.
 * @return The value if found, NULL otherwise.
 * @note The returned string is shared. */
const char *get_ob_key_value(const object *op, const char *const key)
{
	key_value *link;
	const char *canonical_key = find_string(key);

	if (canonical_key == NULL)
	{
		return NULL;
	}

	/* This is copied from get_ob_key_link() above - only 4 lines, and
	 * saves the function call overhead. */
	for (link = op->key_values; link; link = link->next)
	{
		if (link->key == canonical_key)
		{
			return link->value;
		}
	}

	return NULL;
}

/**
 * Updates or sets a key value.
 * @param op Object we're considering.
 * @param canonical_key Key to set or update. Must be a shared string.
 * @param value Value to set. Doesn't need to be a shared string.
 * @param add_key If 0, will not add the key if it doesn't exist in op.
 * @return 1 if key was updated or added, 0 otherwise. */
static int set_ob_key_value_s(object *op, const char *canonical_key, const char *value, int add_key)
{
	key_value *field = NULL, *last = NULL;

	for (field = op->key_values; field; field = field->next)
	{
		if (field->key != canonical_key)
		{
			last = field;
			continue;
		}

		if (field->value)
		{
			FREE_AND_CLEAR_HASH(field->value);
		}

		if (value)
		{
			field->value = add_string(value);
		}
		else
		{
			/* Basically, if the archetype has this key set, we need to
			 * store the NULL value so when we save it, we save the empty
			 * value so that when we load, we get this value back
			 * again. */
			if (get_ob_key_link(&op->arch->clone, canonical_key))
			{
				field->value = NULL;
			}
			else
			{
				/* Delete this link */
				if (field->key)
				{
					FREE_AND_CLEAR_HASH(field->key);
				}

				if (field->value)
				{
					FREE_AND_CLEAR_HASH(field->value);
				}

				if (last)
				{
					last->next = field->next;
				}
				else
				{
					op->key_values = field->next;
				}

				free(field);
			}
		}

		return 1;
	}

	if (!add_key)
	{
		return 0;
	}

	/* There isn't any good reason to store a NULL value in the key/value
	 * list. If the archetype has this key, then we should also have it,
	 * so shouldn't be here. If user wants to store empty strings, should
	 * pass in "" */
	if (value == NULL)
	{
		return 1;
	}

	field = malloc(sizeof(key_value));

	field->key = add_refcount(canonical_key);
	field->value = add_string(value);
	/* Usual prepend-addition. */
	field->next = op->key_values;
	op->key_values = field;

	return 1;
}

/**
 * Updates the key in op to value.
 * @param op Object we're considering.
 * @param key Key to set or update. Doesn't need to be a shared string.
 * @param value Value to set. Doesn't need to be a shared string.
 * @param add_key If 0, will not add the key if it doesn't exist in op.
 * @return 1 if key was updated or added, 0 otherwise.
 * @note This function is merely a wrapper to set_ob_key_value_s() to
 * ensure the key is a shared string.*/
int set_ob_key_value(object *op, const char *key, const char *value, int add_key)
{
	const char *canonical_key = find_string(key);
	int floating_ref = 0, ret;

	if (canonical_key == NULL)
	{
		canonical_key = add_string(key);
		floating_ref = 1;
	}

	ret = set_ob_key_value_s(op, canonical_key, value, add_key);

	if (floating_ref)
	{
		FREE_ONLY_HASH(canonical_key);
	}

	return ret;
}

/**
 * Initialize the table of object initializers. */
void init_object_initializers()
{
	object_initializers[BEACON] = beacon_add;
}
