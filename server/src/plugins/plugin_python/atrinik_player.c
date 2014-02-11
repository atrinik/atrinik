/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Atrinik Python plugin player related code. */

#include <plugin_python.h>

/**
 * Player fields. */
/* @cparser
 * @page plugin_python_player_fields Python player fields
 * <h2>Python player fields</h2>
 * List of the player fields and their meaning. */
static fields_struct fields[] =
{
    {"next", FIELDTYPE_PLAYER, offsetof(player, next), FIELDFLAG_READONLY, 0},
    {"prev", FIELDTYPE_PLAYER, offsetof(player, prev), FIELDFLAG_READONLY, 0},

    {"party", FIELDTYPE_PARTY, offsetof(player, party), FIELDFLAG_READONLY, 0},
    /* Shall not be modified in any way. Instead, one should use @ref
     * Atrinik_Player_Fix "player.Fix()",
     * which will set class_ob to the last @ref CLASS object it finds in
     * player's inventory. In some cases, this is done automatically after
     * a script has finished executing (say events do this, for example). */
    {"class_ob", FIELDTYPE_OBJECT, offsetof(player, class_ob), FIELDFLAG_READONLY, 0},
    {"savebed_map", FIELDTYPE_CARY, offsetof(player, savebed_map), 0, sizeof(((player *) NULL)->savebed_map)},
    {"bed_x", FIELDTYPE_SINT16, offsetof(player, bed_x), 0, 0},
    {"bed_y", FIELDTYPE_SINT16, offsetof(player, bed_y), 0, 0},
    {"ob", FIELDTYPE_OBJECT, offsetof(player, ob), FIELDFLAG_READONLY, 0},
    {"quest_container", FIELDTYPE_OBJECT, offsetof(player, quest_container), FIELDFLAG_READONLY, 0},
    {"target_object", FIELDTYPE_OBJECTREF, offsetof(player, target_object), 0, offsetof(player, target_object_count)},
    {"no_chat", FIELDTYPE_BOOLEAN, offsetof(player, no_chat), 0, 0},
    {"tcl", FIELDTYPE_BOOLEAN, offsetof(player, tcl), 0, 0},
    {"tgm", FIELDTYPE_BOOLEAN, offsetof(player, tgm), 0, 0},
    {"tli", FIELDTYPE_BOOLEAN, offsetof(player, tli), 0, 0},
    {"tls", FIELDTYPE_BOOLEAN, offsetof(player, tls), 0, 0},
    {"tsi", FIELDTYPE_BOOLEAN, offsetof(player, tsi), 0, 0},
    {"cmd_permissions", FIELDTYPE_LIST, offsetof(player, cmd_permissions), 0, FIELDTYPE_CMD_PERMISSIONS},
    {"factions", FIELDTYPE_LIST, offsetof(player, faction_ids), 0, FIELDTYPE_FACTIONS},
    {"fame", FIELDTYPE_SINT64, offsetof(player, fame), 0, FIELDTYPE_FACTIONS},
    {"region_maps", FIELDTYPE_LIST, offsetof(player, region_maps), 0, FIELDTYPE_REGION_MAPS},
    {"container", FIELDTYPE_OBJECT, offsetof(player, container), FIELDFLAG_READONLY, 0},

    {"s_ext_title_flag", FIELDTYPE_BOOLEAN, offsetof(player, socket.ext_title_flag), 0, 0},
    {"s_host", FIELDTYPE_CSTR, offsetof(player, socket.host), FIELDFLAG_READONLY, 0},
    {"s_socket_version", FIELDTYPE_UINT32, offsetof(player, socket.socket_version), FIELDFLAG_READONLY, 0}
};
/* @endcparser */

/**
 * @defgroup plugin_python_player_functions Python player functions
 * Functions that can only be used on players (not the objects they are
 * controlling).
 *
 * To access object's player controller, you can use something like:
 *
 * @code
 * activator = WhoIsActivator()
 * player = activator.Controller()
 * @endcode
 *
 * In the above example, player points to the player structure (which Python
 * is wrapping) that is controlling the object 'activator'. In this way, you
 * can,
 * for example, use something like this to get player's save bed, among other
 * things:
 *
 * @code
 * print(WhoIsActivator().Controller().savebed_map)
 * @endcode
 *@{*/

/**
 * <h1>player.GetEquipment(int slot)</h1>
 * Get a player's current equipment for a given slot.
 * @param slot One of @ref PLAYER_EQUIP_xxx constants.
 * @throws ValueError if 'slot' is lower than 0 or higher than @ref
 * PLAYER_EQUIP_MAX.
 * @return The equipment for the given slot, can be None. */
static PyObject *Atrinik_Player_GetEquipment(Atrinik_Player *pl, PyObject *args)
{
    int slot;

    if (!PyArg_ParseTuple(args, "i", &slot)) {
        return NULL;
    }

    if (slot < 0 || slot >= PLAYER_EQUIP_MAX) {
        PyErr_SetString(PyExc_ValueError, "Invalid slot number.");
        return NULL;
    }

    return wrap_object(pl->pl->equipment[slot]);
}

/**
 * <h1>player.CanCarry(object|int what)</h1>
 * Check whether the player can carry the object 'what', taking weight limit
 * into consideration.
 * @param what Object that player wants to get. This can be the exact weight
 * to check instead of calculating the object's weight.
 * @throws ValueError if 'what' is neither an object nor an integer.
 * @return True if the player can carry the object, False otherwise. */
static PyObject *Atrinik_Player_CanCarry(Atrinik_Player *pl, PyObject *what)
{
    uint32 weight;

    if (PyObject_TypeCheck(what, &Atrinik_ObjectType)) {
        OBJEXISTCHECK((Atrinik_Object *) what);
        weight = WEIGHT_NROF(((Atrinik_Object *) what)->obj, ((Atrinik_Object *) what)->obj->nrof);
    }
    else if (PyInt_Check(what)) {
        weight = PyInt_AsLong(what);
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Invalid value for 'what' parameter.");
        return NULL;
    }

    Py_ReturnBoolean(hooks->player_can_carry(pl->pl->ob, weight));
}

/**
 * <h1>player.AddExp(int|string skill, int exp, int [exact = False], int [level = True])</h1>
 * Add (or subtract) experience.
 * @param skill ID or name of the skill to receive/lose exp in.
 * @param exp How much exp to gain/lose. If 'level' is true, this is the number
 * of levels to gain/lose in the specified skill.
 * @param exact If True, the given exp will not be capped.
 * @param level If True, will calculate exact experience needed for next (or
 * previous) level. */
static PyObject *Atrinik_Player_AddExp(Atrinik_Player *pl, PyObject *args)
{
    PyObject *skill;
    uint32 skill_nr;
    sint64 exp_gain;
    int exact = 0, level = 0;

    if (!PyArg_ParseTuple(args, "OL|ii", &skill, &exp_gain, &exact, &level)) {
        return NULL;
    }

    if (PyInt_Check(skill)) {
        skill_nr = PyInt_AsLong(skill);

        if (skill_nr >= NROFSKILLS) {
            PyErr_Format(PyExc_ValueError, "Skill ID '%d' is invalid; 0-%d should be used.", skill_nr, NROFSKILLS - 1);
            return NULL;
        }
    }
    else if (PyString_Check(skill)) {
        const char *skill_name;

        skill_name = PyString_AsString(skill);

        for (skill_nr = 0; skill_nr < NROFSKILLS; skill_nr++) {
            if (strcmp(hooks->skills[skill_nr].name, skill_name) == 0) {
                break;
            }
        }

        if (skill_nr == NROFSKILLS) {
            PyErr_Format(PyExc_ValueError, "Skill '%s' does not exist.", skill_name);
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Invalid object type for 'skill' parameter.");
        return NULL;
    }

    if (pl->pl->skill_ptr[skill_nr] == NULL) {
        PyErr_Format(AtrinikError, "Player %s does not have the skill '%s'.", pl->pl->ob->name, hooks->skills[skill_nr].name);
        return NULL;
    }

    if (level != 0) {
        level = MAX(1, MIN(MAXLEVEL, pl->pl->skill_ptr[skill_nr]->level + exp_gain));
        exp_gain = hooks->level_exp(level, 1.0) - pl->pl->skill_ptr[skill_nr]->stats.exp;
    }

    hooks->add_exp(pl->pl->ob, exp_gain, skill_nr, exact);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>player.BankDeposit(string text)</h1>
 * Deposit money to bank.
 * @param text How much money to deposit, in string representation.
 * @return Tuple containing the status code (one of @ref BANK_xxx) and amount
 * of money deposited as integer. */
static PyObject *Atrinik_Player_BankDeposit(Atrinik_Player *pl, PyObject *args)
{
    const char *text;
    int ret;
    sint64 value;

    if (!PyArg_ParseTuple(args, "s", &text)) {
        return NULL;
    }

    ret = hooks->bank_deposit(pl->pl->ob, text, &value);

    return Py_BuildValue("(iL)", ret, value);
}

/**
 * <h1>player.BankWithdraw(string text)</h1>
 * Withdraw money from bank.
 * @param text How much money to withdraw, in string representation.
 * @return Tuple containing the status code (one of @ref BANK_xxx) and amount
 * of money withdrawn as integer. */
static PyObject *Atrinik_Player_BankWithdraw(Atrinik_Player *pl, PyObject *args)
{
    const char *text;
    int ret;
    sint64 value;

    if (!PyArg_ParseTuple(args, "s", &text)) {
        return NULL;
    }

    ret = hooks->bank_withdraw(pl->pl->ob, text, &value);

    return Py_BuildValue("(iL)", ret, value);
}

/**
 * <h1>player.BankBalance()</h1>
 * Figure out how much money player has in bank.
 * @return Integer value of the money in bank. */
static PyObject *Atrinik_Player_BankBalance(Atrinik_Player *pl, PyObject *args)
{
    (void) args;

    return Py_BuildValue("L", hooks->bank_get_balance(pl->pl->ob));
}

/**
 * <h1>player.SwapApartments(string oldmap, string newmap, int x, int y)</h1>
 * Swaps oldmap apartment with newmap one.
 *
 * Copies old items from oldmap to newmap at x, y and saves the map.
 * @param oldmap The old apartment map.
 * @param oldmap The new apartment map.
 * @param x X position to copy the items to.
 * @param y Y position to copy the items to.
 * @return True on success, False on failure. */
static PyObject *Atrinik_Player_SwapApartments(Atrinik_Player *pl, PyObject *args)
{
    const char *mapold, *mapnew;
    int x, y;

    if (!PyArg_ParseTuple(args, "ssii", &mapold, &mapnew, &x, &y)) {
        return NULL;
    }

    Py_ReturnBoolean(hooks->swap_apartments(mapold, mapnew, x, y, pl->pl->ob));
}

/**
 * <h1>player.ExecuteCommand(string command)</h1>
 * Make player execute a command.
 * @param command Command to execute.
 * @throws AtrinikError if player is not in a state to execute commands.
 * @return Return value of the command. */
static PyObject *Atrinik_Player_ExecuteCommand(Atrinik_Player *pl, PyObject *args)
{
    const char *command;
    char *cp;

    if (!PyArg_ParseTuple(args, "s", &command)) {
        return NULL;
    }

    if (pl->pl->socket.state != ST_PLAYING) {
        PyErr_SetString(AtrinikError, "Player is not in a state to execute commands.");
        return NULL;
    }

    /* Make a copy of the command, since execute_newserver_command
     * modifies the string. */
    cp = strdup(command);
    hooks->commands_handle(pl->pl->ob, cp);
    free(cp);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>player.FindMarkedObject()</h1>
 * Find marked object in player's inventory.
 * @return The marked object, or None if no object is marked. */
static PyObject *Atrinik_Player_FindMarkedObject(Atrinik_Player *pl, PyObject *args)
{
    (void) args;

    return wrap_object(hooks->find_marked_object(pl->pl->ob));
}

/**
 * <h1>player.Sound(string filename, int [type = @ref CMD_SOUND_EFFECT], int [x
 * = 0], int [y = 0], int [loop = 0], int [volume = 0])</h1>
 * Play a sound to the specified player.
 * @param filename Sound file to play.
 * @param type Sound type being played, one of @ref CMD_SOUND_xxx.
 * @param x X position where the sound is playing from. Can be 0.
 * @param y Y position where the sound is playing from. Can be 0.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
static PyObject *Atrinik_Player_Sound(Atrinik_Player *pl, PyObject *args, PyObject *keywds)
{
    static char *kwlist[] = {"filename", "type", "x", "y", "loop", "volume", NULL};
    const char *filename;
    int type = CMD_SOUND_EFFECT, x = 0, y = 0, loop = 0, volume = 0;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|iiiii", kwlist, &filename, &type, &x, &y, &loop, &volume)) {
        return NULL;
    }

    hooks->play_sound_player_only(pl->pl, type, filename, x, y, loop, volume);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>player.Examine(object obj, bool [ret = False])</h1>
 * Makes player examine the specified object.
 * @param obj Object to examine. */
static PyObject *Atrinik_Player_Examine(Atrinik_Player *pl, PyObject *args)
{
    Atrinik_Object *obj;
    int ret = 0;
    StringBuffer *sb_capture = NULL;

    if (!PyArg_ParseTuple(args, "O!|i", &Atrinik_ObjectType, &obj, &ret)) {
        return NULL;
    }

    if (ret) {
        sb_capture = hooks->stringbuffer_new();
    }

    hooks->examine(pl->pl->ob, obj->obj, sb_capture);

    if (ret) {
        char *cp;
        PyObject *retval;

        cp = hooks->stringbuffer_finish(sb_capture);
        retval = Py_BuildValue("s", cp);
        free(cp);

        return retval;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>player.SendPacket(int command, string format, ...)</h1>
 * Constructs and sends a packet to the player's client.
 * @param command The command ID.
 * @param format Format specifier. For example, "Bs" would imply uint8 +
 * string data, and the format specifier would need to be followed by an
 * integer that is within uint8 data range and a string. */
static PyObject *Atrinik_Player_SendPacket(Atrinik_Player *pl, PyObject *args)
{
    long cmd;
    char *format;
    packet_struct *packet;
    size_t i;
    PyObject *value;

    /* Must have at least 3 arguments. */
    if (PyTuple_Size(args) < 3) {
        PyErr_SetString(PyExc_ValueError, "Insufficient number of arguments.");
        return NULL;
    }

    /* The first argument must be an integer. */
    if (!PyInt_Check(PyTuple_GET_ITEM(args, 0))) {
        PyErr_SetString(PyExc_OverflowError, "Illegal value for 'cmd' function argument.");
        return NULL;
    }

    cmd = PyLong_AsLong(PyTuple_GET_ITEM(args, 0));

    /* It also must be uint8. */
    if (cmd < 0 || (unsigned long) cmd > UINT8_MAX) {
        PyErr_SetString(PyExc_OverflowError, "Invalid integer value for 'cmd' function argument.");
        return NULL;
    }

    packet = hooks->packet_new(cmd, 256, 512);

    /* Get the format specifier. */
    format = PyString_AsString(PyTuple_GET_ITEM(args, 1));

    for (i = 0; format[i] != '\0'; i++) {
        value = PyTuple_GetItem(args, 2 + i);

        if (!value) {
            PyErr_SetString(PyExc_ValueError, "Insufficient number of arguments.");
            return NULL;
        }

        if (format[i] == 'b') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);

                if (val < SINT8_MIN || val > SINT8_MAX) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_sint8(packet, val);
                continue;
            }
        }
        else if (format[i] == 'B') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);

                if (val < 0 || (unsigned long) val > UINT8_MAX) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_uint8(packet, val);
                continue;
            }
        }
        else if (format[i] == 'h') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);

                if (val < SINT16_MIN || val > SINT16_MAX) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_sint16(packet, val);
                continue;
            }
        }
        else if (format[i] == 'H') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);

                if (val < 0 || (unsigned long) val > UINT16_MAX) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_uint16(packet, val);
                continue;
            }
        }
        else if (format[i] == 'i') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);

                if (val < SINT32_MIN || val > SINT32_MAX) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_sint32(packet, val);
                continue;
            }
        }
        else if (format[i] == 'I') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);

                if (val < 0 || (unsigned long) val > UINT32_MAX) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_uint32(packet, val);
                continue;
            }
        }
        else if (format[i] == 'l') {
            if (PyInt_Check(value)) {
                PY_LONG_LONG val = PyLong_AsLongLong(value);

                if (PyErr_Occurred()) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_sint64(packet, val);
                continue;
            }
        }
        else if (format[i] == 'L') {
            if (PyInt_Check(value)) {
                unsigned PY_LONG_LONG val = PyLong_AsUnsignedLongLong(value);

                if (PyErr_Occurred()) {
                    PyErr_Format(PyExc_OverflowError, "Invalid integer value for '%c' format specifier.", format[i]);
                    return NULL;
                }

                hooks->packet_append_uint64(packet, val);
                continue;
            }
        }
        else if (format[i] == 's') {
            if (PyString_Check(value)) {
                hooks->packet_append_string_terminated(packet, PyString_AsString(value));
                continue;
            }
        }
        else if (format[i] == 'x') {
            if (PyBytes_Check(value)) {
                hooks->packet_append_data_len(packet, (uint8 *) PyBytes_AsString(value), PyBytes_Size(value));
                continue;
            }
        }
        else {
            PyErr_Format(PyExc_TypeError, "Illegal format specifier '%c'.", format[i]);
            return NULL;
        }

        PyErr_Format(PyExc_TypeError, "Illegal value for '%c' format specifier.", format[i]);
        return NULL;
    }

    hooks->socket_send_packet(&pl->pl->socket, packet);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>player.DrawInfo(string message, string [color = @ref COLOR_ORANGE], int
 * [type = @ref CHAT_TYPE_GAME], bool [global = False], string [name =
 * None])</h1>
 * Sends a message to the specified player.
 * @param message The message to send.
 * @param color Color to use for the message. Can be one of @ref COLOR_xxx
 * or an HTML color notation.
 * @param type One of @ref CHAT_TYPE_xxx.
 * @param global If True, the message will be broadcasted to all players.
 * @param name Player name that is the source of this message, if applicable.
 * If None and 'type' is not @ref CHAT_TYPE_GAME, player.ob.name will be used.
 * */
static PyObject *Atrinik_Player_DrawInfo(Atrinik_Player *pl, PyObject *args, PyObject *keywds)
{
    static char *kwlist[] = {"message", "color", "type", "global", "name", NULL};
    const char *message, *color, *name;
    uint8 type, global;

    color = COLOR_ORANGE;
    type = CHAT_TYPE_GAME;
    global = 0;
    name = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|sbbz", kwlist, &message, &color, &type, &global, &name)) {
        return NULL;
    }

    if (!name && type != CHAT_TYPE_GAME) {
        name = pl->pl->ob->name;
    }

    hooks->draw_info_type(type, name, color, global ? NULL : pl->pl->ob, message);

    Py_INCREF(Py_None);
    return Py_None;
}

/*@}*/

/** Available Python methods for the AtrinikPlayer type. */
static PyMethodDef methods[] =
{
    {"GetEquipment", (PyCFunction) Atrinik_Player_GetEquipment, METH_VARARGS, 0},
    {"CanCarry", (PyCFunction) Atrinik_Player_CanCarry, METH_O, 0},
    {"AddExp", (PyCFunction) Atrinik_Player_AddExp, METH_VARARGS, 0},
    {"BankDeposit", (PyCFunction) Atrinik_Player_BankDeposit, METH_VARARGS, 0},
    {"BankWithdraw", (PyCFunction) Atrinik_Player_BankWithdraw, METH_VARARGS, 0},
    {"BankBalance", (PyCFunction) Atrinik_Player_BankBalance, METH_NOARGS, 0},
    {"SwapApartments", (PyCFunction) Atrinik_Player_SwapApartments, METH_VARARGS, 0},
    {"ExecuteCommand", (PyCFunction) Atrinik_Player_ExecuteCommand, METH_VARARGS, 0},
    {"FindMarkedObject", (PyCFunction) Atrinik_Player_FindMarkedObject, METH_NOARGS, 0},
    {"Sound", (PyCFunction) Atrinik_Player_Sound, METH_VARARGS | METH_KEYWORDS, 0},
    {"Examine", (PyCFunction) Atrinik_Player_Examine, METH_VARARGS, 0},
    {"SendPacket", (PyCFunction) Atrinik_Player_SendPacket, METH_VARARGS, 0},
    {"DrawInfo", (PyCFunction) Atrinik_Player_DrawInfo, METH_VARARGS | METH_KEYWORDS, 0},
    {NULL, NULL, 0, 0}
};

/**
 * Get player's attribute.
 * @param pl Python player wrapper.
 * @param context Void pointer to the field ID.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *get_attribute(Atrinik_Player *pl, void *context)
{
    return generic_field_getter((fields_struct *) context, pl->pl);
}

/**
 * Set attribute of a player.
 * @param whoptr Python player wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure. */
static int set_attribute(Atrinik_Player *pl, PyObject *value, void *context)
{
    fields_struct *field = (fields_struct *) context;

    if (generic_field_setter(field, pl->pl, value) == -1) {
        return -1;
    }

    if (field->offset == offsetof(player, target_object)) {
        hooks->send_target_command(pl->pl);
    }

    return 0;
}

/**
 * Create a new player wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Player_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Atrinik_Player *pl;

    (void) args;
    (void) kwds;

    pl = (Atrinik_Player *) type->tp_alloc(type, 0);

    if (pl) {
        pl->pl = NULL;
    }

    return (PyObject *) pl;
}

/**
 * Free a player wrapper.
 * @param pl The wrapper to free. */
static void Atrinik_Player_dealloc(Atrinik_Player *pl)
{
    pl->pl = NULL;
#ifndef IS_PY_LEGACY
    Py_TYPE(pl)->tp_free((PyObject *) pl);
#else
    pl->ob_type->tp_free((PyObject *) pl);
#endif
}

/**
 * Return a string representation of a player.
 * @param pl The player.
 * @return Python object containing the name of the player. */
static PyObject *Atrinik_Player_str(Atrinik_Player *pl)
{
    return Py_BuildValue("s", pl->pl->ob->name);
}

static int Atrinik_Player_InternalCompare(Atrinik_Player *left, Atrinik_Player *right)
{
    return (left->pl < right->pl ? -1 : (left->pl == right->pl ? 0 : 1));
}

static PyObject *Atrinik_Player_RichCompare(Atrinik_Player *left, Atrinik_Player *right, int op)
{
    if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_PlayerType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_PlayerType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, Atrinik_Player_InternalCompare(left, right));
}

/**
 * This is filled in when we initialize our player type. */
static PyGetSetDef getseters[NUM_FIELDS + 1];

/** Our actual Python PlayerType. */
PyTypeObject Atrinik_PlayerType =
{
#ifdef IS_PY3K
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,
#endif
    "Atrinik.Player",
    sizeof(Atrinik_Player),
    0,
    (destructor) Atrinik_Player_dealloc,
    NULL, NULL, NULL,
#ifdef IS_PY3K
    NULL,
#else
    (cmpfunc) Atrinik_Player_InternalCompare,
#endif
    0, 0, 0, 0, 0, 0,
    (reprfunc) Atrinik_Player_str,
    0, 0, 0,
    Py_TPFLAGS_DEFAULT,
    "Atrinik players",
    NULL, NULL,
    (richcmpfunc) Atrinik_Player_RichCompare,
    0, 0, 0,
    methods,
    0,
    getseters,
    0, 0, 0, 0, 0, 0, 0,
    Atrinik_Player_new,
    0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
    , 0
#endif
};

/**
 * Initialize the player wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Player_init(PyObject *module)
{
    size_t i;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) get_attribute;
        def->set = (setter) set_attribute;
        def->doc = NULL;
        def->closure = (void *) &fields[i];
    }

    getseters[i].name = NULL;

    Atrinik_PlayerType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_PlayerType) < 0) {
        return 0;
    }

    Py_INCREF(&Atrinik_PlayerType);
    PyModule_AddObject(module, "Player", (PyObject *) &Atrinik_PlayerType);

    return 1;
}

/**
 * Utility method to wrap a player.
 * @param what Player to wrap.
 * @return Python object wrapping the real player. */
PyObject *wrap_player(player *pl)
{
    Atrinik_Player *wrapper;

    /* Return None if no player was to be wrapped. */
    if (!pl) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    wrapper = PyObject_NEW(Atrinik_Player, &Atrinik_PlayerType);

    if (wrapper) {
        wrapper->pl = pl;
    }

    return (PyObject *) wrapper;
}
