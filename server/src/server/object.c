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
 * Object related code. */

#include <global.h>
#include <loader.h>

/** List of active objects that need to be processed */
object *active_objects;

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

/**
 * Gender nouns. */
const char *gender_noun[GENDER_MAX] =
{
	"neuter", "male", "female", "hermaphrodite"
};
/**
 * Subjective pronouns. */
const char *gender_subjective[GENDER_MAX] =
{
	"it", "he", "she", "it"
};
/**
 * Subjective pronouns, with first letter in uppercase. */
const char *gender_subjective_upper[GENDER_MAX] =
{
	"It", "He", "She", "It"
};
/**
 * Objective pronouns. */
const char *gender_objective[GENDER_MAX] =
{
	"it", "him", "her", "it"
};
/**
 * Possessive pronouns. */
const char *gender_possessive[GENDER_MAX] =
{
	"its", "his", "her", "its"
};
/**
 * Reflexive pronouns. */
const char *gender_reflexive[GENDER_MAX] =
{
	"itself", "himself", "herself", "itself"
};

/** Material types. */
materialtype materials[NROFMATERIALS] =
{
	{"paper"},
	{"metal"},
	{"crystal"},
	{"leather"},
	{"wood"},
	{"organics"},
	{"stone"},
	{"cloth"},
	{"magic material"},
	{"liquid"},
	{"soft metal"},
	{"bone"},
	{"ice"}
};

#define NUM_MATERIALS_REAL NROFMATERIALS * NROFMATERIALS_REAL + 1

/**
 * Real material types. This array is initialized by
 * init_materials(). */
material_real_struct material_real[NUM_MATERIALS_REAL];

static void object_remove_inv(object *op);

/**
 * Initialize materials from file. */
void init_materials(void)
{
	int i;
	char filename[MAX_BUF], buf[MAX_BUF];
	FILE *fp;

	/* First initialize default values to the array */
	for (i = 0; i < NUM_MATERIALS_REAL; i++)
	{
		material_real[i].name[0] = '\0';
		material_real[i].quality = 100;
		material_real[i].type = M_NONE;
		material_real[i].def_race = RACE_TYPE_NONE;
	}

	snprintf(filename, sizeof(filename), "%s/materials", settings.datadir);

	if (!(fp = fopen(filename, "r")))
	{
		LOG(llevBug, "Could not open materials file: %s\n", filename);
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		if (sscanf(buf, "material_real %d\n", &i))
		{
			int def_race = RACE_TYPE_NONE, type = M_NONE, quality = 100;
			char name[MAX_BUF];

			if (i > NUM_MATERIALS_REAL)
			{
				LOG(llevError, "Materials file contains declaration for material #%d but it doesn't exist.\n", i);
			}

			name[0] = '\0';

			while (fgets(buf, sizeof(buf), fp))
			{
				if (strcmp(buf, "end\n") == 0)
				{
					break;
				}

				if (!sscanf(buf, "quality %d\n", &quality) && !sscanf(buf, "type %d\n", &type) && !sscanf(buf, "def_race %d\n", &def_race) && !sscanf(buf, "name %[^\n]", name))
				{
					LOG(llevError, "Bogus line in materials file: %s\n", buf);
				}
			}

			if (name[0] != '\0')
			{
				snprintf(material_real[i].name, sizeof(material_real[i].name), "%s ", name);
			}

			material_real[i].quality = quality;
			material_real[i].type = type;
			material_real[i].def_race = def_race;
		}
		else
		{
			LOG(llevError, "Bogus line in materials file: %s\n", buf);
		}
	}

	fclose(fp);
}

/** X offset when searching around a spot. */
int freearr_x[SIZEOFFREE] =
{
	0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1,
	0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1
};

/** Y offset when searching around a spot. */
int freearr_y[SIZEOFFREE] =
{
	0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2,
	-3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3
};

/** Number of spots around a location, including that location (except for 0) */
int maxfree[SIZEOFFREE] =
{
	0, 9, 10, 13, 14, 17, 18, 21, 22, 25, 26, 27, 30, 31, 32, 33, 36, 37, 39, 39, 42, 43, 44, 45,
	48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49
};

/** Direction we're pointing on this spot. */
int freedir[SIZEOFFREE] =
{
	0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 2, 2, 3, 4, 4, 4, 5, 6, 6, 6, 7, 8, 8, 8,
	1, 2, 2, 2, 2, 2, 3, 4, 4, 4, 4, 4, 5, 6, 6, 6, 6, 6, 7, 8, 8, 8, 8, 8
};

/**
 * Progressive object counter (every new object will increase this, even
 * if that object is later removed). */
static long ob_count = 0;

/**
 * Object type-specific functions to call when initializing objects. */
void (*object_initializers[256])(object *);

/**
 * This is a list of pointers that correspond to the FLAG_.. values.
 * This is a simple 1:1 mapping - if FLAG_FRIENDLY is 15, then the 15'th
 * element of this array should match that name.
 *
 * If an entry is NULL, that is a flag not to be loaded/saved.
 * @see flag_defines */
/* @cparser FLAG_(.*)
 * @page plugin_python_object_flags Python object flags
 * <h2>Python object flags</h2>
 * List of the object flags and their meaning. */
const char *object_flag_names[NUM_FLAGS + 1] =
{
	"sleep", "confused", NULL, "scared", "is_blind",
	"is_invisible", "is_ethereal", "is_good", "no_pick", "walk_on",
	"no_pass", "is_animated", "slow_move", "flying", "monster",
	"friendly", NULL, "been_applied", "auto_apply", "is_ready",
	"is_neutral", "see_invisible", "can_roll", "connect_reset", "is_turnable",
	"walk_off", "fly_on", "fly_off", "is_used_up", "identified",
	"reflecting", "changing", "splitting", "hitback", "startequip",
	"blocksview", "undead", "can_stack", "unaggressive", "reflect_missile",
	"reflect_spell", "no_magic", "no_fix_player", "is_evil", NULL,
	"run_away", "pass_thru", "can_pass_thru", "outdoor", "unique",
	"no_drop", "is_indestructible", "can_cast_spell", NULL, NULL,
	"can_use_bow", "can_use_armour", "can_use_weapon", "connect_no_push", "connect_no_release",
	"has_ready_bow", "xrays", NULL, "is_floor", "lifesave",
	"is_magical", NULL, "stand_still", "random_move", "only_attack",
	"wiz", "stealth", NULL, NULL, "cursed",
	"damned", "is_buildable", "no_pvp", NULL, NULL,
	"is_thrown", NULL, NULL, "is_male", "is_female",
	"applied", "inv_locked", NULL, NULL, NULL,
	"has_ready_weapon", "no_skill_ident", "was_wiz", "can_see_in_dark", "is_cauldron",
	"is_dust", NULL, "one_hit", "draw_double_always", "berserk",
	"no_attack", "invulnerable", "quest_item", "is_trapped", NULL,
	NULL, NULL, NULL, NULL, NULL,
	"sys_object", "use_fix_pos", "unpaid", "hidden", "make_invisible",
	"make_ethereal", "is_player", "is_named", NULL, "no_teleport",
	"corpse", "corpse_forced", "player_only", "no_cleric", "one_drop",
	"cursed_perm", "damned_perm", "door_closed", "is_spell", "is_missile",
	"draw_direction", "draw_double", "is_assassin", NULL, "no_save",
	NULL
};
/* @endcparser */

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
		key_value *has_field = object_get_key_link(has, wants_field->key);

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

	/* Do not merge objects if nrof would overflow. We use SINT32_MAX
	 * because sint32 is often used to store nrof instead of uint32. */
	if (ob1->nrof + ob2->nrof > SINT32_MAX)
	{
		return 0;
	}

	/* just some quick hack */
	if (ob1->type == MONEY && ob1->type == ob2->type && ob1->arch == ob2->arch)
	{
		return 1;
	}

	/* Gecko: Moved out special handling of event object nrof */
	/* important: don't merge objects with glow_radius set - or we come
	 * in heavy side effect situations. Because we really not know what
	 * our calling function will do after this merge (and the calling function
	 * then must first find out a merge has happen or not). The sense of stacks
	 * are to store inactive items. Because glow_radius items can be active even
	 * when not applied, merging is simply wrong here. MT. */
	if (((!ob1->nrof || !ob2->nrof) && ob1->type != EVENT_OBJECT) || ob1->glow_radius || ob2->glow_radius)
	{
		return 0;
	}

	/* just a brain dead long check for things NEVER NEVER should be different
	 * this is true under all circumstances for all objects. */
	if (ob1->type != ob2->type || ob1 == ob2 || ob1->arch != ob2->arch || ob1->sub_type != ob2->sub_type || ob1->material != ob2->material || ob1->material_real != ob2->material_real || ob1->magic != ob2->magic || ob1->item_quality != ob2->item_quality || ob1->item_condition != ob2->item_condition || ob1->item_race != ob2->item_race || ob1->speed != ob2->speed || ob1->value !=ob2->value || ob1->weight != ob2->weight)
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
			if (tmp1->type != EVENT_OBJECT || tmp2->type != EVENT_OBJECT)
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
	if (ob1->name != ob2->name || ob1->title != ob2->title || ob1->race != ob2->race || ob1->slaying != ob2->slaying || ob1->msg != ob2->msg || ob1->artifact != ob2->artifact)
	{
		return 0;
	}

	/* Compare the static arrays/structs */
	if ((memcmp(&ob1->stats, &ob2->stats, sizeof(living)) != 0) || (memcmp(&ob1->attack, &ob2->attack, sizeof(ob1->attack)) != 0) || (memcmp(&ob1->protection, &ob2->protection, sizeof(ob1->protection)) != 0))
	{
		return 0;
	}

	/* Ignore REMOVED and BEEN_APPLIED */
	if (ob1->randomitems != ob2->randomitems || ob1->other_arch != ob2->other_arch || (ob1->flags[0] | 0x300 | (1 << FLAG_IS_READY)) != (ob2->flags[0] | 0x300 | (1 << FLAG_IS_READY)) || ob1->flags[1] != ob2->flags[1] || ob1->flags[2] != ob2->flags[2] || ob1->flags[3] != ob2->flags[3] || ob1->path_attuned != ob2->path_attuned || ob1->path_repelled != ob2->path_repelled || ob1->path_denied != ob2->path_denied || ob1->terrain_type != ob2->terrain_type || ob1->terrain_flag != ob2->terrain_flag || ob1->weapon_speed != ob2->weapon_speed || ob1->magic != ob2->magic || ob1->item_level != ob2->item_level || ob1->item_skill != ob2->item_skill || ob1->glow_radius != ob2->glow_radius  || ob1->level != ob2->level || ob1->item_power != ob2->item_power)
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

	/* Don't merge items with differing custom names. */
	if (ob1->custom_name != ob2->custom_name)
	{
		return 0;
	}

	/* Can merge! */
	return 1;
}

/**
 * Tries to merge 'op' with items above and below the object.
 * @param op Object to merge.
 * @return 'op', or the object 'op' was merged into. */
object *object_merge(object *op)
{
	object *tmp;

	if (!op->nrof || !QUERY_FLAG(op, FLAG_CAN_STACK))
	{
		return op;
	}

	if (op->map)
	{
		tmp = GET_MAP_OB_LAST(op->map, op->x, op->y);
	}
	else if (op->env)
	{
		tmp = op->env->inv;
	}
	else
	{
		return op;
	}

	for ( ; tmp; tmp = tmp->below)
	{
		if (tmp != op && CAN_MERGE(op, tmp))
		{
			tmp->nrof += op->nrof;
			update_object(tmp, UP_OBJ_FACE);
			esrv_update_item(UPD_NROF, tmp);

			object_remove(op, REMOVE_NO_WEIGHT);
			object_destroy(op);
			return tmp;
		}
	}

	return op;
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

	if (QUERY_FLAG(op, FLAG_SYS_OBJECT))
	{
		return 0;
	}

	for (sum = 0, inv = op->inv; inv != NULL; inv = inv->below)
	{
		if (QUERY_FLAG(inv, FLAG_SYS_OBJECT))
		{
			continue;
		}

		if (inv->inv)
		{
			sum_weight(inv);
		}

		sum += WEIGHT_NROF(inv, inv->nrof);
	}

	if (op->type == CONTAINER && op->weapon_speed != 1.0f)
	{
		/* We'll store the calculated value in damage_round_tag, so
		 * we can use that as 'cache' for unmodified carrying weight.
		 * This allows us to reliably calculate the weight again in
		 * add_weight() and sub_weight() without rounding errors. */
		op->damage_round_tag = sum;
		sum = (sint32) ((float) sum * op->weapon_speed);
	}

	op->carrying = sum;
	return sum;
}

/**
 * Adds the specified weight to an object, and also updates how much the
 * environment(s) is/are carrying.
 * @param op The object
 * @param weight The weight to add */
void add_weight(object *op, sint32 weight)
{
	while (op)
	{
		if (op->type == CONTAINER && op->weapon_speed != 1.0f)
		{
			sint32 old_carrying = op->carrying;

			op->damage_round_tag += weight;
			op->carrying = (sint32) ((float) op->damage_round_tag * op->weapon_speed);
			weight = op->carrying - old_carrying;
		}
		else
		{
			op->carrying += weight;
		}

		if (op->env && op->env->type == PLAYER)
		{
			esrv_update_item(UPD_WEIGHT, op);
		}

		op = op->env;
	}
}

/**
 * Recursively (outwards) subtracts a number from the weight of an object
 * (and what is carried by its environment(s)).
 * @param op The object
 * @param weight The weight to subtract */
void sub_weight(object *op, sint32 weight)
{
	while (op)
	{
		if (op->type == CONTAINER && op->weapon_speed != 1.0f)
		{
			sint32 old_carrying = op->carrying;

			op->damage_round_tag -= weight;
			op->carrying = (sint32) ((float) op->damage_round_tag * op->weapon_speed);
			weight = old_carrying - op->carrying;
		}
		else
		{
			op->carrying -= weight;
		}

		if (op->env && op->env->type == PLAYER)
		{
			esrv_update_item(UPD_WEIGHT, op);
		}

		op = op->env;
	}
}

/**
 * Utility function.
 * @param op Object we want the environment of. Can't be NULL.
 * @return The outermost environment object for a given object. Will not be NULL. */
object *get_env_recursive(object *op)
{
	while (op->env)
	{
		op = op->env;
	}

	return op;
}

/**
 * Finds the player carrying an object.
 * @param op Item for which we want the carrier (player).
 * @return The player, or NULL if not in an inventory. */
object *is_player_inv(object *op)
{
	for (; op != NULL && op->type != PLAYER; op = op->env)
	{
		if (op->env == op)
		{
			op->env = NULL;
		}
	}

	return op;
}

/**
 * Dumps an object.
 * @param op Object to dump. Can be NULL.
 * @param sb Buffer that will contain object information. Must not be
 * NULL. */
void dump_object(object *op, StringBuffer *sb)
{
	if (op == NULL)
	{
		stringbuffer_append_string(sb, "[NULL pointer]");
		return;
	}

	if (op->arch != NULL)
	{
		stringbuffer_append_printf(sb, "arch %s\n", op->arch->name ? op->arch->name : "(null)");
		get_ob_diff(sb, op, &empty_archetype->clone);
		stringbuffer_append_string(sb, "end\n");
	}
	else
	{
		stringbuffer_append_string(sb, "Object ");
		stringbuffer_append_string(sb, op->name == NULL ? "(null)" : op->name);
		stringbuffer_append_string(sb, "\nend\n");
	}
}

/**
 * Dump an object, complete with its inventory.
 * @param op Object to dump.
 * @param sb Buffer that will contain object information. */
void dump_object_rec(object *op, StringBuffer *sb)
{
	archetype *at;
	object *tmp;

	if (!op)
	{
		return;
	}

	/* Get the difference from the object's archetype. */
	at = op->arch;

	/* No archetype, use empty archetype. */
	if (!at)
	{
		at = empty_archetype;
	}

	stringbuffer_append_printf(sb, "arch %s\n", at->name);
	get_ob_diff(sb, op, &at->clone);

	/* Recursively dump the inventory. */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		dump_object_rec(tmp, sb);
	}

	stringbuffer_append_string(sb, "end\n");
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

	op->owner = NULL;

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
}

static void set_skill_pointers(object *op, object *chosen_skill, object *exp_obj)
{
	op->chosen_skill = chosen_skill;
	op->exp_obj = exp_obj;
}

/**
 * Sets the owner and sets the skill and exp pointers to owner's current
 * skill and experience objects.
 * @param op The object.
 * @param owner The owner. */
void set_owner(object *op, object *owner)
{
	if (owner == NULL || op == NULL)
	{
		return;
	}

	/* Ensure we have a head. */
	owner = HEAD(owner);
	set_owner_simple(op, owner);

	if (owner->type == PLAYER && owner->chosen_skill)
	{
		set_skill_pointers(op, owner->chosen_skill, owner->chosen_skill->exp_obj);
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
 * @param op The object.
 * @param clone_ob The clone. */
void copy_owner(object *op, object *clone_ob)
{
	object *owner = get_owner(clone_ob);

	if (owner == NULL)
	{
		/* players don't have owners - they own themselves. Update
		 * as appropriate. */
		if (clone_ob->type == PLAYER)
		{
			owner = clone_ob;
		}
		else
		{
			return;
		}
	}

	set_owner_simple(op, owner);

	if (clone_ob->chosen_skill)
	{
		set_skill_pointers(op, clone_ob->chosen_skill, clone_ob->exp_obj);
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
	FREE_ONLY_HASH(op->artifact);
	FREE_ONLY_HASH(op->custom_name);

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
	op->attacked_by_count = -1;

	/* give the object a new (unique) count tag */
	op->count = ++ob_count;
}

/**
 * Copy object first frees everything allocated by the second object,
 * and then copies the contents of the first object into the second
 * object, allocating what needs to be allocated.
 * @param op2 Object that we copy from.
 * @param op Object that we copy to.
 * @param no_speed If set, do not touch the active list. */
void copy_object(object *op2, object *op, int no_speed)
{
	int is_removed = QUERY_FLAG(op, FLAG_REMOVED);

	FREE_ONLY_HASH(op->name);
	FREE_ONLY_HASH(op->title);
	FREE_ONLY_HASH(op->race);
	FREE_ONLY_HASH(op->slaying);
	FREE_ONLY_HASH(op->msg);
	FREE_ONLY_HASH(op->artifact);
	FREE_ONLY_HASH(op->custom_name);

	free_key_values(op);

	memcpy((void *)((char *) op + offsetof(object, name)), (void *) ((char *) op2 + offsetof(object, name)), sizeof(object) - offsetof(object, name));

	if (is_removed)
	{
		SET_FLAG(op, FLAG_REMOVED);
	}

	ADD_REF_NOT_NULL_HASH(op->name);
	ADD_REF_NOT_NULL_HASH(op->title);
	ADD_REF_NOT_NULL_HASH(op->race);
	ADD_REF_NOT_NULL_HASH(op->slaying);
	ADD_REF_NOT_NULL_HASH(op->msg);
	ADD_REF_NOT_NULL_HASH(op->artifact);
	ADD_REF_NOT_NULL_HASH(op->custom_name);

	/* Only alter speed_left when we sure we have not done it before */
	if (!no_speed && op->speed < 0 && op->speed_left == op->arch->clone.speed_left)
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

	if (!no_speed)
	{
		update_ob_speed(op);
	}
}

/**
 * Copy an object with an inventory, duplicating the inv too.
 * @param src_ob Object to copy.
 * @param dest_ob Where to copy. */
void copy_object_with_inv(object *src_ob, object *dest_ob)
{
	object *walk, *tmp;

	copy_object(src_ob, dest_ob, 0);

	for (walk = src_ob->inv; walk; walk = walk->below)
	{
		tmp = get_object();
		copy_object_with_inv(walk, tmp);
		insert_ob_in_ob(tmp, dest_ob);
	}
}

/**
 * Grabs an object from the list of unused objects, makes sure it is
 * initialized, and returns it.
 *
 * If there are no free objects, expand_objects() is called to get more.
 * @return The new object. */
object *get_object(void)
{
	object *new_obj = (object *) get_poolchunk(pool_object);

	SET_FLAG(new_obj, FLAG_REMOVED);

	return new_obj;
}

/**
 * If an object with the IS_TURNABLE() flag needs to be turned due
 * to the closest player being on the other side, this function can
 * be called to update the face variable, <u>and</u> how it looks on the
 * map.
 * @param op The object to update. */
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
	/* No reason putting the archetypes objects on the speed list,
	 * since they never really need to be updated. */
	if (OBJECT_FREE(op) && op->speed)
	{
		LOG(llevBug, "Object %s is freed but has speed.\n", op->name);
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
 * Updates the array which represents the map.
 *
 * It takes into account invisible objects (and represent squares covered
 * by invisible objects by whatever is below them (unless it's another
 * invisible object, etc).
 *
 * If the object being updated is beneath a player, the below window
 * of that player is updated.
 * @param op Object to update
 * @param action Hint of what the caller believes need to be done. One of
 * @ref UP_OBJ_xxx values. */
void update_object(object *op, int action)
{
	MapSpace *msp;
	int flags, newflags;

	if (op == NULL)
	{
		LOG(llevError, "update_object() called for NULL object.\n");
		return;
	}

	if (op->env != NULL || !op->map || op->map->in_memory == MAP_SAVING)
	{
		return;
	}

	/* No need to change anything except the map update counter */
	if (action == UP_OBJ_FACE)
	{
		INC_MAP_UPDATE_COUNTER(op->map, op->x, op->y);
		return;
	}

	msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);
	newflags = msp->flags;
	flags = newflags & ~P_NEED_UPDATE;

	/* Always resort layer - but not always flags */
	if (action == UP_OBJ_INSERT)
	{
		/* Force layer rebuild */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;

		/* Handle lighting system */
		if (op->glow_radius)
		{
			adjust_light_source(op->map, op->x, op->y, op->glow_radius);
		}

		/* This is handled a bit more complex, we must always loop the flags! */
		if (QUERY_FLAG(op, FLAG_NO_PASS) || QUERY_FLAG(op, FLAG_PASS_THRU))
		{
			newflags |= P_FLAGS_UPDATE;
		}
		/* Floors define our node - force an update */
		else if (QUERY_FLAG(op, FLAG_IS_FLOOR))
		{
			newflags |= P_FLAGS_UPDATE;
			msp->light_value += op->last_sp;
		}
		/* We don't have to use flag loop - we can set it by hand! */
		else
		{
			if (op->type == CHECK_INV)
			{
				newflags |= P_CHECK_INV;
			}

			if (QUERY_FLAG(op, FLAG_MONSTER))
			{
				newflags |= P_IS_MONSTER;
			}

			if (QUERY_FLAG(op, FLAG_IS_PLAYER))
			{
				newflags |= P_IS_PLAYER;
			}

			if (QUERY_FLAG(op, FLAG_PLAYER_ONLY))
			{
				newflags |= P_PLAYER_ONLY;
			}

			if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
			{
				newflags |= P_BLOCKSVIEW;
			}

			if (QUERY_FLAG(op, FLAG_NO_MAGIC))
			{
				newflags |= P_NO_MAGIC;
			}

			if (QUERY_FLAG(op, FLAG_NO_CLERIC))
			{
				newflags |= P_NO_CLERIC;
			}

			if (QUERY_FLAG(op, FLAG_WALK_ON))
			{
				newflags |= P_WALK_ON;
			}

			if (QUERY_FLAG(op, FLAG_FLY_ON))
			{
				newflags |= P_FLY_ON;
			}

			if (QUERY_FLAG(op, FLAG_WALK_OFF))
			{
				newflags |= P_WALK_OFF;
			}

			if (QUERY_FLAG(op, FLAG_FLY_OFF))
			{
				newflags |= P_FLY_OFF;
			}

			if (QUERY_FLAG(op, FLAG_DOOR_CLOSED))
			{
				newflags |= P_DOOR_CLOSED;
			}

			if (QUERY_FLAG(op, FLAG_NO_PVP))
			{
				newflags |= P_NO_PVP;
			}

			if (op->type == MAGIC_MIRROR)
			{
				newflags |= P_MAGIC_MIRROR;
			}

			if (QUERY_FLAG(op, FLAG_OUTDOOR))
			{
				newflags |= P_OUTDOOR;
			}
		}
	}
	else if (action == UP_OBJ_REMOVE)
	{
		/* Force layer rebuild */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;

		/* Handle lighting system */
		if (op->glow_radius)
		{
			adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
		}

		/* We must rebuild the flags when one of these flags is touched from our object */
		if (QUERY_FLAG(op, FLAG_MONSTER) || QUERY_FLAG(op, FLAG_IS_PLAYER) || QUERY_FLAG(op, FLAG_BLOCKSVIEW) || QUERY_FLAG(op, FLAG_DOOR_CLOSED) || QUERY_FLAG(op, FLAG_PASS_THRU) || QUERY_FLAG(op, FLAG_NO_PASS) || QUERY_FLAG(op, FLAG_PLAYER_ONLY) || QUERY_FLAG(op, FLAG_NO_MAGIC) || QUERY_FLAG(op, FLAG_NO_CLERIC) || QUERY_FLAG(op, FLAG_WALK_ON) || QUERY_FLAG(op, FLAG_FLY_ON) || QUERY_FLAG(op, FLAG_WALK_OFF) || QUERY_FLAG(op, FLAG_FLY_OFF) || QUERY_FLAG(op,	FLAG_IS_FLOOR) || op->type == CHECK_INV)
		{
			newflags |= P_FLAGS_UPDATE;
		}
	}
	else if (action == UP_OBJ_FLAGS)
	{
		/* Force flags rebuild but no tile counter */
		newflags |= P_FLAGS_UPDATE;
	}
	else if (action == UP_OBJ_FLAGFACE)
	{
		/* Force flags rebuild */
		newflags |= P_FLAGS_UPDATE;
		msp->update_tile++;
	}
	else if (action == UP_OBJ_LAYER)
	{
		/* Rebuild layers - most common when we change visibility of the object */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;
	}
	else if (action == UP_OBJ_ALL)
	{
		/* Force full tile update */
		newflags |= (P_FLAGS_UPDATE | P_NEED_UPDATE);
		msp->update_tile++;
	}
	else
	{
		LOG(llevError, "update_object called with invalid action: %d\n", action);
		return;
	}

	if (flags != newflags)
	{
		/* Rebuild flags */
		if (newflags & (P_FLAGS_UPDATE))
		{
			msp->flags |= (newflags | P_NO_ERROR | P_FLAGS_ONLY);
			update_position(op->map, op->x, op->y);
		}
		else
		{
			msp->flags |= newflags;
		}
	}

	if (op->more != NULL)
	{
		update_object(op->more, action);
	}
}

/**
 * Drops the inventory of ob into ob's current environment.
 *
 * Makes some decisions whether to actually drop or not, and/or to
 * create a corpse for the stuff.
 * @param ob The object to drop the inventory for. */
void drop_ob_inv(object *ob)
{
	object *corpse = NULL, *enemy = NULL, *tmp_op = NULL, *tmp = NULL;

	/* We don't handle players here */
	if (ob->type == PLAYER)
	{
		LOG(llevBug, "drop_ob_inv() - tried to drop items of %s\n", ob->name);
		return;
	}

	if (ob->env == NULL && (ob->map == NULL || ob->map->in_memory != MAP_IN_MEMORY))
	{
		return;
	}

	if (ob->enemy && ob->enemy->type == PLAYER)
	{
		enemy = ob->enemy;
	}
	else
	{
		enemy = get_owner(ob->enemy);
	}

	/* Create race corpse and/or drop stuff to floor */
	if ((QUERY_FLAG(ob, FLAG_CORPSE) && !QUERY_FLAG(ob, FLAG_STARTEQUIP)) || QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
	{
		ob_race *race_corpse = race_find(ob->race);

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
		object_remove(tmp_op, 0);

		if (tmp_op->type == QUEST_CONTAINER)
		{
			/* Legal, non freed enemy */
			if (enemy && enemy->type == PLAYER && enemy->count == ob->enemy_count)
			{
				check_quest(enemy, tmp_op);
			}
		}
		else if (!(QUERY_FLAG(ob, FLAG_STARTEQUIP) || (tmp_op->type != RUNE && (QUERY_FLAG(tmp_op, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp_op, FLAG_STARTEQUIP) || QUERY_FLAG(tmp_op, FLAG_NO_DROP)))))
		{
			tmp_op->x = ob->x, tmp_op->y = ob->y;

			/* Always clear these in case the monster used the item */
			CLEAR_FLAG(tmp_op, FLAG_APPLIED);
			CLEAR_FLAG(tmp_op, FLAG_BEEN_APPLIED);

			/* If we have a corpse put the item in it */
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

	/* Drop the corpse */
	if (corpse || QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
	{
		if (enemy && enemy->type == PLAYER)
		{
			if (enemy->count == ob->enemy_count)
			{
				FREE_AND_ADD_REF_HASH(corpse->slaying, enemy->name);
			}
		}
		else if (QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
		{
			corpse->stats.food = 5;
		}

		/* Change sub_type to mark this corpse */
		if (corpse->slaying)
		{
			if (CONTR(enemy)->party)
			{
				FREE_AND_ADD_REF_HASH(corpse->slaying, CONTR(enemy)->party->name);
				corpse->sub_type = ST1_CONTAINER_CORPSE_party;
			}
			else
			{
				corpse->sub_type = ST1_CONTAINER_CORPSE_player;
			}
		}

		/* Store the original food value. */
		corpse->last_eat = corpse->stats.food;

		if (ob->env)
		{
			insert_ob_in_ob(corpse, ob->env);
		}
		else
		{
			insert_ob_in_map(corpse, ob->map, NULL, 0);
		}
	}
}

/**
 * Destroy (free) inventory of an object. Used internally by
 * object_destroy() to recursively free the object's inventory.
 * @param ob Object to free the inventory of. */
static void object_destroy_inv(object *ob)
{
	object *tmp, *next;

	for (tmp = ob->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->inv)
		{
			object_destroy_inv(tmp);
		}

		object_remove(tmp, 0);
	}
}

/**
 * Cleanups and frees everything allocated by an object and gives the
 * memory back to the object mempool.
 *
 * @note The object must have been removed by object_remove() first.
 * @param ob The object to destroy (free). */
void object_destroy(object *ob)
{
	if (!QUERY_FLAG(ob, FLAG_REMOVED))
	{
		StringBuffer *sb = stringbuffer_new();
		char *diff;

		dump_object(ob, sb);
		diff = stringbuffer_finish(sb);
		LOG(llevBug, "Destroy object called with non removed object\n:%s\n", diff);
		free(diff);
	}

	free_key_values(ob);

	if (QUERY_FLAG(ob, FLAG_IS_LINKED))
	{
		connection_object_remove(ob);
	}

	if (ob->type == CONTAINER && ob->attacked_by)
	{
		container_close(NULL, ob);
	}

	/* Remove and free the inventory. */
	object_remove_inv(ob);
	object_destroy_inv(ob);

	/* Remove object from the active list. */
	ob->speed = 0;
	update_ob_speed(ob);

	/* Free attached attrsets */
	if (ob->custom_attrset)
	{
		switch (ob->type)
		{
			case PLAYER:
			/* Players are changed into DEAD_OBJECTs when they logout */
			case DEAD_OBJECT:
				return_poolchunk(ob->custom_attrset, pool_player);
				break;

			case MAGIC_MIRROR:
				magic_mirror_deinit(ob);
				break;

			default:
				LOG(llevBug, "object_destroy() custom attrset found in unsupported object %s (type %d)\n", STRING_OBJ_NAME(ob), ob->type);
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
	FREE_AND_CLEAR_HASH2(ob->artifact);
	FREE_AND_CLEAR_HASH2(ob->custom_name);

	/* Mark object as freed and invalidate all references to it. */
	ob->count = 0;

	/* Return the memory to the mempool. */
	return_poolchunk(ob, pool_object);
}

/**
 * Drop op's inventory on the floor and remove op from the map.
 *
 * Used mainly for physical destruction of normal objects and monsters.
 * @param op Object to destruct. */
void destruct_ob(object *op)
{
	if (op->inv)
	{
		drop_ob_inv(op);
	}

	object_remove(op, 0);
	object_destroy(op);
}

static void object_check_move_off(object *op)
{
	MapSpace *msp;
	object *tmp, *next;
	tag_t tag;
	mapstruct *m;
	int x, y;

	if (QUERY_FLAG(op, FLAG_NO_APPLY))
	{
		return;
	}

	msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

	/* No event on this tile. */
	if (!(msp->flags & (P_WALK_OFF | P_FLY_OFF)))
	{
		return;
	}

	tag = op->count;
	m = op->map;
	x = op->x;
	y = op->y;

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = next)
	{
		next = tmp->above;

		/* Can't apply yourself. */
		if (tmp == op)
		{
			continue;
		}

		if (QUERY_FLAG(op, FLAG_FLYING) ? QUERY_FLAG(tmp, FLAG_FLY_OFF) : QUERY_FLAG(tmp, FLAG_WALK_OFF))
		{
			object_move_on(tmp, op, NULL, 0);

			if (was_destroyed(op, tag))
			{
				return;
			}

			if (op->map != m || op->x != x || op->y != y)
			{
				return;
			}
		}
	}
}

/**
 * This function removes the object op from the linked list of objects
 * which it is currently tied to. When this function is done, the
 * object will have no environment. If the object previously had an
 * environment, the map pointer and x/y coordinates will be updated to
 * the previous environment.
 *
 * @note If you want to remove a lot of items in player's inventory,
 * set FLAG_NO_FIX_PLAYER on the player first and then explicitly call
 * fix_player() on the player.
 * @param op Object to remove.
 * @param flags Combination of @ref REMOVAL_xxx. */
void object_remove(object *op, int flags)
{
	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		LOG(llevBug, "object_remove(): Trying to remove a removed object: %s.\n", query_name(op, NULL));
		return;
	}

	if (op->more)
	{
		object_remove(op->more, flags);
	}

	SET_FLAG(op, FLAG_REMOVED);
	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	op->quickslot = 0;
	CLEAR_FLAG(op, FLAG_IS_READY);

	/* In this case, the object to be removed is in someone's
	 * inventory. */
	if (op->env)
	{
		object *pl;

		if (!QUERY_FLAG(op, FLAG_SYS_OBJECT) && !(flags & REMOVE_NO_WEIGHT))
		{
			sub_weight(op->env, WEIGHT_NROF(op, op->nrof));
		}

		pl = is_player_inv(op->env);

		/* NO_FIX_PLAYER is set when a great many changes are being made
		 * to player's inventory. If set, avoid the call to save CPU
		 * time. */
		if (pl && !QUERY_FLAG(pl, FLAG_NO_FIX_PLAYER))
		{
			fix_player(pl);
		}

		if (op->above)
		{
			op->above->below = op->below;
		}
		else
		{
			op->env->inv = op->below;
		}

		if (op->below)
		{
			op->below->above = op->above;
		}

		/* We set up values so that it could be inserted into the map,
		 * but we don't actually do that - it is up to the caller to
		 * decide what we want to do. */
		op->x = op->env->x, op->y = op->env->y;
		op->map = op->env->map;

		esrv_del_item(op);
		object_callback_remove_inv(op);

		op->above = NULL, op->below = NULL;
		op->env = NULL;
	}
	/* The object is on map. */
	else if (op->map)
	{
		MapSpace *msp;

		/* If this is the base layer object, we assign the next object
		 * to be it if it is from same layer and sub-layer. */
		msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

		if (op->layer && GET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer) == op)
		{
			if (op->above && op->above->layer == op->layer && op->above->sub_layer == op->sub_layer)
			{
				SET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer, op->above);
			}
			else
			{
				SET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer, NULL);
			}
		}

		/* Link the object above us. */
		if (op->above)
		{
			op->above->below = op->below;
		}
		/* Assign below as last one. */
		else
		{
			SET_MAP_SPACE_LAST(msp, op->below);
		}

		/* Relink the object below us, if there is one. */
		if (op->below)
		{
			op->below->above = op->above;
		}
		/* First object goes on above it. */
		else
		{
			SET_MAP_SPACE_FIRST(msp, op->above);
		}

		op->above = NULL;
		op->below = NULL;
		op->env = NULL;

		if (op->map->in_memory != MAP_SAVING)
		{
			msp->update_tile++;
			update_object(op, UP_OBJ_REMOVE);
		}

		object_callback_remove_map(op);

		if (!(flags & REMOVE_NO_WALK_OFF))
		{
			object_check_move_off(op);
		}
	}
	else
	{
		LOG(llevBug, "object_remove(): Tried to remove object that is not on map or in inventory: %s.\n", query_name(op, NULL));
	}
}

/**
 * Recursively remove the inventory of an object.
 * @param op Object. */
static void object_remove_inv(object *op)
{
	object *tmp, *tmp2;

	for (tmp = op->inv; tmp; tmp = tmp2)
	{
		/* Save pointer, gets NULL in object_remove */
		tmp2 = tmp->below;

		if (tmp->inv)
		{
			object_remove_inv(tmp);
		}

		/* No map, no check off */
		object_remove(tmp, 0);
	}
}

/**
 * This function inserts the object in the two-way linked list which
 * represents what is on a map.
 * @param op Object to insert.
 * @param m Map to insert into.
 * @param originator What caused op to be inserted.
 * @param flag Combination of @ref INS_xxx "INS_xxx" values.
 * @return NULL if 'op' was destroyed, 'op' otherwise. */
object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag)
{
	object *tmp, *top;
	MapSpace *mc;
	int x, y, lt;

	if (!m)
	{
		return NULL;
	}

	if (OBJECT_FREE(op))
	{
		LOG(llevBug, "insert_ob_in_map(): Trying to insert freed object %s in map %s.\n", query_name(op, NULL), m->path);
		return NULL;
	}

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		LOG(llevBug, "insert_ob_in_map(): Trying to insert non removed object %s in map %s.\n", query_name(op, NULL), m->path);
		return NULL;
	}

	if (!op->head && op->arch->more && !op->more)
	{
		archetype *at;
		object *prev, *tail;

		prev = op;

		for (at = op->arch->more; at; at = at->more)
		{
			tail = arch_to_object(at);

			tail->type = op->type;
			tail->layer = op->layer;
			tail->sub_layer = op->sub_layer;

			tail->head = op;
			prev->more = tail;

			prev = tail;
		}

		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_SYS_OBJECT);
		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_NO_APPLY);
		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_IS_INVISIBLE);
		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_IS_ETHEREAL);
		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_CAN_PASS_THRU);
		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_FLYING);
		SET_OR_CLEAR_MULTI_FLAG(op, FLAG_BLOCKSVIEW);

		SET_OR_CLEAR_MULTI_FLAG_IF_CLONE(op, FLAG_NO_PASS);
		SET_OR_CLEAR_MULTI_FLAG_IF_CLONE(op, FLAG_BLOCKSVIEW);
	}

	if (op->more)
	{
		op->more->x = HEAD(op)->x + op->more->arch->clone.x;
		op->more->y = HEAD(op)->y + op->more->arch->clone.y;

		if (insert_ob_in_map(op->more, m, originator, flag) == NULL)
		{
			return NULL;
		}
	}

	CLEAR_FLAG(op, FLAG_REMOVED);

	x = op->x;
	y = op->y;
	m = get_map_from_coord(m, &x, &y);

	if (!m)
	{
		LOG(llevBug, "insert_ob_in_map(): Tried to insert object %s outside the map %s (%d, %d).\n", query_name(op, NULL), op->map->path, op->x, op->y);
		return NULL;
	}

	op->x = x;
	op->y = y;
	op->map = m;

	/* Merge objects if possible. */
	if (op->nrof && !(flag & INS_NO_MERGE))
	{
		for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above)
		{
			if (CAN_MERGE(op, tmp))
			{
				op->nrof += tmp->nrof;
				object_remove(tmp, 0);
				object_destroy(tmp);
				break;
			}
		}
	}

	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	CLEAR_FLAG(op, FLAG_APPLIED);
	CLEAR_FLAG(op, FLAG_INV_LOCKED);

	mc = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

	if (op->layer && !QUERY_FLAG(op, FLAG_IS_INVISIBLE))
	{
		top = GET_MAP_SPACE_LAYER(mc, op->layer, op->sub_layer);

		/* Do we have another object on this layer? */
		if (!top)
		{
			int slt;

			/* No, we are the first on this layer - let's search something above us we can chain with. */
			for (lt = op->layer; lt <= NUM_LAYERS && !top; lt++)
			{
				for (slt = op->sub_layer; slt < NUM_SUB_LAYERS && !top; slt++)
				{
					top = GET_MAP_SPACE_LAYER(mc, lt, slt);
				}
			}
		}

		SET_MAP_SPACE_LAYER(mc, op->layer, op->sub_layer, op);

		if (top)
		{
			if (top->below)
			{
				top->below->above = op;
			}
			else
			{
				SET_MAP_SPACE_FIRST(mc, op);
			}

			op->below = top->below;
			top->below = op;
			op->above = top;
		}
		else
		{
			if ((top = GET_MAP_SPACE_LAST(mc)) != NULL)
			{
				top->above = op;
				op->below = top;
			}
			else
			{
				SET_MAP_SPACE_FIRST(mc, op);
			}

			SET_MAP_SPACE_LAST(mc, op);
		}
	}
	else
	{
		if ((top = GET_MAP_SPACE_FIRST(mc)) != NULL)
		{
			/* Easy chaining */
			top->below = op;
			op->above = top;
		}
		else
		{
			SET_MAP_SPACE_LAST(mc, op);
		}

		SET_MAP_SPACE_FIRST(mc, op);
	}

	/* Some object-type-specific adjustments/initialization. */
	if (op->type == PLAYER)
	{
		CONTR(op)->socket.update_tile = 0;
		CONTR(op)->update_los = 1;

		if (op->map->player_first)
		{
			CONTR(op->map->player_first)->map_below = op;
			CONTR(op)->map_above = op->map->player_first;
		}

		op->map->player_first = op;
	}
	else if (op->type == MAP_EVENT_OBJ)
	{
		map_event_obj_init(op);
	}

	/* Mark this tile as changed. */
	mc->update_tile++;
	/* Update flags for this tile. */
	update_object(op, UP_OBJ_INSERT);

	if (!(flag & INS_NO_WALK_ON) && (mc->flags & (P_WALK_ON | P_FLY_ON) || op->more) && !op->head)
	{
		for (tmp = op; tmp; tmp = tmp->more)
		{
			if (object_check_move_on(tmp, originator))
			{
				return NULL;
			}
		}
	}

	return op;
}

/**
 * Checks if any objects has a movement type that matches objects
 * that affect this object on this space. Calls object_move_on() to
 * process these events.
 * @param op Object that may trigger something.
 * @param originator Player, monster or other object that caused 'op' to
 * be inserted into the map. May be NULL.
 * @return 1 if 'op' was destroyed, 0 otherwise. */
int object_check_move_on(object *op, object *originator)
{
	object *tmp;
	tag_t tag;
    mapstruct *m;
    int x, y;

	if (QUERY_FLAG(op, FLAG_NO_APPLY))
	{
		return 0;
	}

	tag = op->count;
	m = op->map;
	x = op->x;
	y = op->y;

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = tmp->above)
	{
		/* Can't apply yourself. */
		if (tmp == op)
		{
			continue;
		}

		if (IS_LIVE(op) && !QUERY_FLAG(op, FLAG_WIZPASS) && QUERY_FLAG(tmp, FLAG_SLOW_MOVE) && (!tmp->terrain_flag || tmp->terrain_flag & op->terrain_flag))
		{
			op->speed_left -= SLOW_PENALTY(tmp) * FABS(op->speed);
		}

		if (QUERY_FLAG(op, FLAG_FLYING) ? QUERY_FLAG(tmp, FLAG_FLY_ON) : QUERY_FLAG(tmp, FLAG_WALK_ON))
		{
			object_move_on(tmp, op, originator, 1);

			if (was_destroyed(op, tag))
			{
				return 1;
			}

			if (op->map != m || op->x != x || op->y != y)
			{
				return 0;
			}
		}
	}

	return 0;
}

/**
 * This function inserts an object of a specified archetype in the map, but
 * if it finds objects of its own type, it'll remove them first.
 * @param arch_string Object's archetype to insert.
 * @param op Object to insert under, supplies coordinates and the map. */
void replace_insert_ob_in_map(char *arch_string, object *op)
{
	object *tmp, *tmp1;

	/* First search for itself and remove any old instances */
	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = tmp->above)
	{
		if (!strcmp(tmp->arch->name, arch_string))
		{
			object_remove(tmp, 0);
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

object *object_stack_get(object *op, uint32 nrof)
{
	object *split;

	nrof = MAX(1, nrof);

	if (MAX(1, op->nrof) <= nrof)
	{
		return op;
	}

	op->nrof -= nrof;
	update_object(op, UP_OBJ_FACE);
	esrv_update_item(UPD_NROF, op);

	if (op->env && !QUERY_FLAG(op, FLAG_SYS_OBJECT))
	{
		sub_weight(op->env, WEIGHT_NROF(op, nrof));
	}

	split = get_object();
	copy_object_with_inv(op, split);

	split->nrof = nrof;
	CLEAR_FLAG(split, FLAG_IS_READY);

	return split;
}

object *object_stack_get_reinsert(object *op, uint32 nrof)
{
	object *split;

	split = object_stack_get(op, nrof);

	if (split != op)
	{
		if (op->map)
		{
			split = insert_ob_in_map(split, op->map, NULL, INS_NO_MERGE);
		}
		else if (op->env)
		{
			split = object_insert_into(split, op->env, INS_NO_MERGE);
		}
	}

	return split;
}

object *object_stack_get_removed(object *op, uint32 nrof)
{
	object *split;

	split = object_stack_get(op, nrof);

	if (split == op)
	{
		object_remove(split, 0);
	}

	return split;
}

/**
 * Decreases a specified number from the amount of an object. If the
 * amount reaches 0, the object is subsequently removed and freed.
 *
 * This function will send an update to client if op is in a player
 * inventory.
 * @param op Object to decrease.
 * @param i Number to remove.
 * @return 'op' if something is left, NULL if the amount reached 0. */
object *decrease_ob_nr(object *op, uint32 i)
{
	/* Objects with op->nrof require this check */
	if (i == 0)
	{
		return op;
	}

	if (i > op->nrof)
	{
		i = op->nrof;
	}

	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		op->nrof -= i;
	}
	else if (op->env)
	{
		if (i < op->nrof)
		{
			op->nrof -= i;

			if (!QUERY_FLAG(op, FLAG_SYS_OBJECT))
			{
				sub_weight(op->env, op->weight * i);
			}
		}
		else
		{
			object_remove(op, 0);
			op->nrof = 0;
		}
	}
	else
	{
		if (i < op->nrof)
		{
			op->nrof -= i;
		}
		else
		{
			object_remove(op, 0);
			op->nrof = 0;
		}
	}

	if (op->nrof)
	{
		return op;
	}

	return NULL;
}

object *object_insert_into(object *op, object *where, int flag)
{
	object *otmp;

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		StringBuffer *sb;
		char *diff;

		sb = stringbuffer_new();
		dump_object(op, sb);
		diff = stringbuffer_finish(sb);
		LOG(llevBug, "Trying to insert (ob) inserted object.\n%s\n", diff);
		free(diff);
		return op;
	}

	if (where == NULL)
	{
		StringBuffer *sb;
		char *diff;

		sb = stringbuffer_new();
		dump_object(op, sb);
		diff = stringbuffer_finish(sb);
		LOG(llevBug, "Trying to put object in NULL.\n%s\n", diff);
		free(diff);
		return op;
	}

	where = HEAD(where);
	op = HEAD(op);

	/* If the object has tail parts, it means the object is a multi-part
	 * object that was on a map prior to this insert call. Thus, we will
	 * want to destroy the tail parts of this object, so if the object
	 * is at some later point inserted on the map again, the tails will
	 * be re-created. */
	if (op->more)
	{
		object *tmp, *next;

		for (tmp = op->more; tmp; tmp = next)
		{
			next = tmp->more;
			object_destroy(tmp);
		}

		op->more = NULL;
	}

	CLEAR_FLAG(op, FLAG_REMOVED);

	if (!QUERY_FLAG(op, FLAG_SYS_OBJECT))
	{
		if (!(flag & INS_NO_MERGE))
		{
			object *tmp;

			for (tmp = where->inv; tmp; tmp = tmp->below)
			{
				if (!QUERY_FLAG(tmp, FLAG_SYS_OBJECT) && CAN_MERGE(tmp, op))
				{
					tmp->nrof += op->nrof;
					esrv_update_item(UPD_NROF, tmp);
					add_weight(where, op->weight * MAX(1, op->nrof));

					SET_FLAG(op, FLAG_REMOVED);
					object_destroy(op);

					return tmp;
				}
			}
		}

		add_weight(where, WEIGHT_NROF(op, op->nrof));
	}

	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	op->map = NULL;
	op->env = where;
	op->above = NULL;
	op->below = NULL;
	op->x = 0, op->y = 0;

	/* Client has no idea of ordering so let's not bother ordering it here.
	 * It sure simplifies this function... */
	if (where->inv == NULL)
	{
		where->inv = op;
	}
	else
	{
		op->below = where->inv;
		op->below->above = op;
		where->inv = op;
	}

	/* Check for event object and set the owner object
	 * event flags. */
	if (op->type == EVENT_OBJECT && op->sub_type)
	{
		where->event_flags |= (1U << (op->sub_type - 1));
	}
	else if (op->type == QUEST_CONTAINER && where->type == CONTAINER)
	{
		where->event_flags |= EVENT_FLAG(EVENT_QUEST);
	}

	/* If player, fix player if not marked as no fix. */
	otmp = is_player_inv(where);

	if (otmp && CONTR(otmp) != NULL)
	{
		if (!QUERY_FLAG(otmp, FLAG_NO_FIX_PLAYER))
		{
			fix_player(otmp);
		}
	}

	if (where->type == PLAYER || where->type == CONTAINER)
	{
		esrv_send_item(op);
	}

	return op;
}

/**
 * This function inserts the object op in the linked list inside the
 * object environment.
 * @param op Object to insert. Must be removed and not NULL. Must not be
 * multipart. May become invalid after return, so use return value of the
 * function.
 * @param where Object to insert into. Must not be NULL. Should be the
 * head part.
 * @return Pointer to inserted item, which will be different than op if
 * object was merged. */
object *insert_ob_in_ob(object *op, object *where)
{
	return object_insert_into(op, where, 0);
}

/**
 * Searches for any objects with a matching archetype at the given map
 * and coordinates.
 * @param at Archetype to look for.
 * @param m Map.
 * @param x X coordinate on map.
 * @param y Y coordinate on map.
 * @return First matching object, or NULL if none matches. */
object *present_arch(archetype *at, mapstruct *m, int x, int y)
{
	object *tmp;

	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		LOG(llevError, "present_arch() called outside map.\n");
		return NULL;
	}

	for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above)
	{
		if (tmp->arch == at)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Searches for any objects with a matching type at the given map and
 * coordinates.
 * @param type Type to find.
 * @param m Map.
 * @param x X coordinate on map.
 * @param y Y coordinate on map.
 * @return First matching object, or NULL if none matches. */
object *present(uint8 type, mapstruct *m, int x, int y)
{
	object *tmp;

	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		LOG(llevError, "Present called outside map.\n");
		return NULL;
	}

	for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above)
	{
		if (tmp->type == type)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Searches for any objects with a matching type variable in the
 * inventory of the given object.
 * @param type Type to search for.
 * @param op Object to search into.
 * @return First matching object, or NULL if none matches. */
object *present_in_ob(uint8 type, object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == type)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Searches for any objects with a matching archetype in the inventory
 * of the given object.
 * @param at Archetype to search for.
 * @param op Where to search.
 * @return First matching object, or NULL if none matches. */
object *present_arch_in_ob(archetype *at, object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->arch == at)
		{
			return tmp;
		}
	}

	return NULL;
}

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
 * @note This only checks to see if there is space for the head of the
 * object - if it is a multispace object, this should be called for all
 * pieces.
 * @todo Document. */
int find_free_spot(archetype *at, object *op, mapstruct *m, int x, int y, int start, int stop)
{
	int i, inx = 0;
	static int altern[SIZEOFFREE];

	for (i = start; i < stop; i++)
	{
		if (!arch_blocked(at, op, m, x + freearr_x[i], y + freearr_y[i]))
		{
			altern[inx++] = i;
		}
		else if (wall(m, x + freearr_x[i], y + freearr_y[i]) && maxfree[i] < stop)
		{
			stop = maxfree[i];
		}
	}

	if (!inx)
	{
		return -1;
	}

	return altern[rndm(1, inx) - 1];
}

/**
 * Works like find_free_spot(), but it will search max number of squares.
 *
 * It will return the first available spot, not a random choice.
 * @todo Document. */
int find_first_free_spot(archetype *at, object *op, mapstruct *m, int x, int y)
{
	int i;

	for (i = 0; i < SIZEOFFREE; i++)
	{
		if (!arch_blocked(at, op, m, x + freearr_x[i], y + freearr_y[i]))
		{
			return i;
		}
	}

	return -1;
}

/**
 * @todo Document. */
int find_first_free_spot2(archetype *at, mapstruct *m, int x, int y, int start, int range)
{
	int i;

	for (i = start; i < range; i++)
	{
		if (!arch_blocked(at, NULL, m, x + freearr_x[i], y + freearr_y[i]))
		{
			return i;
		}
	}

	return -1;
}

/**
 * Randomly permutes an array.
 * @param arr Array to permute.
 * @param begin First index to permute.
 * @param end Second index to permute. */
void permute(int *arr, int begin, int end)
{
	int i, j, tmp, len;

	len = end - begin;

	for (i = begin; i < end; i++)
	{
		j = begin + rndm(1, len) - 1;

		tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
	}
}

/**
 * New function to make monster searching more efficient, and effective!
 * This basically returns a randomized array (in the passed pointer) of
 * the spaces to find monsters. In this way, it won't always look for
 * monsters to the north first. However, the size of the array passed
 * covers all the spaces, so within that size, all the spaces within
 * the 3x3 area will be searched, just not in a predictable order.
 * @param search_arr Array that will be initialized. Must contain at
 * least SIZEOFFREE elements. */
void get_search_arr(int *search_arr)
{
	int i;

	for (i = 0; i < SIZEOFFREE; i++)
	{
		search_arr[i] = i;
	}

	permute(search_arr, 1, SIZEOFFREE1 + 1);
	permute(search_arr, SIZEOFFREE1 + 1, SIZEOFFREE2 + 1);
	permute(search_arr, SIZEOFFREE2 + 1, SIZEOFFREE);
}

/**
 * Computes a direction which you should travel to move of x and y.
 * @param x Delta.
 * @param y Delta.
 * @return Direction */
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
 * Computes an absolute direction.
 * @param d Direction to convert.
 * @return Number between 1 and 8, which represent the "absolute" direction
 * of a number (it actually takes care of "overflow" in previous calculations
 * of a direction). */
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
 * Computes a direction difference.
 * @param dir1 First direction to compare.
 * @param dir2 Second direction to compare.
 * @return How many 45-degrees differences there is between two directions
 * (which are expected to be absolute (see absdir()). */
int dirdiff(int dir1, int dir2)
{
	int d = abs(dir1 - dir2);

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

	if (!get_rangevector(op, target, range_vector, 0))
	{
		return 0;
	}

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
 * Finds out if an object can be picked up.
 * @param who Who is trying to pick up. Can be a monster or a player.
 * @param item Item we're trying to pick up.
 * @return 1 if it can be picked up, 0 otherwise.
 * @note This introduces a weight limitation for monsters. */
int can_pick(object *who, object *item)
{
	if (item->weight <= 0)
	{
		return 0;
	}

	if (QUERY_FLAG(item, FLAG_NO_PICK) && !QUERY_FLAG(item, FLAG_UNPAID))
	{
		return 0;
	}

	if (IS_INVISIBLE(item, who) && !QUERY_FLAG(who, FLAG_SEE_INVISIBLE))
	{
		return 0;
	}

	/* Weight limit for monsters */
	if (who->type != PLAYER && item->weight > (who->weight / 3))
	{
		return 0;
	}

	return 1;
}

/**
 * Create clone from one object to another.
 * @param asrc Object to clone.
 * @return Clone of asrc, including inventory and 'more' body parts. */
object *object_create_clone(object *asrc)
{
	object *dst = NULL, *tmp, *src, *part, *prev, *item;

	if (!asrc)
	{
		return NULL;
	}

	src = asrc;

	if (src->head)
	{
		src = src->head;
	}

	prev = NULL;

	for (part = src; part; part = part->more)
	{
		tmp = get_object();
		copy_object(part, tmp, 0);
		tmp->x -= src->x;
		tmp->y -= src->y;

		if (!part->head)
		{
			dst = tmp;
			tmp->head = NULL;
		}
		else
		{
			tmp->head = dst;
		}

		tmp->more = NULL;

		if (prev)
		{
			prev->more = tmp;
		}

		prev = tmp;
	}

	/* Copy inventory */
	for (item = src->inv; item; item = item->below)
	{
		insert_ob_in_ob(object_create_clone(item), dst);
	}

	return dst;
}

/**
 * Check if object has been destroyed.
 * @param op Object.
 * @param old_tag Object's tag.
 * @return 1 if it was destroyed (removed from map, old_tag does not match
 * object's count or it is freed), 0 otherwise. */
int was_destroyed(object *op, tag_t old_tag)
{
	return op->count != old_tag || OBJECT_FREE(op);
}

/**
 * Creates an object using a string representing its content.
 * @param obstr String to load the object from.
 * @return The newly created object, NULL on failure. */
object *load_object_str(char *obstr)
{
	object *ob = get_object();

	if (!load_object(obstr, ob, NULL, LO_MEMORYMODE, 0))
	{
		LOG(llevBug, "load_object_str(): load_object() failed.");
		return NULL;
	}

	sum_weight(ob);

	return ob;
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

	/* Because auto_apply will be done only *one* time when a new, base
	 * map is loaded, we always clear the flag now. */
	CLEAR_FLAG(op, FLAG_AUTO_APPLY);

	if (op->env && op->env->type == PLAYER)
	{
		LOG(llevDebug, "Object with auto_apply (%s, %s) found in %s.\n", op->name, op->arch->name, op->env->name);
		return 0;
	}

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
				/* Let's give it 10 tries */
				i = 10;
				level = op->stats.exp ? (int) op->stats.exp : get_environment_level(op);

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
			level = op->stats.exp ? (int) op->stats.exp : get_environment_level(op);
			create_treasure(op->randomitems, op, op->map ? GT_ENVIRONMENT : 0, level, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

			/* If we generated on object and put it in this object inventory,
			 * move it to the parent object as the current object is about
			 * to disappear.  An example of this item is the random_* stuff
			 * that is put inside other objects. */
			for (tmp = op->inv; tmp; tmp = tmp2)
			{
				tmp2 = tmp->below;
				object_remove(tmp, 0);

				if (op->env)
				{
					insert_ob_in_ob(tmp, op->env);
				}
			}

			/* No move off needed */
			object_remove(op, 0);
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

	if (!(m = get_map_from_coord(m, &dx, &dy)))
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
key_value *object_get_key_link(const object *ob, const char *key)
{
	key_value *field;

	for (field = ob->key_values; field; field = field->next)
	{
		if (field->key == key)
		{
			return field;
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
const char *object_get_value(const object *op, const char *const key)
{
	key_value *field;
	const char *canonical_key = find_string(key);

	if (canonical_key == NULL)
	{
		return NULL;
	}

	/* This is copied from object_get_key_link() above - only 4 lines, and
	 * saves the function call overhead. */
	for (field = op->key_values; field; field = field->next)
	{
		if (field->key == canonical_key)
		{
			return field->value;
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
static int object_set_value_s(object *op, const char *canonical_key, const char *value, int add_key)
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
			if (object_get_key_link(&op->arch->clone, canonical_key))
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
 * @note This function is merely a wrapper to object_set_value_s() to
 * ensure the key is a shared string.*/
int object_set_value(object *op, const char *key, const char *value, int add_key)
{
	const char *canonical_key = find_string(key);
	int floating_ref = 0, ret;

	if (canonical_key == NULL)
	{
		canonical_key = add_string(key);
		floating_ref = 1;
	}

	ret = object_set_value_s(op, canonical_key, value, add_key);

	if (floating_ref)
	{
		FREE_ONLY_HASH(canonical_key);
	}

	return ret;
}

/**
 * Initialize the table of object initializers. */
void init_object_initializers(void)
{
	object_initializers[BEACON] = beacon_add;
	object_initializers[MAGIC_MIRROR] = magic_mirror_init;
	object_initializers[MAP_INFO] = map_info_init;
	object_initializers[SOUND_AMBIENT] = sound_ambient_init;
}

/**
 * This is a subset of the parse_id command. Basically, name can be a
 * string separated lists of things to match, with certain keywords.
 *
 * Calling function takes care of what action might need to be done and
 * if it is valid (pickup, drop, etc).
 *
 * Brief outline of the procedure:
 *
 * We take apart the name variable into the individual components.
 * cases for 'all' and unpaid are pretty obvious.
 *
 * Next, we check for a count (either specified in name, or in the
 * player object). If count is 1, make a quick check on the name. If
 * count is >1, we need to make plural name.  Return if match.
 *
 * Last, make a check on the full name.
 * @param pl Player (only needed to set count properly).
 * @param op The item we're trying to match.
 * @param name String we're searching.
 * @return Non-zero if we have a match. A higher value means a better
 * match. Zero means no match. */
int item_matched_string(object *pl, object *op, const char *name)
{
	char *cp, local_name[MAX_BUF];
	int count, retval = 0, book_level, book_level2, weapon_type;

	/* strtok is destructive to name */
	strcpy(local_name, name);

	for (cp = strtok(local_name, ","); cp; cp = strtok(NULL, ","))
	{
		/* Get rid of spaces */
		while (cp[0] == ' ')
		{
			cp++;
		}

		/* All is a very generic match - low match value */
		if (!strcasecmp(cp, "all"))
		{
			return 1;
		}

		/* Unpaid is a little more specific */
		if (!strcasecmp(cp, "unpaid") && QUERY_FLAG(op, FLAG_UNPAID))
		{
			return 2;
		}

		if (!strcasecmp(cp, "cursed") && QUERY_FLAG(op, FLAG_IDENTIFIED) && (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			return 2;
		}

		if (!strcasecmp(cp, "unlocked") && !QUERY_FLAG(op, FLAG_INV_LOCKED))
		{
			return 2;
		}

		if (QUERY_FLAG(op, FLAG_IDENTIFIED) && !strcasecmp(cp, "identified"))
		{
			return 2;
		}

		if (!QUERY_FLAG(op, FLAG_IDENTIFIED) && !strcasecmp(cp, "unidentified"))
		{
			return 2;
		}

		if ((op->type == FOOD || op->type == DRINK) && !strcasecmp(cp, "food"))
		{
			return 2;
		}

		if ((op->type == GEM || op->type == JEWEL || op->type == NUGGET || op->type == PEARL) && !strcasecmp(cp, "valuables"))
		{
			return 2;
		}

		if (op->type == WEAPON)
		{
			weapon_type = op->sub_type % 4;

			if (weapon_type == WEAP_1H_IMPACT)
			{
				if (!strcasecmp(cp, "impact weapons"))
				{
					return 2;
				}
			}
			else if (weapon_type == WEAP_1H_SLASH)
			{
				if (!strcasecmp(cp, "slash weapons"))
				{
					return 2;
				}
			}
			else if (weapon_type == WEAP_1H_CLEAVE)
			{
				if (!strcasecmp(cp, "cleave weapons"))
				{
					return 2;
				}
			}
			else if (weapon_type == WEAP_1H_PIERCE)
			{
				if (!strcasecmp(cp, "pierce weapons"))
				{
					return 2;
				}
			}
		}
		else if (op->type == BOOK)
		{
			if (!strcasecmp(cp, "books"))
			{
				return 2;
			}

			if (!op->msg && !strcasecmp(cp, "empty books"))
			{
				return 2;
			}

			if (!QUERY_FLAG(op, FLAG_NO_SKILL_IDENT))
			{
				if (!strcasecmp(cp, "unread books"))
				{
					return 2;
				}

				if (sscanf(cp, "unread level %d books", &book_level) == 1 && op->level == book_level)
				{
					return 2;
				}

				if (sscanf(cp, "unread level %d-%d books", &book_level, &book_level2) == 2 && op->level >= book_level && op->level <= book_level2)
				{
					return 2;
				}
			}
			else
			{
				if (!strcasecmp(cp, "read books"))
				{
					return 2;
				}

				if (sscanf(cp, "read level %d books", &book_level) == 1 && op->level == book_level)
				{
					return 2;
				}

				if (sscanf(cp, "read level %d-%d books", &book_level, &book_level2) == 2 && op->level >= book_level && op->level <= book_level2)
				{
					return 2;
				}
			}
		}

		count = 0;

		/* Allow for things like '100 arrows', but don't accept
		 * strings like '+2', '-1' as numbers. */
		if (isdigit(cp[0]) && (count = atoi(cp)) != 0)
		{
			cp = strchr(cp, ' ');

			/* Get rid of spaces */
			while (cp && cp[0] == ' ')
			{
				cp++;
			}
		}

		if (!cp || cp[0] == '\0' || count < 0)
		{
			return 0;
		}

		/* Base name matched - not bad */
		if (strcasecmp(cp, op->name) == 0 && !count)
		{
			retval = 4;
		}
		/* Need to plurify name for proper match */
		else if (count > 1)
		{
			char newname[MAX_BUF];
			strcpy(newname, op->name);

			if (!strcasecmp(newname, cp))
			{
				retval = 6;
			}
		}
		else if (count == 1)
		{
			if (!strcasecmp(op->name, cp))
			{
				retval = 6;
			}
		}

		if (!strcasecmp(cp, query_name(op, NULL)))
		{
			retval = 20;
		}
		else if (!strcasecmp(cp, query_short_name(op, NULL)))
		{
			retval = 18;
		}
		else if (!strcasecmp(cp, query_base_name(op, pl)))
		{
			retval = 16;
		}
		else if (op->custom_name && !strcasecmp(cp, op->custom_name))
		{
			retval = 15;
		}
		else if (!strncasecmp(cp, query_base_name(op, pl), strlen(cp)))
		{
			retval = 14;
		}
		/* Do substring checks, so things like 'Str+1' will match.
		 * retval of these should perhaps be lower - they are lower
		 * than the specific strcasecmps above, but still higher than
		 * some other match criteria. */
		else if (strstr(query_base_name(op, pl), cp))
		{
			retval = 12;
		}
		else if (strstr(query_short_name(op, NULL), cp))
		{
			retval = 12;
		}
		/* Check for partial custom name, but give a really low priority. */
		else if (op->custom_name && strstr(op->custom_name, cp))
		{
			retval = 3;
		}

		if (retval)
		{
			if (pl->type == PLAYER)
			{
				CONTR(pl)->count = count;
			}

			return retval;
		}
	}

	return 0;
}

/**
 * Get object's gender ID, as defined in @ref GENDER_xxx.
 * @param op Object to get gender ID of.
 * @return The gender ID. */
int object_get_gender(object *op)
{
	if (QUERY_FLAG(op, FLAG_IS_MALE))
	{
		return QUERY_FLAG(op, FLAG_IS_FEMALE) ? GENDER_HERMAPHRODITE : GENDER_MALE;
	}
	else if (QUERY_FLAG(op, FLAG_IS_FEMALE))
	{
		return GENDER_FEMALE;
	}

	return GENDER_NEUTER;
}
