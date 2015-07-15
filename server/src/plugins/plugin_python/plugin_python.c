/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Atrinik python plugin.
 *
 * @author Alex Tokar
 * @author Yann Chachkoff
 */

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
 * The global variables dictionary. */
static PyObject *py_globals_dict = NULL;

/**
 * Structure used to evaluate code in the main thread after a specified delay.
 *
 * Can also be used to execute code from other threads with no ill effects.
 */
typedef struct python_eval_struct {
    struct python_eval_struct *next; ///< Next pointer.
    struct python_eval_struct *prev; ///< Previous pointer.

    PyObject *globals; ///< Globals dictionary.
    PyObject *locals; ///< Locals dictionary.
    PyCodeObject *code; ///< Compiled code to execute.
    double seconds; ///< When to execute.
} python_eval_struct;

/**
 * The first code to evaluate.
 */
static python_eval_struct *python_eval;

/** The Atrinik package docstring. */
static const char package_doc[] =
"The Atrinik Python package provides an interface to the Atrinik server.\n\n"
"The server is able to run Python scripts dynamically, usually as a response "
"to some event.";

/** The Atrinik Type module docstring. */
static const char module_doc_type[] =
"Contains all the Atrinik object types as constants.";

/** The Atrinik Gender module docstring. */
static const char module_doc_gender[] =
"Contains gender related constants.";

/**
 * Useful constants
 */
static const Atrinik_Constant constants[] = {
    {"NORTH", NORTH},
    {"NORTHEAST", NORTHEAST},
    {"EAST", EAST},
    {"SOUTHEAST", SOUTHEAST},
    {"SOUTH", SOUTH},
    {"SOUTHWEST", SOUTHWEST},
    {"WEST", WEST},
    {"NORTHWEST", NORTHWEST},

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
    {"EVENT_ASK_SHOW", EVENT_ASK_SHOW},

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
    {"MEVENT_EXAMINE", MEVENT_EXAMINE},

    {"GEVENT_BORN", GEVENT_BORN},
    {"GEVENT_LOGIN", GEVENT_LOGIN},
    {"GEVENT_LOGOUT", GEVENT_LOGOUT},
    {"GEVENT_PLAYER_DEATH", GEVENT_PLAYER_DEATH},
    {"GEVENT_CACHE_REMOVED", GEVENT_CACHE_REMOVED},

    {"MAP_INFO_NORMAL", MAP_INFO_NORMAL},
    {"MAP_INFO_ALL", MAP_INFO_ALL},

    {"COST_TRUE", COST_TRUE},
    {"COST_BUY", COST_BUY},
    {"COST_SELL", COST_SELL},

    {"APPLY_NORMAL", APPLY_NORMAL},
    {"APPLY_ALWAYS", APPLY_ALWAYS},
    {"APPLY_ALWAYS_UNAPPLY", APPLY_ALWAYS_UNAPPLY},
    {"APPLY_NO_MERGE", APPLY_NO_MERGE},
    {"APPLY_IGNORE_CURSE", APPLY_IGNORE_CURSE},
    {"APPLY_NO_EVENT", APPLY_NO_EVENT},

    {"MAXLEVEL", MAXLEVEL},

    {"CAST_NORMAL", CAST_NORMAL},
    {"CAST_WAND", CAST_WAND},
    {"CAST_ROD", CAST_ROD},
    {"CAST_SCROLL", CAST_SCROLL},
    {"CAST_POTION", CAST_POTION},
    {"CAST_NPC", CAST_NPC},

    {"IDENTIFY_NORMAL", IDENTIFY_NORMAL},
    {"IDENTIFY_ALL", IDENTIFY_ALL},
    {"IDENTIFY_MARKED", IDENTIFY_MARKED},

    {"CHAT_TYPE_ALL", CHAT_TYPE_ALL},
    {"CHAT_TYPE_GAME", CHAT_TYPE_GAME},
    {"CHAT_TYPE_CHAT", CHAT_TYPE_CHAT},
    {"CHAT_TYPE_LOCAL", CHAT_TYPE_LOCAL},
    {"CHAT_TYPE_PRIVATE", CHAT_TYPE_PRIVATE},
    {"CHAT_TYPE_GUILD", CHAT_TYPE_GUILD},
    {"CHAT_TYPE_PARTY", CHAT_TYPE_PARTY},
    {"CHAT_TYPE_OPERATOR", CHAT_TYPE_OPERATOR},

    {"PLAYER_EQUIP_AMMO", PLAYER_EQUIP_AMMO},
    {"PLAYER_EQUIP_AMULET", PLAYER_EQUIP_AMULET},
    {"PLAYER_EQUIP_WEAPON", PLAYER_EQUIP_WEAPON},
    {"PLAYER_EQUIP_SHIELD", PLAYER_EQUIP_SHIELD},
    {"PLAYER_EQUIP_GAUNTLETS", PLAYER_EQUIP_GAUNTLETS},
    {"PLAYER_EQUIP_RING_RIGHT", PLAYER_EQUIP_RING_RIGHT},
    {"PLAYER_EQUIP_HELM", PLAYER_EQUIP_HELM},
    {"PLAYER_EQUIP_ARMOUR", PLAYER_EQUIP_ARMOUR},
    {"PLAYER_EQUIP_BELT", PLAYER_EQUIP_BELT},
    {"PLAYER_EQUIP_PANTS", PLAYER_EQUIP_PANTS},
    {"PLAYER_EQUIP_BOOTS", PLAYER_EQUIP_BOOTS},
    {"PLAYER_EQUIP_CLOAK", PLAYER_EQUIP_CLOAK},
    {"PLAYER_EQUIP_BRACERS", PLAYER_EQUIP_BRACERS},
    {"PLAYER_EQUIP_WEAPON_RANGED", PLAYER_EQUIP_WEAPON_RANGED},
    {"PLAYER_EQUIP_LIGHT", PLAYER_EQUIP_LIGHT},
    {"PLAYER_EQUIP_RING_LEFT", PLAYER_EQUIP_RING_LEFT},
    {"PLAYER_EQUIP_SKILL_ITEM", PLAYER_EQUIP_SKILL_ITEM},

    {"QUEST_TYPE_KILL", QUEST_TYPE_KILL},
    {"QUEST_TYPE_ITEM", QUEST_TYPE_ITEM},
    {"QUEST_TYPE_ITEM_DROP", QUEST_TYPE_ITEM_DROP},
    {"QUEST_TYPE_SPECIAL", QUEST_TYPE_SPECIAL},
    {"QUEST_STATUS_COMPLETED", QUEST_STATUS_COMPLETED},

    {"PARTY_MESSAGE_STATUS", PARTY_MESSAGE_STATUS},
    {"PARTY_MESSAGE_CHAT", PARTY_MESSAGE_CHAT},

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
    {"TERRAIN_AIRBREATH", TERRAIN_AIRBREATH},
    {"TERRAIN_WATERWALK", TERRAIN_WATERWALK},
    {"TERRAIN_WATERBREATH", TERRAIN_WATERBREATH},
    {"TERRAIN_FIREWALK", TERRAIN_FIREWALK},
    {"TERRAIN_FIREBREATH", TERRAIN_FIREBREATH},
    {"TERRAIN_CLOUDWALK", TERRAIN_CLOUDWALK},
    {"TERRAIN_WATER_SHALLOW", TERRAIN_WATER_SHALLOW},
    {"TERRAIN_ALL", TERRAIN_ALL},

    {"P_BLOCKSVIEW", P_BLOCKSVIEW},
    {"P_NO_MAGIC", P_NO_MAGIC},
    {"P_NO_PASS", P_NO_PASS},
    {"P_IS_PLAYER", P_IS_PLAYER},
    {"P_IS_MONSTER", P_IS_MONSTER},
    {"P_PLAYER_ONLY", P_PLAYER_ONLY},
    {"P_DOOR_CLOSED", P_DOOR_CLOSED},
    {"P_CHECK_INV", P_CHECK_INV},
    {"P_NO_PVP", P_NO_PVP},
    {"P_PASS_THRU", P_PASS_THRU},
    {"P_WALK_ON", P_WALK_ON},
    {"P_WALK_OFF", P_WALK_OFF},
    {"P_FLY_OFF", P_FLY_OFF},
    {"P_FLY_ON", P_FLY_ON},
    {"P_MAGIC_MIRROR", P_MAGIC_MIRROR},
    {"P_OUTDOOR", P_OUTDOOR},
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
    {"BANK_DEPOSIT_JADE", BANK_DEPOSIT_JADE},
    {"BANK_DEPOSIT_MITHRIL", BANK_DEPOSIT_MITHRIL},
    {"BANK_DEPOSIT_AMBER", BANK_DEPOSIT_AMBER},

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

    {"NUM_LAYERS", NUM_LAYERS},
    {"NUM_SUB_LAYERS", NUM_SUB_LAYERS},

    {"INVENTORY_ONLY", INVENTORY_ONLY},
    {"INVENTORY_CONTAINERS", INVENTORY_CONTAINERS},
    {"INVENTORY_ALL", INVENTORY_ALL},

    {"GT_ENVIRONMENT", GT_ENVIRONMENT},
    {"GT_STARTEQUIP", GT_STARTEQUIP},
    {"GT_APPLY", GT_APPLY},
    {"GT_ONLY_GOOD", GT_ONLY_GOOD},
    {"GT_UPDATE_INV", GT_UPDATE_INV},
    {"GT_NO_VALUE", GT_NO_VALUE},

    {"SIZEOFFREE1", SIZEOFFREE1},
    {"SIZEOFFREE2", SIZEOFFREE2},
    {"SIZEOFFREE3", SIZEOFFREE3},
    {"SIZEOFFREE", SIZEOFFREE},

    {"NROFREALSPELLS", NROFREALSPELLS},

    {"MAX_TIME", MAX_TIME},

    {"OBJECT_METHOD_UNHANDLED", OBJECT_METHOD_UNHANDLED},
    {"OBJECT_METHOD_OK", OBJECT_METHOD_OK},
    {"OBJECT_METHOD_ERROR", OBJECT_METHOD_ERROR},

    {"UPD_LOCATION", UPD_LOCATION},
    {"UPD_FLAGS", UPD_FLAGS},
    {"UPD_WEIGHT", UPD_WEIGHT},
    {"UPD_FACE", UPD_FACE},
    {"UPD_NAME", UPD_NAME},
    {"UPD_ANIM", UPD_ANIM},
    {"UPD_ANIMSPEED", UPD_ANIMSPEED},
    {"UPD_NROF", UPD_NROF},
    {"UPD_DIRECTION", UPD_DIRECTION},
    {"UPD_TYPE", UPD_TYPE},
    {"UPD_EXTRA", UPD_EXTRA},
    {"UPD_GLOW", UPD_GLOW},

    {"RV_MANHATTAN_DISTANCE", RV_MANHATTAN_DISTANCE},
    {"RV_EUCLIDIAN_DISTANCE", RV_EUCLIDIAN_DISTANCE},
    {"RV_DIAGONAL_DISTANCE", RV_DIAGONAL_DISTANCE},
    {"RV_NO_DISTANCE", RV_NO_DISTANCE},
    {"RV_IGNORE_MULTIPART", RV_IGNORE_MULTIPART},
    {"RV_RECURSIVE_SEARCH", RV_RECURSIVE_SEARCH},
    {"RV_NO_LOAD", RV_NO_LOAD},

    {"ART_CHANCE_UNSET", ART_CHANCE_UNSET},

    {NULL, 0}
};

/**
 * Game object type constants.
 */
static const Atrinik_Constant constants_types[] = {
    {"PLAYER", PLAYER},
    {"BULLET", BULLET},
    {"ROD", ROD},
    {"TREASURE", TREASURE},
    {"POTION", POTION},
    {"FOOD", FOOD},
    {"REGION_MAP", REGION_MAP},
    {"BOOK", BOOK},
    {"CLOCK", CLOCK},
    {"MATERIAL", MATERIAL},
    {"DUPLICATOR", DUPLICATOR},
    {"LIGHTNING", LIGHTNING},
    {"ARROW", ARROW},
    {"BOW", BOW},
    {"WEAPON", WEAPON},
    {"ARMOUR", ARMOUR},
    {"PEDESTAL", PEDESTAL},
    {"CONFUSION", CONFUSION},
    {"DOOR", DOOR},
    {"KEY", KEY},
    {"MAP", MAP},
    {"MAGIC_MIRROR", MAGIC_MIRROR},
    {"SPELL", SPELL},
    {"SHIELD", SHIELD},
    {"HELMET", HELMET},
    {"PANTS", PANTS},
    {"MONEY", MONEY},
    {"CLASS", CLASS},
    {"GRAVESTONE", GRAVESTONE},
    {"AMULET", AMULET},
    {"PLAYER_MOVER", PLAYER_MOVER},
    {"CREATOR", CREATOR},
    {"SKILL", SKILL},
    {"BLINDNESS", BLINDNESS},
    {"GOD", GOD},
    {"DETECTOR", DETECTOR},
    {"SKILL_ITEM", SKILL_ITEM},
    {"DEAD_OBJECT", DEAD_OBJECT},
    {"DRINK", DRINK},
    {"MARKER", MARKER},
    {"HOLY_ALTAR", HOLY_ALTAR},
    {"PEARL", PEARL},
    {"GEM", GEM},
    {"SOUND_AMBIENT", SOUND_AMBIENT},
    {"FIREWALL", FIREWALL},
    {"CHECK_INV", CHECK_INV},
    {"EXIT", EXIT},
    {"SHOP_FLOOR", SHOP_FLOOR},
    {"RING", RING},
    {"FLOOR", FLOOR},
    {"FLESH", FLESH},
    {"INORGANIC", INORGANIC},
    {"LIGHT_APPLY", LIGHT_APPLY},
    {"WALL", WALL},
    {"LIGHT_SOURCE", LIGHT_SOURCE},
    {"MISC_OBJECT", MISC_OBJECT},
    {"MONSTER", MONSTER},
    {"SPAWN_POINT", SPAWN_POINT},
    {"LIGHT_REFILL", LIGHT_REFILL},
    {"SPAWN_POINT_MOB", SPAWN_POINT_MOB},
    {"SPAWN_POINT_INFO", SPAWN_POINT_INFO},
    {"BOOK_SPELL", BOOK_SPELL},
    {"ORGANIC", ORGANIC},
    {"CLOAK", CLOAK},
    {"CONE", CONE},
    {"SPINNER", SPINNER},
    {"GATE", GATE},
    {"BUTTON", BUTTON},
    {"HANDLE", TYPE_HANDLE},
    {"WORD_OF_RECALL", WORD_OF_RECALL},
    {"SIGN", SIGN},
    {"BOOTS", BOOTS},
    {"GLOVES", GLOVES},
    {"BASE_INFO", BASE_INFO},
    {"RANDOM_DROP", RANDOM_DROP},
    {"BRACERS", BRACERS},
    {"POISONING", POISONING},
    {"SAVEBED", SAVEBED},
    {"WAND", WAND},
    {"ABILITY", ABILITY},
    {"SCROLL", SCROLL},
    {"DIRECTOR", DIRECTOR},
    {"GIRDLE", GIRDLE},
    {"FORCE", FORCE},
    {"POTION_EFFECT", POTION_EFFECT},
    {"JEWEL", JEWEL},
    {"NUGGET", NUGGET},
    {"EVENT_OBJECT", EVENT_OBJECT},
    {"WAYPOINT_OBJECT", WAYPOINT_OBJECT},
    {"QUEST_CONTAINER", QUEST_CONTAINER},
    {"CONTAINER", CONTAINER},
    {"WEALTH", WEALTH},
    {"BEACON", BEACON},
    {"MAP_EVENT_OBJ", MAP_EVENT_OBJ},
    {"COMPASS", COMPASS},
    {"MAP_INFO", MAP_INFO},
    {"SWARM_SPELL", SWARM_SPELL},
    {"RUNE", RUNE},
    {"CLIENT_MAP_INFO", CLIENT_MAP_INFO},
    {"POWER_CRYSTAL", POWER_CRYSTAL},
    {"CORPSE", CORPSE},
    {"DISEASE", DISEASE},
    {"SYMPTOM", SYMPTOM},

    {NULL, 0}
};

/**
 * Gender constants.
 */
static const Atrinik_Constant constants_gender[] = {
    {"NEUTER", GENDER_NEUTER},
    {"MALE", GENDER_MALE},
    {"FEMALE", GENDER_FEMALE},
    {"HERMAPHRODITE", GENDER_HERMAPHRODITE},

    {NULL, 0}
};

/**
 * Color constants
 */
static const char *const constants_colors[][2] = {
    {"COLOR_WHITE", COLOR_WHITE},
    {"COLOR_ORANGE", COLOR_ORANGE},
    {"COLOR_NAVY", COLOR_NAVY},
    {"COLOR_RED", COLOR_RED},
    {"COLOR_GREEN", COLOR_GREEN},
    {"COLOR_BLUE", COLOR_BLUE},
    {"COLOR_GRAY", COLOR_GRAY},
    {"COLOR_BROWN", COLOR_BROWN},
    {"COLOR_PURPLE", COLOR_PURPLE},
    {"COLOR_PINK", COLOR_PINK},
    {"COLOR_YELLOW", COLOR_YELLOW},
    {"COLOR_DK_NAVY", COLOR_DK_NAVY},
    {"COLOR_DK_GREEN", COLOR_DK_GREEN},
    {"COLOR_DK_ORANGE", COLOR_DK_ORANGE},
    {"COLOR_HGOLD", COLOR_HGOLD},
    {"COLOR_DGOLD", COLOR_DGOLD},
    {NULL, NULL}
};

/** The Python cache. */
static python_cache_entry *python_cache = NULL;

/**
 * Initialize the context stack.
 */
static void initContextStack(void)
{
    current_context = NULL;
    context_stack = NULL;
}

/**
 * Push context to the context stack and to current context.
 * @param context The context to push.
 */
static void pushContext(PythonContext *context)
{
    if (current_context == NULL) {
        context_stack = context;
        context->down = NULL;
    } else {
        context->down = current_context;
    }

    current_context = context;
}

/**
 * Pop the first context from the current context, replacing it by the
 * next one in the list.
 * @return NULL if there is no current context, the previous current
 * context otherwise.
 */
static PythonContext *popContext(void)
{
    PythonContext *oldcontext;

    if (current_context) {
        oldcontext = current_context;
        current_context = current_context->down;
        return oldcontext;
    }

    return NULL;
}

/**
 * Free a context.
 * @param context Context to free.
 */
static void freeContext(PythonContext *context)
{
    free(context);
}

/**
 * Run a Python file. 'path' is automatically resolved to the current
 * maps directory.
 * @param path The Python file in the maps directory to run (absolute).
 * @param globals Globals dictionary.
 * @param locals Locals dictionary. May be NULL.
 * @return The returned object, if any.
 */
static PyObject *py_runfile(const char *path, PyObject *globals,
        PyObject *locals)
{
    char *fullpath;
    FILE *fp;
    PyObject *ret = NULL;

    fullpath = strdup(hooks->create_pathname(path));

    fp = fopen(fullpath, "r");

    if (fp) {
#ifdef WIN32
        StringBuffer *sb;
        char *cp;

        fclose(fp);

        sb = hooks->stringbuffer_new();
        hooks->stringbuffer_append_printf(sb, "exec(open('%s').read())",
                fullpath);
        cp = hooks->stringbuffer_finish(sb);
        PyRun_String(cp, Py_file_input, globals, locals);
        efree(cp);
#else
        ret = PyRun_File(fp, fullpath, Py_file_input, globals, locals);
        fclose(fp);
#endif
    }

    free(fullpath);

    return ret;
}

/**
 * Simplified interface to py_runfile(); automatically constructs the
 * globals dictionary with the Python builtins.
 * @param path The Python file in the maps directory to run (absolute).
 * @param locals Locals dictionary. May be NULL.
 */
static void py_runfile_simple(const char *path, PyObject *locals)
{
    PyObject *globals, *ret;

    /* Construct globals dictionary. */
    globals = PyDict_New();
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());

    /* Run the Python code. */
    ret = py_runfile(path, globals, locals);

    Py_DECREF(globals);
    Py_XDECREF(ret);
}

/**
 * Log a Python exception. Will also send the exception to any online
 * DMs or those with /resetmap command permission.
 */
static void PyErr_LOG(void)
{
    PyObject *locals;
    PyObject *ptype, *pvalue, *ptraceback;

    /* Fetch the exception data. */
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

    /* Construct locals dictionary, with the exception data. */
    locals = PyDict_New();
    PyDict_SetItemString(locals, "exc_type", ptype);
    PyDict_SetItemString(locals, "exc_value", pvalue);

    if (ptraceback != NULL) {
        PyDict_SetItemString(locals, "exc_traceback", ptraceback);
    } else {
        Py_INCREF(Py_None);
        PyDict_SetItemString(locals, "exc_traceback", Py_None);
    }

    py_runfile_simple("/python/events/python_exception.py", locals);
    Py_DECREF(locals);

    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
}

/**
 * Outputs the compiled bytecode for a given python file, using in-memory
 * caching of bytecode.
 */
static PyCodeObject *compilePython(char *filename)
{
    struct stat stat_buf;
    python_cache_entry *cache;

    if (stat(filename, &stat_buf)) {
        LOG(DEBUG, "Python: The script file %s can't be stat()ed.", filename);
        return NULL;
    }

    HASH_FIND_STR(python_cache, filename, cache);

    if (!cache || cache->cached_time < stat_buf.st_mtime) {
        FILE *fp;
        struct _node *n;
        PyCodeObject *code = NULL;

        if (cache) {
            HASH_DEL(python_cache, cache);
            Py_XDECREF(cache->code);
            free(cache->file);
            free(cache);
        }

        fp = fopen(filename, "r");

        if (!fp) {
            LOG(BUG, "Python: The script file %s can't be opened.", filename);
            return NULL;
        }

#ifdef WIN32
        {
            char buf[HUGE_BUF], *pystr = NULL;
            size_t buf_len = 0, pystr_len = 0;

            while (fgets(buf, sizeof(buf), fp)) {
                buf_len = strlen(buf);
                pystr_len += buf_len;
                pystr = realloc(pystr, sizeof(char) * (pystr_len + 1));
                strcpy(pystr + pystr_len - buf_len, buf);
                pystr[pystr_len] = '\0';
            }

            n = PyParser_SimpleParseString(pystr, Py_file_input);
            free(pystr);
        }
#else
        n = PyParser_SimpleParseFile(fp, filename, Py_file_input);
#endif

        if (n) {
            code = PyNode_Compile(n, filename);
            PyNode_Free(n);
        }

        fclose(fp);

        if (PyErr_Occurred()) {
            PyErr_LOG();
            return NULL;
        }

        cache = malloc(sizeof(*cache));
        cache->file = strdup(filename);
        cache->code = code;
        cache->cached_time = stat_buf.st_mtime;
        HASH_ADD_KEYPTR(hh, python_cache, cache->file, strlen(cache->file),
                cache);
    }

    return cache->code;
}

static int do_script(PythonContext *context, const char *filename)
{
    PyCodeObject *pycode;
    PyObject *dict, *ret;
    PyGILState_STATE gilstate;

    if (filename == NULL) {
        return 0;
    }

    if (context->event && !hooks->map_path_isabs(filename)) {
        char *path;
        object *env;

        env = hooks->get_env_recursive(context->event);

        path = hooks->map_get_path(env->map, filename, 0, NULL);
        FREE_AND_COPY_HASH(context->event->race, path);
        efree(path);
    }

    if (context->event != NULL && hooks->string_endswith(filename, ".xml")) {
        char *path;

        if (hooks->string_endswith(filename, "quest.xml")) {
            char *dirname, inf_filename[MAX_BUF];
            const char *cp;
            size_t i;

            for (cp = context->who->name, i = 0; *cp != '\0'; cp++) {
                if (i == sizeof(inf_filename) - 1) {
                    break;
                }

                if (*cp == '_' || *cp == ' ' || isalpha(*cp) || isdigit(*cp)) {
                    inf_filename[i] = *cp == ' ' ? '_' : tolower(*cp);
                    i++;
                }
            }

            inf_filename[i] = '\0';
            strncat(inf_filename, ".py", sizeof(inf_filename) - i - 1);

            dirname = hooks->path_dirname(filename);
            path = hooks->path_join(dirname, inf_filename);
            efree(dirname);
        } else {
            char *cp;

            cp = hooks->string_sub(filename, 0, -3 MEMORY_DEBUG_INFO);
            path = hooks->string_join("", cp, "py", NULL);
            efree(cp);
        }

        FREE_AND_COPY_HASH(context->event->race, path);
        efree(path);
    }

    gilstate = PyGILState_Ensure();

    pycode = compilePython(hooks->create_pathname(
            context->event != NULL ? context->event->race : filename));
    if (pycode != NULL) {
        if (hooks->settings->python_reload_modules) {
            PyObject *modules = PyImport_GetModuleDict(), *key, *value;
            Py_ssize_t pos = 0;
            const char *m_filename;
            char m_buf[MAX_BUF];

            /* Create path name to the Python scripts directory. */
            strncpy(m_buf, hooks->create_pathname("/python"),
                    sizeof(m_buf) - 1);

            /* Go through the loaded modules. */
            while (PyDict_Next(modules, &pos, &key, &value)) {
                m_filename = PyModule_GetFilename(value);

                if (!m_filename) {
                    PyErr_Clear();
                    continue;
                }

                /* If this module was loaded from one of our script files,
                 * reload it. */
                if (!strncmp(m_filename, m_buf, strlen(m_buf))) {
                    PyImport_ReloadModule(value);

                    if (PyErr_Occurred()) {
                        PyErr_LOG();
                    }
                }
            }
        }

        pushContext(context);
        dict = PyDict_Copy(py_globals_dict);
        PyDict_SetItemString(dict, "activator",
                wrap_object(context->activator));
        PyDict_SetItemString(dict, "pl",
                wrap_player(context->activator != NULL &&
                context->activator->type == PLAYER ?
                    CONTR(context->activator) : NULL));
        PyDict_SetItemString(dict, "me", wrap_object(context->who));

        if (context->text) {
            PyDict_SetItemString(dict, "msg",
                    Py_BuildValue("s", context->text));
        } else if (context->event && context->event->sub_type == EVENT_SAY) {
            PyDict_SetItemString(dict, "msg", Py_BuildValue(""));
        }

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 2
        ret = PyEval_EvalCode((PyObject *) pycode, dict, NULL);
#else
        ret = PyEval_EvalCode(pycode, dict, NULL);
#endif

        if (PyErr_Occurred()) {
            PyErr_LOG();
        }

        Py_XDECREF(ret);
        Py_DECREF(dict);

        PyGILState_Release(gilstate);

        return 1;
    }

    PyGILState_Release(gilstate);

    return 0;
}

/** @copydoc command_func */
static void command_custom_python(object *op, const char *command, char *params)
{
    PythonContext *context = malloc(sizeof(PythonContext));
    if (context == NULL) {
        return;
    }

    context->activator = op;
    context->who = op;
    context->other = op;
    context->event = NULL;
    context->parms[0] = 0;
    context->parms[1] = 0;
    context->parms[2] = 0;
    context->parms[3] = 0;
    context->text = params ? params : "";
    context->options = NULL;
    context->returnvalue = 0;

    char path[MAX_BUF];
    snprintf(VS(path), "/python/commands/%s.py", command);
    if (!do_script(context, path)) {
        freeContext(context);
        return;
    }

    context = popContext();
    freeContext(context);
}

/** Documentation for Atrinik_LoadObject(). */
static const char doc_Atrinik_LoadObject[] =
".. function:: LoadObject(text).\n\n"
"Load an object from a string dump, for example, one stored using "
":meth:`Atrinik.Object.Object.Save`.\n\n"
":param text: The object text dump.\n"
":type text: str\n"
":returns: New object, loaded from the text or None in case of failure.\n"
":rtype: :class:`Atrinik.Object.Object` or None";

/**
 * Implements Atrinik.LoadObject() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_LoadObject(PyObject *self, PyObject *args)
{
    char *dumpob;

    if (!PyArg_ParseTuple(args, "s", &dumpob)) {
        return NULL;
    }

    return wrap_object(hooks->load_object_str(dumpob));
}

/** Documentation for Atrinik_ReadyMap(). */
static const char doc_Atrinik_ReadyMap[] =
".. function:: ReadyMap(path, unique=False).\n\n"
"Make sure the named map is loaded into memory, loading it if necessary.\n\n"
":param path: Path to the map.\n"
":type path: str\n"
":param unique: Whether the destination should be loaded as a unique map, for "
"example, apartments.\n"
":type unique: bool\n"
":returns: The map associated with the specified path or None in case of "
"failure.\n"
":rtype: :class:`Atrinik.Map.Map` or None";

/**
 * Implements Atrinik.ReadyMap() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_ReadyMap(PyObject *self, PyObject *args)
{
    const char *path;
    int flags = 0, unique = 0;

    if (!PyArg_ParseTuple(args, "s|i", &path, &unique)) {
        return NULL;
    }

    if (unique) {
        flags |= MAP_PLAYER_UNIQUE;
    }

    return wrap_map(hooks->ready_map_name(path, NULL, flags));
}

/** Documentation for Atrinik_FindPlayer(). */
static const char doc_Atrinik_FindPlayer[] =
".. function:: FindPlayer(name).\n\n"
"Find a player by name.\n\n"
":param name: The player name to find.\n"
":type name: str\n"
":returns: The player's object if found, None otherwise.\n"
":rtype: :class:`Atrinik.Object.Object` or None";

/**
 * Implements Atrinik.FindPlayer() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_FindPlayer(PyObject *self, PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    player *pl = hooks->find_player(name);
    if (pl != NULL) {
        return wrap_object(pl->ob);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_PlayerExists(). */
static const char doc_Atrinik_PlayerExists[] =
".. function:: PlayerExists(name).\n\n"
"Check if player exists.\n\n"
":param name: The player name to check.\n"
":type name: str\n"
":returns: True if the player exists, False otherwise.\n"
":rtype: bool";

/**
 * Implements Atrinik.PlayerExists() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_PlayerExists(PyObject *self, PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    char *cp = strdup(name);
    hooks->player_cleanup_name(cp);
    int ret = hooks->player_exists(cp);
    free(cp);

    return Py_BuildBoolean(ret);
}

/** Documentation for Atrinik_WhoAmI(). */
static const char doc_Atrinik_WhoAmI[] =
".. function:: WhoAmI().\n\n"
"Get the owner of the active script (the object that has the event "
"handler).\n\n"
":returns: The script owner.\n"
":rtype: :class:`Atrinik.Object.Object` or None\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.WhoAmI() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_WhoAmI(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return wrap_object(current_context->who);
}

/** Documentation for Atrinik_WhoIsActivator(). */
static const char doc_Atrinik_WhoIsActivator[] =
".. function:: WhoIsActivator().\n\n"
"Get the object that activated the current event.\n\n"
":returns: The script activator.\n"
":rtype: :class:`Atrinik.Object.Object` or None\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.WhoIsActivator() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_WhoIsActivator(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return wrap_object(current_context->activator);
}

/** Documentation for Atrinik_WhoIsOther(). */
static const char doc_Atrinik_WhoIsOther[] =
".. function:: WhoIsOther().\n\n"
"Get another object related to the event. What this object is depends on the "
"event.\n\n"
":returns: The other object.\n"
":rtype: :class:`Atrinik.Object.Object` or None\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.WhoIsOther() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_WhoIsOther(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return wrap_object(current_context->other);
}

/** Documentation for Atrinik_WhatIsEvent(). */
static const char doc_Atrinik_WhatIsEvent[] =
".. function:: WhatIsEvent().\n\n"
"Get the event object that caused this event to trigger.\n\n"
":returns: The event object.\n"
":rtype: :class:`Atrinik.Object.Object` or None\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.WhatIsEvent() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_WhatIsEvent(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return wrap_object(current_context->event);
}

/** Documentation for Atrinik_GetEventNumber(). */
static const char doc_Atrinik_GetEventNumber[] =
".. function:: GetEventNumber().\n\n"
"Get the ID of the event that is being triggered.\n\n"
":returns: Event ID.\n"
":rtype: int\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).\n"
":raises Atrinik.AtrinikError: If there is no event object.";

/**
 * Implements Atrinik.GetEventNumber() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetEventNumber(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    if (current_context->event == NULL) {
        PyErr_SetString(AtrinikError, "There is no event object.");
        return NULL;
    }

    return Py_BuildValue("i", current_context->event->sub_type);
}

/** Documentation for Atrinik_WhatIsMessage(). */
static const char doc_Atrinik_WhatIsMessage[] =
".. function:: WhatIsMessage().\n\n"
"Gets the actual message in SAY events.\n\n"
":returns: The message.\n"
":rtype: str or None\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.WhatIsMessage() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_WhatIsMessage(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return Py_BuildValue("s", current_context->text);
}

/** Documentation for Atrinik_GetOptions(). */
static const char doc_Atrinik_GetOptions[] =
".. function:: GetOptions().\n\n"
"Gets the script options (as passed in the event's slaying field).\n\n"
":returns: The script options.\n"
":rtype: str or None\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.GetOptions() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetOptions(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return Py_BuildValue("s", current_context->options);
}

/** Documentation for Atrinik_GetReturnValue(). */
static const char doc_Atrinik_GetReturnValue[] =
".. function:: GetReturnValue().\n\n"
"Gets the script's return value.\n\n"
":returns: The return value.\n"
":rtype: int\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.GetReturnValue() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetReturnValue(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    return Py_BuildValue("i", current_context->returnvalue);
}

/** Documentation for Atrinik_SetReturnValue(). */
static const char doc_Atrinik_SetReturnValue[] =
".. function:: SetReturnValue(value).\n\n"
"Sets the script's return value.\n\n"
":param value: The new return value.\n"
":type value: int\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.SetReturnValue() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_SetReturnValue(PyObject *self, PyObject *args)
{
    int value;

    if (!PyArg_ParseTuple(args, "i", &value)) {
        return NULL;
    }

    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    current_context->returnvalue = value;

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_GetEventParameters(). */
static const char doc_Atrinik_GetEventParameters[] =
".. function:: GetEventParameters().\n\n"
"Get the parameters of an event. This varies from event to event, and some "
"events pass all parameters as 0. EVENT_ATTACK usually passes damage done and "
"the WC of the hit as second and third parameter, respectively.\n\n"
":returns: A list of the event parameters. The last entry is the event flags, "
"used to determine whom to call :meth:`Atrinik.Object.Object.Update` on after "
"executing the script.\n"
":rtype: list\n"
":raises Atrinik.AtrinikError: If there's no event context (for example, the "
"script is running in a thread).";

/**
 * Implements Atrinik.GetEventParameters() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetEventParameters(PyObject *self)
{
    if (current_context == NULL) {
        PyErr_SetString(AtrinikError, "There is no event context.");
        return NULL;
    }

    PyObject *list = PyList_New(0);
    for (size_t i = 0; i < arraysize(current_context->parms); i++) {
        PyList_Append(list, Py_BuildValue("i", current_context->parms[i]));
    }

    return list;
}

/** Documentation for Atrinik_RegisterCommand(). */
static const char doc_Atrinik_RegisterCommand[] =
".. function:: RegisterCommand(name, speed, flags=0).\n\n"
"Register a custom command ran using Python script.\n\n"
":param name: Name of the command. For example, \"roll\" in order to create "
"/roll command. Note the lack of forward slash in the name.\n"
":type name: str\n"
":param speed: How long it takes to execute the command; 1.0 is usually fine.\n"
":type speed: float\n"
":param flags: Optional flags.\n"
":type flags: int";

/**
 * Implements Atrinik.RegisterCommand() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_RegisterCommand(PyObject *self, PyObject *args)
{
    const char *name;
    double speed;
    uint64_t flags = 0;

    if (!PyArg_ParseTuple(args, "sd|K", &name, &speed, &flags)) {
        return NULL;
    }

    hooks->commands_add(name, command_custom_python, speed, flags);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_CreatePathname(). */
static const char doc_Atrinik_CreatePathname[] =
".. function:: CreatePathname(path).\n\n"
"Creates path to a file in the maps directory. For example, '/hall_of_dms' -> "
"'../maps/hall_of_dms'.\n\n"
":param path: Path to the map.\n"
":type path: str\n"
":returns: Real path of the map on the system.\n"
":rtype: str";

/**
 * Implements Atrinik.CreatePathname() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CreatePathname(PyObject *self, PyObject *args)
{
    const char *path;

    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }

    return Py_BuildValue("s", hooks->create_pathname(path));
}

/** Documentation for Atrinik_GetTime(). */
static const char doc_Atrinik_GetTime[] =
".. function:: GetTime().\n\n"
"Get the game time.\n\n"
":returns: A dictionary containing all the information about the in-game "
"time:\n\n"
"  * **year**: Current year.\n"
"  * **month**: Current month.\n"
"  * **month_name**: Name of the current month.\n"
"  * **day**: Day.\n"
"  * **hour**: Hour.\n"
"  * **minute**: Minute.\n"
"  * **dayofweek**: Day of the week.\n"
"  * **dayofweek_name**: Name of the week day.\n"
"  * **season**: Season.\n"
"  * **season_name**: Name of the season.\n"
"  * **periodofday**: Period of the day.\n"
"  * **periodofday_name**: Name of the period of the day.\n"
":rtype: dict";

/**
 * Implements Atrinik.GetTime() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetTime(PyObject *self)
{
    timeofday_t tod;
    hooks->get_tod(&tod);

    PyObject *dict = PyDict_New();
    PyDict_SetItemString(dict, "year",
            Py_BuildValue("i", tod.year + 1));
    PyDict_SetItemString(dict, "month",
            Py_BuildValue("i", tod.month + 1));
    PyDict_SetItemString(dict, "month_name",
            Py_BuildValue("s", hooks->month_name[tod.month]));
    PyDict_SetItemString(dict, "day",
            Py_BuildValue("i", tod.day + 1));
    PyDict_SetItemString(dict, "hour",
            Py_BuildValue("i", tod.hour));
    PyDict_SetItemString(dict, "minute",
            Py_BuildValue("i", tod.minute));
    PyDict_SetItemString(dict, "dayofweek",
            Py_BuildValue("i", tod.dayofweek + 1));
    PyDict_SetItemString(dict, "dayofweek_name",
            Py_BuildValue("s", hooks->weekdays[tod.dayofweek]));
    PyDict_SetItemString(dict, "season",
            Py_BuildValue("i", tod.season + 1));
    PyDict_SetItemString(dict, "season_name",
            Py_BuildValue("s", hooks->season_name[tod.season]));
    PyDict_SetItemString(dict, "periodofday",
            Py_BuildValue("i", tod.periodofday + 1));
    PyDict_SetItemString(dict, "periodofday_name",
            Py_BuildValue("s", hooks->periodsofday[tod.periodofday]));

    return dict;
}

/** Documentation for Atrinik_FindParty(). */
static const char doc_Atrinik_FindParty[] =
".. function:: FindParty(name).\n\n"
"Find a party by name.\n\n"
":param name: The party name to find.\n"
":type name: str\n"
":returns: The party if found, None otherwise.\n"
":rtype: :class:`Atrinik.Party.Party` or None";

/**
 * Implements Atrinik.FindParty() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_FindParty(PyObject *self, PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    return wrap_party(hooks->find_party(name));
}

/** Documentation for Atrinik_Logger(). */
static const char doc_Atrinik_Logger[] =
".. function:: Logger(level, message).\n\n"
"Logs a message.\n\n"
":param level: Level of the log message, eg, \"BUG\", \"ERROR\", \"CHAT\", "
"\"INFO\", etc.\n"
":type level: str\n"
":param message: The message to log. Cannot contain newlines.\n"
":type message: str";

/**
 * Implements Atrinik.Logger() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Logger(PyObject *self, PyObject *args)
{
    const char *mode, *string;

    if (!PyArg_ParseTuple(args, "ss", &mode, &string)) {
        return NULL;
    }

    logger_print(hooks->logger_get_level(mode), __FUNCTION__, __LINE__,
                 string);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_GetRangeVectorFromMapCoords(). */
static const char doc_Atrinik_GetRangeVectorFromMapCoords[] =
".. function:: GetRangeVectorFromMapCoords(map1, x1, y1, map2, x2, y2, "
"flags=0).\n\n"
"Get the distance and direction from one map coordinate to another.\n\n"
":param map1: From which map to get the distance from.\n"
":type map1: :class:`Atrinik.Map.Map`\n"
":param x1: X coordinate on *map1*.\n"
":type x1: int\n"
":param y1: Y coordinate on *map1*.\n"
":type y1: int\n"
":param map2: From which map to get the distance to.\n"
":type map2: :class:`Atrinik.Map.Map`\n"
":param x2: X coordinate on *map2*.\n"
":type x2: int\n"
":param y2: Y coordinate on *map2*.\n"
":type y2: int\n"
":param flags: One or a combination of RV_xxx, eg, :attr:"
"`~Atrinik.RV_MANHATTAN_DISTANCE`\n"
":type flags: int\n"
":returns: None if the distance couldn't be calculated, otherwise a "
"tuple containing:\n\n"
"  * Direction from the first coordinate to the second, eg, :attr:\n"
"    `~Atrinik.NORTH`\n"
"  * Distance between the two coordinates.\n"
"  * X distance.\n"
"  * Y distance.\n"
":rtype: tuple or None";

/**
 * Implements Atrinik.GetRangeVectorFromMapCoords() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_GetRangeVectorFromMapCoords(PyObject *self,
        PyObject *args)
{
    Atrinik_Map *map, *map2;
    int x, y, x2, y2, flags = 0;

    if (!PyArg_ParseTuple(args, "O!iiO!ii|i", &Atrinik_MapType, &map, &x, &y,
            &Atrinik_MapType, &map2, &x2, &y2, &flags)) {
        return NULL;
    }

    rv_vector rv;
    if (!hooks->get_rangevector_from_mapcoords(map->map, x, y,
            map2->map, x2, y2, &rv, flags)) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject *tuple = PyTuple_New(4);
    PyTuple_SET_ITEM(tuple, 0, Py_BuildValue("i", rv.direction));
    PyTuple_SET_ITEM(tuple, 1, Py_BuildValue("i", rv.distance));
    PyTuple_SET_ITEM(tuple, 2, Py_BuildValue("i", rv.distance_x));
    PyTuple_SET_ITEM(tuple, 3, Py_BuildValue("i", rv.distance_y));

    return tuple;
}

/** Documentation for Atrinik_CostString(). */
static const char doc_Atrinik_CostString[] =
".. function:: CostString(value).\n\n"
"Build a string representation of the value in the game's money syntax, for "
"example, a value of 134 would become \"1 silver coin and 34 copper "
"coins\".\n\n"
":param value: Value to build the string from.\n"
":type value: int\n"
":returns: The string.\n"
":rtype: str";

/**
 * Implements Atrinik.CostString() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CostString(PyObject *self, PyObject *args)
{
    int64_t value;

    if (!PyArg_ParseTuple(args, "L", &value)) {
        return NULL;
    }

    return Py_BuildValue("s", hooks->shop_get_cost_string(value));
}

/** Documentation for Atrinik_CacheAdd(). */
static const char doc_Atrinik_CacheAdd[] =
".. function:: CacheAdd(key, what).\n\n"
"Store 'what' in memory identified by unique identifier 'key'.\n\n"
"The object will be stored forever in memory, until it's either removed by "
":func:`~Atrinik.CacheRemove` or the server is shut down; in both cases, the "
"object will be closed, if applicable (databases, file objects, etc)\n\n."
"A stored object can be retrieved at any time using :func:"
"`~Atrinik.CacheGet`.\n\n"
":param key: The unique identifier for the cache entry.\n"
":type key: str\n"
":param what: Any Python object (string, integer, database, etc) to store in "
"memory.\n"
":type what: object\n"
":returns: True if the object was cached successfully, False otherwise (cache "
"entry with same key name already exists).\n"
":rtype: bool";

/**
 * Implements Atrinik.CacheAdd() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CacheAdd(PyObject *self, PyObject *args)
{
    const char *key;
    PyObject *what;

    if (!PyArg_ParseTuple(args, "sO", &key, &what)) {
        return NULL;
    }

    /* Add it to the cache. */
    int ret = hooks->cache_add(key, what, CACHE_FLAG_PYOBJ | CACHE_FLAG_GEVENT);
    if (ret) {
        Py_INCREF(what);
    }

    return Py_BuildBoolean(ret);
}

/** Documentation for Atrinik_CacheGet(). */
static const char doc_Atrinik_CacheGet[] =
".. function:: CacheGet(key).\n\n"
"Attempt to find a cache entry identified by 'key' that was previously added "
"using :func:`~Atrinik.CacheAdd`.\n\n"
":param key: Unique identifier of the cache entry to find.\n"
":type key: str\n"
":returns: The cache entry.\n"
":rtype: object\n"
":raises ValueError: If the cache entry could not be found.";

/**
 * Implements Atrinik.CacheGet() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CacheGet(PyObject *self, PyObject *args)
{
    const char *key;

    if (!PyArg_ParseTuple(args, "s", &key)) {
        return NULL;
    }

    shstr *sh_key = hooks->find_string(key);
    if (sh_key == NULL) {
        goto not_found;
    }

    cache_struct *result = hooks->cache_find(sh_key);
    if (result == NULL) {
        goto not_found;
    }

    /* Even if the cache entry was found, pretend it doesn't exist if
     * CACHE_FLAG_PYOBJ is not set. */
    if (!(result->flags & CACHE_FLAG_PYOBJ)) {
        goto not_found;
    }

    Py_INCREF((PyObject *) result->ptr);
    return result->ptr;

not_found:
    PyErr_SetString(PyExc_ValueError, "No such cache entry.");
    return NULL;
}

/** Documentation for Atrinik_CacheRemove(). */
static const char doc_Atrinik_CacheRemove[] =
".. function:: CacheRemove(key).\n\n"
"Remove a cache entry that was added with a previous call to :func:"
"`~Atrinik.CacheAdd`.\n\n"
":param key: Unique identifier of the cache entry to remove.\n"
":type key: str\n"
":returns: True is always returned.\n"
":rtype: bool\n"
":raises ValueError: If the cache entry could not be removed (it didn't exist)";

/**
 * Implements Atrinik.CacheRemove() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CacheRemove(PyObject *self, PyObject *args)
{
    const char *key;

    if (!PyArg_ParseTuple(args, "s", &key)) {
        return NULL;
    }

    shstr *sh_key = hooks->find_string(key);
    if (sh_key == NULL) {
        goto not_found;
    }

    if (!hooks->cache_remove(sh_key)) {
        goto not_found;
    }

    Py_INCREF(Py_True);
    return Py_True;

not_found:
    PyErr_SetString(PyExc_ValueError, "No such cache entry.");
    return NULL;
}

/** Documentation for Atrinik_GetFirst(). */
static const char doc_Atrinik_GetFirst[] =
".. function:: GetFirst(what).\n\n"
"Get first member of various linked lists.\n\n"
":param what: What list to get first member of. Available list names:\n\n"
"  * player: First player.\n"
"  * map: First map.\n"
"  * archetype: First archetype.\n"
"  * party: First party.\n"
"  * region: First region.\n"
":type what: str\n"
":returns: First member of the specified linked list.\n"
":rtype: :class:`Atrinik.Player.Player` or :class:`Atrinik.Map.Map` or "
":class:`Atrinik.Party.Party` or :class:`Atrinik.Region.Region`\n"
":raises ValueError: If *what* is invalid.";

/**
 * Implements Atrinik.GetFirst() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_GetFirst(PyObject *self, PyObject *args)
{
    const char *what;

    if (!PyArg_ParseTuple(args, "s", &what)) {
        return NULL;
    }

    if (strcmp(what, "player") == 0) {
        return wrap_player(*hooks->first_player);
    } else if (strcmp(what, "map") == 0) {
        return wrap_map(*hooks->first_map);
    } else if (strcmp(what, "party") == 0) {
        return wrap_party(*hooks->first_party);
    } else if (strcmp(what, "region") == 0) {
        return wrap_region(*hooks->first_region);
    }

    PyErr_Format(PyExc_ValueError, "'%s' is not a valid linked list.", what);
    return NULL;
}

/** Documentation for Atrinik_CreateMap(). */
static const char doc_Atrinik_CreateMap[] =
".. function:: CreateMap(width, height, path).\n\n"
"Creates an empty map.\n\n"
":param width: The new map's width.\n"
":type width: int\n"
":param height: The new map's height.\n"
":type height: int\n"
":param path: Path to the new map. This should be a unique path to avoid "
"collisions. \"/python-maps/\" is prepended to this to ensure no collision "
"with regular maps.\n"
":type path: str\n"
":returns: The new empty map.\n"
":rtype: :class:`Atrinik.Map.Map`";

/**
 * Implements Atrinik.CreateMap() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CreateMap(PyObject *self, PyObject *args)
{
    int width, height;
    const char *path;

    if (!PyArg_ParseTuple(args, "iis", &width, &height, &path)) {
        return NULL;
    }

    mapstruct *m = hooks->get_empty_map(width, height);
    char buf[HUGE_BUF];
    snprintf(VS(buf), "/python-maps/%s", path);
    m->path = hooks->add_string(buf);

    return wrap_map(m);
}

/** Documentation for Atrinik_CreateObject(). */
static const char doc_Atrinik_CreateObject[] =
".. function:: CreateObject(archname).\n\n"
"Creates a new object. Note that if the created object is not put on map or "
"inside an inventory of another object, it will be considered a leaked object. "
"Use :meth:`Atrinik.Object.Object.Destroy` to free it if you no longer need "
"it.\n\n"
":param archname: Name of the arch to create.\n"
":type archname: str\n"
":returns: The newly created object.\n"
":rtype: :class:`Atrinik.Object.Object`\n"
":raises Atrinik.AtrinikError: If *archname* is not a valid archetype.";

/**
 * Implements Atrinik.CreateObject() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_CreateObject(PyObject *self, PyObject *args)
{
    const char *archname;

    if (!PyArg_ParseTuple(args, "s", &archname)) {
        return NULL;
    }

    archetype_t *at = hooks->arch_find(archname);
    if (at == NULL) {
        PyErr_Format(AtrinikError, "The archetype '%s' doesn't exist.",
                archname);
        return NULL;
    }

    return wrap_object(hooks->arch_to_object(at));
}

/** Documentation for Atrinik_GetTicks(). */
static const char doc_Atrinik_GetTicks[] =
".. function:: GetTicks().\n\n"
"Acquires the current server ticks value.\n\n"
":returns: The server ticks.\n"
":rtype: int";

/**
 * Implements Atrinik.GetTicks() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetTicks(PyObject *self)
{
    return Py_BuildValue("l", *hooks->pticks);
}

/** Documentation for Atrinik_GetArchetype(). */
static const char doc_Atrinik_GetArchetype[] =
".. function:: GetArchetype(archname).\n\n"
"Finds an archetype.\n\n"
":param archname: Name of the archetype to find.\n"
":type archname: str\n"
":returns: The archetype.\n"
":rtype: :class:`Atrinik.Archetype.Archetype`\n"
":raises Atrinik.AtrinikError: If *archname* is not a valid archetype.";

/**
 * Implements Atrinik.GetArchetype() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_GetArchetype(PyObject *self, PyObject *args)
{
    const char *archname;

    if (!PyArg_ParseTuple(args, "s", &archname)) {
        return NULL;
    }

    archetype_t *at = hooks->arch_find(archname);
    if (at == NULL) {
        PyErr_Format(AtrinikError, "The archetype '%s' doesn't exist.",
                archname);
        return NULL;
    }

    return wrap_archetype(at);
}

/** Documentation for Atrinik_print(). */
static const char doc_Atrinik_print[] =
".. function:: print(...).\n\n"
"Prints the string representations of the given objects to the server log, as "
"well as all online DMs. Essentially a replacement for standard library "
"print() function.\n\n";

/**
 * Implements print() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_print(PyObject *self, PyObject *args)
{
    StringBuffer *sb = hooks->stringbuffer_new();

    for (Py_ssize_t i = 0; i < PyTuple_Size(args); i++) {
        if (i > 0) {
            hooks->stringbuffer_append_string(sb, " ");
        }

        hooks->stringbuffer_append_string(sb,
                PyString_AsString(PyObject_Str(PyTuple_GetItem(args, i))));
    }

    char *cp = hooks->stringbuffer_finish(sb);

    PyObject *locals = PyDict_New();
    PyDict_SetItemString(locals, "print_msg", Py_BuildValue("s", cp));
    efree(cp);

    py_runfile_simple("/python/events/python_print.py", locals);
    Py_DECREF(locals);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Eval(). */
static const char doc_Atrinik_Eval[] =
".. function:: Eval(code, seconds=0.0).\n\n"
"Executes the specified code from the main thread after the specified delay in "
"seconds.\n\n"
":param code: The code to compile and execute.\n"
":type code: str\n"
":param seconds: How long to wait, eg, 0.5 for half a second, 10.0 for 10 "
"seconds, etc.\n"
":type seconds: float";

/**
 * Implements Atrinik.Eval() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Eval(PyObject *self, PyObject *args)
{
    double seconds = 0.0f;
    const char *s;
    struct _node *n;
    PyCodeObject *code = NULL;

    if (!PyArg_ParseTuple(args, "s|d", &s, &seconds)) {
        return NULL;
    }

    n = PyParser_SimpleParseString(s, Py_file_input);

    if (n != NULL) {
        code = PyNode_Compile(n, "eval'd code");
        PyNode_Free(n);
    }

    if (PyErr_Occurred()) {
        PyErr_LOG();
        return NULL;
    }

    if (code != NULL) {
        python_eval_struct *tmp;
        PyGILState_STATE gilstate;
        struct timeval tv;

        gettimeofday(&tv, NULL);

        tmp = malloc(sizeof(*tmp));
        tmp->globals = PyEval_GetGlobals();
        Py_INCREF(tmp->globals);
        tmp->locals = PyEval_GetLocals();
        Py_INCREF(tmp->locals);
        tmp->code = code;
        tmp->seconds = tv.tv_sec + tv.tv_usec / 1000000. + seconds;
        DL_APPEND(python_eval, tmp);

        gilstate = PyGILState_Ensure();
        DL_APPEND(python_eval, tmp);
        PyGILState_Release(gilstate);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_GetSettings(). */
static const char doc_Atrinik_GetSettings[] =
".. function:: GetSettings().\n\n"
"Acquire a dictionary containing the server's settings.\n\n"
":returns: Dictionary with the server's settings, such as the maps path.\n"
":rtype: dict";

/**
 * Implements Atrinik.GetSettings() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_GetSettings(PyObject *self)
{
    PyObject *dict = PyDict_New();
    PyDict_SetItemString(dict, "port", Py_BuildValue("H",
            hooks->settings->port));
    PyDict_SetItemString(dict, "libpath", Py_BuildValue("s",
            hooks->settings->libpath));
    PyDict_SetItemString(dict, "datapath", Py_BuildValue("s",
            hooks->settings->datapath));
    PyDict_SetItemString(dict, "mapspath", Py_BuildValue("s",
            hooks->settings->mapspath));
    PyDict_SetItemString(dict, "httppath", Py_BuildValue("s",
            hooks->settings->httppath));
    PyDict_SetItemString(dict, "metaserver_url", Py_BuildValue("s",
            hooks->settings->metaserver_url));
    PyDict_SetItemString(dict, "server_host", Py_BuildValue("s",
            hooks->settings->server_host));
    PyDict_SetItemString(dict, "server_name", Py_BuildValue("s",
            hooks->settings->server_name));
    PyDict_SetItemString(dict, "server_desc", Py_BuildValue("s",
            hooks->settings->server_desc));
    PyDict_SetItemString(dict, "world_maker", Py_BuildBoolean(
            hooks->settings->world_maker));
    PyDict_SetItemString(dict, "unit_tests", Py_BuildBoolean(
            hooks->settings->unit_tests));
    PyDict_SetItemString(dict, "plugin_unit_tests", Py_BuildBoolean(
            hooks->settings->plugin_unit_tests));
    PyDict_SetItemString(dict, "plugin_unit_test", Py_BuildValue("s",
            hooks->settings->plugin_unit_test));
    PyDict_SetItemString(dict, "magic_devices_level", Py_BuildValue("b",
            hooks->settings->magic_devices_level));
    PyDict_SetItemString(dict, "magic_devices_level", Py_BuildValue("b",
            hooks->settings->magic_devices_level));
    PyDict_SetItemString(dict, "item_power_factor", Py_BuildValue("d",
            hooks->settings->item_power_factor));
    PyDict_SetItemString(dict, "python_reload_modules", Py_BuildBoolean(
            hooks->settings->python_reload_modules));
    PyDict_SetItemString(dict, "default_permission_groups", Py_BuildValue("s",
            hooks->settings->default_permission_groups));

    PyObject *list_allowed_chars = PyList_New(ALLOWED_CHARS_NUM);
    PyDict_SetItemString(dict, "allowed_chars", list_allowed_chars);
    PyObject *list_limits = PyList_New(ALLOWED_CHARS_NUM);
    PyDict_SetItemString(dict, "limits", list_limits);

    for (Py_ssize_t i = 0; i < ALLOWED_CHARS_NUM; i++) {
        PyList_SetItem(list_allowed_chars, i, Py_BuildValue("s",
                hooks->settings->allowed_chars[i]));
        PyList_SetItem(list_limits, i, Py_BuildValue("KK",
                (unsigned PY_LONG_LONG) hooks->settings->limits[i][0],
                (unsigned PY_LONG_LONG) hooks->settings->limits[i][1]));
    }

    PyDict_SetItemString(dict, "control_allowed_ips", Py_BuildValue("s",
            hooks->settings->control_allowed_ips));
    PyDict_SetItemString(dict, "control_player", Py_BuildValue("s",
            hooks->settings->control_player));
    PyDict_SetItemString(dict, "recycle_tmp_maps", Py_BuildBoolean(
            hooks->settings->recycle_tmp_maps));
    PyDict_SetItemString(dict, "http_url", Py_BuildValue("s",
            hooks->settings->http_url));
    return dict;
}

/** Documentation for Atrinik_Process(). */
static const char doc_Atrinik_Process[] =
".. function:: Process().\n\n"
"Simulates server processing.\n\n"
":raises Atrinik.AtrinikError: If called outside the plugin unit tests.";

/**
 * Implements Atrinik.Process() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Process(PyObject *self)
{
    if (!hooks->settings->plugin_unit_tests) {
        PyErr_SetString(AtrinikError, "Method cannot be used outside of unit "
                "tests.");
        return NULL;
    }

    hooks->main_process();

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * Here is the Python Declaration Table, used by the interpreter to make
 * an interface with the C code.
 */
static PyMethodDef AtrinikMethods[] = {
    {"LoadObject", (PyCFunction) Atrinik_LoadObject, METH_VARARGS,
            doc_Atrinik_LoadObject},
    {"ReadyMap", (PyCFunction) Atrinik_ReadyMap, METH_VARARGS,
            doc_Atrinik_ReadyMap},
    {"FindPlayer", (PyCFunction) Atrinik_FindPlayer, METH_VARARGS,
            doc_Atrinik_FindPlayer},
    {"PlayerExists", (PyCFunction) Atrinik_PlayerExists, METH_VARARGS,
            doc_Atrinik_PlayerExists},
    {"WhoAmI", (PyCFunction) Atrinik_WhoAmI, METH_NOARGS,
            doc_Atrinik_WhoAmI},
    {"WhoIsActivator", (PyCFunction) Atrinik_WhoIsActivator, METH_NOARGS,
            doc_Atrinik_WhoIsActivator},
    {"WhoIsOther", (PyCFunction) Atrinik_WhoIsOther, METH_NOARGS,
            doc_Atrinik_WhoIsOther},
    {"WhatIsEvent", (PyCFunction) Atrinik_WhatIsEvent, METH_NOARGS,
            doc_Atrinik_WhatIsEvent},
    {"GetEventNumber", (PyCFunction) Atrinik_GetEventNumber, METH_NOARGS,
            doc_Atrinik_GetEventNumber},
    {"WhatIsMessage", (PyCFunction) Atrinik_WhatIsMessage, METH_NOARGS,
            doc_Atrinik_WhatIsMessage},
    {"GetOptions", (PyCFunction) Atrinik_GetOptions, METH_NOARGS,
            doc_Atrinik_GetOptions},
    {"GetReturnValue", (PyCFunction) Atrinik_GetReturnValue, METH_NOARGS,
            doc_Atrinik_GetReturnValue},
    {"SetReturnValue", (PyCFunction) Atrinik_SetReturnValue, METH_VARARGS,
            doc_Atrinik_SetReturnValue},
    {"GetEventParameters", (PyCFunction) Atrinik_GetEventParameters,
            METH_NOARGS, doc_Atrinik_GetEventParameters},
    {"RegisterCommand", (PyCFunction) Atrinik_RegisterCommand, METH_VARARGS,
            doc_Atrinik_RegisterCommand},
    {"CreatePathname", (PyCFunction) Atrinik_CreatePathname, METH_VARARGS,
            doc_Atrinik_CreatePathname},
    {"GetTime", (PyCFunction) Atrinik_GetTime, METH_NOARGS,
            doc_Atrinik_GetTime},
    {"FindParty", (PyCFunction) Atrinik_FindParty, METH_VARARGS,
            doc_Atrinik_FindParty},
    {"Logger", (PyCFunction) Atrinik_Logger, METH_VARARGS,
            doc_Atrinik_Logger},
    {"GetRangeVectorFromMapCoords",
            (PyCFunction) Atrinik_GetRangeVectorFromMapCoords, METH_VARARGS,
            doc_Atrinik_GetRangeVectorFromMapCoords},
    {"CostString", (PyCFunction) Atrinik_CostString, METH_VARARGS,
            doc_Atrinik_CostString},
    {"CacheAdd", (PyCFunction) Atrinik_CacheAdd, METH_VARARGS,
            doc_Atrinik_CacheAdd},
    {"CacheGet", (PyCFunction) Atrinik_CacheGet, METH_VARARGS,
            doc_Atrinik_CacheGet},
    {"CacheRemove", (PyCFunction) Atrinik_CacheRemove, METH_VARARGS,
            doc_Atrinik_CacheRemove},
    {"GetFirst", (PyCFunction) Atrinik_GetFirst, METH_VARARGS,
            doc_Atrinik_GetFirst},
    {"CreateMap", (PyCFunction) Atrinik_CreateMap, METH_VARARGS,
            doc_Atrinik_CreateMap},
    {"CreateObject", (PyCFunction) Atrinik_CreateObject, METH_VARARGS,
            doc_Atrinik_CreateObject},
    {"GetTicks", (PyCFunction) Atrinik_GetTicks, METH_NOARGS,
            doc_Atrinik_GetTicks},
    {"GetArchetype", (PyCFunction) Atrinik_GetArchetype, METH_VARARGS,
            doc_Atrinik_GetArchetype},
    {"print", (PyCFunction) Atrinik_print, METH_VARARGS,
            doc_Atrinik_print},
    {"Eval", (PyCFunction) Atrinik_Eval, METH_VARARGS,
            doc_Atrinik_Eval},
    {"GetSettings", (PyCFunction) Atrinik_GetSettings, METH_NOARGS,
            doc_Atrinik_GetSettings},
    {"Process", (PyCFunction) Atrinik_Process, METH_NOARGS,
            doc_Atrinik_Process},
    {NULL, NULL, 0, 0}
};

/**
 * Handles normal events.
 * @param args List of arguments for context.
 * @return 0 on failure, script's return value otherwise.
 */
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

    if (!do_script(context, script)) {
        freeContext(context);
        return 0;
    }

    context = popContext();

    if (context->parms[3] == SCRIPT_FIX_ALL) {
        if (context->other && IS_LIVE(context->other)) {
            hooks->living_update(context->other);
        }

        if (context->who && IS_LIVE(context->who)) {
            hooks->living_update(context->who);
        }

        if (context->activator && IS_LIVE(context->activator)) {
            hooks->living_update(context->activator);
        }
    } else if (context->parms[3] == SCRIPT_FIX_ACTIVATOR &&
            IS_LIVE(context->activator)) {
        hooks->living_update(context->activator);
    }

    rv = context->returnvalue;
    freeContext(context);

    return rv;
}

/**
 * Handles map events.
 * @param args List of arguments for context.
 * @return 0 on failure, script's return value otherwise.
 */
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

    if (!do_script(context, script)) {
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
 * @return 0.
 */
static int handle_global_event(int event_type, va_list args)
{
    PythonContext *context;

    switch (event_type) {
    case GEVENT_CACHE_REMOVED:
    {
        void *ptr = va_arg(args, void *);
        uint32_t flags = *(uint32_t *) va_arg(args, void *);

        if (flags & CACHE_FLAG_PYOBJ) {
            PyObject *retval;
            PyGILState_STATE gilstate;

            gilstate = PyGILState_Ensure();

            /* Attempt to close file/database/etc objects. */
            retval = PyObject_CallMethod(ptr, "close", "");

            /* No close() method, ignore the exception. */
            if (PyErr_Occurred() &&
                    PyErr_ExceptionMatches(PyExc_AttributeError)) {
                PyErr_Clear();
            }

            Py_XDECREF(retval);

            /* Decrease the reference count. */
            Py_DECREF((PyObject *) ptr);

            PyGILState_Release(gilstate);
        }

        return 0;
    }

    case GEVENT_TICK:
    {
        python_eval_struct *eval, *tmp;
        PyGILState_STATE gilstate;
        PyObject *ret;
        double seconds;

        gilstate = PyGILState_Ensure();

        if (python_eval != NULL) {
            struct timeval tv;

            gettimeofday(&tv, NULL);
            seconds = tv.tv_sec + tv.tv_usec / 1000000.;
        }

        DL_FOREACH_SAFE(python_eval, eval, tmp)
        {
            if (seconds < eval->seconds) {
                continue;
            }

            DL_DELETE(python_eval, eval);

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 2
            ret = PyEval_EvalCode((PyObject *) eval->code, eval->globals,
                    eval->locals);
#else
            ret = PyEval_EvalCode(eval->code, eval->globals, eval->locals);
#endif

            if (PyErr_Occurred()) {
                PyErr_LOG();
            }

            Py_XDECREF(ret);
            Py_DECREF(eval->globals);
            Py_DECREF(eval->locals);
            Py_DECREF(eval->code);

            free(eval);
        }

        PyGILState_Release(gilstate);

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

    switch (event_type) {
    case GEVENT_BORN:
        context->activator = va_arg(args, void *);
        break;

    case GEVENT_LOGIN:
        context->activator = ((player *) va_arg(args, void *))->ob;
        context->text = va_arg(args, void *);
        break;

    case GEVENT_LOGOUT:
        context->activator = ((player *) va_arg(args, void *))->ob;
        context->text = va_arg(args, void *);
        break;

    case GEVENT_PLAYER_DEATH:
        break;
    }

    if (!do_script(context, "/python/events/python_event.py")) {
        freeContext(context);
        return 0;
    }

    context = popContext();
    freeContext(context);

    return 0;
}

/**
 * Handles unit test event.
 * @param args List of arguments for context.
 * @return 0.
 */
static int handle_unit_event(va_list args)
{
    PythonContext *context = malloc(sizeof(*context));
    if (context == NULL) {
        return 0;
    }

    context->activator = va_arg(args, object *);
    context->who = va_arg(args, object *);
    context->other = NULL;
    context->event = NULL;
    context->parms[0] = 0;
    context->parms[1] = 0;
    context->parms[2] = 0;
    context->parms[3] = 0;
    context->text = "";
    context->options = NULL;
    context->returnvalue = 0;

    if (do_script(context, "/python/events/python_unit.py")) {
        context = popContext();
    }

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

    switch (event_type) {
    case PLUGIN_EVENT_NORMAL:
        result = handle_event(args);
        break;

    case PLUGIN_EVENT_MAP:
        result = handle_map_event(args);
        break;

    case PLUGIN_EVENT_GLOBAL:
        result = handle_global_event(eventcode, args);
        break;

    case PLUGIN_EVENT_UNIT:
        result = handle_unit_event(args);
        break;

    default:
        LOG(BUG, "Python: Requested unknown event type %d.", event_type);
        break;
    }

    va_end(args);
    return &result;
}

MODULEAPI void getPluginProperty(int *type, ...)
{
    va_list args;
    const char *propname;
    int size;
    char *buf;

    va_start(args, type);
    propname = va_arg(args, const char *);

    if (!strcmp(propname, "Identification")) {
        buf = va_arg(args, char *);
        size = va_arg(args, int);
        va_end(args);
        snprintf(buf, size, PLUGIN_NAME);
    } else if (!strcmp(propname, "FullName")) {
        buf = va_arg(args, char *);
        size = va_arg(args, int);
        va_end(args);
        snprintf(buf, size, PLUGIN_VERSION);
    }

    va_end(args);
}

MODULEAPI void postinitPlugin(void)
{
    PyGILState_STATE gilstate;

    hooks->register_global_event(PLUGIN_NAME, GEVENT_CACHE_REMOVED);
    hooks->register_global_event(PLUGIN_NAME, GEVENT_TICK);
    initContextStack();

    gilstate = PyGILState_Ensure();
    py_runfile_simple("/python/events/python_init.py", NULL);

    if (PyErr_Occurred()) {
        PyErr_LOG();
    }

    PyGILState_Release(gilstate);
}

#ifdef IS_PY3K
static PyModuleDef AtrinikModule = {
    PyModuleDef_HEAD_INIT,
    "Atrinik",
    NULL,
    -1,
    AtrinikMethods,
    NULL, NULL, NULL, NULL
};

static PyObject *PyInit_Atrinik(void)
{
    PyObject *m = PyModule_Create(&AtrinikModule);
    Py_INCREF(m);
    return m;
}
#endif

/**
 * Create a module.
 * @param parent Parent module.
 * @param name Name of the module.
 * @return The new module created using PyModule_New().
 */
static PyObject *module_create(PyObject *parent, const char *name)
{
    char tmp[MAX_BUF];
    snprintf(VS(tmp), "Atrinik_%s", name);
    PyObject *module = PyModule_New(tmp);
    PyDict_SetItemString(PyModule_GetDict(parent), name, module);
    return module;
}

/**
 * Creates a new module containing integer constants, and adds it to the
 * specified module.
 * @param module Module to add to.
 * @param name Name of the created module.
 * @param constants Constants to add.
 * @param doc Docstring for the created module.
 */
static void module_add_constants(PyObject *module, const char *name,
        const Atrinik_Constant *consts, const char *doc)
{
    size_t i = 0;
    PyObject *module_tmp;

    /* Create the new module. */
    module_tmp = module_create(module, name);
    PyModule_AddStringConstant(module_tmp, "__doc__", doc);

    /* Append constants. */
    while (consts[i].name) {
        PyModule_AddIntConstant(module_tmp, consts[i].name, consts[i].value);
        i++;
    }
}

/**
 * Construct a list from C array and add it to the specified module.
 * @param module Module to add to.
 * @param name Name of the list.
 * @param array Pointer to the C array.
 * @param array_size Number of entries in the C array.
 * @param type Type of the entries in the C array.
 */
static void module_add_array(PyObject *module, const char *name, void *array,
        size_t array_size, field_type type)
{
    size_t i;
    PyObject *list;

    /* Create a new list. */
    list = PyList_New(0);

    /* Add entries to the list. */
    for (i = 0; i < array_size; i++) {
        if (type == FIELDTYPE_INT32) {
            PyList_Append(list, Py_BuildValue("i", ((int32_t *) array)[i]));
        } else if (type == FIELDTYPE_CSTR) {
            PyList_Append(list, Py_BuildValue("s", ((char **) array)[i]));
        }
    }

    /* Add it to the module dictionary. */
    PyDict_SetItemString(PyModule_GetDict(module), name, list);
}

MODULEAPI void initPlugin(struct plugin_hooklist *hooklist)
{
    PyObject *m, *d, *module_tmp;
    int i;
    PyThreadState *py_tstate = NULL;

    hooks = hooklist;

#ifdef IS_PY26
    Py_Py3kWarningFlag++;
#endif

#ifdef IS_PY3K
    PyImport_AppendInittab("Atrinik", &PyInit_Atrinik);
#endif

    Py_Initialize();
    PyEval_InitThreads();

#ifdef IS_PY3K
    m = PyImport_ImportModule("Atrinik");
#else
    m = Py_InitModule("Atrinik", AtrinikMethods);
#endif

    PyModule_AddStringConstant(m, "__doc__", package_doc);
    d = PyModule_GetDict(m);
    AtrinikError = PyErr_NewException("Atrinik.error", NULL, NULL);
    PyDict_SetItemString(d, "AtrinikError", AtrinikError);

    module_tmp = module_create(m, "Object");
    if (!Atrinik_Object_init(module_tmp)) {
        return;
    }

    module_tmp = module_create(m, "Map");
    if (!Atrinik_Map_init(module_tmp)) {
        return;
    }

    module_tmp = module_create(m, "Party");
    if (!Atrinik_Party_init(module_tmp)) {
        return;
    }

    module_tmp = module_create(m, "Region");
    if (!Atrinik_Region_init(module_tmp)) {
        return;
    }

    module_tmp = module_create(m, "Player");
    if (!Atrinik_Player_init(module_tmp)) {
        return;
    }

    module_tmp = module_create(m, "Archetype");
    if (!Atrinik_Archetype_init(module_tmp)) {
        return;
    }

    module_tmp = module_create(m, "AttrList");
    if (!Atrinik_AttrList_init(module_tmp)) {
        return;
    }

    module_add_constants(m, "Type", constants_types, module_doc_type);
    module_add_array(m, "freearr_x", hooks->freearr_x, SIZEOFFREE,
            FIELDTYPE_INT32);
    module_add_array(m, "freearr_y", hooks->freearr_y, SIZEOFFREE,
            FIELDTYPE_INT32);

    /* Initialize integer constants */
    for (i = 0; constants[i].name; i++) {
        PyModule_AddIntConstant(m, constants[i].name, constants[i].value);
    }

    /* Initialize integer constants */
    for (i = 0; constants_colors[i][0]; i++) {
        PyModule_AddStringConstant(m, constants_colors[i][0],
                constants_colors[i][1]);
    }

    module_tmp = module_create(m, "Gender");
    PyModule_AddStringConstant(module_tmp, "__doc__", module_doc_gender);
    module_add_array(module_tmp, "gender_noun",
            hooks->gender_noun, GENDER_MAX, FIELDTYPE_CSTR);
    module_add_array(module_tmp, "gender_subjective",
            hooks->gender_subjective, GENDER_MAX, FIELDTYPE_CSTR);
    module_add_array(module_tmp, "gender_subjective_upper",
            hooks->gender_subjective_upper, GENDER_MAX, FIELDTYPE_CSTR);
    module_add_array(module_tmp, "gender_objective",
            hooks->gender_objective, GENDER_MAX, FIELDTYPE_CSTR);
    module_add_array(module_tmp, "gender_possessive",
            hooks->gender_possessive, GENDER_MAX, FIELDTYPE_CSTR);
    module_add_array(module_tmp, "gender_reflexive",
            hooks->gender_reflexive, GENDER_MAX, FIELDTYPE_CSTR);

    for (i = 0; constants_gender[i].name; i++) {
        PyModule_AddIntConstant(module_tmp, constants_gender[i].name,
                constants_gender[i].value);
    }

    /* Create the global scope dictionary. */
    py_globals_dict = PyDict_New();
    /* Add the builtings to the global scope. */
    PyDict_SetItemString(py_globals_dict, "__builtins__", PyEval_GetBuiltins());
    /* Add Atrinik module members to the global scope. */
    PyRun_String("from Atrinik import *", Py_file_input, py_globals_dict, NULL);

    py_tstate = PyGILState_GetThisThreadState();
    PyEval_ReleaseThread(py_tstate);
}

MODULEAPI void closePlugin(void)
{
    hooks->cache_remove_by_flags(CACHE_FLAG_GEVENT);
    PyGILState_Ensure();
    Py_Finalize();
}

/**
 * Sets face field.
 * @param ptr Pointer to ::New_Face structure.
 * @param face_id ID of the face to set.
 * @return 0 on success, -1 on failure.
 */
static int set_face_field(void *ptr, long face_id)
{
    if (face_id < 0 || face_id >= *hooks->nrofpixmaps) {
        PyErr_Format(PyExc_ValueError, "Illegal value for face field: %ld",
                face_id);
        return -1;
    }

    *(New_Face **) ptr = &(*hooks->new_faces)[face_id];
    return 0;
}

/**
 * Sets animation field.
 * @param ptr Pointer to ::uint16 structure member.
 * @param anim_id ID of the animation to set.
 * @return 0 on success, -1 on failure.
 */
static int set_animation_field(void *ptr, long anim_id)
{
    if (anim_id < 0 || anim_id >= *hooks->num_animations) {
        PyErr_Format(PyExc_ValueError, "Illegal value for animation field: %ld",
                anim_id);
        return -1;
    }

    *(uint16_t *) ptr = (uint16_t) anim_id;
    return 0;
}

/**
 * A generic field setter for all interfaces.
 * @param type Type of the field.
 * @param[out] field_ptr Field pointer.
 * @param value Value to set for the field pointer.
 * @return 0 on success, -1 on failure.
 */
int generic_field_setter(fields_struct *field, void *ptr, PyObject *value)
{
    void *field_ptr;

    if ((field->flags & FIELDFLAG_READONLY)) {
        INTRAISE("Trying to modify readonly field.");
    }

    field_ptr = (char *) ptr + field->offset;

    switch (field->type) {
    case FIELDTYPE_SHSTR:

        if (value == Py_None) {
            FREE_AND_CLEAR_HASH(*(shstr **) field_ptr);
        } else if (PyString_Check(value)) {
            FREE_AND_CLEAR_HASH(*(shstr **) field_ptr);
            FREE_AND_COPY_HASH(*(shstr **) field_ptr, PyString_AsString(value));
        } else {
            INTRAISE("Illegal value for shared string field.");
        }

        break;

    case FIELDTYPE_CSTR:

        if (value == Py_None || PyString_Check(value)) {
            if (*(char **) field_ptr != NULL) {
                efree(*(char **) field_ptr);
            }

            if (value == Py_None) {
                *(char **) field_ptr = NULL;
            } else {
                *(char **) field_ptr = estrdup(PyString_AsString(value));
            }
        } else {
            INTRAISE("Illegal value for C string field.");
        }

        break;

    case FIELDTYPE_CARY:

        if (value == Py_None) {
            ((char *) field_ptr)[0] = '\0';
        } else if (PyString_Check(value)) {
            memcpy(field_ptr, PyString_AsString(value), field->extra_data);
            ((char *) field_ptr)[field->extra_data] = '\0';
        } else {
            INTRAISE("Illegal value for C char array field.");
        }

        break;

    case FIELDTYPE_UINT8:

        if (PyInt_Check(value)) {
            long val = PyLong_AsLong(value);

            if (val < 0 || (unsigned long) val > UINT8_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for uint8 field.");
                return -1;
            }

            *(uint8_t *) field_ptr = (uint8_t) val;
        } else {
            INTRAISE("Illegal value for uint8 field.");
        }

        break;

    case FIELDTYPE_INT8:

        if (PyInt_Check(value)) {
            long val = PyLong_AsLong(value);

            if (val < INT8_MIN || val > INT8_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for sint8 field.");
                return -1;
            }

            *(int8_t *) field_ptr = (int8_t) val;
        } else {
            INTRAISE("Illegal value for sint8 field.");
        }

        break;

    case FIELDTYPE_UINT16:

        if (PyInt_Check(value)) {
            long val = PyLong_AsLong(value);

            if (val < 0 || (unsigned long) val > UINT16_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for uint16 field.");
                return -1;
            }

            *(uint16_t *) field_ptr = (uint16_t) val;
        } else {
            INTRAISE("Illegal value for uint16 field.");
        }

        break;

    case FIELDTYPE_INT16:

        if (PyInt_Check(value)) {
            long val = PyLong_AsLong(value);

            if (val < INT16_MIN || val > INT16_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for sint16 field.");
                return -1;
            }

            *(int16_t *) field_ptr = (int16_t) val;
        } else {
            INTRAISE("Illegal value for sint16 field.");
        }

        break;

    case FIELDTYPE_UINT32:

        if (PyInt_Check(value)) {
            long val = PyLong_AsLong(value);

            if (val < 0 || (unsigned long) val > UINT32_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for uint32 field.");
                return -1;
            }

            *(uint32_t *) field_ptr = (uint32_t) val;
        } else {
            INTRAISE("Illegal value for uint32 field.");
        }

        break;

    case FIELDTYPE_INT32:

        if (PyInt_Check(value)) {
            long val = PyLong_AsLong(value);

            if (val < INT32_MIN || val > INT32_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for sint32 field.");
                return -1;
            }

            *(int32_t *) field_ptr = (int32_t) val;
        } else {
            INTRAISE("Illegal value for sint32 field.");
        }

        break;

    case FIELDTYPE_UINT64:

        if (PyInt_Check(value)) {
            unsigned PY_LONG_LONG val = PyLong_AsUnsignedLongLong(value);

            if (PyErr_Occurred()) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for uint64 field.");
                return -1;
            }

            *(uint64_t *) field_ptr = (uint64_t) val;
        } else {
            INTRAISE("Illegal value for uint64 field.");
        }

        break;

    case FIELDTYPE_INT64:

        if (PyInt_Check(value)) {
            PY_LONG_LONG val = PyLong_AsLongLong(value);

            if (PyErr_Occurred()) {
                PyErr_SetString(PyExc_OverflowError,
                        "Invalid integer value for sint64 field.");
                return -1;
            }

            *(int64_t *) field_ptr = (int64_t) val;
        } else {
            INTRAISE("Illegal value for sint64 field.");
        }

        break;

    case FIELDTYPE_FLOAT:

        if (PyFloat_Check(value)) {
            *(float *) field_ptr = PyFloat_AsDouble(value) * 1.0;
        } else if (PyInt_Check(value)) {
            *(float *) field_ptr = PyLong_AsLong(value) * 1.0;
        } else {
            INTRAISE("Illegal value for float field.");
        }

        break;

    case FIELDTYPE_DOUBLE:

        if (PyFloat_Check(value)) {
            *(double *) field_ptr = PyFloat_AsDouble(value) * 1.0;
        } else if (PyInt_Check(value)) {
            *(double *) field_ptr = PyLong_AsLong(value) * 1.0;
        } else {
            INTRAISE("Illegal value for double field.");
        }

        break;

    case FIELDTYPE_OBJECT:

        if (value == Py_None) {
            *(object **) field_ptr = NULL;
        } else if (PyObject_TypeCheck(value, &Atrinik_ObjectType)) {
            OBJEXISTCHECK_INT((Atrinik_Object *) value);
            *(object **) field_ptr = (object *) ((Atrinik_Object *) value)->obj;
        } else {
            INTRAISE("Illegal value for object field.");
        }

        break;

    case FIELDTYPE_OBJECT2:
    case FIELDTYPE_OBJECT_ITERATOR:
        INTRAISE("Field type not implemented.");
        break;

    case FIELDTYPE_MAP:

        if (value == Py_None) {
            *(mapstruct **) field_ptr = NULL;
        } else if (PyObject_TypeCheck(value, &Atrinik_MapType)) {
            *(mapstruct **) field_ptr =
                    (mapstruct *) ((Atrinik_Map *) value)->map;
        } else {
            INTRAISE("Illegal value for map field.");
        }

        break;

    case FIELDTYPE_OBJECTREF:
    {
        void *field_ptr2 = (char *) ptr + field->extra_data;

        if (value == Py_None) {
            *(object **) field_ptr = NULL;
            *(tag_t *) field_ptr2 = 0;
        } else if (PyObject_TypeCheck(value, &Atrinik_ObjectType)) {
            object *tmp;

            OBJEXISTCHECK_INT((Atrinik_Object *) value);

            tmp = (object *) ((Atrinik_Object *) value)->obj;
            *(object **) field_ptr = tmp;
            *(tag_t *) field_ptr2 = tmp->count;
        } else {
            INTRAISE("Illegal value for object+reference field.");
        }

        break;
    }

    case FIELDTYPE_REGION:

        if (value == Py_None) {
            *(region_struct **) field_ptr = NULL;
        } else if (PyObject_TypeCheck(value, &Atrinik_RegionType)) {
            *(region_struct **) field_ptr =
                    (region_struct *) ((Atrinik_Region *) value)->region;
        } else {
            INTRAISE("Illegal value for region field.");
        }

        break;

    case FIELDTYPE_PARTY:

        if (value == Py_None) {
            *(party_struct **) field_ptr = NULL;
        } else if (PyObject_TypeCheck(value, &Atrinik_PartyType)) {
            *(party_struct **) field_ptr =
                    (party_struct *) ((Atrinik_Party *) value)->party;
        } else {
            INTRAISE("Illegal value for party field.");
        }

        break;

    case FIELDTYPE_ARCH:

        if (value == Py_None) {
            *(archetype_t **) field_ptr = NULL;
        } else if (PyObject_TypeCheck(value, &Atrinik_ArchetypeType)) {
            *(archetype_t **) field_ptr =
                    (archetype_t *) ((Atrinik_Archetype *) value)->at;
        } else if (PyString_Check(value)) {
            const char *archname;
            archetype_t *arch;

            archname = PyString_AsString(value);
            arch = hooks->arch_find(archname);

            if (!arch) {
                PyErr_Format(AtrinikError, "Could not find archetype '%s'.",
                        archname);
                return -1;
            } else {
                *(archetype_t **) field_ptr = arch;
            }
        } else {
            INTRAISE("Illegal value for archetype field.");
        }

        break;

    case FIELDTYPE_PLAYER:

        if (value == Py_None) {
            *(player **) field_ptr = NULL;
        } else if (PyObject_TypeCheck(value, &Atrinik_PlayerType)) {
            *(player **) field_ptr = (player *) ((Atrinik_Player *) value)->pl;
        } else {
            INTRAISE("Illegal value for player field.");
        }

        break;

    case FIELDTYPE_FACE:

        if (PyTuple_Check(value)) {
            if (PyTuple_GET_SIZE(value) != 2) {
                PyErr_Format(PyExc_ValueError,
                        "Tuple for face field must have exactly two values.");
                return -1;
            } else if (!PyInt_Check(PyTuple_GET_ITEM(value, 1))) {
                PyErr_SetString(PyExc_ValueError,
                        "Second value of tuple used for face field is not an "
                        "integer.");
                return -1;
            }

            return set_face_field(field_ptr,
                    PyLong_AsLong(PyTuple_GET_ITEM(value, 1)));
        } else if (PyInt_Check(value)) {
            return set_face_field(field_ptr, PyLong_AsLong(value));
        } else if (PyString_Check(value)) {
            return set_face_field(field_ptr,
                    hooks->find_face(PyString_AsString(value), 0));
        } else {
            INTRAISE("Illegal value for face field.");
        }

        break;

    case FIELDTYPE_ANIMATION:

        if (PyTuple_Check(value)) {
            if (PyTuple_GET_SIZE(value) != 2) {
                PyErr_Format(PyExc_ValueError,
                        "Tuple for animation field must have exactly two "
                        "values.");
                return -1;
            } else if (!PyInt_Check(PyTuple_GET_ITEM(value, 1))) {
                PyErr_SetString(PyExc_ValueError,
                        "Second value of tuple used for animation field is not "
                        "an integer.");
                return -1;
            }

            return set_animation_field(field_ptr,
                    PyLong_AsLong(PyTuple_GET_ITEM(value, 1)));
        } else if (PyInt_Check(value)) {
            return set_animation_field(field_ptr,  PyLong_AsLong(value));
        } else if (PyString_Check(value)) {
            return set_animation_field(field_ptr,
                    hooks->find_animation(PyString_AsString(value)));
        } else {
            INTRAISE("Illegal value for animation field.");
        }

        break;

    case FIELDTYPE_BOOLEAN:

        if (value == Py_True) {
            *(uint8_t *) field_ptr = 1;
        } else if (value == Py_False) {
            *(uint8_t *) field_ptr = 0;
        } else {
            INTRAISE("Illegal value for boolean field.");
        }

        break;

    case FIELDTYPE_CONNECTION:

        if (PyInt_Check(value)) {
            hooks->connection_object_add(ptr, ((object *) ptr)->map,
                    PyLong_AsLong(value));
        } else {
            INTRAISE("Illegal value for connection field.");
        }

        break;

    case FIELDTYPE_TREASURELIST:

        if (PyString_Check(value)) {
            *(treasurelist **) field_ptr =
                    hooks->find_treasurelist(PyString_AsString(value));
        } else {
            INTRAISE("Illegal value for treasure list field.");
        }

        break;

    default:
        break;
    }

    return 0;
}

/**
 * A generic field getter for all interfaces.
 * @param type Type of the field.
 * @param field_ptr Field pointer.
 * @param field_ptr2 Field pointer for extra data.
 * @return Python object containing value of field_ptr (and field_ptr2, if
 * applicable).
 */
PyObject *generic_field_getter(fields_struct *field, void *ptr)
{
    void *field_ptr;

    field_ptr = (char *) ptr + field->offset;

    switch (field->type) {
    case FIELDTYPE_SHSTR:
    case FIELDTYPE_CSTR:
    {
        char *str = *(char **) field_ptr;

        if (str == NULL) {
            Py_INCREF(Py_None);
            return Py_None;
        }

        return Py_BuildValue("s", str);
    }

    case FIELDTYPE_CARY:
        return Py_BuildValue("s", (char *) field_ptr);

    case FIELDTYPE_UINT8:
        return Py_BuildValue("B", *(uint8_t *) field_ptr);

    case FIELDTYPE_INT8:
        return Py_BuildValue("b", *(int8_t *) field_ptr);

    case FIELDTYPE_UINT16:
        return Py_BuildValue("H", *(uint16_t *) field_ptr);

    case FIELDTYPE_INT16:
        return Py_BuildValue("h", *(int16_t *) field_ptr);

    case FIELDTYPE_UINT32:
        return Py_BuildValue("I", *(uint32_t *) field_ptr);

    case FIELDTYPE_INT32:
        return Py_BuildValue("i", *(int32_t *) field_ptr);

    case FIELDTYPE_UINT64:
        return Py_BuildValue("K", *(uint64_t *) field_ptr);

    case FIELDTYPE_INT64:
        return Py_BuildValue("L", *(int64_t *) field_ptr);

    case FIELDTYPE_FLOAT:
        return Py_BuildValue("f", *(float *) field_ptr);

    case FIELDTYPE_DOUBLE:
        return Py_BuildValue("d", *(double *) field_ptr);

    case FIELDTYPE_MAP:
        return wrap_map(*(mapstruct **) field_ptr);

    case FIELDTYPE_OBJECT:
        return wrap_object(*(object **) field_ptr);

    case FIELDTYPE_OBJECT2:
        return wrap_object(field_ptr);

    case FIELDTYPE_OBJECT_ITERATOR:
        return wrap_object_iterator(*(object **) field_ptr);

    case FIELDTYPE_OBJECTREF:
    {
        object *obj = *(object **) field_ptr;
        tag_t tag = *(tag_t *) (void *) ((char *) ptr + field->extra_data);

        return wrap_object(OBJECT_VALID(obj, tag) ? obj : NULL);
    }

    case FIELDTYPE_REGION:
        return wrap_region(*(region_struct **) field_ptr);

    case FIELDTYPE_PARTY:
        return wrap_party(*(party_struct **) field_ptr);

    case FIELDTYPE_ARCH:
        return wrap_archetype(*(archetype_t **) field_ptr);

    case FIELDTYPE_PLAYER:
        return wrap_player(*(player **) field_ptr);

    case FIELDTYPE_FACE:
        return Py_BuildValue("(sH)", (*(New_Face **) field_ptr)->name,
                (*(New_Face **) field_ptr)->number);

    case FIELDTYPE_ANIMATION:
        return Py_BuildValue("(sH)",
                (&(*hooks->animations)[*(uint16_t *) field_ptr])->name,
                *(uint16_t *) field_ptr);

    case FIELDTYPE_BOOLEAN:
        return Py_BuildBoolean(*(uint8_t *) field_ptr);

    case FIELDTYPE_LIST:
        return wrap_attr_list(ptr, field->offset, field->extra_data);

    case FIELDTYPE_CONNECTION:
        return Py_BuildValue("i", hooks->connection_object_get_value(ptr));

    case FIELDTYPE_TREASURELIST:
    {
        treasurelist *tl = *(treasurelist **) field_ptr;
        if (tl == NULL) {
            Py_INCREF(Py_None);
            return Py_None;
        }

        return Py_BuildValue("s", tl->name);
    }

    default:
        break;
    }

    RAISE("Unknown field type.");
}

/**
 * Generic rich comparison function.
 * @param op
 * @param result
 * @return
 */
PyObject *generic_rich_compare(int op, int result)
{
    /* Based on how Python 3.0 (GPL compatible) implements it for internal
     * types. */
    switch (op) {
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
 * @return Integer value the function returned.
 */
int python_call_int(PyObject *callable, PyObject *arglist)
{
    /* Call the Python function. */
    PyObject *result = PyEval_CallObject(callable, arglist);

    int retval = 0;
    /* Check the result. */
    if (result != NULL && PyInt_Check(result)) {
        retval = PyInt_AsLong(result);
    }

    Py_XDECREF(result);
    Py_DECREF(arglist);

    return retval;
}
