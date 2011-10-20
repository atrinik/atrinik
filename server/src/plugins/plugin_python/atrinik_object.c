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
	{"count", FIELDTYPE_UINT32, offsetof(object, count), FIELDFLAG_READONLY, 0},

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
	{"z", FIELDTYPE_SINT16, offsetof(object, z), 0, 0},
	{"zoom_x", FIELDTYPE_SINT16, offsetof(object, zoom_x), 0, 0},
	{"zoom_y", FIELDTYPE_SINT16, offsetof(object, zoom_y), 0, 0},
	{"rotate", FIELDTYPE_SINT16, offsetof(object, rotate), 0, 0},
	{"align", FIELDTYPE_SINT16, offsetof(object, align), 0, 0},
	{"alpha", FIELDTYPE_UINT8, offsetof(object, alpha), 0, 0},
	/* Returns the object's face in a tuple containing the face name as
	 * string, and the face ID as integer.\n
	 * There are a few different ways to set object's face. You can use
	 * the face name (obj.face = "eyes.101"), the ID (obj.face = 1000),
	 * or the tuple returned by a previous call to obj.face. */
	{"face", FIELDTYPE_FACE, offsetof(object, face), 0, 0},
	/* Returns the object's animation in a tuple containing the animation
	 * name as string, and the animation ID as integer.\n
	 * There are a few different ways to set object's animation. You can
	 * use the animation name (obj.animation = "raas"), the ID (obj.animation = 100),
	 * or the tuple returned by a previous call to obj.animation. */
	{"animation", FIELDTYPE_ANIMATION, offsetof(object, animation_id), 0, 0},
	/* See notes for object's animation. */
	{"inv_animation", FIELDTYPE_ANIMATION, offsetof(object, inv_animation_id), 0, 0},
	{"other_arch", FIELDTYPE_ARCH, offsetof(object, other_arch), 0, 0}
};
/* @endcparser */

/**
 * @defgroup plugin_python_object_functions Python object functions
 * Object related functions used in Atrinik Python plugin.
 *@{*/

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
static PyObject *Atrinik_Object_GetGod(Atrinik_Object *obj, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(obj);

	return Py_BuildValue("s", hooks->determine_god(obj->obj));
}

/**
 * <h1>object.SetGod(string name)</h1>
 * Make an object become follower of a different god.
 *
 * The object must have the 'divine prayers' skill.
 * @param name Name of the god. */
static PyObject *Atrinik_Object_SetGod(Atrinik_Object *obj, PyObject *args)
{
	const char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (hooks->command_rskill(obj->obj, "divine prayers"))
	{
		object *god = hooks->find_god(name);

		hooks->become_follower(obj->obj, god);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.TeleportTo(string map, int x, int y, bool [unique = False], bool [sound = True])</h1>
 * Teleport object to the given position of map.
 * @param path Map path to teleport the object to.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @param unique If True, the destination will be unique map for the player.
 * @param sound If False, will not play a sound effect. */
static PyObject *Atrinik_Object_TeleportTo(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"path", "x", "y", "unique", "sound", NULL};
	const char *path;
	object *tmp;
	int x, y, unique = 0, sound = 1;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "sii|ii", kwlist, &path, &x, &y, &unique, &sound))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	tmp = hooks->get_object();
	FREE_AND_COPY_HASH(EXIT_PATH(tmp), path);
	EXIT_X(tmp) = x;
	EXIT_Y(tmp) = y;

	if (unique)
	{
		tmp->last_eat = MAP_PLAYER_MAP;
	}

	hooks->enter_exit(obj->obj, tmp);

	if (obj->obj->map && sound)
	{
		hooks->play_sound_map(obj->obj->map, CMD_SOUND_EFFECT, "teleport.ogg", obj->obj->x, obj->obj->y, 0, 0);
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
 * Forces 'object' to apply 'what'.
 * @param what What object to apply.
 * @param flags Reasonable combination of the following:
 * - <b>APPLY_TOGGLE</b>: Normal apply (toggle)
 * - <b>APPLY_ALWAYS</b>: Always apply (never unapply)
 * - <b>UNAPPLY_ALWAYS</b>: Always unapply (never apply)
 * - <b>UNAPPLY_NOMERGE</b>: Don't merge unapplied items
 * - <b>UNAPPLY_IGNORE_CURSE</b>: Unapply cursed items
 * @retval 0 Object cannot apply objects of that type.
 * @retval 1 Object was applied.
 * @retval 2 Object must be in inventory to be applied. */
static PyObject *Atrinik_Object_Apply(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *what;
	int flags;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &what, &flags))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(what);

	return Py_BuildValue("i", hooks->manual_apply(obj->obj, what->obj, flags));
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
static PyObject *Atrinik_Object_Communicate(Atrinik_Object *obj, PyObject *args)
{
	const char *message;
	char *str;

	if (!PyArg_ParseTuple(args, "s", &message))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	str = hooks->strdup_local(message);
	hooks->communicate(obj->obj, str);
	free(str);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Say(string message)</h1>
 * Object says message to everybody on its map.
 * @param message The message to say. */
static PyObject *Atrinik_Object_Say(Atrinik_Object *obj, PyObject *args)
{
	const char *message;
	char buf[HUGE_BUF];
	int mode = 0;

	if (!PyArg_ParseTuple(args, "s|i", &message, &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	snprintf(buf, sizeof(buf), "%s says: %s", hooks->query_name(obj->obj, NULL), message);
	hooks->draw_info_map(0, COLOR_NAVY, obj->obj->map, obj->obj->x, obj->obj->y, MAP_INFO_NORMAL, NULL, NULL, buf);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Write(string message, string [color = @ref COLOR_ORANGE], int [flags = 0])</h1>
 * Writes a message to a specific player object.
 * @param message The message to write.
 * @param color Color to write the message in. Can be one of
 * @ref COLOR_xxx or a HTML color notation.
 * @param flags Optional flags, one of @ref NDI_xxx. */
static PyObject *Atrinik_Object_Write(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"message", "color", "flags", NULL};
	int flags = 0;
	const char *message, *color = COLOR_ORANGE;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|si", kwlist, &message, &color, &flags))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	hooks->draw_info_flags(flags, color, obj->obj, message);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetGender()</h1>
 * Get an object's gender.
 * @return One of the constants from Gender module @ref plugin_python_constants_gender "constants". */
static PyObject *Atrinik_Object_GetGender(Atrinik_Object *obj, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(obj);

	return Py_BuildValue("i", hooks->object_get_gender(obj->obj));
}

/**
 * <h1>object.SetGender(int gender)</h1>
 * Changes the gender of object.
 * @param gender The new gender to set. One of the constants from Gender module @ref plugin_python_constants_gender "constants". */
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

	/* Update the player's client if object was a player. */
	if (obj->obj->type == PLAYER)
	{
		CONTR(obj->obj)->socket.ext_title_flag = 1;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Fix()</h1>
 * Recalculate player's or monster's stats depending on equipment, forces,
 * skills, etc. */
static PyObject *Atrinik_Object_Fix(Atrinik_Object *obj, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(obj);

	hooks->fix_player(obj->obj);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Hit(object target, int damage)</h1>
 * Make 'object' hit 'target' for the specified amount of damage.
 * @param target The object to hit.
 * @param damage How much damage to do. If -1, the target object will be
 * killed, otherwise the actual damage done is calculated depending on
 * the hitter's attack types, the target's protections, etc.
 * @throws ValueError if 'target' is not on map or is not alive. */
static PyObject *Atrinik_Object_Hit(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *target;
	int damage;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &target, &damage))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(target);

	/* Cannot kill objects that are not alive or on map. */
	if (!target->obj->map || !IS_LIVE(target->obj))
	{
		PyErr_SetString(PyExc_ValueError, "object.Hit(): Invalid object to hit/kill.");
		return NULL;
	}

	/* Kill the target. */
	if (damage == -1)
	{
		target->obj->stats.hp = -1;
		hooks->kill_object(target->obj, 0, obj->obj, 0);
	}
	/* Do damage. */
	else
	{
		hooks->hit_player(target->obj, damage, obj->obj, 0);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Cast(int spell, object [target = None], int [mode = -1], int [direction = 0], string [option = None])</h1>
 * Object casts the spell ID 'spell'.
 * @param spell ID of the spell to cast. You can lookup spell IDs by name
 * using @ref Atrinik_GetSpellNr "GetSpellNr()".
 * @param target Target object for spells that require a valid target.
 * @param mode One of @ref CAST_xxx. If -1 (default), will try to figure
 * out the appropriate mode automatically.
 * @param direction The direction to cast the spell in.
 * @param option Additional string option, required by some spells (create
 * food for example). */
static PyObject *Atrinik_Object_Cast(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"target", "spell", "mode", "direction", "option", NULL};
	Atrinik_Object *target = NULL;
	int spell, direction = 0, mode = -1;
	const char *option = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "i|O!iis", kwlist, &spell, &Atrinik_ObjectType, &target, &mode, &direction, &option))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (target)
	{
		OBJEXISTCHECK(target);
	}

	/* Figure out the mode automatically. */
	if (mode == -1)
	{
		if (obj->obj->type != PLAYER)
		{
			mode = CAST_NPC;
		}
		else
		{
			mode = CAST_NORMAL;
		}
	}
	/* Ensure the mode is valid. */
	else if (mode == CAST_NORMAL && target && target != obj && obj->obj->type != PLAYER)
	{
		mode = CAST_NPC;
	}

	hooks->cast_spell(target ? target->obj : obj->obj, obj->obj, direction, spell, 1, mode, option);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CreateForce(string name, int time)</h1>
 * Create a force object in object's inventory.
 * @param name String ID of the force object.
 * @param time If non-zero, the force will be removed again after
 * time / 0.02 ticks. Optional, defaults to 0.
 * @return The created force object. */
static PyObject *Atrinik_Object_CreateForce(Atrinik_Object *obj, PyObject *args)
{
	const char *name;
	int expire_time = 0;
	object *force;

	if (!PyArg_ParseTuple(args, "s|i", &name, &expire_time))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	force = hooks->get_archetype("force");

	if (expire_time > 0)
	{
		SET_FLAG(force, FLAG_IS_USED_UP);
		force->stats.food = expire_time;
		force->speed = 0.02f;
	}
	else
	{
		force->speed = 0.0;
	}

	hooks->update_ob_speed(force);
	FREE_AND_COPY_HASH(force->name, name);

	return wrap_object(hooks->insert_ob_in_ob(force, obj->obj));
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
static object *object_find_object(object *tmp, int mode, shstr *archname, shstr *name, shstr *title, int type, PyObject *list, int unpaid)
{
	object *tmp2;

	while (tmp)
	{
		if ((!archname || tmp->arch->name == archname) && (!name || tmp->name == name) && (!title || tmp->title == title) && (type == -1 || tmp->type == type) && (!unpaid || QUERY_FLAG(tmp, FLAG_UNPAID)))
		{
			if (list)
			{
				PyList_Append(list, wrap_object(tmp));
			}
			else
			{
				return tmp;
			}
		}

		if (tmp->inv && (mode == INVENTORY_ALL || (mode == INVENTORY_CONTAINERS && tmp->type == CONTAINER)))
		{
			tmp2 = object_find_object(tmp->inv, mode, archname, name, title, type, list, unpaid);

			if (tmp2 && !list)
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
 * <h1>object.FindObject(int [mode = INVENTORY_ONLY], string [archname = None], string [name = None], string [title = None], int [type = -1], bool [multiple = False], bool [unpaid = False])</h1>
 * Looks for a certain object in object's inventory.
 * @param mode How to search the inventory. One of @ref INVENTORY_xxx.
 * @param archname Arch name of the object to search for. If None, can be any.
 * @param name Name of the object. If None, can be any.
 * @param title Title of the object. If None, can be any.
 * @param type Type of the object. If -1, can be any.
 * @param multiple If True, the return value will be a list of all
 * matching objects, instead of just the first one found.
 * @param unpaid Only match unpaid objects.
 * @throws ValueError if there were no conditions to search for.
 * @return The object we wanted if found, None otherwise. */
static PyObject *Atrinik_Object_FindObject(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"mode", "archname", "name", "title", "type", "multiple", "unpaid", NULL};
	uint8 mode = INVENTORY_ONLY;
	int type = -1, multiple = 0, unpaid = 0;
	shstr *archname = NULL, *name = NULL, *title = NULL;
	object *match;
	PyObject *list = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "|Bzzziii", kwlist, &mode, &archname, &name, &title, &type, &multiple, &unpaid))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (!archname && !name && !title && type == -1 && !unpaid)
	{
		PyErr_SetString(PyExc_ValueError, "object.FindObject(): No conditions to search for given.");
		return NULL;
	}

	if (multiple)
	{
		list = PyList_New(0);
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

	match = object_find_object(obj->obj->inv, mode, archname, name, title, type, list, unpaid);

	if (multiple)
	{
		return list;
	}

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
 * @warning Be careful when removing one of the objects involved in the
 * event activation (such as the activator/event/etc). It is recommended
 * you use "SetReturnValue(1)" or similar before the script exits if
 * doing so. */
static PyObject *Atrinik_Object_Remove(Atrinik_Object *obj, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(obj);
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
static PyObject *Atrinik_Object_SetPosition(Atrinik_Object *obj, PyObject *args)
{
	int x, y;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	hooks->transfer_ob(obj->obj, x, y, 0, NULL, NULL);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CastIdentify(object target, int mode, object [marked = None])</h1>
 * Cast identify on target.
 * @param target The target object.
 * @param mode One of @ref identify_modes.
 * @param marked Marked item. */
static PyObject *Atrinik_Object_CastIdentify(Atrinik_Object *obj, PyObject *args)
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
static PyObject *Atrinik_Object_Save(Atrinik_Object *obj, PyObject *args)
{
	PyObject *ret;
	StringBuffer *sb;
	char *result;

	(void) args;
	OBJEXISTCHECK(obj);

	sb = hooks->stringbuffer_new();
	hooks->dump_object_rec(obj->obj, sb);
	result = hooks->stringbuffer_finish(sb);
	ret = Py_BuildValue("s", result);
	free(result);

	return ret;
}

/**
 * <h1>object.GetCost(object what, int type)</h1>
 * Get cost of an object in integer value.
 * @param what The object to query cost for.
 * @param type One of @ref COST_xxx.
 * @return The cost of the item as integer. */
static PyObject *Atrinik_Object_GetCost(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *what;
	int flag;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &what, &flag))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(what);

	return Py_BuildValue("L", hooks->query_cost(what->obj, obj->obj, flag));
}

/**
 * <h1>object.GetMoney()</h1>
 * Get all the money the object is carrying as integer.
 * @note Can only be used on player or container objects.
 * @return The amount of money the object is carrying in copper (as
 * integer). */
static PyObject *Atrinik_Object_GetMoney(Atrinik_Object *obj, PyObject *args)
{
	(void) args;
	OBJEXISTCHECK(obj);

	return Py_BuildValue("L", hooks->query_money(obj->obj));
}

/**
 * <h1>object.PayAmount(int value)</h1>
 * Get the object to pay a specified amount of money in copper.
 * @param value The amount of money in copper to pay for.
 * @return True if the object paid the money (the object had enough money
 * in inventory), False otherwise. */
static PyObject *Atrinik_Object_PayAmount(Atrinik_Object *obj, PyObject *args)
{
	sint64 value;

	if (!PyArg_ParseTuple(args, "L", &value))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	Py_ReturnBoolean(hooks->pay_for_amount(value, obj->obj));
}

/**
 * <h1>object.Clone(int [mode = CLONE_WITH_INVENTORY])</h1>
 * Clone an object.
 *
 * You should do something with the clone.
 * @ref Atrinik_Object_TeleportTo "TeleportTo()" or
 * @ref Atrinik_Object_InsertInside "InsertInside()" are useful functions
 * for that.
 * @param mode Optional mode, one of:
 * - <b>CLONE_WITH_INVENTORY</b> (default)
 * - <b>CLONE_WITHOUT_INVENTORY</b>
 * @return The cloned object. */
static PyObject *Atrinik_Object_Clone(Atrinik_Object *obj, PyObject *args)
{
	int mode = 0;
	object *clone_ob;

	if (!PyArg_ParseTuple(args, "|i", &mode))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (!mode)
	{
		clone_ob = hooks->object_create_clone(obj->obj);
	}
	else
	{
		clone_ob = hooks->get_object();
		hooks->copy_object(obj->obj, clone_ob, 0);
	}

	if (clone_ob->type == PLAYER || QUERY_FLAG(clone_ob, FLAG_IS_PLAYER))
	{
		clone_ob->type = MONSTER;
		CLEAR_FLAG(clone_ob, FLAG_IS_PLAYER);
	}

	return wrap_object(clone_ob);
}

/**
 * <h1>object.ReadKey(string key)</h1>
 * Get key value of an object.
 * @param key Key to look for.
 * @return Value for the key if found, None otherwise. */
static PyObject *Atrinik_Object_ReadKey(Atrinik_Object *obj, PyObject *args)
{
	const char *key;

	if (!PyArg_ParseTuple(args, "s", &key))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	return Py_BuildValue("s", hooks->object_get_value(obj->obj, key));
}

/**
 * <h1>object.WriteKey(string key, string [value = None], int [add_key = True])</h1>
 * Set the key value of an object.
 * @param key Key to look for.
 * @param value Value to set for the key. If not passed, will clear the
 * key's value if the key is found.
 * @param add_key Whether to add the key if it's not found in the object.
 * @return 1 on success, 0 on failure. */
static PyObject *Atrinik_Object_WriteKey(Atrinik_Object *obj, PyObject *args)
{
	const char *key, *value = NULL;
	int add_key = 1;

	if (!PyArg_ParseTuple(args, "s|si", &key, &value, &add_key))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	return Py_BuildValue("i", hooks->object_set_value(obj->obj, key, value, add_key));
}

/**
 * <h1>object.GetName(object [caller = None])</h1>
 * An equivalent of query_base_name().
 * @param caller Object calling this.
 * @return Full name of the object, including material, name, title,
 * etc. */
static PyObject *Atrinik_Object_GetName(Atrinik_Object *what, PyObject *args)
{
	Atrinik_Object *ob = NULL;

	if (!PyArg_ParseTuple(args, "|O!", &Atrinik_ObjectType, &ob))
	{
		return NULL;
	}

	OBJEXISTCHECK(what);

	if (ob)
	{
		OBJEXISTCHECK(ob);
	}

	return Py_BuildValue("s", hooks->query_short_name(what->obj, ob ? ob->obj : what->obj));
}

/**
 * <h1>object.Controller()</h1>
 * Get object's controller (the player).
 * @throws AtrinikError if 'object' is not a player.
 * @return The controller if there is one, None otherwise. */
static PyObject *Atrinik_Object_Controller(Atrinik_Object *what, PyObject *args)
{
	(void) args;

	OBJEXISTCHECK(what);

	if (what->obj->type != PLAYER)
	{
		RAISE("Can only be used on players.");
	}

	return wrap_player(CONTR(what->obj));
}

/**
 * <h1>object.Protection(int nr)</h1>
 * Get object's protection value for the given protection ID.
 * @param nr Protection ID. One of ::_attacks.
 * @throws IndexError if the protection ID is invalid.
 * @return The protection value. */
static PyObject *Atrinik_Object_Protection(Atrinik_Object *what, PyObject *args)
{
	int nr;

	if (!PyArg_ParseTuple(args, "i", &nr))
	{
		return NULL;
	}

	OBJEXISTCHECK(what);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Protection ID is invalid.");
		return NULL;
	}

	return Py_BuildValue("b", what->obj->protection[nr]);
}

/**
 * <h1>object.SetProtection(int nr, int val)</h1>
 * Set object's protection value for the given protection ID.
 * @param nr Protection ID. One of ::_attacks.
 * @param val Value to set.
 * @throws IndexError if the protection ID is invalid.
 * @throws OverflowError if the value to set is not in valid range. */
static PyObject *Atrinik_Object_SetProtection(Atrinik_Object *what, PyObject *args)
{
	int nr;
	sint8 val;

	if (!PyArg_ParseTuple(args, "ib", &nr, &val))
	{
		return NULL;
	}

	OBJEXISTCHECK(what);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Protection ID is invalid.");
		return NULL;
	}

	what->obj->protection[nr] = val;

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Attack(int nr)</h1>
 * Get object's attack value for the given attack ID.
 * @param nr Attack ID. One of ::_attacks.
 * @throws IndexError if the attack ID is invalid.
 * @return The attack value. */
static PyObject *Atrinik_Object_Attack(Atrinik_Object *what, PyObject *args)
{
	int nr;

	if (!PyArg_ParseTuple(args, "i", &nr))
	{
		return NULL;
	}

	OBJEXISTCHECK(what);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Attack ID is invalid.");
		return NULL;
	}

	return Py_BuildValue("B", what->obj->attack[nr]);
}

/**
 * <h1>object.SetAttack(int nr, int val)</h1>
 * Set object's attack value for the given attack ID.
 * @param nr Attack ID. One of ::_attacks.
 * @param val Value to set.
 * @throws IndexError if the attack ID is invalid.
 * @throws OverflowError if the value to set is not in valid range. */
static PyObject *Atrinik_Object_SetAttack(Atrinik_Object *what, PyObject *args)
{
	int nr;
	uint8 val;

	if (!PyArg_ParseTuple(args, "iB", &nr, &val))
	{
		return NULL;
	}

	OBJEXISTCHECK(what);

	if (nr < 0 || nr >= NROFATTACKS)
	{
		PyErr_SetString(PyExc_IndexError, "Attack ID is invalid.");
		return NULL;
	}

	what->obj->attack[nr] = val;

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.ChangeAbil(object what)</h1>
 * Permanently alters an object's stats/flags based on another what.
 * @param what Object that is giving bonuses/penalties to 'object'.
 * @return True if we successfully changed a stat, False if nothing was changed. */
static PyObject *Atrinik_Object_ChangeAbil(Atrinik_Object *obj, PyObject *args)
{
	Atrinik_Object *what;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &what))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);
	OBJEXISTCHECK(what);

	Py_ReturnBoolean(hooks->change_abil(obj->obj, what->obj));
}

/**
 * <h1>object.Decrease(int [num = 1])</h1>
 * Decreases an object, removing it if there's nothing left to decrease.
 * @param num How much to decrease the object by.
 * @return 'object' if something is left, None if the amount reached 0. */
static PyObject *Atrinik_Object_Decrease(Atrinik_Object *what, PyObject *args)
{
	uint32 num = 1;

	if (!PyArg_ParseTuple(args, "|I", &num))
	{
		return NULL;
	}

	OBJEXISTCHECK(what);

	return wrap_object(hooks->decrease_ob_nr(what->obj, num));
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
static PyObject *Atrinik_Object_SquaresAround(Atrinik_Object *what, PyObject *args, PyObject *keywds)
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

	OBJEXISTCHECK(what);

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
			xt = what->obj->x + i;
			yt = what->obj->y + j;

			/* Skip ourselves. */
			if (xt == what->obj->x && yt == what->obj->y)
			{
				continue;
			}

			m = hooks->get_map_from_coord(what->obj->map, &xt, &yt);

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

				m2 = what->obj->map;
				xt2 = what->obj->x;
				yt2 = what->obj->y;

				if (!hooks->get_rangevector_from_mapcoords(m2, xt2, yt2, m, xt, yt, &rv, RV_NO_DISTANCE))
				{
					continue;
				}

				BRESENHAM_INIT(rv.distance_x, rv.distance_y, fraction, stepx, stepy, dx2, dy2);

				while (1)
				{
					BRESENHAM_STEP(xt2, yt2, fraction, stepx, stepy, dx2, dy2);
					m2 = hooks->get_map_from_coord(m2, &xt2, &yt2);

					if (m2 == NULL || (type & AROUND_BLOCKSVIEW && GET_MAP_FLAGS(m2, xt2, yt2) & P_BLOCKSVIEW) || (type & AROUND_PLAYER_ONLY && GET_MAP_FLAGS(m2, xt2, yt2) & P_PLAYER_ONLY) || (type & AROUND_WALL && hooks->wall(m2, xt2, yt2)) || (callable && python_call_int(callable, Py_BuildValue("(OiiO)", wrap_map(m2), xt2, yt2, wrap_object(what->obj)))))
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
				if ((type & AROUND_BLOCKSVIEW && GET_MAP_FLAGS(m, xt, yt) & P_BLOCKSVIEW) || (type & AROUND_PLAYER_ONLY && GET_MAP_FLAGS(m, xt, yt) & P_PLAYER_ONLY) || (type & AROUND_WALL && hooks->wall(m, xt, yt)) || (callable && python_call_int(callable, Py_BuildValue("(OiiO)", wrap_map(m), xt, yt, wrap_object(what->obj)))))
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
 * containing:
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

/**
 * <h1>object.CreateTreasure(string [treasure = None], int [level = -1], int [flags = 0], int [a_chance = @ref ART_CHANCE_UNSET])</h1>
 * Create treasure inside (or below, if GT_ENVIRONMENT flag was set) the object.
 * @param treasure Treasure list name to generate. If None, will try to
 * generate treasure based on the object's randomitems.
 * @param level Level of the generated items. If 0, will try to guess the
 * level to use based on the object's level or the difficulty value of
 * the map the object is on. If neither is applicable, will use MAXLEVEL.
 * @param flags A combination of @ref GT_xxx.
 * @param a_chance Chance for the treasure to become artifact, if possible.
 * A value of 0 will disable any chance for artifacts.
 * @throws ValueError if treasure is not valid. */
static PyObject *Atrinik_Object_CreateTreasure(Atrinik_Object *obj, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"treasure", "level", "flags", "a_chance", NULL};
	const char *treasure_name = NULL;
	int level = 0, flags = 0, a_chance = ART_CHANCE_UNSET;
	treasurelist *t;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "|ziii", kwlist, &treasure_name, &level, &flags, &a_chance))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	/* Figure out the treasure list. */
	if (treasure_name)
	{
		t = hooks->find_treasurelist(treasure_name);
	}
	else
	{
		t = obj->obj->randomitems;
	}

	/* Invalid treasure list. */
	if (!t)
	{
		if (treasure_name)
		{
			PyErr_Format(PyExc_ValueError, "CreateTreasure(): '%s' is not a valid treasure list.", treasure_name);
		}
		else
		{
			PyErr_SetString(PyExc_ValueError, "CreateTreasure(): Object has no treasure list.");
		}

		return NULL;
	}

	/* Figure out the level if none was given. */
	if (!level)
	{
		/* Try the object's level first. */
		if (obj->obj->level)
		{
			level = obj->obj->level;
		}
		/* Otherwise the map's difficulty. */
		else if (obj->obj->map)
		{
			level = obj->obj->map->difficulty;
		}
		/* Default to MAXLEVEL. */
		else
		{
			level = MAXLEVEL;
		}
	}

	/* Create the treasure. */
	hooks->create_treasure(t, obj->obj, flags, level, T_STYLE_UNSET, a_chance, 0, NULL);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Move(int direction)</h1>
 * Move the object in the specified direction. The object must have the
 * correct (combination of) object::terrain_flag set in order to able to
 * move onto the new square.
 * @param direction Direction to move into, one of @ref direction_constants.
 * @throws AtrinikError if object is not on map.
 * @return True if the object was moved successfully, False otherwise. */
static PyObject *Atrinik_Object_Move(Atrinik_Object *obj, PyObject *args)
{
	int direction;

	if (!PyArg_ParseTuple(args, "i", &direction))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (!obj->obj->map)
	{
		PyErr_SetString(AtrinikError, "object.Move(): Object not on map.");
		return NULL;
	}

	if (obj->obj->type == PLAYER)
	{
		Py_ReturnBoolean(hooks->move_player(obj->obj, direction));
	}
	else
	{
		Py_ReturnBoolean(hooks->move_ob(obj->obj, direction, obj->obj));
	}
}

/**
 * <h1>object.Activate()</h1>
 * Activates the object's connection, if it has one. */
static PyObject *Atrinik_Object_Activate(Atrinik_Object *obj, PyObject *args)
{
	(void) args;

	OBJEXISTCHECK(obj);
	hooks->push_button(obj->obj);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Artificate(string name)</h1>
 * Copies artifact abilities to the specified object.
 * @param name Name of the artifact to copy abilities from.
 * @throws AtrinikError if the object already has artifact abilities.
 * @throws AtrinikError if the object's type doesn't match any artifact list.
 * @throws AtrinikError if the artifact name is invalid. */
static PyObject *Atrinik_Object_Artificate(Atrinik_Object *obj, PyObject *args)
{
	const char *name = NULL;
	artifactlist *artlist;
	artifact *art;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (obj->obj->artifact)
	{
		PyErr_SetString(AtrinikError, "Object already has artifact abilities.");
		return NULL;
	}

	artlist = hooks->find_artifactlist(obj->obj->arch->clone.type);

	if (!artlist)
	{
		PyErr_SetString(AtrinikError, "No artifact list matching the object's type.");
		return NULL;
	}

	for (art = artlist->items; art; art = art->next)
	{
		if (!strcmp(art->name, name))
		{
			hooks->give_artifact_abilities(obj->obj, art);
			break;
		}
	}

	if (!art)
	{
		PyErr_SetString(AtrinikError, "Invalid artifact name.");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/*@}*/

/** Available Python methods for the AtrinikObject object */
static PyMethodDef methods[] =
{
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
	{"Write", (PyCFunction) Atrinik_Object_Write, METH_VARARGS | METH_KEYWORDS, 0},
	{"GetGender", (PyCFunction) Atrinik_Object_GetGender, METH_NOARGS, 0},
	{"SetGender", (PyCFunction) Atrinik_Object_SetGender, METH_VARARGS, 0},
	{"Fix", (PyCFunction) Atrinik_Object_Fix, METH_NOARGS, 0},
	{"Hit", (PyCFunction) Atrinik_Object_Hit, METH_VARARGS, 0},
	{"Cast", (PyCFunction) Atrinik_Object_Cast, METH_VARARGS | METH_KEYWORDS, 0},
	{"CreateForce", (PyCFunction) Atrinik_Object_CreateForce, METH_VARARGS, 0},
	{"CreateObject", (PyCFunction) Atrinik_Object_CreateObject, METH_VARARGS | METH_KEYWORDS, 0},
	{"FindObject", (PyCFunction) Atrinik_Object_FindObject, METH_VARARGS | METH_KEYWORDS, 0},
	{"Remove", (PyCFunction) Atrinik_Object_Remove, METH_NOARGS, 0},
	{"SetPosition", (PyCFunction) Atrinik_Object_SetPosition, METH_VARARGS, 0},
	{"CastIdentify", (PyCFunction) Atrinik_Object_CastIdentify, METH_VARARGS, 0},
	{"Save", (PyCFunction) Atrinik_Object_Save, METH_NOARGS, 0},
	{"GetCost", (PyCFunction) Atrinik_Object_GetCost, METH_VARARGS, 0},
	{"GetMoney", (PyCFunction) Atrinik_Object_GetMoney, METH_NOARGS, 0},
	{"PayAmount", (PyCFunction) Atrinik_Object_PayAmount, METH_VARARGS, 0},
	{"Clone", (PyCFunction) Atrinik_Object_Clone, METH_VARARGS, 0},
	{"ReadKey", (PyCFunction) Atrinik_Object_ReadKey, METH_VARARGS, 0},
	{"WriteKey", (PyCFunction) Atrinik_Object_WriteKey, METH_VARARGS, 0},
	{"GetName", (PyCFunction) Atrinik_Object_GetName, METH_VARARGS, 0},
	{"Controller", (PyCFunction) Atrinik_Object_Controller, METH_NOARGS, 0},
	{"Protection", (PyCFunction) Atrinik_Object_Protection, METH_VARARGS, 0},
	{"SetProtection", (PyCFunction) Atrinik_Object_SetProtection, METH_VARARGS, 0},
	{"Attack", (PyCFunction) Atrinik_Object_Attack, METH_VARARGS, 0},
	{"SetAttack", (PyCFunction) Atrinik_Object_SetAttack, METH_VARARGS, 0},
	{"ChangeAbil", (PyCFunction) Atrinik_Object_ChangeAbil, METH_VARARGS, 0},
	{"Decrease", (PyCFunction) Atrinik_Object_Decrease, METH_VARARGS, 0},
	{"SquaresAround", (PyCFunction) Atrinik_Object_SquaresAround, METH_VARARGS | METH_KEYWORDS, 0},
	{"GetRangeVector", (PyCFunction) Atrinik_Object_GetRangeVector, METH_VARARGS, 0},
	{"CreateTreasure", (PyCFunction) Atrinik_Object_CreateTreasure, METH_VARARGS | METH_KEYWORDS, 0},
	{"Move", (PyCFunction) Atrinik_Object_Move, METH_VARARGS, 0},
	{"Activate", (PyCFunction) Atrinik_Object_Activate, METH_NOARGS, 0},
	{"Artificate", (PyCFunction) Atrinik_Object_Artificate, METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

/**
 * Get object's attribute.
 * @param obj Python object wrapper.
 * @param context Void pointer to the field.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *Object_GetAttribute(Atrinik_Object *obj, void *context)
{
	OBJEXISTCHECK(obj);

	return generic_field_getter((fields_struct *) context, obj->obj);
}

/**
 * Set attribute of an object.
 * @param obj Python object wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure. */
static int Object_SetAttribute(Atrinik_Object *obj, PyObject *value, void *context)
{
	object *tmp;
	fields_struct *field = (fields_struct *) context;

	OBJEXISTCHECK_INT(obj);

	if ((field->flags & FIELDFLAG_PLAYER_READONLY) && obj->obj->type == PLAYER)
	{
		INTRAISE("Trying to modify a field that is read-only for player objects.");
	}

	if (generic_field_setter(field, obj->obj, value) == -1)
	{
		return -1;
	}

	/* Make sure the inventory image/text is updated. */
	for (tmp = obj->obj->env; tmp; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			hooks->esrv_send_item(tmp, obj->obj);
		}
	}

	/* Special handling for some player stuff. */
	if (obj->obj->type == PLAYER)
	{
		switch (field->offset)
		{
			case offsetof(object, stats.Str):
				CONTR(obj->obj)->orig_stats.Str = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Dex):
				CONTR(obj->obj)->orig_stats.Dex = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Con):
				CONTR(obj->obj)->orig_stats.Con = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Wis):
				CONTR(obj->obj)->orig_stats.Wis = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Pow):
				CONTR(obj->obj)->orig_stats.Pow = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Cha):
				CONTR(obj->obj)->orig_stats.Cha = (sint8) PyInt_AsLong(value);
				break;

			case offsetof(object, stats.Int):
				CONTR(obj->obj)->orig_stats.Int = (sint8) PyInt_AsLong(value);
				break;
		}

		if (field->flags & FIELDFLAG_PLAYER_FIX)
		{
			hooks->fix_player(obj->obj);
		}
	}

	/* Update object's speed. */
	if (field->offset == offsetof(object, speed))
	{
		hooks->update_ob_speed(obj->obj);
	}
	/* Handle object's type changing. */
	else if (field->offset == offsetof(object, type))
	{
		/* Changing to a spawn point monster requires special handling:
		 * as the object was most likely created and put on active list,
		 * we must remove it from the active list, as spawn point monsters
		 * are not allowed to be on the list. */
		if (obj->obj->type == SPAWN_POINT_MOB)
		{
			float old_speed;

			/* Store original speed, as in order to actually remove the object
			 * from the active list, we need to set its speed to 0 and make it
			 * a non-SPAWN_POINT_MOB type. */
			old_speed = obj->obj->speed;
			obj->obj->speed = 0.0f;
			obj->obj->type = MONSTER;
			/* Remove it from the active list. */
			hooks->update_ob_speed(obj->obj);

			/* Restore original speed and type info. */
			obj->obj->speed = old_speed;
			obj->obj->type = SPAWN_POINT_MOB;
		}
	}
	/* Direction, update object's facing. */
	else if (field->offset == offsetof(object, direction))
	{
		obj->obj->anim_last_facing = obj->obj->anim_last_facing_last = obj->obj->facing = obj->obj->direction;

		/* If the object is animated and turnable, updated its face as well. */
		if (obj->obj->animation_id && QUERY_FLAG(obj->obj, FLAG_IS_TURNABLE))
		{
			SET_ANIMATION(obj->obj, (NUM_ANIMATIONS(obj->obj) / NUM_FACINGS(obj->obj)) * obj->obj->direction + obj->obj->state);
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

/**
 * Atrinik object bool check.
 * @param obj The object. */
static int atrinik_object_bool(Atrinik_Object *obj)
{
	if (!obj || !obj->obj || obj->obj->count != obj->count || OBJECT_FREE(obj->obj))
	{
		return 0;
	}

	return 1;
}

/** This is filled in when we initialize our object type. */
static PyGetSetDef getseters[NUM_FIELDS + NUM_FLAGS + 1];

/**
 * The number protocol for Atrinik objects. */
static PyNumberMethods AtrinikObjectNumber =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	(inquiry) atrinik_object_bool,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

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
	NULL,
	&AtrinikObjectNumber,
	0, 0, 0, 0,
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
