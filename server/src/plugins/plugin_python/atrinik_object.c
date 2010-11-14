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
 * Atrinik Python plugin object related code. */

#include <plugin_python.h>

/**
 * All the possible fields of an object. */
/* @cparser
 * @page plugin_python_object_fields Python object fields
 * <h2>Python object fields</h2>
 * List of the object fields and their meaning. */
static fields_struct fields[] =
{
	{"below", FIELDTYPE_OBJECT, offsetof(object, below), FIELDFLAG_READONLY, 0},
	{"above", FIELDTYPE_OBJECT, offsetof(object, above), FIELDFLAG_READONLY, 0},
	{"inv", FIELDTYPE_OBJECT, offsetof(object, inv), FIELDFLAG_READONLY, 0},
	{"env", FIELDTYPE_OBJECT, offsetof(object, env), FIELDFLAG_READONLY, 0},
	{"map", FIELDTYPE_MAP, offsetof(object, map), FIELDFLAG_READONLY, 0},
	{"name", FIELDTYPE_SHSTR, offsetof(object, name), FIELDFLAG_PLAYER_READONLY, 0},
	{"custom_name", FIELDTYPE_SHSTR, offsetof(object, custom_name), 0, 0},
	{"title", FIELDTYPE_SHSTR, offsetof(object, title), 0, 0},
	{"race", FIELDTYPE_SHSTR, offsetof(object, race), 0, 0},
	{"slaying", FIELDTYPE_SHSTR, offsetof(object, slaying), 0, 0},
	{"msg", FIELDTYPE_SHSTR, offsetof(object, msg), 0, 0},
	{"artifact", FIELDTYPE_SHSTR, offsetof(object, artifact), 0, 0},
	{"weight", FIELDTYPE_SINT32, offsetof(object, weight), 0, 0},

	{"weight_limit", FIELDTYPE_UINT32, offsetof(object, weight_limit), 0, 0},
	{"carrying", FIELDTYPE_SINT32, offsetof(object, carrying), 0, 0},
	{"path_attuned", FIELDTYPE_UINT32, offsetof(object, path_attuned), 0, 0},
	{"path_repelled", FIELDTYPE_UINT32, offsetof(object, path_repelled), 0, 0},
	{"path_denied", FIELDTYPE_UINT32, offsetof(object, path_denied), 0, 0},
	{"value", FIELDTYPE_SINT64, offsetof(object, value), 0, 0},
	{"nrof", FIELDTYPE_UINT32, offsetof(object, nrof), 0, 0},

	{"enemy", FIELDTYPE_OBJECTREF, offsetof(object, enemy), FIELDFLAG_PLAYER_READONLY, offsetof(object, enemy_count)},
	{"attacked_by", FIELDTYPE_OBJECTREF, offsetof(object, attacked_by), FIELDFLAG_READONLY, offsetof(object, attacked_by_count)},
	{"owner", FIELDTYPE_OBJECTREF, offsetof(object, owner), FIELDFLAG_READONLY, offsetof(object, ownercount)},

	{"x", FIELDTYPE_SINT16, offsetof(object, x), FIELDFLAG_READONLY, 0},
	{"y", FIELDTYPE_SINT16, offsetof(object, y), FIELDFLAG_READONLY, 0},
	{"attacked_by_distance", FIELDTYPE_SINT16, offsetof(object, attacked_by_distance), 0, 0},
	{"last_damage", FIELDTYPE_UINT16, offsetof(object, last_damage), 0, 0},
	{"terrain_type", FIELDTYPE_UINT16, offsetof(object, terrain_type), 0, 0},
	{"terrain_flag", FIELDTYPE_UINT16, offsetof(object, terrain_flag), 0, 0},
	{"material", FIELDTYPE_UINT16, offsetof(object, material), 0, 0},
	{"material_real", FIELDTYPE_SINT16, offsetof(object, material_real), 0, 0},

	{"last_heal", FIELDTYPE_SINT16, offsetof(object, last_heal), 0, 0},
	{"last_sp", FIELDTYPE_SINT16, offsetof(object, last_sp), 0, 0},
	{"last_grace", FIELDTYPE_SINT16, offsetof(object, last_grace), 0, 0},
	{"last_eat", FIELDTYPE_SINT16, offsetof(object, last_eat), 0, 0},

	{"animation_id", FIELDTYPE_UINT16, offsetof(object, animation_id), 0, 0},
	{"inv_animation_id", FIELDTYPE_UINT16, offsetof(object, inv_animation_id), 0, 0},
	{"magic", FIELDTYPE_SINT8, offsetof(object, magic), 0, 0},
	{"state", FIELDTYPE_UINT8, offsetof(object, state), 0, 0},
	{"level", FIELDTYPE_SINT8, offsetof(object, level), FIELDFLAG_PLAYER_READONLY, 0},
	{"direction", FIELDTYPE_SINT8, offsetof(object, direction), 0, 0},
	{"facing", FIELDTYPE_SINT8, offsetof(object, facing), 0, 0},
	{"quick_pos", FIELDTYPE_UINT8, offsetof(object, quick_pos), 0, 0},
	{"quickslot", FIELDTYPE_UINT8, offsetof(object, quickslot), FIELDFLAG_READONLY, 0},

	{"type", FIELDTYPE_UINT8, offsetof(object, type), 0, 0},
	{"sub_type", FIELDTYPE_UINT8, offsetof(object, sub_type), 0, 0},
	{"item_quality", FIELDTYPE_UINT8, offsetof(object, item_quality), 0, 0},
	{"item_condition", FIELDTYPE_UINT8, offsetof(object, item_condition), 0, 0},
	{"item_race", FIELDTYPE_UINT8, offsetof(object, item_race), 0, 0},
	{"item_level", FIELDTYPE_UINT8, offsetof(object, item_level), 0, 0},
	{"item_skill", FIELDTYPE_UINT8, offsetof(object, item_skill), 0, 0},
	{"glow_radius", FIELDTYPE_SINT8, offsetof(object, glow_radius), 0, 0},
	{"move_status", FIELDTYPE_SINT8, offsetof(object, move_status), 0, 0},
	{"move_type", FIELDTYPE_UINT8, offsetof(object, move_type), 0, 0},

	{"anim_enemy_dir", FIELDTYPE_SINT8, offsetof(object, anim_enemy_dir), 0, 0},
	{"anim_moving_dir", FIELDTYPE_SINT8, offsetof(object, anim_moving_dir), 0, 0},
	{"anim_enemy_dir_last", FIELDTYPE_SINT8, offsetof(object, anim_enemy_dir_last), 0, 0},
	{"anim_moving_dir_last", FIELDTYPE_SINT8, offsetof(object, anim_moving_dir_last), 0, 0},
	{"anim_last_facing", FIELDTYPE_SINT8, offsetof(object, anim_last_facing), 0, 0},
	{"anim_last_facing_last", FIELDTYPE_SINT8, offsetof(object, anim_last_facing_last), 0, 0},
	{"anim_speed", FIELDTYPE_UINT8, offsetof(object, anim_speed), 0, 0},
	{"last_anim", FIELDTYPE_UINT8, offsetof(object, last_anim), 0, 0},
	{"behavior", FIELDTYPE_UINT8, offsetof(object, behavior), 0, 0},
	{"run_away", FIELDTYPE_UINT8, offsetof(object, run_away), 0, 0},

	{"layer", FIELDTYPE_UINT8, offsetof(object, layer), 0, 0},
	{"speed", FIELDTYPE_FLOAT, offsetof(object, speed), FIELDFLAG_PLAYER_READONLY, 0},
	{"speed_left", FIELDTYPE_FLOAT, offsetof(object, speed_left), 0, 0},
	{"weapon_speed", FIELDTYPE_FLOAT, offsetof(object, weapon_speed), 0, 0},
	{"weapon_speed_left", FIELDTYPE_FLOAT, offsetof(object, weapon_speed_left), 0, 0},
	{"weapon_speed_add", FIELDTYPE_FLOAT, offsetof(object, weapon_speed_add), 0, 0},
	{"exp", FIELDTYPE_SINT64, offsetof(object, stats.exp), 0, 0},

	{"hp", FIELDTYPE_SINT32, offsetof(object, stats.hp), 0, 0},
	{"maxhp", FIELDTYPE_SINT32, offsetof(object, stats.maxhp), FIELDFLAG_PLAYER_READONLY, 0},
	{"sp", FIELDTYPE_SINT16, offsetof(object, stats.sp), 0, 0},
	{"maxsp", FIELDTYPE_SINT16, offsetof(object, stats.maxsp), FIELDFLAG_PLAYER_READONLY, 0},
	{"grace", FIELDTYPE_SINT16, offsetof(object, stats.grace), 0, 0},
	{"maxgrace", FIELDTYPE_SINT16, offsetof(object, stats.maxgrace), FIELDFLAG_PLAYER_READONLY, 0},

	{"food", FIELDTYPE_SINT16, offsetof(object, stats.food), 0, 0},
	{"dam", FIELDTYPE_SINT16, offsetof(object, stats.dam), FIELDFLAG_PLAYER_READONLY, 0},
	{"wc", FIELDTYPE_SINT16, offsetof(object, stats.wc), FIELDFLAG_PLAYER_READONLY, 0},
	{"ac", FIELDTYPE_SINT16, offsetof(object, stats.ac), FIELDFLAG_PLAYER_READONLY, 0},
	{"wc_range", FIELDTYPE_UINT8, offsetof(object, stats.wc_range), 0, 0},

	{"Str", FIELDTYPE_SINT8, offsetof(object, stats.Str), FIELDFLAG_PLAYER_FIX, 0},
	{"Dex", FIELDTYPE_SINT8, offsetof(object, stats.Dex), FIELDFLAG_PLAYER_FIX, 0},
	{"Con", FIELDTYPE_SINT8, offsetof(object, stats.Con), FIELDFLAG_PLAYER_FIX, 0},
	{"Wis", FIELDTYPE_SINT8, offsetof(object, stats.Wis), FIELDFLAG_PLAYER_FIX, 0},
	{"Cha", FIELDTYPE_SINT8, offsetof(object, stats.Cha), FIELDFLAG_PLAYER_FIX, 0},
	{"Int", FIELDTYPE_SINT8, offsetof(object, stats.Int), FIELDFLAG_PLAYER_FIX, 0},
	{"Pow", FIELDTYPE_SINT8, offsetof(object, stats.Pow), FIELDFLAG_PLAYER_FIX, 0},

	{"arch", FIELDTYPE_ARCH, offsetof(object, arch), 0, 0},
	{"terrain_flag", FIELDTYPE_UINT16, offsetof(object, terrain_flag), 0, 0},
	{"z", FIELDTYPE_SINT16, offsetof(object, z), 0, 0},
	{"zoom", FIELDTYPE_UINT8, offsetof(object, zoom), 0, 0},
	{"align", FIELDTYPE_SINT16, offsetof(object, align), 0, 0},
	{"alpha", FIELDTYPE_UINT8, offsetof(object, alpha), 0, 0}
};
/* @endcparser */

/**
 * @defgroup plugin_python_object_functions Python object functions
 * Object related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>object.GetSkill(int type, int id)</h1>
 * Fetch a skill or exp_skill object from the specified object.
 * @param type Type of the object to look for. Unused.
 * @param id ID of the skill or experience object.
 * @return The object if found.
 * @deprecated Use @ref Atrinik_Player_GetSkill "player.GetSkill()" instead. */
static PyObject *Atrinik_Object_GetSkill(Atrinik_Object *whoptr, PyObject *args)
{
	object *tmp;
	int type, id;

	PyErr_WarnEx(PyExc_DeprecationWarning, "object.GetSkill() is deprecated; use player.GetSkill() instead.", 1);

	if (!PyArg_ParseTuple(args, "ii", &type, &id))
	{
		return NULL;
	}

	/* Browse the inventory of object to find a matching skill or exp_obj. */
	for (tmp = WHO->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type != type)
		{
			continue;
		}

		if (tmp->type == SKILL && tmp->stats.sp == id)
		{
			return wrap_object(tmp);
		}

		if (tmp->type == EXPERIENCE && tmp->sub_type == id)
		{
			return wrap_object(tmp);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetSkill(int type, int skillid, long level, long value)</h1>
 * Set object's experience in the skill to a new value.
 *
 * Also can change the level of a skill.
 * @param type Type of the skill, should be TYPE_SKILL
 * @param skillid ID of the skill
 * @param level Level to set for the skill. Must be non zero to set,
 * otherwise experience is set instead.
 * @param value Experience to set for the skill. Only set if level is
 * lower than 1.
 * @deprecated Use @ref Atrinik_Player_AddExp "player.AddExp()" instead. */
static PyObject *Atrinik_Object_SetSkill(Atrinik_Object *whoptr, PyObject *args)
{
	object *tmp;
	int type, skill, currentxp;
	long level, value;

	PyErr_WarnEx(PyExc_DeprecationWarning, "object.SetSkill() is deprecated; use player.AddExp() instead.", 1);

	if (!PyArg_ParseTuple(args, "iill", &type, &skill, &level, &value))
	{
		return NULL;
	}

	/* We don't set anything in exp_obj types */
	if (type != SKILL)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	/* Browse the inventory of object to find a matching skill. */
	for (tmp = WHO->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == type && tmp->stats.sp == skill)
		{
			/*LOG(-1,"LEVEL1 %d (->%d) :: %s (exp %d)\n",tmp->level,level,hooks->query_name(tmp), tmp->stats.exp);*/

			/* This is a bit tricky: some skills are marked with exp -1
			 * or -2 as special used skills (level but no exp):
			 * if we have here a level > 0, we set level but NEVER exp.
			 * if we have level == 0, we only set exp - the
			 * addexp */
			if (level > 0)
			{
				tmp->level = level;
			}
			else
			{
				currentxp = tmp->stats.exp;
				value = value - currentxp;

				hooks->add_exp(WHO, value, skill, 0);
			}

			/*LOG(-1,"LEVEL2 %d (->%d) :: %s (exp %d)\n",tmp->level,level,hooks->query_name(tmp), tmp->stats.exp);*/

			/* We will sure change skill exp, mark for update */
			if (WHO->type == PLAYER && CONTR(WHO))
			{
				CONTR(WHO)->update_skills = 1;
			}

			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	RAISE("Unknown skill");
}

/**
 * <h1>object.ActivateRune(object who)</h1>
 * Activate a rune.
 * @param who Who should be affected by the effects of the rune.
 * @throws TypeError if 'object' is not of type @ref RUNE "TYPE_RUNE". */
static PyObject *Atrinik_Object_ActivateRune(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *who;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &who))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(who);

	if (obj->obj->type != RUNE)
	{
		PyErr_SetString(PyExc_TypeError, "object.ActivateRune(): 'object' is not a rune.");
		return NULL;
	}

	hooks->spring_trap(obj->obj, who->obj);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetGod()</h1>
 * Determine the object's god.
 * @return Returns a string of the god's name. */
static PyObject *Atrinik_Object_GetGod(Atrinik_Object *whoptr, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("s", hooks->determine_god(WHO));
}

/**
 * <h1>object.SetGod(string name)</h1>
 * Make an object become follower of a different god.
 *
 * The object must have the 'divine prayers' skill.
 * @param name Name of the god. */
static PyObject *Atrinik_Object_SetGod(Atrinik_Object *whoptr, PyObject *args)
{
	const char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (hooks->command_rskill(WHO, "divine prayers"))
	{
		object *god = hooks->find_god(name);

		hooks->become_follower(WHO, god);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.TeleportTo(string map, int x, int y, int [unique = False], int [sound = True])</h1>
 * Teleport object to the given position of map.
 * @param path Map path to teleport the object to.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @param unique If True, the destination will be unique map for the player.
 * @param sound If False, will not play a sound effect. */
static PyObject *Atrinik_Object_TeleportTo(Atrinik_Object *whoptr, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"path", "x", "y", "unique", "sound", NULL};
	const char *path;
	object *tmp;
	int x, y, unique = 0, sound = 1;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "sii|ii", kwlist, &path, &x, &y, &unique, &sound))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	tmp = hooks->get_object();
	FREE_AND_COPY_HASH(EXIT_PATH(tmp), path);
	EXIT_X(tmp) = x;
	EXIT_Y(tmp) = y;

	if (unique)
	{
		tmp->last_eat = MAP_PLAYER_MAP;
	}

	hooks->enter_exit(WHO, tmp);

	if (WHO->map && sound)
	{
		hooks->play_sound_map(WHO->map, CMD_SOUND_EFFECT, "teleport.ogg", WHO->x, WHO->y, 0, 0);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.InsertInto(object where)</h1>
 * Insert 'object' into 'where'.
 * @param where Where to insert 'object'. */
static PyObject *Atrinik_Object_InsertInto(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *where;
	object *tmp;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &where))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(where);

	if (!QUERY_FLAG(obj->obj, FLAG_REMOVED))
	{
		hooks->object_remove_esrv_update(obj->obj);
	}

	tmp = hooks->insert_ob_in_ob(obj->obj, where->obj);

	if (tmp)
	{
		object *pl = hooks->object_need_esrv_update(tmp);

		if (pl)
		{
			hooks->esrv_send_item(pl, tmp);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Apply(object what, int flags)</h1>
 * Forces object to apply what.
 * @param what What object to apply.
 * @param flags Reasonable combination of the following:
 * - <b>Atrinik.APPLY_TOGGLE</b>: Normal apply (toggle)
 * - <b>Atrinik.APPLY_ALWAYS</b>: Always apply (never unapply)
 * - <b>Atrinik.UNAPPLY_ALWAYS</b>: Always unapply (never apply)
 * - <b>Atrinik.UNAPPLY_NOMERGE</b>: Don't merge unapplied items
 * - <b>Atrinik.UNAPPLY_IGNORE_CURSE</b>: Unapply cursed items
 * @return Return values:
 * - <b>0</b>: Object cannot apply objects of that type.
 * - <b>1</b>: Object was applied, or not...
 * - <b>2</b>: Object must be in inventory to be applied. */
static PyObject *Atrinik_Object_Apply(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int flags;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &whatptr, &flags))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(whatptr);

	return Py_BuildValue("i", hooks->manual_apply(WHO, WHAT, flags));
}

/**
 * <h1>object.Take(object what)</h1>
 * Force 'object' to pick up 'what'.
 * @param what The object to pick up. If a string, this is equivalent of
 * /take command.
 * @throws TypeError if 'what' is neither an Atrinik object nor a string. */
static PyObject *Atrinik_Object_Take(Atrinik_Object *obj, PyObject *what)
{
	OBJEXISTCHECK(obj);

	if (PyObject_TypeCheck(what, &Atrinik_ObjectType))
	{
		OBJEXISTCHECK((Atrinik_Object *) what);
		hooks->pick_up(obj->obj, ((Atrinik_Object *) what)->obj, 0);
	}
	else if (PyString_Check(what))
	{
		hooks->command_take(obj->obj, PyString_AsString(what));
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "object.Take(): Argument 'what' must be either Atrinik object or string.");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Drop(object what)</h1>
 * Make 'object' drop 'what'.
 * @param what Object to drop. If a string, this is equivalent of /drop
 * command.
 * @throws TypeError if 'what' is neither an Atrinik object nor a string. */
static PyObject *Atrinik_Object_Drop(Atrinik_Object *obj, PyObject *what)
{
	OBJEXISTCHECK(obj);

	if (PyObject_TypeCheck(what, &Atrinik_ObjectType))
	{
		OBJEXISTCHECK((Atrinik_Object *) what);
		hooks->drop(obj->obj, ((Atrinik_Object *) what)->obj, 0);
	}
	else if (PyString_Check(what))
	{
		hooks->command_drop(obj->obj, PyString_AsString(what));
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "object.Drop(): Argument 'what' must be either Atrinik object or string.");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Communicate(string message)</h1>
 * Object says message to everybody on its map. Emote commands may be used.
 * @param message The message to say. */
static PyObject *Atrinik_Object_Communicate(Atrinik_Object *whoptr, PyObject *args)
{
	char *message;

	if (!PyArg_ParseTuple(args, "s", &message))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	hooks->communicate(WHO, message);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Say(string message, int [mode = 0])</h1>
 * Object says message to everybody on its map.
 * @param message The message to say.
 * @param mode If set to non-zero, message is not prefixed with
 * "object.name says: ". */
static PyObject *Atrinik_Object_Say(Atrinik_Object *whoptr, PyObject *args)
{
	const char *message;
	char buf[HUGE_BUF];
	int mode = 0;

	if (!PyArg_ParseTuple(args, "s|i", &message, &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (mode)
	{
		hooks->new_info_map(NDI_NAVY | NDI_UNIQUE, WHO->map, WHO->x, WHO->y, MAP_INFO_NORMAL, message);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s says: %s", hooks->query_name(WHO, NULL), message);
		hooks->new_info_map(NDI_NAVY | NDI_UNIQUE, WHO->map, WHO->x, WHO->y, MAP_INFO_NORMAL, buf);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SayTo(object target, string message, int [mode = 0])</h1>
 * NPC talks only to player but map gets "xxx talks to yyy" msg too.
 * @param target Target object the NPC is talking to.
 * @param message The message to say.
 * @param mode If set to non-zero, there is no "xxx talks to yyy" map
 * message. The message is not prefixed with "xxx says: " either. */
static PyObject *Atrinik_Object_SayTo(Atrinik_Object *whoptr, PyObject *args)
{
	object *target;
	Atrinik_Object *obptr2;
	const char *message;
	int mode = 0;

	if (!PyArg_ParseTuple(args, "O!s|i", &Atrinik_ObjectType, &obptr2, &message, &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(obptr2);

	target = obptr2->obj;

	if (mode)
	{
		hooks->new_draw_info(NDI_NAVY | NDI_UNIQUE, target, message);
	}
	else
	{
		char buf[HUGE_BUF];

		snprintf(buf, sizeof(buf), "%s talks to %s.", hooks->query_name(WHO, NULL), hooks->query_name(target, NULL));
		hooks->new_info_map_except(NDI_UNIQUE, WHO->map, WHO->x, WHO->y, MAP_INFO_NORMAL, WHO, target, buf);

		snprintf(buf, sizeof(buf), "\n%s says: %s", hooks->query_name(WHO, NULL), message);
		hooks->new_draw_info(NDI_NAVY | NDI_UNIQUE, target, buf);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Write(string message, int color)</h1>
 * Writes a message to a specific player object.
 * @param message The message to write.
 * @param color Color to write the message in. Defaults to orange. */
static PyObject *Atrinik_Object_Write(Atrinik_Object *whoptr, PyObject *args)
{
	int color = NDI_UNIQUE | NDI_ORANGE;
	const char *message;

	if (!PyArg_ParseTuple(args, "s|i", &message, &color))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	hooks->new_draw_info(color, WHO, message);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetGender()</h1>
 * Get an object's gender.
 * @retval NEUTER No gender.
 * @retval MALE Male.
 * @retval FEMALE Female.
 * @retval HERMAPHRODITE Both male and female. */
static PyObject *Atrinik_Object_GetGender(Atrinik_Object *whoptr, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("i", hooks->object_get_gender(WHO));
}

/**
 * <h1>object.SetGender(int gender)</h1>
 * Changes the gender of object.
 * @param gender The new gender to set. One of:
 * - <b>NEUTER</b>: No gender.
 * - <b>MALE</b>: Male.
 * - <b>FEMALE</b>: Female.
 * - <b>HERMAPHRODITE</b>: Both male and female. */
static PyObject *Atrinik_Object_SetGender(Atrinik_Object *obj, PyObject *args)
{
	int gender;

	if (!PyArg_ParseTuple(args, "i", &gender))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	/* Set object to neuter */
	CLEAR_FLAG(obj->obj, FLAG_IS_MALE);
	CLEAR_FLAG(obj->obj, FLAG_IS_FEMALE);

	if (gender == GENDER_MALE || gender == GENDER_HERMAPHRODITE)
	{
		SET_FLAG(obj->obj, FLAG_IS_MALE);
	}

	if (gender == GENDER_FEMALE || gender == GENDER_HERMAPHRODITE)
	{
		SET_FLAG(obj->obj, FLAG_IS_FEMALE);
	}

	/* Update the players client if object was a player */
	if (obj->obj->type == PLAYER)
	{
		CONTR(obj->obj)->socket.ext_title_flag = 1;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetGuildForce(string rank_string)</h1>
 *
 * Set the current rank of object to rank_string.
 *
 * This only sets the title. The guild tag is in slaying field of the
 * force object.
 *
 * To test for guild force, use
 * @ref Atrinik_Object_GetGuildForce "Atrinik.GetGuildForce()".
 *
 * To set the guild tag you can use this function, because it returns the
 * guild force object after setting the title.
 *
 * @param rank_string The rank string in the guild to set.
 * @return The guild force object if found. */
static PyObject *Atrinik_Object_SetGuildForce(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;
	const char *guild;

	if (!PyArg_ParseTuple(args, "s", &guild))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name == hooks->shstr_cons->GUILD_FORCE && walk->arch->name == hooks->shstr_cons->guild_force)
		{
			/* We find the rank of the player, now change it to new one */
			if (walk->title)
			{
				FREE_AND_CLEAR_HASH(walk->title);
			}

			if (guild && strcmp(guild, ""))
			{
				FREE_AND_COPY_HASH(walk->title, guild);
			}

			/* Demand update to client */
			CONTR(WHO)->socket.ext_title_flag = 1;

			return wrap_object(walk);
		}
	}

	LOG(llevDebug, "Python Warning:: SetGuildForce: Object %s has no guild_force!\n", hooks->query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetGuildForce()</h1>
 * Get the guild force from player's inventory.
 * @return The guild force object if found. */
static PyObject *Atrinik_Object_GetGuildForce(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;

	(void) args;
	OBJEXISTCHECK(whoptr);

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name == hooks->shstr_cons->GUILD_FORCE && walk->arch->name == hooks->shstr_cons->guild_force)
		{
			return wrap_object(walk);
		}
	}

	LOG(llevDebug, "Python Warning:: GetGuildForce: Object %s has no guild_force!\n", hooks->query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Fix()</h1>
 * Recalculate player's or monster's stats depending on equipment, forces
 * skills, etc. */
static PyObject *Atrinik_Object_Fix(Atrinik_Object *whoptr, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(whoptr);

	hooks->fix_player(WHO);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Kill(object what, int how)</h1>
 * Kill an object.
 * @param what The object to kill
 * @param how How to kill the object.
 * @warning Untested. */
static PyObject *Atrinik_Object_Kill(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int ktype;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &whatptr, &ktype))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(whatptr);

	WHAT->speed = 0;
	WHAT->speed_left = 0.0;
	hooks->update_ob_speed(WHAT);

	if (QUERY_FLAG(WHAT, FLAG_REMOVED))
	{
		LOG(llevDebug, "Warning (from KillObject): Trying to remove removed object\n");
		RAISE("Trying to remove removed object");
	}
	else
	{
		WHAT->stats.hp = -1;
		hooks->kill_object(WHAT, 1, WHO, ktype);
	}

	/* This is to avoid the attack routine to continue after we called
	 * killObject, since the attacked object no longer exists.
	 * By fixing guile_current_other to NULL, guile_use_weapon_script will
	 * return -1, meaning the attack function must be immediately terminated. */
	if (WHAT == current_context->other)
	{
		current_context->other = NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CastAbility(object target, int spellno, int mode, int direction, string option)</h1>
 * Object casts the ability numbered spellno on target.
 * @param target The target object.
 * @param spellno ID of the spell to cast.
 * @param mode Atrinik.CAST_NORMAL or Atrinik.CAST_POTION.
 * @param direction The direction to cast the ability in.
 * @param option Additional string option(s). */
static PyObject *Atrinik_Object_CastAbility(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *target;
	int spell, dir, mode;
	SpellTypeFrom item;
	char *stringarg;

	if (!PyArg_ParseTuple(args, "O!iiis", &Atrinik_ObjectType, &target, &spell, &mode, &dir, &stringarg))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(target);

	if (WHO->type != PLAYER)
	{
		item = spellNPC;
	}
	else
	{
		if (!mode)
		{
			item = spellNormal;
		}
		else
		{
			item = spellPotion;
		}
	}

	hooks->cast_spell(target->obj, WHO, dir, spell, 1, item, stringarg);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.DoKnowSpell(int spell)</h1>
 * Check if object knowns a given spell.
 * @param spell ID of the spell to check for.
 * @return 1 if the object knows the spell, 0 otherwise. */
static PyObject *Atrinik_Object_DoKnowSpell(Atrinik_Object *whoptr, PyObject *args)
{
	int spell;

	if (!PyArg_ParseTuple(args, "i", &spell))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("i", hooks->check_spell_known(WHO, spell));
}

/**
 * <h1>object.AcquireSpell(int spell, int mode)</h1>
 * Object will learn or unlearn spell, depending on the mode.
 * @param spell ID of the spell to learn/unlearn.
 * @param mode Possible modes:
 * - <b>Atrinik.LEARN</b>: Learn the spell.
 * - <b>Atrinik.UNLEARN</b>: Unlearn the spell. */
static PyObject *Atrinik_Object_AcquireSpell(Atrinik_Object *whoptr, PyObject *args)
{
	int spell, mode;

	if (!PyArg_ParseTuple(args, "ii", &spell, &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (mode)
	{
		hooks->do_forget_spell(WHO, spell);
	}
	else
	{
		hooks->do_learn_spell(WHO, spell, 0);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.DoKnowSkill(int skill)</h1>
 * Check if object knowns a given skill.
 * @param skill ID of the skill to check for.
 * @return 1 if the object knows the skill, 0 otherwise. */
static PyObject *Atrinik_Object_DoKnowSkill(Atrinik_Object *whoptr, PyObject *args)
{
	int skill;

	if (!PyArg_ParseTuple(args, "i", &skill))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("i", hooks->check_skill_known(WHO, skill));
}

/**
 * <h1>object.AcquireSkill(int skillno)</h1>
 * Object will learn or unlearn skill.
 * @param skillno ID of the skill to learn/unlearn. */
static PyObject *Atrinik_Object_AcquireSkill(Atrinik_Object *whoptr, PyObject *args)
{
	int skill;

	if (!PyArg_ParseTuple(args, "i", &skill))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	hooks->learn_skill(WHO, NULL, NULL, skill, 0);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.FindMarkedObject()</h1>
 * Find marked object in object's inventory.
 * @return The marked object, or None if no object is marked. */
static PyObject *Atrinik_Object_FindMarkedObject(Atrinik_Object *whoptr, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(whoptr);

	return wrap_object(hooks->find_marked_object(WHO));
}

/**
 * <h1>object.CreatePlayerForce(string force_name, int time)</h1>
 * Creates and inserts an invisible player force in object.
 *
 * The values of the force will affect the object it is in, which should
 * usually be player.
 * @param force_name Name of the player force
 * @param time If non-zero, the force will be removed again after
 * time / 0.02 ticks. Optional, defaults to 0.
 * @return The new player force object. */
static PyObject *Atrinik_Object_CreatePlayerForce(Atrinik_Object *whereptr, PyObject *args)
{
	const char *txt;
	object *myob;
	int time = 0;

	if (!PyArg_ParseTuple(args, "s|i", &txt, &time))
	{
		return NULL;
	}

	OBJEXISTCHECK(whereptr);

	myob = hooks->get_archetype("player_force");

	if (!myob)
	{
		LOG(llevDebug, "Python WARNING:: CreatePlayerForce: Can't find archetype 'player_force'\n");
		RAISE("Can't find archetype 'player_force'");
	}

	/* For temporary forces */
	if (time > 0)
	{
		SET_FLAG(myob, FLAG_IS_USED_UP);
		myob->stats.food = time;
		myob->speed = 0.02f;
		hooks->update_ob_speed(myob);
	}

	/* Setup the force and put it in activator */
	if (myob->name)
	{
		FREE_AND_CLEAR_HASH(myob->name);
	}

	FREE_AND_COPY_HASH(myob->name, txt);
	myob = hooks->insert_ob_in_ob(myob, WHERE);

	hooks->esrv_send_item(WHERE, myob);

	return wrap_object(myob);
}

/**
 * <h1>object.GetQuestObject(string quest_name)</h1>
 * Get a quest object for specified quest.
 * @param quest_name Name of the quest to look for.
 * @return The quest object if found. */
static PyObject *Atrinik_Object_GetQuestObject(Atrinik_Object *whoptr, PyObject *args)
{
	const char *quest_name;
	object *walk;

	if (!PyArg_ParseTuple(args, "s", &quest_name))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = CONTR(WHO)->quest_container->inv; walk; walk = walk->below)
	{
		if (!strcmp(walk->name, quest_name))
		{
			return wrap_object(walk);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.StartQuest(string quest_name)</h1>
 * Create a quest object inside the specified object, starting a new
 * quest.
 * @param quest_name Name of the quest.
 * @return The newly created quest object. */
static PyObject *Atrinik_Object_StartQuest(Atrinik_Object *whoptr, PyObject *args)
{
	object *quest_object;
	const char *quest_name;

	if (!PyArg_ParseTuple(args, "s", &quest_name))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	quest_object = hooks->get_archetype(QUEST_CONTAINER_ARCHETYPE);
	quest_object->magic = 0;
	FREE_AND_COPY_HASH(quest_object->name, quest_name);
	hooks->insert_ob_in_ob(quest_object, CONTR(WHO)->quest_container);

	return wrap_object(quest_object);
}

/**
 * <h1>object.CreatePlayerInfo(string name)</h1>
 * Creates a player info object of specified name in object's inventory.
 *
 * The values of a player info object will NOT affect the object it is
 * in.
 * @param name Name of the player info
 * @return The new player info object */
static PyObject *Atrinik_Object_CreatePlayerInfo(Atrinik_Object *whereptr, PyObject *args)
{
	const char *txt;
	object *myob;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	OBJEXISTCHECK(whereptr);

	myob = hooks->get_archetype("player_info");

	if (!myob)
	{
		LOG(llevDebug, "Python WARNING:: CreatePlayerInfo: Cant't find archtype 'player_info'\n");
		RAISE("Cant't find archtype 'player_info'");
	}

	/* Setup the info and put it in activator */
	if (myob->name)
	{
		FREE_AND_CLEAR_HASH(myob->name);
	}

	FREE_AND_COPY_HASH(myob->name, txt);
	myob = hooks->insert_ob_in_ob(myob, WHERE);

	hooks->esrv_send_item(WHERE, myob);

	return wrap_object(myob);
}

/**
 * <h1>object.GetPlayerInfo(string name)</h1>
 * Get the first player info object with the specified name in object's
 * inventory.
 * @param name Name of the player info
 * @return The player info object if found, None otherwise. */
static PyObject *Atrinik_Object_GetPlayerInfo(Atrinik_Object *whoptr, PyObject *args)
{
	const char *name;
	object *walk;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	/* Get the first linked player info arch in this inventory */
	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && walk->arch->name == hooks->shstr_cons->player_info && !strcmp(walk->name, name))
		{
			return wrap_object(walk);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetNextPlayerInfo(object player_info)</h1>
 * Get next player info object in object's inventory with same name as
 * player_info.
 * @param player_info Previously found player info object.
 * @return The next player info object if found, None otherwise.
 * @todo Remove? */
static PyObject *Atrinik_Object_GetNextPlayerInfo(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *myob;
	char name[128];
	object *walk;

	(void) whoptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &myob))
	{
		return NULL;
	}

	OBJEXISTCHECK(myob);

	/* Our check paramters: arch "force_info", name of this arch */
	strncpy(name, STRING_OBJ_NAME(myob->obj), 127);
	name[63] = '\0';

	/* Get the next linked player_info arch in this inventory */
	for (walk = myob->obj->below; walk != NULL; walk = walk->below)
	{
		if (walk->name && walk->arch->name == hooks->shstr_cons->player_info && !strcmp(walk->name, name))
		{
			return wrap_object(walk);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CreateForce(string id)</h1>
 * Create a force object in object's inventory.
 * @param id String ID of the force object.
 * @return The created invisible force object. */
static PyObject *Atrinik_Object_CreateForce(Atrinik_Object *obj, PyObject *args)
{
	const char *id;
	object *force;

	if (!PyArg_ParseTuple(args, "s", &id))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	force = hooks->get_archetype("force");
	force->speed = 0.0;
	hooks->update_ob_speed(force);
	FREE_AND_COPY_HASH(force->slaying, id);
	FREE_AND_COPY_HASH(force->name, id);
	force = hooks->insert_ob_in_ob(force, obj->obj);

	return wrap_object(force);
}

/**
 * <h1>object.CreateObject(string archname, int [nrof = 1], int [value = -1], bool [identified = True])</h1>
 * Creates an object from archname and inserts it into 'object'.
 * @param archname Name of the arch to create.
 * @param nrof Number of objects to create.
 * @param value If not -1, will be used as value for the new object.
 * @param identified If False the object will not be identified.
 * @throws AtrinikError if 'archname' references an invalid archetype.
 * @return The created object. */
static PyObject *Atrinik_Object_CreateObject(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"archname", "nrof", "value", "identified", NULL};
	const char *archname;
	uint32 nrof = 1;
	sint64 value = -1;
	int identified = 1;
	archetype *at;
	object *tmp, *env;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|ILi", kwlist, &archname, &nrof, &value, &identified))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	at = hooks->find_archetype(archname);

	if (!at)
	{
		PyErr_Format(AtrinikError, "object.CreateObject(): The archetype '%s' doesn't exist.", archname);
		return NULL;
	}

	tmp = hooks->arch_to_object(at);

	if (value != -1)
	{
		tmp->value = value;
	}

	if (nrof > 1)
	{
		tmp->nrof = nrof;
	}

	if (identified)
	{
		SET_FLAG(tmp, FLAG_IDENTIFIED);
	}

	tmp = hooks->insert_ob_in_ob(tmp, obj->obj);

	/* Make sure inventory image/text is updated */
	for (env = obj->obj; env; env = env->env)
	{
		if (env->type == PLAYER)
		{
			hooks->esrv_send_item(env, tmp);
		}
	}

	return wrap_object(tmp);
}

/** @cond */
/**
 * Helper function for Atrinik_Object_FindObject() to recursively
 * check inventories. */
static object *object_find_object(object *tmp, int mode, shstr *archname, shstr *name, shstr *title, int type)
{
	object *tmp2;

	while (tmp)
	{
		if ((!archname || tmp->arch->name == archname) && (!name || tmp->name == name) && (!title || tmp->title == title) && (type == -1 || tmp->type == type))
		{
			return tmp;
		}

		if (tmp->inv && (mode == INVENTORY_ALL || (mode == INVENTORY_CONTAINERS && tmp->type == CONTAINER)))
		{
			tmp2 = object_find_object(tmp->inv, mode, archname, name, title, type);

			if (tmp2)
			{
				return tmp2;
			}
		}

		tmp = tmp->below;
	}

	return NULL;
}
/** @endcond */

/**
 * <h1>object.FindObject(int [mode = INVENTORY_ONLY], string [archname = None], string [name = None], string [title = None], int [type = -1])</h1>
 * Looks for a certain object in object's inventory.
 * @param mode How to search the inventory. One of @ref INVENTORY_xxx.
 * @param archname Arch name of the object to search for. If None, can be any.
 * @param name Name of the object. If None, can be any.
 * @param title Title of the object. If None, can be any.
 * @param type Type of the object. If -1, can be any.
 * @throws ValueError if there were no conditions to search for.
 * @return The object we wanted if found, None otherwise. */
static PyObject *Atrinik_Object_FindObject(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"mode", "archname", "name", "title", "type", NULL};
	uint8 mode = INVENTORY_ONLY;
	int type = -1;
	shstr *archname = NULL, *name = NULL, *title = NULL;
	object *match;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "|Bzzzi", kwlist, &mode, &archname, &name, &title, &type))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (!archname && !name && !title && type == -1)
	{
		PyErr_SetString(PyExc_ValueError, "object.FindObject(): No conditions to search for given.");
		return NULL;
	}

	/* Try to find the strings we got from Python in the shared strings
	 * library. If they are not found, it is impossible that the inventory
	 * lookups succeed. */
	if (archname)
	{
		archname = hooks->find_string(archname);

		if (!archname)
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	if (name)
	{
		name = hooks->find_string(name);

		if (!name)
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	if (title)
	{
		title = hooks->find_string(title);

		if (!title)
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	match = object_find_object(obj->obj->inv, mode, archname, name, title, type);

	if (match)
	{
		return wrap_object(match);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Remove()</h1>
 * Takes the object out of whatever map or inventory it is in. The object
 * can then be inserted or teleported somewhere else, or just left alone
 * for the garbage collection to take care of.
 * @throws AtrinikError if one of the involved objects is attempted to be
 * removed (activator, who or other). */
static PyObject *Atrinik_Object_Remove(Atrinik_Object *obj, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(obj);

	/* Don't allow removing any of the involved objects. Messes things up... */
	if (current_context->activator == obj->obj || current_context->who == obj->obj || current_context->other == obj->obj)
	{
		RAISE("You are not allowed to remove one of the active objects.");
	}

	hooks->object_remove_esrv_update(obj->obj);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetPosition(int x, int y)</h1>
 * Sets new position coordinates for the object.
 *
 * Cannot be used to move objects out of containers, use Drop() or
 * TeleportTo() for that.
 * @param x New X position on the same map
 * @param y New Y position on the same map */
static PyObject *Atrinik_Object_SetPosition(Atrinik_Object *whoptr, PyObject *args)
{
	int x, y;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	hooks->transfer_ob(WHO, x, y, 0, NULL, NULL);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.IdentifyItem(object target, int mode, object [marked = None])</h1>
 * Identify item(s) in target's inventory.
 * @param target The target object.
 * @param mode Possible modes:
 * - <b>Atrinik.IDENTIFY_NORMAL</b>: Normal identify.
 * - <b>Atrinik.IDENTIFY_ALL</b>: Identify all items, if 'marked' is set,
 *   all items inside that.
 * - <b>Atrinik.IDENTIFY_MARKED</b>: Identify only marked item.
 * @param marked Marked item. */
static PyObject *Atrinik_Object_IdentifyItem(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *target;
	PyObject *marked = NULL;
	object *ob = NULL;
	int mode;

	if (!PyArg_ParseTuple(args, "O!i|O", &Atrinik_ObjectType, &target, &mode, &marked))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(target);

	if (marked && marked != Py_None)
	{
		if (!PyObject_TypeCheck(marked, &Atrinik_ObjectType))
		{
			PyErr_SetString(PyExc_TypeError, "Must be Atrinik.Object");
			return NULL;
		}

		OBJEXISTCHECK((Atrinik_Object *) marked);
		ob = ((Atrinik_Object *) marked)->obj;
	}

	hooks->cast_identify(target->obj, obj->obj->level, ob, mode);
	hooks->play_sound_map(obj->obj->map, CMD_SOUND_EFFECT, hooks->spells[SP_IDENTIFY].sound, obj->obj->x, obj->obj->y, 0, 0);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Save()</h1>
 * Dump an object, as if it was being saved to map or player file. Useful
 * for saving the object somewhere for loading later with
 * @ref Atrinik_LoadObject "LoadObject()".
 * @return String containing the saved object. */
static PyObject *Atrinik_Object_Save(Atrinik_Object *whoptr, PyObject *args)
{
	PyObject *ret;
	StringBuffer *sb;
	char *result;

	(void) args;
	OBJEXISTCHECK(whoptr);

	sb = hooks->stringbuffer_new();
	hooks->dump_object(WHO, sb);
	result = hooks->stringbuffer_finish(sb);
	ret = Py_BuildValue("s", result);
	free(result);

	return ret;
}

/**
 * <h1>object.GetItemCost(object what, int type)</h1>
 * Get cost of an object in integer value.
 * @param what The object to query cost for.
 * @param type Possible types:
 * - <b>Atrinik.COST_TRUE</b>
 * - <b>Atrinik.COST_BUY</b>
 * - <b>Atrinik.COST_SELL</b>
 * @return The cost of the item as integer. */
static PyObject *Atrinik_Object_GetItemCost(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int flag;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &whatptr, &flag))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(whatptr);

	return Py_BuildValue("i", hooks->query_cost(WHAT, WHO, flag));
}

/**
 * <h1>object.GetMoney()</h1>
 * Get all the money the object is carrying as integer.
 * @return The amount of money the object is carrying in copper (as
 * integer). */
static PyObject *Atrinik_Object_GetMoney(Atrinik_Object *whoptr, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("i", hooks->query_money(WHO));
}

/**
 * <h1>object.PayForItem(object what)</h1>
 * @warning Untested.
 * @todo Test, document. */
static PyObject *Atrinik_Object_PayForItem(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whatptr))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(whatptr);

	return Py_BuildValue("i", hooks->pay_for_item(WHAT, WHO));
}

/**
 * <h1>object.PayAmount(int value)</h1>
 * Get the object to pay a specified amount of money in copper.
 * @param value The amount of money in copper to pay for.
 * @return 1 if the object paid the money (the object had enough money in
 * inventory), 0 otherwise. */
static PyObject *Atrinik_Object_PayAmount(Atrinik_Object *whoptr, PyObject *args)
{
	int to_pay;

	if (!PyArg_ParseTuple(args, "i", &to_pay))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("i", hooks->pay_for_amount(to_pay, WHO));
}

/**
 * <h1>object.Clone(int [mode = Atrinik.CLONE_WITH_INVENTORY])</h1>
 * Clone an object.
 *
 * You should do something with the clone.
 * @ref Atrinik_Object_TeleportTo "TeleportTo()" or
 * @ref Atrinik_Object_InsertInside "InsertInside()" are useful functions
 * for that.
 * @param mode Optional mode, one of:
 * - <b>Atrinik.CLONE_WITH_INVENTORY</b> (default)
 * - <b>Atrinik.CLONE_WITHOUT_INVENTORY</b>
 * @return The cloned object. */
static PyObject *Atrinik_Object_Clone(Atrinik_Object *whoptr, PyObject *args)
{
	int mode = 0;
	object *clone;

	if (!PyArg_ParseTuple(args, "|i", &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (!mode)
	{
		clone = hooks->object_create_clone(WHO);
	}
	else
	{
		clone = hooks->get_object();
		hooks->copy_object(WHO, clone, 0);
	}

	if (clone->type == PLAYER || QUERY_FLAG(clone, FLAG_IS_PLAYER))
	{
		clone->type = MONSTER;
		CLEAR_FLAG(clone, FLAG_IS_PLAYER);
	}

	return wrap_object(clone);
}

/**
 * <h1>object.ReadKey(string key)</h1>
 * Get key value of an object.
 * @param key Key to look for.
 * @return Value for the key if found, None otherwise. */
static PyObject *Atrinik_Object_ReadKey(Atrinik_Object *whoptr, PyObject *args)
{
	const char *key;

	if (!PyArg_ParseTuple(args, "s", &key))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("s", hooks->object_get_value(WHO, key));
}

/**
 * <h1>object.WriteKey(string key, string [value = None], int [add_key = True])</h1>
 * Set the key value of an object.
 * @param key Key to look for.
 * @param value Value to set for the key. If not passed, will clear the
 * key's value if the key is found.
 * @param add_key Whether to add the key if it's not found in the object.
 * @return 1 on success, 0 on failure. */
static PyObject *Atrinik_Object_WriteKey(Atrinik_Object *whoptr, PyObject *args)
{
	const char *key, *value = NULL;
	int add_key = 1;

	if (!PyArg_ParseTuple(args, "s|si", &key, &value, &add_key))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	return Py_BuildValue("i", hooks->object_set_value(WHO, key, value, add_key));
}

/**
 * <h1>object.GetName(object [caller = None])</h1>
 * An equivalent of query_base_name().
 * @param caller Object calling this.
 * @return Full name of the object, including material, name, title,
 * etc. */
static PyObject *Atrinik_Object_GetName(Atrinik_Object *whatptr, PyObject *args)
{
	Atrinik_Object *ob = NULL;

	if (!PyArg_ParseTuple(args, "|O!", &Atrinik_ObjectType, &ob))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	if (ob)
	{
		OBJEXISTCHECK(ob);
	}

	return Py_BuildValue("s", hooks->query_short_name(WHAT, ob ? ob->obj : WHAT));
}

/**
 * <h1>object.CreateTimer(long delay, int mode)</h1>
 * Create a timer for an object.
 * @param delay How long until the timer event gets executed for the object
 * @param mode If 1, delay is in seconds, if 2, delay is in ticks (8 ticks = 1 second)
 * @return 0 and higher means success (and represents ID of the created timer),
 * anything lower means a failure of some sort. */
static PyObject *Atrinik_Object_CreateTimer(Atrinik_Object *whatptr, PyObject *args)
{
	int mode, timer;
	long delay;

	if (!PyArg_ParseTuple(args, "li", &delay, &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	timer = hooks->cftimer_find_free_id();

	if (timer != TIMER_ERR_ID)
	{
		int res = hooks->cftimer_create(timer, delay, WHAT, mode);

		if (res != TIMER_ERR_NONE)
		{
			timer = res;
		}
	}

	return Py_BuildValue("i", timer);
}

/**
 * <h1>player.Sound(string filename, int [type = @ref CMD_SOUND_EFFECT], int [x = 0], int [y = 0], int [loop = 0], int [volume = 0])</h1>
 * Play a sound to the specified player.
 * @param filename Sound file to play.
 * @param type Sound type being played, one of @ref CMD_SOUND_xxx.
 * @param x X position where the sound is playing from. Can be 0.
 * @param y Y position where the sound is playing from. Can be 0.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
static PyObject *Atrinik_Object_Sound(Atrinik_Object *whoptr, PyObject *args)
{
	int type = CMD_SOUND_EFFECT, x = 0, y = 0, loop = 0, volume = 0;
	const char *filename;

	if (!PyArg_ParseTuple(args, "s|iiiii", &filename, &type, &x, &y, &loop, &volume))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);

	if (WHO->type != PLAYER)
	{
		RAISE("Sound(): Can only be used on players.");
	}

	hooks->play_sound_player_only(CONTR(WHO), type, filename, x, y, loop, volume);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Controller()</h1>
 * Get object's controller (the player).
 * @throws AtrinikError if 'object' is not a player.
 * @return The controller if there is one, None otherwise. */
static PyObject *Atrinik_Object_Controller(Atrinik_Object *whatptr, PyObject *args)
{
	(void) args;

	OBJEXISTCHECK(whatptr);

	if (WHAT->type != PLAYER)
	{
		RAISE("Can only be used on players.");
	}

	return wrap_player(CONTR(WHAT));
}

/**
 * <h1>object.Protection(int nr)</h1>
 * Get object's protection value for the given protection ID.
 * @param nr Protection ID. One of ::_attacks.
 * @throws IndexError if the protection ID is invalid.
 * @return The protection value. */
static PyObject *Atrinik_Object_Protection(Atrinik_Object *whatptr, PyObject *args)
{
	int nr;

	if (!PyArg_ParseTuple(args, "i", &nr))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Protection ID is invalid.");
		return NULL;
	}

	return Py_BuildValue("b", WHAT->protection[nr]);
}

/**
 * <h1>object.SetProtection(int nr, int val)</h1>
 * Set object's protection value for the given protection ID.
 * @param nr Protection ID. One of ::_attacks.
 * @param val Value to set.
 * @throws IndexError if the protection ID is invalid.
 * @throws OverflowError if the value to set is not in valid range.
 * @return The new protection value. */
static PyObject *Atrinik_Object_SetProtection(Atrinik_Object *whatptr, PyObject *args)
{
	int nr;
	sint8 val;

	if (!PyArg_ParseTuple(args, "ib", &nr, &val))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Protection ID is invalid.");
		return NULL;
	}

	WHAT->protection[nr] = val;
	return Py_BuildValue("b", WHAT->protection[nr]);
}

/**
 * <h1>object.Attack(int nr)</h1>
 * Get object's attack value for the given attack ID.
 * @param nr Attack ID. One of ::_attacks.
 * @throws IndexError if the attack ID is invalid.
 * @return The attack value. */
static PyObject *Atrinik_Object_Attack(Atrinik_Object *whatptr, PyObject *args)
{
	int nr;

	if (!PyArg_ParseTuple(args, "i", &nr))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Attack ID is invalid.");
		return NULL;
	}

	return Py_BuildValue("b", WHAT->attack[nr]);
}

/**
 * <h1>object.SetAttack(int nr, int val)</h1>
 * Set object's attack value for the given attack ID.
 * @param nr Attack ID. One of ::_attacks.
 * @param val Value to set.
 * @throws IndexError if the attack ID is invalid.
 * @throws OverflowError if the value to set is not in valid range.
 * @return The new attack value. */
static PyObject *Atrinik_Object_SetAttack(Atrinik_Object *whatptr, PyObject *args)
{
	int nr;
	uint8 val;

	if (!PyArg_ParseTuple(args, "iB", &nr, &val))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Attack ID is invalid.");
		return NULL;
	}

	WHAT->attack[nr] = val;
	return Py_BuildValue("b", WHAT->attack[nr]);
}

/**
 * <h1>object.ChangeAbil(object what)</h1>
 * Permanently alters an object's stats/flags based on another what.
 * @param what Object that is giving bonuses/penalties to 'object'.
 * @return 1 if we sucessfully changed a stat, 0 if nothing was changed. */
static PyObject *Atrinik_Object_ChangeAbil(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whatptr))
	{
		return NULL;
	}

	OBJEXISTCHECK(whoptr);
	OBJEXISTCHECK(whatptr);

	return Py_BuildValue("i", hooks->change_abil(WHO, WHAT));
}

/**
 * <h1>object.Decrease(int [num = 1])</h1>
 * Decreases an object, removing it if there's nothing left to decrease.
 * @param num How much to decrease the object by.
 * @return 'object' if something is left, None if the amount reached 0. */
static PyObject *Atrinik_Object_Decrease(Atrinik_Object *whatptr, PyObject *args)
{
	uint32 num = 1;

	if (!PyArg_ParseTuple(args, "|I", &num))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	return wrap_object(hooks->decrease_ob_nr(WHAT, num));
}

/**
 * <h1>object.SquaresAround(int range, int [type = @ref AROUND_ALL], int [beyond = False], function [callable = None])</h1>
 * Looks around the specified object and returns a list of tuples
 * containing the squares around it in a specified range. The tuples
 * have a format of <b>(map, x, y)</b>.
 *
 * Example that ignores walls and floors with grass:
 * @code
from Atrinik import *

activator = WhoIsActivator()

def cmp_squares(m, x, y, obj):
	# 'obj' is now same as 'activator'.
	try:
		return m.GetLayer(x, y, LAYER_FLOOR)[0].name == "grass"
	# Exception was raised; ignore it, as it probably means there is no
	# floor.
	except:
		return False

for (m, x, y) in activator.SquaresAround(1, type = AROUND_WALL, callable = cmp_squares):
	for ob in m.GetLayer(x, y, LAYER_FLOOR):
		print(ob)
 * @endcode
 * @param range Range around which to look at the squares. Must be higher
 * than 0.
 * @param type One or a combination of @ref AROUND_xxx constants.
 * @param beyond If True and one of checks from 'type' parameter matches, all
 * squares beyond the one being checked will be ignored as well.
 * @param callable Defines function to call for comparisons. The function
 * should have parameters in the order of <b>map, x, y, obj</b> where
 * map is the map, x/y are the coordinates and obj is the object that SquaresAround()
 * was called for. The function should return True if the square should be
 * considered ignored, False otherwise. 'type' being @ref AROUND_ALL takes
 * no effect if this is set, but it can be combined with @ref AROUND_xxx "other types".
 * @return A list containing tuples of the squares. */
static PyObject *Atrinik_Object_SquaresAround(Atrinik_Object *whatptr, PyObject *args, PyObject *keywds)
{
	uint8 range, type = AROUND_ALL;
	static char *kwlist[] = {"range", "type", "beyond", "callable", NULL};
	int i, j, xt, yt, beyond = 0;
	mapstruct *m;
	PyObject *callable = NULL, *list;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "B|BiO", kwlist, &range, &type, &beyond, &callable))
	{
		return NULL;
	}

	OBJEXISTCHECK(whatptr);

	if (range == 0)
	{
		PyErr_SetString(PyExc_ValueError, "SquaresAround(): 'range' must be higher than 0.");
		return NULL;
	}

	if (callable && !PyCallable_Check(callable))
	{
		PyErr_SetString(PyExc_TypeError, "Argument 'callable' must be callable.");
		return NULL;
	}

	list = PyList_New(0);

	/* Go through the squares in the specified range. */
	for (i = -range; i <= range; i++)
	{
		for (j = -range; j <= range; j++)
		{
			xt = WHAT->x + i;
			yt = WHAT->y + j;

			/* Skip ourselves. */
			if (xt == WHAT->x && yt == WHAT->y)
			{
				continue;
			}

			m = hooks->get_map_from_coord(WHAT->map, &xt, &yt);

			if (!m)
			{
				continue;
			}

			/* We want all squares. */
			if (type == AROUND_ALL && !callable)
			{
				SQUARES_AROUND_ADD(m, xt, yt);
			}
			/* Only those that are not blocked by view, or beyond a wall, etc,
			 * so use the Bresenham algorithm. */
			else if (beyond)
			{
				int xt2, yt2, fraction, dx2, dy2, stepx, stepy;
				mapstruct *m2;
				rv_vector rv;

				m2 = WHAT->map;
				xt2 = WHAT->x;
				yt2 = WHAT->y;

				if (!hooks->get_rangevector_from_mapcoords(m2, xt2, yt2, m, xt, yt, &rv, RV_NO_DISTANCE))
				{
					continue;
				}

				BRESENHAM_INIT(rv.distance_x, rv.distance_y, fraction, stepx, stepy, dx2, dy2);

				while (1)
				{
					BRESENHAM_STEP(xt2, yt2, fraction, stepx, stepy, dx2, dy2);
					m2 = hooks->get_map_from_coord(m2, &xt2, &yt2);

					if (m2 == NULL || (type & AROUND_BLOCKSVIEW && GET_MAP_FLAGS(m2, xt2, yt2) & P_BLOCKSVIEW) || (type & AROUND_PLAYER_ONLY && GET_MAP_FLAGS(m2, xt2, yt2) & P_PLAYER_ONLY) || (type & AROUND_WALL && hooks->wall(m2, xt2, yt2)) || (callable && python_call_int(callable, Py_BuildValue("(OiiO)", wrap_map(m2), xt2, yt2, wrap_object(WHAT)))))
					{
						break;
					}

					if (m2 == m && xt2 == xt && yt2 == yt)
					{
						SQUARES_AROUND_ADD(m, xt, yt);
						break;
					}
				}
			}
			/* We only want to ignore squares that either block view, or have
			 * a wall, etc, but not any squares behind them. */
			else
			{
				if ((type & AROUND_BLOCKSVIEW && GET_MAP_FLAGS(m, xt, yt) & P_BLOCKSVIEW) || (type & AROUND_PLAYER_ONLY && GET_MAP_FLAGS(m, xt, yt) & P_PLAYER_ONLY) || (type & AROUND_WALL && hooks->wall(m, xt, yt)) || (callable && python_call_int(callable, Py_BuildValue("(OiiO)", wrap_map(m), xt, yt, wrap_object(WHAT)))))
				{
					continue;
				}

				SQUARES_AROUND_ADD(m, xt, yt);
			}
		}
	}

	return list;
}

/**
 * <h1>object.GetRangeVector(object to, int [flags = 0])</h1>
 * Get the distance and direction from one object to another.
 * @param to Object to which the distance is calculated.
 * @param flags One or a combination of @ref range_vector_flags.
 * @return None if the distance couldn't be calculated, otherwise a tuple
 * containining:
 *  - Direction 'object' should head to reach 'to', one of @ref direction_constants.
 *  - Distance between 'object' and 'to'.
 *  - X distance.
 *  - Y distance.
 *  - Part of the object that is closest. */
static PyObject *Atrinik_Object_GetRangeVector(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *to;
	int flags = 0;
	rv_vector rv;
	PyObject *tuple;

	if (!PyArg_ParseTuple(args, "O!|i", &Atrinik_ObjectType, &to, &flags))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(to);

	if (!hooks->get_rangevector(obj->obj, to->obj, &rv, flags))
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	tuple = PyTuple_New(5);
	PyTuple_SET_ITEM(tuple, 0, Py_BuildValue("i", rv.direction));
	PyTuple_SET_ITEM(tuple, 1, Py_BuildValue("i", rv.distance));
	PyTuple_SET_ITEM(tuple, 2, Py_BuildValue("i", rv.distance_x));
	PyTuple_SET_ITEM(tuple, 3, Py_BuildValue("i", rv.distance_y));
	PyTuple_SET_ITEM(tuple, 4, wrap_object(rv.part));

	return tuple;
}

/*@}*/

/** Available Python methods for the AtrinikObject object */
static PyMethodDef methods[] =
{
	{"GetSkill", (PyCFunction) Atrinik_Object_GetSkill, METH_VARARGS, 0},
	{"SetSkill", (PyCFunction) Atrinik_Object_SetSkill, METH_VARARGS, 0},
	{"ActivateRune", (PyCFunction) Atrinik_Object_ActivateRune, METH_VARARGS, 0},
	{"GetGod", (PyCFunction) Atrinik_Object_GetGod, METH_NOARGS, 0},
	{"SetGod", (PyCFunction) Atrinik_Object_SetGod, METH_VARARGS, 0},
	{"TeleportTo", (PyCFunction) Atrinik_Object_TeleportTo, METH_VARARGS | METH_KEYWORDS, 0},
	{"InsertInto", (PyCFunction) Atrinik_Object_InsertInto, METH_VARARGS, 0},
	{"Apply", (PyCFunction) Atrinik_Object_Apply, METH_VARARGS, 0},
	{"Take", (PyCFunction) Atrinik_Object_Take, METH_O, 0},
	{"Drop", (PyCFunction) Atrinik_Object_Drop, METH_O, 0},
	{"Communicate", (PyCFunction) Atrinik_Object_Communicate, METH_VARARGS, 0},
	{"Say", (PyCFunction) Atrinik_Object_Say, METH_VARARGS, 0},
	{"SayTo", (PyCFunction) Atrinik_Object_SayTo, METH_VARARGS, 0},
	{"Write", (PyCFunction) Atrinik_Object_Write, METH_VARARGS, 0},
	{"GetGender", (PyCFunction) Atrinik_Object_GetGender, METH_NOARGS, 0},
	{"SetGender", (PyCFunction) Atrinik_Object_SetGender, METH_VARARGS, 0},
	{"SetGuildForce", (PyCFunction) Atrinik_Object_SetGuildForce, METH_VARARGS, 0},
	{"GetGuildForce", (PyCFunction) Atrinik_Object_GetGuildForce, METH_NOARGS, 0},
	{"Fix", (PyCFunction) Atrinik_Object_Fix, METH_NOARGS, 0},
	{"Kill", (PyCFunction) Atrinik_Object_Kill, METH_VARARGS, 0},
	{"CastAbility", (PyCFunction) Atrinik_Object_CastAbility, METH_VARARGS, 0},
	{"DoKnowSpell", (PyCFunction) Atrinik_Object_DoKnowSpell, METH_VARARGS, 0},
	{"AcquireSpell", (PyCFunction) Atrinik_Object_AcquireSpell, METH_VARARGS, 0},
	{"DoKnowSkill", (PyCFunction) Atrinik_Object_DoKnowSkill, METH_VARARGS, 0},
	{"AcquireSkill", (PyCFunction) Atrinik_Object_AcquireSkill, METH_VARARGS, 0},
	{"FindMarkedObject", (PyCFunction) Atrinik_Object_FindMarkedObject, METH_NOARGS, 0},
	{"CreatePlayerForce", (PyCFunction) Atrinik_Object_CreatePlayerForce, METH_VARARGS, 0},
	{"GetQuestObject", (PyCFunction) Atrinik_Object_GetQuestObject, METH_VARARGS, 0},
	{"StartQuest", (PyCFunction) Atrinik_Object_StartQuest, METH_VARARGS, 0},
	{"CreatePlayerInfo", (PyCFunction) Atrinik_Object_CreatePlayerInfo, METH_VARARGS, 0},
	{"GetPlayerInfo", (PyCFunction) Atrinik_Object_GetPlayerInfo, METH_VARARGS, 0},
	{"GetNextPlayerInfo", (PyCFunction) Atrinik_Object_GetNextPlayerInfo, METH_VARARGS, 0},
	{"CreateForce", (PyCFunction) Atrinik_Object_CreateForce, METH_VARARGS, 0},
	{"CreateObject", (PyCFunction) Atrinik_Object_CreateObject, METH_VARARGS | METH_KEYWORDS, 0},
	{"FindObject", (PyCFunction) Atrinik_Object_FindObject, METH_VARARGS | METH_KEYWORDS, 0},
	{"Remove", (PyCFunction) Atrinik_Object_Remove, METH_NOARGS, 0},
	{"SetPosition", (PyCFunction) Atrinik_Object_SetPosition, METH_VARARGS, 0},
	{"IdentifyItem", (PyCFunction) Atrinik_Object_IdentifyItem, METH_VARARGS, 0},
	{"Save", (PyCFunction) Atrinik_Object_Save, METH_NOARGS, 0},
	{"GetItemCost", (PyCFunction) Atrinik_Object_GetItemCost, METH_VARARGS, 0},
	{"GetMoney", (PyCFunction) Atrinik_Object_GetMoney, METH_NOARGS, 0},
	{"PayForItem", (PyCFunction) Atrinik_Object_PayForItem, METH_VARARGS, 0},
	{"PayAmount", (PyCFunction) Atrinik_Object_PayAmount, METH_VARARGS, 0},
	{"Clone", (PyCFunction) Atrinik_Object_Clone, METH_VARARGS, 0},
	{"ReadKey", (PyCFunction) Atrinik_Object_ReadKey, METH_VARARGS, 0},
	{"WriteKey", (PyCFunction) Atrinik_Object_WriteKey, METH_VARARGS, 0},
	{"GetName", (PyCFunction) Atrinik_Object_GetName, METH_VARARGS, 0},
	{"CreateTimer", (PyCFunction) Atrinik_Object_CreateTimer, METH_VARARGS, 0},
	{"Sound", (PyCFunction) Atrinik_Object_Sound, METH_VARARGS, 0},
	{"Controller", (PyCFunction) Atrinik_Object_Controller, METH_NOARGS, 0},
	{"Protection", (PyCFunction) Atrinik_Object_Protection, METH_VARARGS, 0},
	{"SetProtection", (PyCFunction) Atrinik_Object_SetProtection, METH_VARARGS, 0},
	{"Attack", (PyCFunction) Atrinik_Object_Attack, METH_VARARGS, 0},
	{"SetAttack", (PyCFunction) Atrinik_Object_SetAttack, METH_VARARGS, 0},
	{"ChangeAbil", (PyCFunction) Atrinik_Object_ChangeAbil, METH_VARARGS, 0},
	{"Decrease", (PyCFunction) Atrinik_Object_Decrease, METH_VARARGS, 0},
	{"SquaresAround", (PyCFunction) Atrinik_Object_SquaresAround, METH_VARARGS | METH_KEYWORDS, 0},
	{"GetRangeVector", (PyCFunction) Atrinik_Object_GetRangeVector, METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

/**
 * Get object's attribute.
 * @param whoptr Python object wrapper.
 * @param context Void pointer to the field.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *Object_GetAttribute(Atrinik_Object *whoptr, void *context)
{
	OBJEXISTCHECK(whoptr);

	return generic_field_getter((fields_struct *) context, WHO);
}

/**
 * Set attribute of an object.
 * @param whoptr Python object wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure. */
static int Object_SetAttribute(Atrinik_Object *whoptr, PyObject *value, void *context)
{
	object *tmp;
	fields_struct *field = (fields_struct *) context;

	OBJEXISTCHECK_INT(whoptr);

	if ((field->flags & FIELDFLAG_PLAYER_READONLY) && WHO->type == PLAYER)
	{
		INTRAISE("Trying to modify a field that is read-only for player objects.");
	}

	if (generic_field_setter(field, WHO, value) == -1)
	{
		return -1;
	}

	/* Make sure the inventory image/text is updated. */
	for (tmp = WHO->env; tmp; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			hooks->esrv_send_item(tmp, WHO);
		}
	}

	/* Special handling for some player stuff. */
	if (WHO->type == PLAYER)
	{
		switch (field->offset)
		{
			case offsetof(object, stats.Str):
				CONTR(WHO)->orig_stats.Str = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Dex):
				CONTR(WHO)->orig_stats.Dex = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Con):
				CONTR(WHO)->orig_stats.Con = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Wis):
				CONTR(WHO)->orig_stats.Wis = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Pow):
				CONTR(WHO)->orig_stats.Pow = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Cha):
				CONTR(WHO)->orig_stats.Cha = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Int):
				CONTR(WHO)->orig_stats.Int = (sint8) PyInt_AsLong(value);
				break;
		}

		if (field->flags & FIELDFLAG_PLAYER_FIX)
		{
			hooks->fix_player(WHO);
		}
	}

	return 0;
}

/**
 * Get object's flag.
 * @param obj Python object wrapper.
 * @param context Void pointer to the flag ID.
 * @retval Py_True The object has the flag set.
 * @retval Py_False The object doesn't have the flag set.
 * @retval NULL An error occurred. */
static PyObject *Object_GetFlag(Atrinik_Object *obj, void *context)
{
	size_t flagno = (size_t) context;

	/* Should not happen. */
	if (flagno >= NUM_FLAGS)
	{
		PyErr_SetString(PyExc_OverflowError, "Invalid flag ID.");
		return NULL;
	}

	OBJEXISTCHECK(obj);

	Py_ReturnBoolean(QUERY_FLAG(obj->obj, flagno));
}

/**
 * Set flag for an object.
 * @param obj Python object wrapper.
 * @param val Value to set. Should be either Py_True or Py_False.
 * @param context Void pointer to the flag ID.
 * @return 0 on success, -1 on failure. */
static int Object_SetFlag(Atrinik_Object *obj, PyObject *val, void *context)
{
	object *env;
	size_t flagno = (size_t) context;

	/* Should not happen. */
	if (flagno >= NUM_FLAGS)
	{
		PyErr_SetString(PyExc_OverflowError, "Invalid flag ID.");
		return -1;
	}

	OBJEXISTCHECK_INT(obj);

	if (val == Py_True)
	{
		SET_FLAG(obj->obj, flagno);
	}
	else if (val == Py_False)
	{
		CLEAR_FLAG(obj->obj, flagno);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "Flag value must be either True or False.");
		return -1;
	}

	/* Make sure the inventory image/text/etc is updated */
	for (env = obj->obj->env; env; env = env->env)
	{
		if (env->type == PLAYER)
		{
			hooks->esrv_send_item(env, obj->obj);
		}
	}

	return 0;
}

/**
 * Create a new object wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Object_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Atrinik_Object *self = (Atrinik_Object *) type->tp_alloc(type, 0);

	(void) args;
	(void) kwds;

	if (self)
	{
		self->obj = NULL;
		self->count = 0;
	}

	return (PyObject *) self;
}

/**
 * Free an object wrapper.
 * @param self The wrapper to free. */
static void Atrinik_Object_dealloc(PyObject *self)
{
	((Atrinik_Object *) self)->obj = NULL;
	((Atrinik_Object *) self)->count = 0;
#ifndef IS_PY_LEGACY
	Py_TYPE(self)->tp_free(self);
#else
	self->ob_type->tp_free(self);
#endif
}

/**
 * Return a string representation of an object.
 * @param self The object type.
 * @return Python object containing the arch name and name of the object. */
static PyObject *Atrinik_Object_str(Atrinik_Object *self)
{
	OBJEXISTCHECK(self);
	return PyString_FromFormat("[%s \"%s\"]", STRING_OBJ_ARCH_NAME(self->obj), STRING_OBJ_NAME(self->obj));
}

static int Atrinik_Object_InternalCompare(Atrinik_Object *left, Atrinik_Object *right)
{
	OBJEXISTCHECK_INT(left);
	OBJEXISTCHECK_INT(right);
	return (left->obj < right->obj ? -1 : (left->obj == right->obj ? 0 : 1));
}

static PyObject *Atrinik_Object_RichCompare(Atrinik_Object *left, Atrinik_Object *right, int op)
{
	int result;

	if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_ObjectType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_ObjectType))
	{
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}

	result = Atrinik_Object_InternalCompare(left, right);

	/* Handle removed objects. */
	if (result == -1 && PyErr_Occurred())
	{
		return NULL;
	}

	return generic_rich_compare(op, result);
}

/**
 * Start iterating.
 * @param seq Object to start iterating from.
 * @return A new instance of PyObject, containing Atrinik_Object, with a reference
 * to 'seq'. */
static PyObject *object_iter(PyObject *seq)
{
	Atrinik_Object *obj, *orig_obj = (Atrinik_Object *) seq;

	OBJEXISTCHECK(orig_obj);

	obj = PyObject_NEW(Atrinik_Object, &Atrinik_ObjectType);
	Py_INCREF(seq);
	obj->iter = (Atrinik_Object *) seq;
	obj->iter_type = OBJ_ITER_TYPE_ONE;

	/* Select which iteration type we're doing. It's possible that
	 * an object has both below and above set (it's not the first and
	 * not the last object), in which case we will prefer below. */
	if (orig_obj->obj->below)
	{
		obj->iter_type = OBJ_ITER_TYPE_BELOW;
	}
	else if (orig_obj->obj->above)
	{
		obj->iter_type = OBJ_ITER_TYPE_ABOVE;
	}

	return (PyObject *) obj;
}

/**
 * Get next object for iteration.
 * @param obj Previous object.
 * @return Next object, NULL if there is nothing left. */
static PyObject *object_iternext(Atrinik_Object *obj)
{
	/* Do we need to iterate? */
	if (obj->iter_type != OBJ_ITER_TYPE_NONE)
	{
		object *tmp;

		OBJEXISTCHECK(obj->iter);
		tmp = obj->iter->obj;

		/* Check which way we're iterating. */
		if (obj->iter_type == OBJ_ITER_TYPE_BELOW)
		{
			obj->iter->obj = tmp->below;
		}
		else if (obj->iter_type == OBJ_ITER_TYPE_ABOVE)
		{
			obj->iter->obj = tmp->above;
		}
		else if (obj->iter_type == OBJ_ITER_TYPE_ONE)
		{
			obj->iter->obj = NULL;
		}

		obj->iter->count = obj->iter->obj ? obj->iter->obj->count : 0;

		/* Nothing left, so mark iter_type to show that. */
		if (!obj->iter->obj)
		{
			obj->iter_type = OBJ_ITER_TYPE_NONE;
		}

		return wrap_object(tmp);
	}

	return NULL;
}

/** This is filled in when we initialize our object type. */
static PyGetSetDef getseters[NUM_FIELDS + NUM_FLAGS + 1];

/** Our actual Python ObjectType. */
PyTypeObject Atrinik_ObjectType =
{
#ifdef IS_PY3K
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	PyObject_HEAD_INIT(NULL)
	0,
#endif
	"Atrinik.Object",
	sizeof(Atrinik_Object),
	0,
	(destructor) Atrinik_Object_dealloc,
	NULL, NULL, NULL,
#ifdef IS_PY3K
	NULL,
#else
	(cmpfunc) Atrinik_Object_InternalCompare,
#endif
	0, 0, 0, 0, 0, 0,
	(reprfunc) Atrinik_Object_str,
	0, 0, 0,
	Py_TPFLAGS_DEFAULT,
	"Atrinik objects",
	NULL, NULL,
	(richcmpfunc) Atrinik_Object_RichCompare,
	0,
	(getiterfunc) object_iter,
	(iternextfunc) object_iternext,
	methods,
	0,
	getseters,
	0, 0, 0, 0, 0, 0, 0,
	Atrinik_Object_new,
	0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
	, 0
#endif
};

/**
 * Initialize the object wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Object_init(PyObject *module)
{
	size_t i, flagno;
	char buf[MAX_BUF];

	/* Field getseters */
	for (i = 0; i < NUM_FIELDS; i++)
	{
		PyGetSetDef *def = &getseters[i];

		def->name = fields[i].name;
		def->get = (getter) Object_GetAttribute;
		def->set = (setter) Object_SetAttribute;
		def->doc = NULL;
		def->closure = (void *) &fields[i];
	}

	/* Flag getseters */
	for (flagno = 0; flagno < NUM_FLAGS; flagno++)
	{
		if (hooks->object_flag_names[flagno])
		{
			PyGetSetDef *def = &getseters[i++];

			strncpy(buf, "f_", sizeof(buf) - 1);
			strncat(buf, hooks->object_flag_names[flagno], sizeof(buf) - strlen(buf) - 1);
			def->name = hooks->strdup_local(buf);

			def->get = (getter) Object_GetFlag;
			def->set = (setter) Object_SetFlag;
			def->doc = NULL;
			def->closure = (void *) flagno;
		}
	}

	getseters[i].name = NULL;

	Atrinik_ObjectType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_ObjectType) < 0)
	{
		return 0;
	}

	Py_INCREF(&Atrinik_ObjectType);
	PyModule_AddObject(module, "Object", (PyObject *) &Atrinik_ObjectType);

	return 1;
}

/**
 * Utility method to wrap an object.
 * @param what Object to wrap.
 * @return Python object wrapping the real object. */
PyObject *wrap_object(object *what)
{
	Atrinik_Object *wrapper;

	/* Return None if no object was to be wrapped. */
	if (!what || OBJECT_FREE(what))
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Object, &Atrinik_ObjectType);

	if (wrapper)
	{
		wrapper->obj = HEAD(what);
		wrapper->count = wrapper->obj->count;
	}

	return (PyObject *) wrapper;
}
