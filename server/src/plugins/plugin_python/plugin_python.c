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
 * Atrinik python plugin. */

#ifdef WIN32
#include <fcntl.h>
#endif
#include <plugin_python.h>

#include <compile.h>
#include <eval.h>
#ifdef STR
/* STR is redefined in node.h. Since this file doesn't use STR, we remove it */
#undef STR
#endif
#include <node.h>

/** Hooks. */
struct plugin_hooklist *hooks;

/** A generic exception that we use for error messages. */
PyObject *AtrinikError;

/** The context stack. */
static PythonContext *context_stack;
/** Current context. */
PythonContext *current_context;

/**
 * Useful constants */
/* @cparser
 * @page plugin_python_constants Python constants
 * <h2>Python constants</h2>
 * List of the Python plugin constants and their meaning. */
static Atrinik_Constant constants[] =
{
	{"NORTH", NORTH},
	{"NORTHEAST", NORTHEAST},
	{"EAST", EAST},
	{"SOUTHEAST", SOUTHEAST},
	{"SOUTH", SOUTH},
	{"SOUTHWEST", SOUTHWEST},
	{"WEST", WEST},
	{"NORTHWEST", NORTHWEST},

	{"llevError", llevError},
	{"llevBug", llevBug},
	{"llevInfo", llevInfo},
	{"llevDebug", llevDebug},

	{"PLUGIN_EVENT_NORMAL", PLUGIN_EVENT_NORMAL},
	{"PLUGIN_EVENT_GLOBAL", PLUGIN_EVENT_GLOBAL},
	{"PLUGIN_EVENT_MAP", PLUGIN_EVENT_MAP},

	{"EVENT_APPLY", EVENT_APPLY},
	{"EVENT_ATTACK", EVENT_ATTACK},
	{"EVENT_DEATH", EVENT_DEATH},
	{"EVENT_DROP", EVENT_DROP},
	{"EVENT_PICKUP", EVENT_PICKUP},
	{"EVENT_SAY", EVENT_SAY},
	{"EVENT_STOP", EVENT_STOP},
	{"EVENT_TIME", EVENT_TIME},
	{"EVENT_THROW", EVENT_THROW},
	{"EVENT_TRIGGER", EVENT_TRIGGER},
	{"EVENT_CLOSE", EVENT_CLOSE},
	{"EVENT_TIMER", EVENT_TIMER},

	{"MEVENT_ENTER", MEVENT_ENTER},
	{"MEVENT_LEAVE", MEVENT_LEAVE},
	{"MEVENT_RESET", MEVENT_RESET},
	{"MEVENT_SPELL_CAST", MEVENT_SPELL_CAST},
	{"MEVENT_SKILL_USED", MEVENT_SKILL_USED},
	{"MEVENT_DROP", MEVENT_DROP},
	{"MEVENT_PICK", MEVENT_PICK},
	{"MEVENT_PUT", MEVENT_PUT},
	{"MEVENT_APPLY", MEVENT_APPLY},
	{"MEVENT_LOGIN", MEVENT_LOGIN},
	{"MEVENT_CMD_DROP", MEVENT_CMD_DROP},
	{"MEVENT_CMD_TAKE", MEVENT_CMD_TAKE},

	{"GEVENT_BORN", GEVENT_BORN},
	{"GEVENT_LOGIN", GEVENT_LOGIN},
	{"GEVENT_LOGOUT", GEVENT_LOGOUT},
	{"GEVENT_PLAYER_DEATH", GEVENT_PLAYER_DEATH},
	{"GEVENT_CACHE_REMOVED", GEVENT_CACHE_REMOVED},

	{"MAP_INFO_NORMAL", MAP_INFO_NORMAL},
	{"MAP_INFO_ALL", MAP_INFO_ALL},

	{"COST_TRUE", F_TRUE},
	{"COST_BUY", F_BUY},
	{"COST_SELL", F_SELL},

	{"APPLY_TOGGLE", 0},
	{"APPLY_ALWAYS", AP_APPLY},
	{"UNAPPLY_ALWAYS", AP_UNAPPLY},
	{"UNAPPLY_NO_MERGE", AP_NO_MERGE},
	{"UNAPPLY_IGNORE_CURSE", AP_IGNORE_CURSE},
	{"APPLY_NO_EVENT", AP_NO_EVENT},

	{"NEUTER", GENDER_NEUTER},
	{"MALE", GENDER_MALE},
	{"FEMALE", GENDER_FEMALE},
	{"HERMAPHRODITE", GENDER_HERMAPHRODITE},

	{"MAXLEVEL", MAXLEVEL},

	{"CAST_NORMAL", 0},
	{"CAST_POTION", 1},

	{"LEARN", 0},
	{"UNLEARN", 1},

	{"UNIDENTIFIED", 0},
	{"IDENTIFIED", 1},

	{"IDENTIFY_NORMAL", IDENTIFY_MODE_NORMAL},
	{"IDENTIFY_ALL", IDENTIFY_MODE_ALL},
	{"IDENTIFY_MARKED", IDENTIFY_MODE_MARKED},

	{"CLONE_WITH_INVENTORY", 0},
	{"CLONE_WITHOUT_INVENTORY", 1},

	{"EXP_AGILITY", EXP_AGILITY},
	{"EXP_MENTAL", EXP_MENTAL},
	{"EXP_MAGICAL", EXP_MAGICAL},
	{"EXP_PERSONAL", EXP_PERSONAL},
	{"EXP_PHYSICAL", EXP_PHYSICAL},
	{"EXP_WISDOM", EXP_WISDOM},

	{"COLOR_WHITE", NDI_WHITE},
	{"COLOR_ORANGE", NDI_ORANGE},
	{"COLOR_NAVY", NDI_NAVY},
	{"COLOR_RED", NDI_RED},
	{"COLOR_GREEN", NDI_GREEN},
	{"COLOR_BLUE", NDI_BLUE},
	{"COLOR_GREY", NDI_GREY},
	{"COLOR_BROWN", NDI_BROWN},
	{"COLOR_PURPLE", NDI_PURPLE},
	{"COLOR_PINK", NDI_PINK},
	{"COLOR_YELLOW", NDI_YELLOW},
	{"COLOR_DK_NAVY", NDI_DK_NAVY},
	{"COLOR_DK_GREEN", NDI_DK_GREEN},
	{"COLOR_DK_ORANGE", NDI_DK_ORANGE},

	{"NDI_SAY", NDI_SAY},
	{"NDI_SHOUT", NDI_SHOUT},
	{"NDI_TELL", NDI_TELL},
	{"NDI_PLAYER", NDI_PLAYER},
	{"NDI_ANIM", NDI_ANIM},
	{"NDI_EMOTE", NDI_EMOTE},
	{"NDI_ALL", NDI_ALL},

	{"PLAYER_EQUIP_MAIL", PLAYER_EQUIP_MAIL},
	{"PLAYER_EQUIP_GAUNTLET", PLAYER_EQUIP_GAUNTLET},
	{"PLAYER_EQUIP_BRACER", PLAYER_EQUIP_BRACER},
	{"PLAYER_EQUIP_HELM", PLAYER_EQUIP_HELM},
	{"PLAYER_EQUIP_BOOTS", PLAYER_EQUIP_BOOTS},
	{"PLAYER_EQUIP_CLOAK", PLAYER_EQUIP_CLOAK},
	{"PLAYER_EQUIP_GIRDLE", PLAYER_EQUIP_GIRDLE},
	{"PLAYER_EQUIP_SHIELD", PLAYER_EQUIP_SHIELD},
	{"PLAYER_EQUIP_RRING", PLAYER_EQUIP_RRING},
	{"PLAYER_EQUIP_LRING", PLAYER_EQUIP_LRING},
	{"PLAYER_EQUIP_AMULET", PLAYER_EQUIP_AMULET},
	{"PLAYER_EQUIP_WEAPON", PLAYER_EQUIP_WEAPON},
	{"PLAYER_EQUIP_BOW", PLAYER_EQUIP_BOW},

	{"QUEST_TYPE_SPECIAL", QUEST_TYPE_SPECIAL},
	{"QUEST_TYPE_KILL", QUEST_TYPE_KILL},
	{"QUEST_TYPE_KILL_ITEM", QUEST_TYPE_KILL_ITEM},
	{"QUEST_STATUS_COMPLETED", QUEST_STATUS_COMPLETED},

	{"TYPE_PLAYER", PLAYER},
	{"TYPE_BULLET", BULLET},
	{"TYPE_ROD", ROD},
	{"TYPE_TREASURE", TREASURE},
	{"TYPE_POTION", POTION},
	{"TYPE_FOOD", FOOD},
	{"TYPE_POISON", POISON},
	{"TYPE_BOOK", BOOK},
	{"TYPE_CLOCK", CLOCK},
	{"TYPE_LIGHTNING", LIGHTNING},
	{"TYPE_ARROW", ARROW},
	{"TYPE_BOW", BOW},
	{"TYPE_WEAPON", WEAPON},
	{"TYPE_ARMOUR", ARMOUR},
	{"TYPE_PEDESTAL", PEDESTAL},
	{"TYPE_ALTAR", ALTAR},
	{"TYPE_CONFUSION", CONFUSION},
	{"TYPE_DOOR", DOOR},
	{"TYPE_KEY", KEY},
	{"TYPE_MAP", MAP},
	{"TYPE_MMISSILE", MMISSILE},
	{"TYPE_TIMED_GATE", TIMED_GATE},
	{"TYPE_TRIGGER", TRIGGER},
	{"TYPE_MAGIC_EAR", MAGIC_EAR},
	{"TYPE_TRIGGER_BUTTON", TRIGGER_BUTTON},
	{"TYPE_TRIGGER_ALTAR", TRIGGER_ALTAR},
	{"TYPE_TRIGGER_PEDESTAL", TRIGGER_PEDESTAL},
	{"TYPE_SHIELD", SHIELD},
	{"TYPE_HELMET", HELMET},
	{"TYPE_HORN", HORN},
	{"TYPE_MONEY", MONEY},
	{"TYPE_CLASS", CLASS},
	{"TYPE_GRAVESTONE", GRAVESTONE},
	{"TYPE_AMULET", AMULET},
	{"TYPE_PLAYERMOVER", PLAYERMOVER},
	{"TYPE_TELEPORTER", TELEPORTER},
	{"TYPE_CREATOR", CREATOR},
	{"TYPE_SKILL", SKILL},
	{"TYPE_EXPERIENCE", EXPERIENCE},
	{"TYPE_BOMB", BOMB},
	{"TYPE_THROWN_OBJ", THROWN_OBJ},
	{"TYPE_BLINDNESS", BLINDNESS},
	{"TYPE_GOD", GOD},
	{"TYPE_DETECTOR", DETECTOR},
	{"TYPE_SKILL_ITEM", SKILL_ITEM},
	{"TYPE_DEAD_OBJECT", DEAD_OBJECT},
	{"TYPE_DRINK", DRINK},
	{"TYPE_MARKER", MARKER},
	{"TYPE_HOLY_ALTAR", HOLY_ALTAR},
	{"TYPE_PEARL", PEARL},
	{"TYPE_GEM", GEM},
	{"TYPE_FIREWALL", FIREWALL},
	{"TYPE_CHECK_INV", CHECK_INV},
	{"TYPE_MOOD_FLOOR", MOOD_FLOOR},
	{"TYPE_EXIT", EXIT},
	{"TYPE_SHOP_FLOOR", SHOP_FLOOR},
	{"TYPE_SHOP_MAT", SHOP_MAT},
	{"TYPE_RING", RING},
	{"TYPE_FLOOR", FLOOR},
	{"TYPE_FLESH", FLESH},
	{"TYPE_INORGANIC", INORGANIC},
	{"TYPE_LIGHT_APPLY", LIGHT_APPLY},
	{"TYPE_LIGHTER", LIGHTER},
	{"TYPE_WALL", WALL},
	{"TYPE_LIGHT_SOURCE", LIGHT_SOURCE},
	{"TYPE_MISC_OBJECT", MISC_OBJECT},
	{"TYPE_MONSTER", MONSTER},
	{"TYPE_SPAWN_POINT", SPAWN_POINT},
	{"TYPE_LIGHT_REFILL", LIGHT_REFILL},
	{"TYPE_SPAWN_POINT_MOB", SPAWN_POINT_MOB},
	{"TYPE_SPAWN_POINT_INFO", SPAWN_POINT_INFO},
	{"TYPE_SPELLBOOK", SPELLBOOK},
	{"TYPE_ORGANIC", ORGANIC},
	{"TYPE_CLOAK", CLOAK},
	{"TYPE_CONE", CONE},
	{"TYPE_SPINNER", SPINNER},
	{"TYPE_GATE", GATE},
	{"TYPE_BUTTON", BUTTON},
	{"TYPE_HANDLE", HANDLE},
	{"TYPE_PIT", PIT},
	{"TYPE_TRAPDOOR", TRAPDOOR},
	{"TYPE_WORD_OF_RECALL", WORD_OF_RECALL},
	{"TYPE_SIGN", SIGN},
	{"TYPE_BOOTS", BOOTS},
	{"TYPE_GLOVES", GLOVES},
	{"TYPE_BASE_INFO", BASE_INFO},
	{"TYPE_RANDOM_DROP", RANDOM_DROP},
	{"TYPE_CONVERTER", CONVERTER},
	{"TYPE_BRACERS", BRACERS},
	{"TYPE_POISONING", POISONING},
	{"TYPE_SAVEBED", SAVEBED},
	{"TYPE_WAND", WAND},
	{"TYPE_ABILITY", ABILITY},
	{"TYPE_SCROLL", SCROLL},
	{"TYPE_DIRECTOR", DIRECTOR},
	{"TYPE_GIRDLE", GIRDLE},
	{"TYPE_FORCE", FORCE},
	{"TYPE_POTION_EFFECT", POTION_EFFECT},
	{"TYPE_JEWEL", JEWEL},
	{"TYPE_NUGGET", NUGGET},
	{"TYPE_EVENT_OBJECT", EVENT_OBJECT},
	{"TYPE_WAYPOINT_OBJECT", WAYPOINT_OBJECT},
	{"TYPE_QUEST_CONTAINER", QUEST_CONTAINER},
	{"TYPE_CLOSE_CON", CLOSE_CON},
	{"TYPE_CONTAINER", CONTAINER},
	{"TYPE_ARMOUR_IMPROVER", ARMOUR_IMPROVER},
	{"TYPE_WEAPON_IMPROVER", WEAPON_IMPROVER},
	{"TYPE_WEALTH", WEALTH},
	{"TYPE_SKILLSCROLL", SKILLSCROLL},
	{"TYPE_DEEP_SWAMP", DEEP_SWAMP},
	{"TYPE_IDENTIFY_ALTAR", IDENTIFY_ALTAR},
	{"TYPE_SWARM_SPELL", SWARM_SPELL},
	{"TYPE_RUNE", RUNE},
	{"TYPE_POWER_CRYSTAL", POWER_CRYSTAL},
	{"TYPE_CORPSE", CORPSE},
	{"TYPE_DISEASE", DISEASE},
	{"TYPE_SYMPTOM", SYMPTOM},

	{"PARTY_MESSAGE_STATUS", PARTY_MESSAGE_STATUS},
	{"PARTY_MESSAGE_CHAT", PARTY_MESSAGE_CHAT},

	{"COST_TRUE", F_TRUE},
	{"COST_BUY", F_BUY},
	{"COST_SELL", F_SELL},

	{"ATNR_IMPACT", ATNR_IMPACT},
	{"ATNR_SLASH", ATNR_SLASH},
	{"ATNR_CLEAVE", ATNR_CLEAVE},
	{"ATNR_PIERCE", ATNR_PIERCE},
	{"ATNR_WEAPON_MAGIC", ATNR_WEAPON_MAGIC},
	{"ATNR_FIRE", ATNR_FIRE},
	{"ATNR_COLD", ATNR_COLD},
	{"ATNR_ELECTRICITY", ATNR_ELECTRICITY},
	{"ATNR_POISON", ATNR_POISON},
	{"ATNR_ACID", ATNR_ACID},
	{"ATNR_MAGIC", ATNR_MAGIC},
	{"ATNR_MIND", ATNR_MIND},
	{"ATNR_BLIND", ATNR_BLIND},
	{"ATNR_PARALYZE", ATNR_PARALYZE},
	{"ATNR_FORCE", ATNR_FORCE},
	{"ATNR_GODPOWER", ATNR_GODPOWER},
	{"ATNR_CHAOS", ATNR_CHAOS},
	{"ATNR_DRAIN", ATNR_DRAIN},
	{"ATNR_SLOW", ATNR_SLOW},
	{"ATNR_CONFUSION", ATNR_CONFUSION},
	{"ATNR_INTERNAL", ATNR_INTERNAL},
	{"NROFATTACKS", NROFATTACKS},

	{"TERRAIN_NOTHING", TERRAIN_NOTHING},
	{"TERRAIN_AIRBREATH", TERRAIN_NOTHING},
	{"TERRAIN_WATERWALK", TERRAIN_WATERWALK},
	{"TERRAIN_WATERBREATH", TERRAIN_WATERBREATH},
	{"TERRAIN_FIREWALK", TERRAIN_FIREWALK},
	{"TERRAIN_FIREBREATH", TERRAIN_FIREBREATH},
	{"TERRAIN_CLOUDWALK", TERRAIN_CLOUDWALK},
	{"TERRAIN_ALL", TERRAIN_ALL},

	{"P_BLOCKSVIEW", P_BLOCKSVIEW},
	{"P_NO_MAGIC", P_NO_MAGIC},
	{"P_NO_PASS", P_NO_PASS},
	{"P_IS_PLAYER", P_IS_PLAYER},
	{"P_IS_ALIVE", P_IS_ALIVE},
	{"P_NO_CLERIC", P_NO_CLERIC},
	{"P_PLAYER_ONLY", P_PLAYER_ONLY},
	{"P_DOOR_CLOSED", P_DOOR_CLOSED},
	{"P_CHECK_INV", P_CHECK_INV},
	{"P_NO_PVP", P_NO_PVP},
	{"P_PASS_THRU", P_PASS_THRU},
	{"P_MAGIC_EAR", P_MAGIC_EAR},
	{"P_WALK_ON", P_WALK_ON},
	{"P_WALK_OFF", P_WALK_OFF},
	{"P_FLY_OFF", P_FLY_OFF},
	{"P_FLY_ON", P_FLY_ON},
	{"P_MAGIC_MIRROR", P_MAGIC_MIRROR},
	{"P_OUT_OF_MAP", P_OUT_OF_MAP},
	{"P_FLAGS_ONLY", P_FLAGS_ONLY},
	{"P_FLAGS_UPDATE", P_FLAGS_UPDATE},
	{"P_NEED_UPDATE", P_NEED_UPDATE},
	{"P_NO_ERROR", P_NO_ERROR},
	{"P_NO_TERRAIN", P_NO_TERRAIN},

	{"CMD_SOUND_EFFECT", CMD_SOUND_EFFECT},
	{"CMD_SOUND_BACKGROUND", CMD_SOUND_BACKGROUND},
	{"CMD_SOUND_ABSOLUTE", CMD_SOUND_ABSOLUTE},

	{"BANK_SYNTAX_ERROR", BANK_SYNTAX_ERROR},
	{"BANK_SUCCESS", BANK_SUCCESS},

	{"BANK_WITHDRAW_HIGH", BANK_WITHDRAW_HIGH},
	{"BANK_WITHDRAW_MISSING", BANK_WITHDRAW_MISSING},
	{"BANK_WITHDRAW_OVERWEIGHT", BANK_WITHDRAW_OVERWEIGHT},

	{"BANK_DEPOSIT_COPPER", BANK_DEPOSIT_COPPER},
	{"BANK_DEPOSIT_SILVER", BANK_DEPOSIT_SILVER},
	{"BANK_DEPOSIT_GOLD", BANK_DEPOSIT_GOLD},
	{"BANK_DEPOSIT_MITHRIL", BANK_DEPOSIT_MITHRIL},

	{"AROUND_ALL", AROUND_ALL},
	{"AROUND_WALL", AROUND_WALL},
	{"AROUND_BLOCKSVIEW", AROUND_BLOCKSVIEW},
	{"AROUND_PLAYER_ONLY", AROUND_PLAYER_ONLY},

	{"LAYER_SYS", LAYER_SYS},
	{"LAYER_FLOOR", LAYER_FLOOR},
	{"LAYER_FMASK", LAYER_FMASK},
	{"LAYER_ITEM", LAYER_ITEM},
	{"LAYER_ITEM2", LAYER_ITEM2},
	{"LAYER_WALL", LAYER_WALL},
	{"LAYER_LIVING", LAYER_LIVING},
	{"LAYER_EFFECT", LAYER_EFFECT},

	{NULL, 0}
};
/* @endcparser */

/** All the custom commands. */
static PythonCmd CustomCommand[NR_CUSTOM_CMD];
/** Contains the index of the next command that needs to be run. */
static int NextCustomCommand;

/** Maximum number of cached scripts. */
#define PYTHON_CACHE_SIZE 256

/** One cache entry. */
typedef struct
{
	/** The script file. */
	const char *file;

	/** The cached code. */
	PyCodeObject *code;

	/** Last cached time. */
	time_t cached_time;

	/** Last used time. */
	time_t used_time;
} cacheentry;

/** The Python cache. */
static cacheentry python_cache[PYTHON_CACHE_SIZE];

static int cmd_customPython(object *op, char *params);

/**
 * Initialize the context stack. */
static void initContextStack()
{
	current_context = NULL;
	context_stack = NULL;
}

/**
 * Push context to the context stack and to current context.
 * @param context The context to push. */
static void pushContext(PythonContext *context)
{
	if (current_context == NULL)
	{
		context_stack = context;
		context->down = NULL;
	}
	else
	{
		context->down = current_context;
	}

	current_context = context;
}

/**
 * Pop the first context from the current context, replacing it by the
 * next one in the list.
 * @return NULL if there is no current context, the previous current
 * context otherwise. */
static PythonContext *popContext()
{
	PythonContext *oldcontext;

	if (current_context)
	{
		oldcontext = current_context;
		current_context = current_context->down;
		return oldcontext;
	}

	return NULL;
}

/**
 * Free a context.
 * @param context Context to free. */
static void freeContext(PythonContext *context)
{
	free(context);
}

/**
 * @defgroup plugin_python_functions Python functions
 *@{*/

/**
 * <h1>Atrinik.LoadObject(<i>\<string\></i> string)</h1>
 *
 * Load an object from string, for example, one stored using
 * @ref Atrinik_Object_Save "Save()".
 * @param string The string from which to load the actual object. */
static PyObject *Atrinik_LoadObject(PyObject *self, PyObject *args)
{
	char *dumpob;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &dumpob))
	{
		return NULL;
	}

	return wrap_object(hooks->load_object_str(dumpob));
}

/**
 * <h1>Atrinik.MatchString(<i>\<string\></i> firststr, <i>\<string\></i>
 * secondstr)</h1>
 *
 * Case insensitive string comparison.
 *
 * @param firststr The first string
 * @param secondstr The second string, can contain regular expressions
 * @return 1 if the two strings are the same, or 0 if they differ */
static PyObject *Atrinik_MatchString(PyObject *self, PyObject *args)
{
	char *premiere, *seconde;

	(void) self;

	if (!PyArg_ParseTuple(args, "ss", &premiere, &seconde))
	{
		return NULL;
	}

	return Py_BuildValue("i", (hooks->re_cmp(premiere, seconde) != NULL) ? 1 : 0);
}

/**
 * <h1>Atrinik.ReadyMap(<i>\<string\></i> name, <i>\<int\></i> unique)
 * </h1>
 *
 * Make sure the named map is loaded into memory.
 *
 * @param name Path to the map
 * @param unique Must be 1 if the map is unique. Optional, defaults to 0.
 * @return The loaded Atrinik map
 * @todo Don't crash if unique is wrong */
static PyObject *Atrinik_ReadyMap(PyObject *self, PyObject *args)
{
	char *mapname;
	int flags = 0, unique = 0;

	(void) self;

	if (!PyArg_ParseTuple(args, "s|i", &mapname, &unique))
	{
		return NULL;
	}

	if (unique)
	{
		flags = MAP_PLAYER_UNIQUE;
	}

	return wrap_map(hooks->ready_map_name(mapname, flags));
}

/**
 * <h1>Atrinik.CheckMap(<i>\<string\></i> arch, <i>\<string\></i>
 * map_path, <i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * @warning Unfinished, do not use.
 * @todo Finish. */
static PyObject *Atrinik_CheckMap(PyObject *self, PyObject *args)
{
	char *what;
	char *mapstr;
	int x, y;
	/*  object* foundob; */

	(void) self;

	/* Gecko: replaced coordinate tuple with separate x and y coordinates */
	if (!PyArg_ParseTuple(args, "ssii", &what, &mapstr, &x, &y))
	{
		return NULL;
	}

	RAISE("CheckMap() is not finished!");

	/*  foundob = present_arch(find_archetype(what), has_been_loaded(mapstr), x, y);
	    return wrap_object(foundob);*/
}

/**
 * <h1>Atrinik.FindPlayer(<i>\<string\></i> name)</h1>
 * Find a player.
 *
 * @param name The player name
 * @return The player's object if found, None otherwise */
static PyObject *Atrinik_FindPlayer(PyObject *self, PyObject *args)
{
	player *foundpl;
	object *foundob = NULL;
	char *txt;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	hooks->adjust_player_name(txt);
	foundpl = hooks->find_player(txt);

	if (foundpl != NULL)
	{
		foundob = foundpl->ob;
	}

	return wrap_object(foundob);
}

/**
 * <h1>Atrinik.PlayerExists(<i>\<string\></i> name)</h1>
 * Check if player exists.
 *
 * @param name The player name
 * @return 1 if the player exists, 0 otherwise */
static PyObject *Atrinik_PlayerExists(PyObject *self, PyObject *args)
{
	char *player_name;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &player_name))
	{
		return NULL;
	}

	hooks->adjust_player_name(player_name);

	return Py_BuildValue("i", hooks->player_exists(player_name));
}

/**
 * <h1>Atrinik.WhoAmI()</h1>
 * Get the owner of the active script (the object that has the event
 * handler).
 * @return The script owner. */
static PyObject *Atrinik_WhoAmI(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->who);
}

/**
 * <h1>Atrinik.WhoIsActivator()</h1>
 * Get the object that activated the current event.
 * @return The script activator. */
static PyObject *Atrinik_WhoIsActivator(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->activator);
}

/**
 * <h1>Atrinik.WhoIsOther()</h1>
 * @warning Untested. */
static PyObject *Atrinik_WhoIsOther(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->other);
}

/**
 * <h1>Atrinik.WhatIsEvent()</h1>
 * Get the event object that caused this event to trigger.
 * @return The event object. */
static PyObject *Atrinik_WhatIsEvent(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->event);
}

/**
 * <h1>Atrinik.GetEventNumber()</h1>
 * Get the ID of the event that is being triggered.
 * @return Event ID. */
static PyObject *Atrinik_GetEventNumber(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("i", current_context->event->sub_type);
}

/**
 * <h1>Atrinik.WhatIsMessage()</h1>
 * Gets the actual message in SAY events.
 * @return The message. */
static PyObject *Atrinik_WhatIsMessage(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("s", current_context->text);
}

/**
 * <h1>Atrinik.GetOptions()</h1>
 * Gets the script options (as passed in the event's slaying field).
 * @return The script options. */
static PyObject *Atrinik_GetOptions(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("s", current_context->options);
}

/**
 * <h1>Atrinik.GetReturnValue()</h1>
 * Gets the script's return value.
 * @return The return value */
static PyObject *Atrinik_GetReturnValue(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("i", current_context->returnvalue);
}

/**
 * <h1>Atrinik.SetReturnValue(<i>\<int\></i> value)</h1>
 * Sets the script's return value.
 * @param value The new return value */
static PyObject *Atrinik_SetReturnValue(PyObject *self, PyObject *args)
{
	int value;

	(void) self;

	if (!PyArg_ParseTuple(args, "i", &value))
	{
		return NULL;
	}

	current_context->returnvalue = value;

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>Atrinik.GetEventParameters()</h1>
 * Get the parameters of an event. This varies from event to event, and
 * some events pass all parameters as 0. EVENT_ATTACK usually passes damage
 * done and the WC of the hit as second and third parameter, respectively.
 * @return A list of the event parameters. The last entry is the event flags,
 * used to determine whom to call fix_player() on after executing the script. */
static PyObject *Atrinik_GetEventParameters(PyObject *self, PyObject *args)
{
	unsigned int i;
	PyObject *list = PyList_New(0);

	(void) self;
	(void) args;

	for (i = 0; i < sizeof(current_context->parms) / sizeof(current_context->parms[0]); i++)
	{
		PyList_Append(list, Py_BuildValue("i", current_context->parms[i]));
	}

	return list;
}

/**
 * <h1>Atrinik.GetSpellNr(<i>\<string\></i> name)</h1>
 * Gets the number of the named spell.
 * @param name The spell name
 * @return Number of the spell, -1 if no such spell exists. */
static PyObject *Atrinik_GetSpellNr(PyObject *self, PyObject *args)
{
	char *spell;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &spell))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->look_up_spell_name(spell));
}

/**
 * <h1>Atrinik.GetSpell(<i>\<int\></i> spell)</h1>
 * Get various information about a spell, including things like its
 * level, type, etc.
 * @param spell ID of the spell. */
static PyObject *Atrinik_GetSpell(PyObject *self, PyObject *args)
{
	int spell;
	PyObject *dict;

	(void) self;

	if (!PyArg_ParseTuple(args, "i", &spell))
	{
		return NULL;
	}

	if (spell < 0 || spell >= NROFREALSPELLS)
	{
		RAISE("Invalid ID of a spell.");
	}

	dict = PyDict_New();

	PyDict_SetItemString(dict, "name", Py_BuildValue("s", hooks->spells[spell].name));
	PyDict_SetItemString(dict, "level", Py_BuildValue("i", hooks->spells[spell].level));
	PyDict_SetItemString(dict, "type", Py_BuildValue("s", hooks->spells[spell].type == SPELL_TYPE_WIZARD ? "wizard" : "priest"));
	PyDict_SetItemString(dict, "sp", Py_BuildValue("i", hooks->spells[spell].sp));
	PyDict_SetItemString(dict, "time", Py_BuildValue("i", hooks->spells[spell].time));

	return dict;
}

/**
 * <h1>Atrinik.GetSkillNr(<i>\<string\></i> name)</h1>
 * Gets the number of the named skill.
 * @param name The skill name
 * @return Number of the skill, -1 if no such skill exists. */
static PyObject *Atrinik_GetSkillNr(PyObject *self, PyObject *args)
{
	char *skill;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &skill))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->lookup_skill_by_name(skill));
}

/**
 * <h1>Atrinik.RegisterCommand(<i>\<string\></i> cmdname,
 * <i>\<string\></i> scriptname, <i>\<double\></i> speed)</h1>
 * Register a custom command. */
static PyObject *Atrinik_RegisterCommand(PyObject *self, PyObject *args)
{
	char *cmdname, *scriptname;
	double cmdspeed;
	int i;

	(void) self;

	if (!PyArg_ParseTuple(args, "ssd", &cmdname, &scriptname, &cmdspeed))
	{
		return NULL;
	}

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		if (CustomCommand[i].name)
		{
			if (!strcmp(CustomCommand[i].name, cmdname))
			{
				RAISE("This command is already registered");
			}
		}
	}

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		if (CustomCommand[i].name == NULL)
		{
			CustomCommand[i].name = hooks->strdup_local(cmdname);
			CustomCommand[i].script = hooks->strdup_local(scriptname);
			CustomCommand[i].speed = cmdspeed;
			break;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>Atrinik.CreatePathname(<i>\<string\></i> path)</h1>
 * Creates path to file in the maps directory using the create_pathname()
 * function.
 * @param path Path to file to create.
 * @return The path to file in the maps directory. */
static PyObject *Atrinik_CreatePathname(PyObject *self, PyObject *args)
{
	char *path;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &path))
	{
		return NULL;
	}

	return Py_BuildValue("s", hooks->create_pathname(path));
}

/**
 * <h1>Atrinik.GetTime()</h1>
 * Get game time using a hook for get_tod().
 * @return A dictionary containing all the information about the in-game
 * time:
 * - <b>year</b>: Current year.
 * - <b>month</b>: Current month.
 * - <b>month_name</b>: Name of the current month.
 * - <b>day</b>: Day.
 * - <b>hour</b>: Hour.
 * - <b>minute</b>: Minute.
 * - <b>dayofweek</b>: Day of the week.
 * - <b>dayofweek_name</b>: Name of the week day.
 * - <b>season</b>: Season.
 * - <b>season_name</b>: Name of the season.
 * - <b>periodofday</b>: Period of the day.
 * - <b>periodofday_name</b>: Name of the period of the day. */
static PyObject *Atrinik_GetTime(PyObject *self, PyObject *args)
{
	PyObject *dict = PyDict_New();
	timeofday_t tod;

	(void) self;
	(void) args;

	hooks->get_tod(&tod);

	PyDict_SetItemString(dict, "year", Py_BuildValue("i", tod.year + 1));
	PyDict_SetItemString(dict, "month", Py_BuildValue("i", tod.month + 1));
	PyDict_SetItemString(dict, "month_name", Py_BuildValue("s", hooks->month_name[tod.month]));
	PyDict_SetItemString(dict, "day", Py_BuildValue("i", tod.day + 1));
	PyDict_SetItemString(dict, "hour", Py_BuildValue("i", tod.hour));
	PyDict_SetItemString(dict, "minute", Py_BuildValue("i", tod.minute + 1));
	PyDict_SetItemString(dict, "dayofweek", Py_BuildValue("i", tod.dayofweek + 1));
	PyDict_SetItemString(dict, "dayofweek_name", Py_BuildValue("s", hooks->weekdays[tod.dayofweek]));
	PyDict_SetItemString(dict, "season", Py_BuildValue("i", tod.season + 1));
	PyDict_SetItemString(dict, "season_name", Py_BuildValue("s", hooks->season_name[tod.season]));
	PyDict_SetItemString(dict, "periodofday", Py_BuildValue("i", tod.periodofday + 1));
	PyDict_SetItemString(dict, "periodofday_name", Py_BuildValue("s", hooks->periodsofday[tod.periodofday]));

	return dict;
}

/**
 * <h1>Atrinik.LocateBeacon(<i>\<string\></i> beacon_name)</h1>
 * Locate a beacon.
 * @param beacon_name The beacon name to find.
 * @return The beacon if found, None otherwise. */
static PyObject *Atrinik_LocateBeacon(PyObject *self, PyObject *args)
{
	char *beacon_name;
	const char *name = NULL;
	object *myob;

	if (!PyArg_ParseTuple(args, "s", &beacon_name))
	{
		return NULL;
	}

	(void) self;

	FREE_AND_COPY_HASH(name, beacon_name);
	myob = hooks->beacon_locate(name);
	FREE_AND_CLEAR_HASH(name);

	return wrap_object(myob);
}

/**
 * <h1>Atrinik.FindParty(<i>\<string\></i> partyname)</h1>
 * Find a party by name.
 * @param partyname The party name to find.
 * @return The party if found, None otherwise. */
static PyObject *Atrinik_FindParty(PyObject *self, PyObject *args)
{
	char *partyname;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &partyname))
	{
		return NULL;
	}

	return wrap_party(hooks->find_party(partyname));
}

/**
 * <h1>Atrinik.CleanupChatString(<i>\<string\></i> string)</h1>
 * Cleans up a chat string, using cleanup_chat_string().
 * @param string The string to cleanup.
 * @return Cleaned up string - can be None. */
static PyObject *Atrinik_CleanupChatString(PyObject *self, PyObject *args)
{
	char *string;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &string))
	{
		return NULL;
	}

	return Py_BuildValue("s", hooks->cleanup_chat_string(string));
}

/**
 * <h1>Atrinik.LOG(<i>\<int\></i> mode, <i>\<string\></i> string)</h1>
 * Logs a message.
 * @param mode Logging mode to use, one of:
 * - llevError: An irrecoverable error. Will shut down the server.
 * - llevBug: A bug.
 * - llevInfo: Info.
 * - llevDebug: Debug information.
 * @param string The message to log. */
static PyObject *Atrinik_LOG(PyObject *self, PyObject *args)
{
	char *string;
	int mode;

	(void) self;

	if (!PyArg_ParseTuple(args, "is", &mode, &string))
	{
		return NULL;
	}

	LOG(mode, string);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>Atrinik.DestroyTimer(<i>\<int\></i> timer)</h1>
 * Destroy an existing timer.
 * @param timer ID of the timer.
 * @return 0 on success, anything lower on failure. */
static PyObject *Atrinik_DestroyTimer(PyObject *self, PyObject *args)
{
	int id;

	(void) self;

	if (!PyArg_ParseTuple(args, "i", &id))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->cftimer_destroy(id));
}

/**
 * <h1>Atrinik.FindFace(<i>\<string\></i> face)</h1>
 * Find a face ID by its name.
 * @param face Name of the face to find.
 * @return ID of the face. */
static PyObject *Atrinik_FindFace(PyObject *self, PyObject *args)
{
	char *name;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->find_face(name, 0));
}

/**
 * <h1>Atrinik.FindAnimation(<i>\<string\></i> animation)</h1>
 * Find an animation ID by its name.
 * @param animation Name of the animation to find.
 * @return ID of the animation. */
static PyObject *Atrinik_FindAnimation(PyObject *self, PyObject *args)
{
	char *name;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->find_animation(name));
}

/**
 * <h1>Atrinik.GetGenderStr(<i>\<int\></i> gender, <i>\<string\></i> type)</h1>
 * Get string representation of a gender ID depending on 'type'.
 * @param gender Gender ID. One of @ref GENDER_xxx, or -1 to get a list of
 * possible genders.
 * @param type Type of the gender string to get:
 * - <b>noun</b>: 'male', 'female', ...
 * - <b>subjective</b>: 'he', 'she', ...
 * - <b>subjective_upper</b>: 'He', 'She', ...
 * - <b>objective</b>: 'him', 'her', 'it', ...
 * - <b>possessive</b>: 'his', 'her', ...
 * - <b>reflexive</b>: 'himself', 'herself', ...
 * @return String representation of the gender, or a list of possible genders
 * if 'gender' was -1. */
static PyObject *Atrinik_GetGenderStr(PyObject *self, PyObject *args)
{
	int gender;
	char *type;
	const char **arr;

	(void) self;

	if (!PyArg_ParseTuple(args, "is", &gender, &type))
	{
		return NULL;
	}

	if (gender < -1 || gender >= GENDER_MAX)
	{
		RAISE("GetGenderStr(): Invalid value for gender parameter.");
	}

	if (!strcmp(type, "noun"))
	{
		arr = hooks->gender_noun;
	}
	else if (!strcmp(type, "subjective"))
	{
		arr = hooks->gender_subjective;
	}
	else if (!strcmp(type, "subjective_upper"))
	{
		arr = hooks->gender_subjective_upper;
	}
	else if (!strcmp(type, "objective"))
	{
		arr = hooks->gender_objective;
	}
	else if (!strcmp(type, "possessive"))
	{
		arr = hooks->gender_possessive;
	}
	else if (!strcmp(type, "reflexive"))
	{
		arr = hooks->gender_reflexive;
	}
	else
	{
		RAISE("GetGenderStr(): Invalid value for type parameter.");
	}

	if (gender == -1)
	{
		PyObject *list = PyList_New(0);
		size_t i;

		for (i = 0; i < GENDER_MAX; i++)
		{
			PyList_Append(list, Py_BuildValue("s", arr[i]));
		}

		return list;
	}

	return Py_BuildValue("s", arr[gender]);
}

/**
 * <h1>GetRangeVectorFromMapCoords(map map, int x, int y, map map2, int x2, int y2, int [flags = 0])</h1>
 * Get the distance and direction from one map coordinate to another.
 * @param map From which map to get distance from.
 * @param x X on 'map'.
 * @param y Y on 'map'.
 * @param map2 Which map to get distance to.
 * @param x2 X on 'map2'.
 * @param y2 Y on 'map2'.
 * @param flags One or a combination of @ref range_vector_flags.
 * @return None if the distance couldn't be calculated, otherwise a tuple
 * containining:
 *  - Direction from the first coordinate to the second, one of @ref direction_constants.
 *  - Distance between the two coordinates.
 *  - X distance.
 *  - Y distance. */
static PyObject *Atrinik_GetRangeVectorFromMapCoords(PyObject *self, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"map", "x", "y", "map2", "x2", "y2", NULL};
	Atrinik_Map *map, *map2;
	int x, y, x2, y2, flags = 0;
	rv_vector rv;
	PyObject *tuple;

	(void) self;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!iiO!ii|i", kwlist, &Atrinik_MapType, &map, &x, &y, &Atrinik_MapType, &map2, &x2, &y2, &flags))
	{
		return NULL;
	}

	if (!hooks->get_rangevector_from_mapcoords(map->map, x, y, map2->map, x2, y2, &rv, flags))
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	tuple = PyTuple_New(4);
	PyTuple_SET_ITEM(tuple, 0, Py_BuildValue("i", rv.direction));
	PyTuple_SET_ITEM(tuple, 1, Py_BuildValue("i", rv.distance));
	PyTuple_SET_ITEM(tuple, 2, Py_BuildValue("i", rv.distance_x));
	PyTuple_SET_ITEM(tuple, 3, Py_BuildValue("i", rv.distance_y));

	return tuple;
}

/**
 * <h1>CostString(int value)</h1>
 * Build a string representation of the value in the game's money syntax, for
 * example, a value of 134 would become "1 silver coin and 34 copper coins".
 * @param value Value to build the string from.
 * @return The built string. */
static PyObject *Atrinik_CostString(PyObject *self, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"value", NULL};
	sint64 value;

	(void) self;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "L", kwlist, &value))
	{
		return NULL;
	}

	return Py_BuildValue("s", hooks->cost_string_from_value(value));
}

/**
 * <h1>CacheAdd(string key, object what)</h1>
 * Store 'what' in memory identified by unique identifier 'key'.
 *
 * The object will be stored forever in memory, until it's either removed by
 * @ref Atrinik_CacheRemove "CacheRemove()" or the server is shut down; in both
 * cases, the object will be closed, if applicable (databases, file objects, etc).
 *
 * A stored object can be retrieved at any time using @ref Atrinik_CacheGet "CacheGet()".
 * @param key The unique identifier for the cache entry.
 * @param what Any Python object (string, integer, database, etc) to store in
 * memory.
 * @return True if the object was cached successfully, False otherwise (cache
 * entry with same key name already exists).
 * @param value Value to build the string from.
 * @return The built string. */
static PyObject *Atrinik_CacheAdd(PyObject *self, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"key", "what", NULL};
	char *key;
	PyObject *what;
	int ret;

	(void) self;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "sO", kwlist, &key, &what))
	{
		return NULL;
	}

	/* Add it to the cache. */
	ret = hooks->cache_add(key, what, CACHE_FLAG_PYOBJ | CACHE_FLAG_GEVENT);

	if (ret)
	{
		Py_INCREF(what);
	}

	Py_ReturnBoolean(ret);
}

/**
 * <h1>CacheGet(string key)</h1>
 * Attempt to find a cache entry identified by 'key' that was previously
 * added using @ref Atrinik_CacheAdd "CacheAdd()".
 * @param key Unique identifier of the cache entry to find.
 * @throws ValueError if the cache entry could not be found.
 * @return The cache entry. An exception is raised if the cache entry was
 * not found. */
static PyObject *Atrinik_CacheGet(PyObject *self, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"key", NULL};
	char *key;
	shstr *sh_key;
	cache_struct *result;

	(void) self;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "s", kwlist, &key))
	{
		return NULL;
	}

	sh_key = hooks->find_string(key);

	/* Even if the cache entry was found, pretend it doesn't exist if
	 * CACHE_FLAG_PYOBJ is not set. */
	if (!sh_key || !(result = hooks->cache_find(sh_key)) || !(result->flags & CACHE_FLAG_PYOBJ))
	{
		PyErr_SetString(PyExc_ValueError, "No such cache entry.");
		return NULL;
	}
	else
	{
		Py_INCREF((PyObject *) result->ptr);
		return (PyObject *) result->ptr;
	}
}

/**
 * <h1>CacheRemove(string key)</h1>
 * Remove a cache entry that was added with a previous call to
 * @ref Atrinik_CacheAdd "CacheAdd()".
 * @param key Unique identifier of the cache entry to remove.
 * @throws ValueError if the cache entry could not be removed (it didn't
 * exist).
 * @return True if the cache entry was removed. An exception is raised if
 * the cache entry was not found. */
static PyObject *Atrinik_CacheRemove(PyObject *self, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"key", NULL};
	char *key;
	shstr *sh_key;

	(void) self;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "s", kwlist, &key))
	{
		return NULL;
	}

	sh_key = hooks->find_string(key);

	if (!sh_key || !hooks->cache_remove(sh_key))
	{
		PyErr_SetString(PyExc_ValueError, "No such cache entry.");
		return NULL;
	}
	else
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
}

/*@}*/

/**
 * Open a Python file.
 * @param filename File to open.
 * @return Python object of the file, NULL on failure. */
static PyObject *python_openfile(char *filename)
{
	PyObject *scriptfile;
#ifdef IS_PY3K
	int fd = open(filename, O_RDONLY);

	if (fd == -1)
	{
		return NULL;
	}

	scriptfile = PyFile_FromFd(fd, filename, "r", -1, NULL, NULL, NULL, 1);
#else
	if (!(scriptfile = PyFile_FromString(filename, "r")))
	{
		return NULL;
	}
#endif

	return scriptfile;
}

/**
 * Return a FILE object from a Python file object.
 * @param obj Python object of the file.
 * @return FILE pointer to the file. */
static FILE *python_pyfile_asfile(PyObject *obj)
{
#ifdef IS_PY3K
	return fdopen(PyObject_AsFileDescriptor(obj), "r");
#else
	return PyFile_AsFile(obj);
#endif
}

/**
 * Outputs the compiled bytecode for a given python file, using in-memory
 * caching of bytecode. */
static PyCodeObject *compilePython(char *filename)
{
	PyObject *scriptfile;
	shstr *sh_path = NULL;
	struct stat stat_buf;
	struct _node *n;
	int i;
	cacheentry *replace = NULL, *run = NULL;

	if (!(scriptfile = python_openfile(filename)))
	{
		LOG(llevDebug, "PYTHON:: The script file %s can't be opened.\n", filename);
		return NULL;
	}

	if (stat(filename, &stat_buf))
	{
		LOG(llevDebug, "PYTHON:: The script file %s can't be stat()ed.\n", filename);
		Py_DECREF(scriptfile);
		return NULL;
	}

	FREE_AND_COPY_HASH(sh_path, filename);

	/* Search through cache. Three cases:
	 * 1) script in cache, but older than file  -> replace cached
	 * 2) script in cache and up to date        -> use cached
	 * 3) script not in cache, cache not full   -> add to end of cache
	 * 4) script not in cache, cache full       -> replace least recently used */
	for (i = 0; i < PYTHON_CACHE_SIZE; i++)
	{
		/* Script not in cache, cache not full */
		if (python_cache[i].file == NULL)
		{
			/* Add to end of cache. */
			replace = &python_cache[i];
			break;
		}
		else if (python_cache[i].file == sh_path)
		{
			/* Script in cache */
			if (python_cache[i].code == NULL || (python_cache[i].cached_time < stat_buf.st_mtime))
			{
				/* Cache older than file, replace cached. */
				replace = &python_cache[i];
			}
			else
			{
				/* cache uptodate, use cached*/
				replace = NULL;
				run = &python_cache[i];
			}

			break;
		}
		else if (replace == NULL || python_cache[i].used_time < replace->used_time)
		{
			/* If we haven't found it yet, set replace to the oldest cache */
			replace = &python_cache[i];
		}
	}

	/* Replace a specific cache index with the file. */
	if (replace)
	{
		FILE *pyfile;

		Py_XDECREF(replace->code);
		replace->code = NULL;

		/* Need to replace path string? */
		if (replace->file != sh_path)
		{
			if (replace->file)
			{
				FREE_AND_CLEAR_HASH(replace->file);
			}

			FREE_AND_COPY_HASH(replace->file, sh_path);
		}

		pyfile = python_pyfile_asfile(scriptfile);

		if ((n = PyParser_SimpleParseFile(pyfile, filename, Py_file_input)))
		{
			replace->code = PyNode_Compile(n, filename);
			PyNode_Free(n);
		}

		if (PyErr_Occurred())
		{
			PyErr_Print();
		}
		else
		{
			replace->cached_time = stat_buf.st_mtime;
			replace->used_time = time(NULL);
		}

		run = replace;
	}

	FREE_AND_CLEAR_HASH(sh_path);
	Py_DECREF(scriptfile);

	if (run)
	{
		return run->code;
	}

	return NULL;
}

static int do_script(PythonContext *context, const char *filename, object *event)
{
	PyCodeObject *pycode;
	PyObject *dict, *ret;
	char path[HUGE_BUF];

	strncpy(path, hooks->create_pathname(filename), sizeof(path) - 1);

	if (event && path[0] != '/')
	{
		char tmp_path[HUGE_BUF];
		object *outermost = event;

		while (outermost && outermost->env)
		{
			outermost = outermost->env;
		}

		if (outermost && outermost->map)
		{
			hooks->normalize_path(outermost->map->path, filename, tmp_path);
			strncpy(path, hooks->create_pathname(tmp_path), sizeof(path) - 1);
		}
	}

	pycode = compilePython(path);

	if (pycode)
	{
#ifdef PYTHON_DEBUG
		PyObject *modules = PyImport_GetModuleDict(), *key, *value;
		Py_ssize_t pos = 0;
		const char *m_filename;
		char m_buf[MAX_BUF];

		/* Create path name to the Python scripts directory. */
		strncpy(m_buf, hooks->create_pathname("/python"), sizeof(m_buf) - 1);

		/* Go through the loaded modules. */
		while (PyDict_Next(modules, &pos, &key, &value))
		{
			m_filename = PyModule_GetFilename(value);

			if (!m_filename)
			{
				PyErr_Clear();
				continue;
			}

			/* If this module was loaded from one of our script files,
			 * reload it. */
			if (!strncmp(m_filename, m_buf, strlen(m_buf)))
			{
				PyImport_ReloadModule(value);

				if (PyErr_Occurred())
				{
					PyErr_Print();
				}
			}
		}
#endif

		pushContext(context);
		dict = PyDict_New();
		PyDict_SetItemString(dict, "__builtins__", PyEval_GetBuiltins());
		ret = PyEval_EvalCode(pycode, dict, NULL);

		if (PyErr_Occurred())
		{
			PyErr_Print();
		}

		Py_XDECREF(ret);
		Py_DECREF(dict);

		return 1;
	}

	return 0;
}

/**
 * Handles normal events.
 * @param args List of arguments for context.
 * @return 0 on failure, script's return value otherwise. */
static int handle_event(va_list args)
{
	char *script;
	PythonContext *context = malloc(sizeof(PythonContext));
	int rv;

	context->activator = va_arg(args, object *);
	context->who = va_arg(args, object *);
	context->other = va_arg(args, object *);
	context->event = va_arg(args, object *);
	context->text = va_arg(args, char *);
	context->parms[0] = va_arg(args, int);
	context->parms[1] = va_arg(args, int);
	context->parms[2] = va_arg(args, int);
	context->parms[3] = va_arg(args, int);
	script = va_arg(args, char *);
	context->options = va_arg(args, char *);
	context->returnvalue = 0;

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON:: Start script file >%s<\n", script);
	LOG(llevDebug, "PYTHON:: Call data: o1:>%s< o2:>%s< o3:>%s< text:>%s< i1:%d i2:%d i3:%d i4:%d\n", STRING_OBJ_NAME(context->activator), STRING_OBJ_NAME(context->who), STRING_OBJ_NAME(context->other), STRING_SAFE(context->text), context->parms[0], context->parms[1], context->parms[2], context->parms[3]);
#endif

	if (!do_script(context, script, context->who))
	{
		freeContext(context);
		return 0;
	}

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "fixing. ");
#endif

	context = popContext();

	if (context->parms[3] == SCRIPT_FIX_ALL)
	{
		if (context->other && IS_LIVE(context->other))
		{
			hooks->fix_player(context->other);
		}

		if (context->who && IS_LIVE(context->who))
		{
			hooks->fix_player(context->who);
		}

		if (context->activator && IS_LIVE(context->activator))
		{
			hooks->fix_player(context->activator);
		}
	}
	else if (context->parms[3] == SCRIPT_FIX_ACTIVATOR && IS_LIVE(context->activator))
	{
		hooks->fix_player(context->activator);
	}

	rv = context->returnvalue;
	freeContext(context);

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "done (returned: %d)!\n", rv);
#endif

	return rv;
}

/**
 * Handles map events.
 * @param args List of arguments for context.
 * @return 0 on failure, script's return value otherwise. */
static int handle_map_event(va_list args)
{
	PythonContext *context = calloc(1, sizeof(PythonContext));
	char *script;
	int rv;

	context->activator = va_arg(args, object *);
	context->event = va_arg(args, object *);
	context->other = va_arg(args, object *);
	context->who = va_arg(args, object *);
	script = va_arg(args, char *);
	context->options = va_arg(args, char *);
	context->text = va_arg(args, char *);
	context->parms[0] = va_arg(args, int);

	if (!do_script(context, script, context->who))
	{
		freeContext(context);
		return 0;
	}

	context = popContext();
	rv = context->returnvalue;
	freeContext(context);

	return rv;
}

/**
 * Handles global event.
 * @param event_type The event ID.
 * @param args List of arguments for context.
 * @return 0. */
static int handle_global_event(int event_type, va_list args)
{
	PythonContext *context;

	switch (event_type)
	{
		case GEVENT_CACHE_REMOVED:
		{
			void *ptr = va_arg(args, void *);
			uint32 flags = *(uint32 *) va_arg(args, void *);

			if (flags & CACHE_FLAG_PYOBJ)
			{
				/* Attemp to close file/database/etc objects. */
				if (PyObject_GetAttrString((PyObject *) ptr, "close"))
				{
					PyObject_CallMethod((PyObject *) ptr, "close", "");
				}

				/* Decrease the reference count. */
				Py_DECREF((PyObject *) ptr);
			}

			return 0;
		}
	}

	context = malloc(sizeof(PythonContext));
	context->activator = NULL;
	context->who = NULL;
	context->other = NULL;
	context->event = NULL;
	context->parms[0] = 0;
	context->parms[1] = 0;
	context->parms[2] = 0;
	context->parms[3] = 0;
	context->text = NULL;
	context->options = NULL;
	context->returnvalue = 0;

	switch (event_type)
	{
		case GEVENT_BORN:
			context->activator = (object *) va_arg(args, void *);
			break;

		case GEVENT_LOGIN:
			context->activator = ((player *) va_arg(args, void *))->ob;
			context->text = (char *) va_arg(args, void *);
			break;

		case GEVENT_LOGOUT:
			context->activator = ((player *) va_arg(args, void *))->ob;
			context->text = (char *) va_arg(args, void *);
			break;

		case GEVENT_PLAYER_DEATH:
			break;
	}

	if (!do_script(context, "/python/events/python_event.py", NULL))
	{
		freeContext(context);
		return 0;
	}

	context = popContext();
	freeContext(context);

	return 0;
}

MODULEAPI void *triggerEvent(int *type, ...)
{
	va_list args;
	int eventcode, event_type;
	static int result = 0;

	va_start(args, type);
	event_type = va_arg(args, int);
	eventcode = va_arg(args, int);

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON:: triggerEvent(): eventcode %d\n", eventcode);
#endif

	switch (event_type)
	{
		case PLUGIN_EVENT_NORMAL:
			result = handle_event(args);
			break;

		case PLUGIN_EVENT_MAP:
			result = handle_map_event(args);
			break;

		case PLUGIN_EVENT_GLOBAL:
			result = handle_global_event(eventcode, args);
			break;

		default:
			LOG(llevBug, "PYTHON:: BUG: Requested unknown event type %d.\n", event_type);
			break;
	}

	va_end(args);
	return &result;
}

MODULEAPI void *getPluginProperty(int *type, ...)
{
	va_list args;
	const char *propname;
	int i, size;
	char *buf;

	va_start(args, type);
	propname = va_arg(args, const char *);

	if (!strcmp(propname, "command?"))
	{
		const char *cmdname = va_arg(args, const char *);
		CommArray_s *rtn_cmd = va_arg(args, CommArray_s *);

		va_end(args);

		for (i = 0; i < NR_CUSTOM_CMD; i++)
		{
			if (CustomCommand[i].name != NULL)
			{
				if (!strcmp(CustomCommand[i].name, cmdname))
				{
					rtn_cmd->name = CustomCommand[i].name;
					rtn_cmd->time = (float) CustomCommand[i].speed;
					rtn_cmd->func = cmd_customPython;
					rtn_cmd->flags = 0;
					NextCustomCommand = i;
					return rtn_cmd;
				}
			}
		}

		return NULL;
	}
	else if (!strcmp(propname, "Identification"))
	{
		buf = va_arg(args, char *);
		size = va_arg(args, int);
		va_end(args);
		snprintf(buf, size, PLUGIN_NAME);
		return NULL;
	}
	else if (!strcmp(propname, "FullName"))
	{
		buf = va_arg(args, char *);
		size = va_arg(args, int);
		va_end(args);
		snprintf(buf, size, PLUGIN_VERSION);
		return NULL;
	}

	va_end(args);
	return NULL;
}

/**
 * Run custom command using Python script.
 * @param op Object running the command.
 * @param params Command parameters.
 * @return 0 on failure, return value of the script otherwise. */
static int cmd_customPython(object *op, char *params)
{
	PythonContext *context = malloc(sizeof(PythonContext));
	int rv;

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON:: cmd_customPython called:: script file: %s\n", CustomCommand[NextCustomCommand].script);
#endif

	context->activator = op;
	context->who = op;
	context->other = op;
	context->event = NULL;
	context->parms[0] = 0;
	context->parms[1] = 0;
	context->parms[2] = 0;
	context->parms[3] = 0;
	context->text = params;
	context->options = NULL;
	context->returnvalue = 0;

	if (!do_script(context, CustomCommand[NextCustomCommand].script, NULL))
	{
		freeContext(context);
		return 0;
	}

	context = popContext();
	rv = context->returnvalue;
	freeContext(context);

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "done (returned: %d)!\n", rv);
#endif

	return rv;
}

MODULEAPI void postinitPlugin()
{
	PyObject *scriptfile;
	char path[HUGE_BUF];

	LOG(llevDebug, "PYTHON:: Start postinitPlugin.\n");
	initContextStack();

	strncpy(path, hooks->create_pathname("/python/events/python_init.py"), sizeof(path) - 1);
	scriptfile = python_openfile(path);

	if (scriptfile)
	{
		FILE *pyfile = python_pyfile_asfile(scriptfile);

		PyRun_SimpleFile(pyfile, path);
		Py_DECREF(scriptfile);
	}
}

/**
 * Here is the Python Declaration Table, used by the interpreter to make
 * an interface with the C code. */
static PyMethodDef AtrinikMethods[] =
{
	{"LoadObject",          Atrinik_LoadObject,            METH_VARARGS, 0},
	{"ReadyMap",            Atrinik_ReadyMap,              METH_VARARGS, 0},
	{"CheckMap",            Atrinik_CheckMap,              METH_VARARGS, 0},
	{"MatchString",         Atrinik_MatchString,           METH_VARARGS, 0},
	{"FindPlayer",          Atrinik_FindPlayer,            METH_VARARGS, 0},
	{"PlayerExists",        Atrinik_PlayerExists,          METH_VARARGS, 0},
	{"GetOptions",          Atrinik_GetOptions,            METH_VARARGS, 0},
	{"GetReturnValue",      Atrinik_GetReturnValue,        METH_VARARGS, 0},
	{"SetReturnValue",      Atrinik_SetReturnValue,        METH_VARARGS, 0},
	{"GetSpellNr",          Atrinik_GetSpellNr,            METH_VARARGS, 0},
	{"GetSpell",            Atrinik_GetSpell,              METH_VARARGS, 0},
	{"GetSkillNr",          Atrinik_GetSkillNr,            METH_VARARGS, 0},
	{"WhoAmI",              Atrinik_WhoAmI,                METH_VARARGS, 0},
	{"WhoIsActivator",      Atrinik_WhoIsActivator,        METH_VARARGS, 0},
	{"WhoIsOther",          Atrinik_WhoIsOther,            METH_VARARGS, 0},
	{"WhatIsEvent",         Atrinik_WhatIsEvent,           METH_VARARGS, 0},
	{"GetEventNumber",      Atrinik_GetEventNumber,        METH_VARARGS, 0},
	{"WhatIsMessage",       Atrinik_WhatIsMessage,         METH_VARARGS, 0},
	{"RegisterCommand",     Atrinik_RegisterCommand,       METH_VARARGS, 0},
	{"CreatePathname",      Atrinik_CreatePathname,        METH_VARARGS, 0},
	{"GetTime",             Atrinik_GetTime,               METH_VARARGS, 0},
	{"LocateBeacon",        Atrinik_LocateBeacon,          METH_VARARGS, 0},
	{"FindParty",           Atrinik_FindParty,             METH_VARARGS, 0},
	{"CleanupChatString",   Atrinik_CleanupChatString,     METH_VARARGS, 0},
	{"LOG",                 Atrinik_LOG,                   METH_VARARGS, 0},
	{"DestroyTimer",        Atrinik_DestroyTimer,          METH_VARARGS, 0},
	{"FindFace",            Atrinik_FindFace,              METH_VARARGS, 0},
	{"FindAnimation",       Atrinik_FindAnimation,         METH_VARARGS, 0},
	{"GetEventParameters",  Atrinik_GetEventParameters,    METH_VARARGS, 0},
	{"GetGenderStr",        Atrinik_GetGenderStr,          METH_VARARGS, 0},
	{"GetRangeVectorFromMapCoords", (PyCFunction) Atrinik_GetRangeVectorFromMapCoords, METH_VARARGS | METH_KEYWORDS, 0},
	{"CostString", (PyCFunction) Atrinik_CostString, METH_VARARGS | METH_KEYWORDS, 0},
	{"CacheAdd", (PyCFunction) Atrinik_CacheAdd, METH_VARARGS | METH_KEYWORDS, 0},
	{"CacheGet", (PyCFunction) Atrinik_CacheGet, METH_VARARGS | METH_KEYWORDS, 0},
	{"CacheRemove", (PyCFunction) Atrinik_CacheRemove, METH_VARARGS | METH_KEYWORDS, 0},
	{NULL, NULL, 0, 0}
};

#ifdef IS_PY3K
static PyModuleDef AtrinikModule =
{
	PyModuleDef_HEAD_INIT,
	"Atrinik",
	NULL,
	-1,
	AtrinikMethods,
	NULL, NULL, NULL, NULL
};

static PyObject *PyInit_Atrinik()
{
	PyObject *m = PyModule_Create(&AtrinikModule);
	Py_INCREF(m);
	return m;
}
#endif

MODULEAPI void initPlugin(struct plugin_hooklist *hooklist)
{
	PyObject *m, *d;
	int i;

	hooks = hooklist;

	LOG(llevDebug, "Atrinik Plugin loading...\n");

#ifdef IS_PY26
	Py_Py3kWarningFlag++;
#endif

#ifdef IS_PY3K
	PyImport_AppendInittab("Atrinik", &PyInit_Atrinik);
#endif

	Py_Initialize();

	LOG(llevDebug, "PYTHON:: Start initAtrinik.\n");

#ifdef IS_PY3K
	m = PyImport_ImportModule("Atrinik");
#else
	m = Py_InitModule("Atrinik", AtrinikMethods);
#endif

	d = PyModule_GetDict(m);
	AtrinikError = PyErr_NewException("Atrinik.error", NULL, NULL);
	PyDict_SetItemString(d, "error", AtrinikError);

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		CustomCommand[i].name = NULL;
		CustomCommand[i].script = NULL;
		CustomCommand[i].speed = 0.0;
	}

	if (!Atrinik_Object_init(m) || !Atrinik_Map_init(m) || !Atrinik_Party_init(m) || !Atrinik_Region_init(m) || !Atrinik_Player_init(m) || !Atrinik_Archetype_init(m))
	{
		return;
	}

	/* Initialize integer constants */
	for (i = 0; constants[i].name; i++)
	{
		PyModule_AddIntConstant(m, constants[i].name, constants[i].value);
	}

	LOG(llevDebug, "  [Done]\n");
}

MODULEAPI void closePlugin()
{
	LOG(llevInfo, "Python Plugin closing.\n");
	hooks->cache_remove_by_flags(CACHE_FLAG_GEVENT);
	Py_Finalize();
}

/**
 * A generic field setter for all interfaces.
 * @param type Type of the field.
 * @param[out] field_ptr Field pointer.
 * @param value Value to set for the field pointer.
 * @return 0 on success, -1 on failure. */
int generic_field_setter(fields_struct *field, void *ptr, PyObject *value)
{
	void *field_ptr;

	if ((field->flags & FIELDFLAG_READONLY))
	{
		INTRAISE("Trying to modify readonly field.");
	}

	field_ptr = (void *) ((char *) ptr + field->offset);

	switch (field->type)
	{
		case FIELDTYPE_SHSTR:
			if (value == Py_None)
			{
				FREE_AND_CLEAR_HASH(*(shstr **) field_ptr);
			}
			else if (PyString_Check(value))
			{
				FREE_AND_CLEAR_HASH(*(shstr **) field_ptr);
				FREE_AND_COPY_HASH(*(shstr **) field_ptr, PyString_AsString(value));
			}
			else
			{
				INTRAISE("Illegal value for shared string field.");
			}

			break;

		case FIELDTYPE_CSTR:
			if (value == Py_None)
			{
				FREE_AND_NULL_PTR(*(char **) field_ptr);
			}
			else if (PyString_Check(value))
			{
				free(*(char **) field_ptr);
				*(char **) field_ptr = hooks->strdup_local(PyString_AsString(value));
			}
			else
			{
				INTRAISE("Illegal value for C string field.");
			}

			break;

		case FIELDTYPE_CARY:
			if (value == Py_None)
			{
				((char *) field_ptr)[0] = '\0';
			}
			else if (PyString_Check(value))
			{
				memcpy((char *) field_ptr, PyString_AsString(value), field->extra_data);
				((char *) field_ptr)[field->extra_data] = '\0';
			}
			else
			{
				INTRAISE("Illegal value for C char array field.");
			}

			break;

		case FIELDTYPE_UINT8:
			if (PyInt_Check(value))
			{
				long val = PyLong_AsLong(value);

				if (val < 0 || (unsigned long) val > UINT8_MAX)
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for uint8 field.");
					return -1;
				}

				*(uint8 *) field_ptr = (uint8) val;
			}
			else
			{
				INTRAISE("Illegal value for uint8 field.");
			}

			break;

		case FIELDTYPE_SINT8:
			if (PyInt_Check(value))
			{
				long val = PyLong_AsLong(value);

				if (val < SINT8_MIN || val > SINT8_MAX)
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for sint8 field.");
					return -1;
				}

				*(sint8 *) field_ptr = (sint8) val;
			}
			else
			{
				INTRAISE("Illegal value for sint8 field.");
			}

			break;

		case FIELDTYPE_UINT16:
			if (PyInt_Check(value))
			{
				long val = PyLong_AsLong(value);

				if (val < 0 || (unsigned long) val > UINT16_MAX)
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for uint16 field.");
					return -1;
				}

				*(uint16 *) field_ptr = (uint16) val;
			}
			else
			{
				INTRAISE("Illegal value for uint16 field.");
			}

			break;

		case FIELDTYPE_SINT16:
			if (PyInt_Check(value))
			{
				long val = PyLong_AsLong(value);

				if (val < SINT16_MIN || val > SINT16_MAX)
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for sint16 field.");
					return -1;
				}

				*(sint16 *) field_ptr = (sint16) val;
			}
			else
			{
				INTRAISE("Illegal value for sint16 field.");
			}

			break;

		case FIELDTYPE_UINT32:
			if (PyInt_Check(value))
			{
				long val = PyLong_AsLong(value);

				if (val < 0 || (unsigned long) val > UINT32_MAX)
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for uint32 field.");
					return -1;
				}

				*(uint32 *) field_ptr = (uint32) val;
			}
			else
			{
				INTRAISE("Illegal value for uint32 field.");
			}

			break;

		case FIELDTYPE_SINT32:
			if (PyInt_Check(value))
			{
				long val = PyLong_AsLong(value);

				if (val < SINT32_MIN || val > SINT32_MAX)
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for sint32 field.");
					return -1;
				}

				*(sint32 *) field_ptr = (sint32) val;
			}
			else
			{
				INTRAISE("Illegal value for sint32 field.");
			}

			break;

		case FIELDTYPE_UINT64:
			if (PyInt_Check(value))
			{
				unsigned PY_LONG_LONG val = PyLong_AsUnsignedLongLong(value);

				if (PyErr_Occurred())
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for uint64 field.");
					return -1;
				}

				*(uint64 *) field_ptr = (uint64) val;
			}
			else
			{
				INTRAISE("Illegal value for uint64 field.");
			}

			break;

		case FIELDTYPE_SINT64:
			if (PyInt_Check(value))
			{
				PY_LONG_LONG val = PyLong_AsLongLong(value);

				if (PyErr_Occurred())
				{
					PyErr_SetString(PyExc_OverflowError, "Invalid integer value for sint64 field.");
					return -1;
				}

				*(sint64 *) field_ptr = (sint64) val;
			}
			else
			{
				INTRAISE("Illegal value for sint64 field.");
			}

			break;

		case FIELDTYPE_FLOAT:
			if (PyFloat_Check(value))
			{
				*(float *) field_ptr = (float) PyFloat_AsDouble(value);
			}
			else if (PyInt_Check(value))
			{
				*(float *) field_ptr = (float) PyLong_AsLong(value);
			}
			else
			{
				INTRAISE("Illegal value for float field.");
			}

			break;

		case FIELDTYPE_OBJECT:
			if (value == Py_None)
			{
				*(object **) field_ptr = NULL;
			}
			else if (PyObject_TypeCheck(value, &Atrinik_ObjectType))
			{
				*(object **) field_ptr = (object *) ((Atrinik_Object *) value)->obj;
			}
			else
			{
				INTRAISE("Illegal value for object field.");
			}

			break;

		case FIELDTYPE_MAP:
			if (value == Py_None)
			{
				*(mapstruct **) field_ptr = NULL;
			}
			else if (PyObject_TypeCheck(value, &Atrinik_MapType))
			{
				*(mapstruct **) field_ptr = (mapstruct *) ((Atrinik_Map *) value)->map;
			}
			else
			{
				INTRAISE("Illegal value for map field.");
			}

			break;

		case FIELDTYPE_OBJECTREF:
		{
			void *field_ptr2 = (void *) ((char *) ptr + field->extra_data);
			object *tmp = (object *) ((Atrinik_Object *) value)->obj;

			if (value == Py_None)
			{
				*(object **) field_ptr = NULL;
				*(tag_t *) field_ptr2 = 0;
			}
			else if (PyObject_TypeCheck(value, &Atrinik_ObjectType))
			{
				*(object **) field_ptr = tmp;
				*(tag_t *) field_ptr2 = tmp->count;
			}
			else
			{
				INTRAISE("Illegal value for object+reference field.");
			}

			break;
		}

		case FIELDTYPE_REGION:
			if (value == Py_None)
			{
				*(region **) field_ptr = NULL;
			}
			else if (PyObject_TypeCheck(value, &Atrinik_RegionType))
			{
				*(region **) field_ptr = (region *) ((Atrinik_Region *) value)->region;
			}
			else
			{
				INTRAISE("Illegal value for region field.");
			}

			break;

		case FIELDTYPE_PARTY:
			if (value == Py_None)
			{
				*(party_struct **) field_ptr = NULL;
			}
			else if (PyObject_TypeCheck(value, &Atrinik_PartyType))
			{
				*(party_struct **) field_ptr = (party_struct *) ((Atrinik_Party *) value)->party;
			}
			else
			{
				INTRAISE("Illegal value for party field.");
			}

			break;

		case FIELDTYPE_ARCH:
			if (value == Py_None)
			{
				*(archetype **) field_ptr = NULL;
			}
			else if (PyObject_TypeCheck(value, &Atrinik_ArchetypeType))
			{
				*(archetype **) field_ptr = (archetype *) ((Atrinik_Archetype *) value)->at;
			}
			else
			{
				INTRAISE("Illegal value for archetype field.");
			}

			break;
	}

	return 0;
}

/**
 * A generic field getter for all interfaces.
 * @param type Type of the field.
 * @param field_ptr Field pointer.
 * @param field_ptr2 Field pointer for extra data.
 * @return Python object containing value of field_ptr (and field_ptr2, if applicable). */
PyObject *generic_field_getter(fields_struct *field, void *ptr)
{
	void *field_ptr;

	field_ptr = (void *) ((char *) ptr + field->offset);

	switch (field->type)
	{
		case FIELDTYPE_SHSTR:
		case FIELDTYPE_CSTR:
		{
			char *str = *(char **) field_ptr;
			return Py_BuildValue("s", str ? str : "");
		}

		case FIELDTYPE_CARY:
			return Py_BuildValue("s", (char *) field_ptr);

		case FIELDTYPE_UINT8:
			return Py_BuildValue("B", *(uint8 *) field_ptr);

		case FIELDTYPE_SINT8:
			return Py_BuildValue("b", *(sint8 *) field_ptr);

		case FIELDTYPE_UINT16:
			return Py_BuildValue("H", *(uint16 *) field_ptr);

		case FIELDTYPE_SINT16:
			return Py_BuildValue("h", *(sint16 *) field_ptr);

		case FIELDTYPE_UINT32:
			return Py_BuildValue("I", *(uint32 *) field_ptr);

		case FIELDTYPE_SINT32:
			return Py_BuildValue("l", *(sint32 *) field_ptr);

		case FIELDTYPE_UINT64:
			return Py_BuildValue("K", *(uint64 *) field_ptr);

		case FIELDTYPE_SINT64:
			return Py_BuildValue("L", *(sint64 *) field_ptr);

		case FIELDTYPE_FLOAT:
			return Py_BuildValue("f", *(float *) field_ptr);

		case FIELDTYPE_MAP:
			return wrap_map(*(mapstruct **) field_ptr);

		case FIELDTYPE_OBJECT:
			return wrap_object(*(object **) field_ptr);

		case FIELDTYPE_OBJECTREF:
		{
			object *obj = *(object **) field_ptr;
			tag_t tag = *(tag_t *) (void *) ((char *) ptr + field->extra_data);;

			return wrap_object(OBJECT_VALID(obj, tag) ? obj : NULL);
		}

		case FIELDTYPE_REGION:
			return wrap_region(*(region **) field_ptr);

		case FIELDTYPE_PARTY:
			return wrap_party(*(party_struct **) field_ptr);

		case FIELDTYPE_ARCH:
			return wrap_archetype(*(archetype **) field_ptr);
	}

	RAISE("BUG: Unknown field type.");
}

/**
 * This function exists to workaround a warning under GCC 4.4.1 (i686):
 * dereferencing type-punned pointer will break strict-aliasing rules
 * @param ob Python object to increase reference of. */
void Py_INCREF_TYPE(PyTypeObject *ob)
{
	Py_INCREF(ob);
}

/**
 * Generic rich comparison function.
 * @param op
 * @param result
 * @return  */
PyObject *generic_rich_compare(int op, int result)
{
	/* Based on how Python 3.0 (GPL compatible) implements it for internal types. */
	switch (op)
	{
		case Py_EQ:
			result = (result == 0);
			break;
		case Py_NE:
			result = (result != 0);
			break;
		case Py_LE:
			result = (result <= 0);
			break;
		case Py_GE:
			result = (result >= 0);
			break;
		case Py_LT:
			result = (result == -1);
			break;
		case Py_GT:
			result = (result == 1);
			break;
	}

	return PyBool_FromLong(result);
}

/**
 * Call a function defined in Python script with the specified arguments.
 * @param callable What to call.
 * @param arglist Arguments to call the function with. Will have reference
 * decreased.
 * @return Integer value the function returned. */
int python_call_int(PyObject *callable, PyObject *arglist)
{
	PyObject *result;
	int retval = 0;

	/* Call the Python function. */
	result = PyEval_CallObject(callable, arglist);

	/* Check the result. */
	if (result && PyInt_Check(result))
	{
		retval = PyInt_AsLong(result);
	}

	Py_XDECREF(result);
	Py_DECREF(arglist);

	return retval;
}
