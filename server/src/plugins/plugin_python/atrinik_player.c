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
 * Atrinik Python plugin player related code.
 *
 * @author Alex Tokar
 */

#include <plugin_python.h>
#include <packet.h>
#include <faction.h>

/**
 * Player fields.
 */
static fields_struct fields[] = {
    {"next", FIELDTYPE_PLAYER, offsetof(player, next), FIELDFLAG_READONLY, 0,
            "Next player in a list.; Atrinik.Player.Player or None (readonly)"},
    {"prev", FIELDTYPE_PLAYER, offsetof(player, prev), FIELDFLAG_READONLY, 0,
            "Previous player in a list.; Atrinik.Player.Player or None "
            "(readonly)"},

    {"party", FIELDTYPE_PARTY, offsetof(player, party), FIELDFLAG_READONLY, 0,
            "Party the player is a member of.; Atrinik.Party.Party or None "
            "(readonly)"},
    {"class_ob", FIELDTYPE_OBJECT, offsetof(player, class_ob),
            FIELDFLAG_READONLY, 0, "Class object of the player. Cannot be set, "
            "as it's always set to the last CLASS type object that is found in "
            "the player's inventory after calling "
            ":meth:`Atrinik.Object.Object.Update`.; Atrinik.Object.Object or "
            "None (readonly)"},
    {"savebed_map", FIELDTYPE_CARY, offsetof(player, savebed_map), 0,
            sizeof(((player *) NULL)->savebed_map),
            "Path to the player's savebed map.; str"},
    {"bed_x", FIELDTYPE_INT16, offsetof(player, bed_x), 0, 0,
            "X coordinate of the player's savebed.; int"},
    {"bed_y", FIELDTYPE_INT16, offsetof(player, bed_y), 0, 0,
            "Y coordinate of the player's savebed.; int"},
    {"ob", FIELDTYPE_OBJECT, offsetof(player, ob), FIELDFLAG_READONLY, 0,
            "The :class:`Atrinik.Object.Object` representing the player.; "
            "Atrinik.Object.Object (readonly)"},
    {"quest_container", FIELDTYPE_OBJECT, offsetof(player, quest_container),
            FIELDFLAG_READONLY, 0, "Player's quest container.; "
            "Atrinik.Object.Object (readonly)"},
    {"target_object", FIELDTYPE_OBJECTREF, offsetof(player, target_object), 0,
            offsetof(player, target_object_count), "Currently targeted "
            "NPC/monster.;Atrinik.Object.Object or None"},
    {"no_chat", FIELDTYPE_BOOLEAN, offsetof(player, no_chat), 0, 0,
            "If true, the player is not allowed to chat.; bool"},
    {"tcl", FIELDTYPE_BOOLEAN, offsetof(player, tcl), 0, 0,
            "If true, the player ignores collision with terrain.; bool"},
    {"tgm", FIELDTYPE_BOOLEAN, offsetof(player, tgm), 0, 0,
            "If true, the player is in god-mode and cannot die or take damage "
            "from any source.; bool"},
    {"tli", FIELDTYPE_BOOLEAN, offsetof(player, tli), 0, 0,
            "If true, the player has lighting disabled.; bool"},
    {"tls", FIELDTYPE_BOOLEAN, offsetof(player, tls), 0, 0,
            "If true, the player ignores line of sight.; bool"},
    {"tsi", FIELDTYPE_BOOLEAN, offsetof(player, tsi), 0, 0,
            "If true, the player can see system-invisible objects.; bool"},
    {"cmd_permissions", FIELDTYPE_LIST, offsetof(player, cmd_permissions), 0,
            FIELDTYPE_CMD_PERMISSIONS, "Player's command permissions.; "
            "Atrinik.AttrList.AttrList"},
    {"factions", FIELDTYPE_LIST, offsetof(player, factions), 0,
            FIELDTYPE_FACTIONS, "Player's factions.; "
            "Atrinik.AttrList.AttrList"},
    {"fame", FIELDTYPE_INT64, offsetof(player, fame), 0, 0,
            "Fame (or infamy) of the player.; int"},
    {"container", FIELDTYPE_OBJECT, offsetof(player, container),
            FIELDFLAG_READONLY, 0, "Container the player has open.; "
            "Atrinik.Object.Object or None (readonly)"},
    {"combat", FIELDTYPE_BOOLEAN, offsetof(player, combat),
            0, 0, "Whether the player is ready to engage in combat and should "
            "swing their weapon at targeted enemies.; bool"},
    {"combat_force", FIELDTYPE_BOOLEAN, offsetof(player, combat_force),
            0, 0, "Whether the player should swing their weapon at their "
            "target, be it friend or foe.; bool"},

    {"s_ext_title_flag", FIELDTYPE_BOOLEAN,
            offsetof(player, socket.ext_title_flag), 0, 0,
            "If True, will force updating the player's map name.; bool"},
    {"s_socket_version", FIELDTYPE_UINT32,
            offsetof(player, socket.socket_version), FIELDFLAG_READONLY, 0,
            "Socket version of the player's client.; int (readonly)"},
    {"s_packets", FIELDTYPE_LIST,
            offsetof(player, socket.packets), 0, FIELDTYPE_PACKETS,
            "Packets that have been enqueued to the player's client.; "
            "Atrinik.AttrList.AttrList"}
};

/** Documentation for Atrinik_Player_GetEquipment(). */
static const char doc_Atrinik_Player_GetEquipment[] =
".. method:: GetEquipment(slot).\n\n"
"Get player's current equipment object for a given slot.\n\n"
":param slot: The slot number; one of PLAYER_EQUIP_xxx, eg, :attr:"
"`~Atrinik.PLAYER_EQUIP_LEGGINGS`.\n"
":type slot: int\n"
":returns: The equipment object for the given slot, None if there's no object "
"in the slot.\n"
":rtype: :class:`Atrinik.Object.Object` or None\n"
":throws ValueError: If the *slot* number is invalid.";

/**
 * Implements Atrinik.Player.GetEquipment() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_GetEquipment(Atrinik_Player *self,
        PyObject *args)
{
    int slot;

    if (!PyArg_ParseTuple(args, "i", &slot)) {
        return NULL;
    }

    if (slot < 0 || slot >= PLAYER_EQUIP_MAX) {
        PyErr_SetString(PyExc_ValueError, "Invalid slot number.");
        return NULL;
    }

    return wrap_object(self->pl->equipment[slot]);
}

/** Documentation for Atrinik_Player_CanCarry(). */
static const char doc_Atrinik_Player_CanCarry[] =
".. method:: CanCarry(what).\n\n"
"Check whether the player can carry *what*, taking weight limit into "
"consideration.\n\n"
":param what: Object that player wants to get. This can be the exact weight to "
"check instead of calculating the object's weight.\n"
":type what: :class:`Atrinik.Object.Object` or int\n"
":returns: Whether the player can carry the *what*.\n"
":rtype: bool";

/**
 * Implements Atrinik.Player.Player.CanCarry() Python method.
 * @copydoc PyMethod_OBJECT
 */
static PyObject *Atrinik_Player_CanCarry(Atrinik_Player *self, PyObject *what)
{
    uint32_t weight;

    if (PyObject_TypeCheck(what, &Atrinik_ObjectType)) {
        OBJEXISTCHECK((Atrinik_Object *) what);
        weight = WEIGHT_NROF(((Atrinik_Object *) what)->obj,
                ((Atrinik_Object *) what)->obj->nrof);
    } else if (PyInt_Check(what)) {
        weight = PyInt_AsLong(what);
    } else {
        PyErr_SetString(PyExc_TypeError,
                "Invalid object type for 'what' parameter.");
        return NULL;
    }

    return Py_BuildBoolean(hooks->player_can_carry(self->pl->ob, weight));
}

/** Documentation for Atrinik_Player_AddExp(). */
static const char doc_Atrinik_Player_AddExp[] =
".. method:: AddExp(skill, exp, exact=False, level=False).\n\n"
"Add (or subtract) experience.\n\n"
":param skill: ID or name of the skill to receive/lose exp in.\n"
":type skill: int or str\n"
":param exp: How much exp to gain/lose. If *level* is true, this is the number "
"of levels to gain/lose in the specified skill.\n"
":type exp: int\n"
":param exact: If True, the given exp will not be capped.\n"
":type exact: bool\n"
":param level: If True, will calculate exact experience needed for next (or "
"previous) level.\n"
":type level: bool\n"
":raises ValueError: If the skill ID/name is invalid.\n"
":raises Atrinik.AtrinikError: If the player doesn't have the specified skill.";

/**
 * Implements Atrinik.Player.Player.AddExp() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_AddExp(Atrinik_Player *self, PyObject *args)
{
    PyObject *skill;
    uint32_t skill_nr;
    int64_t exp_gain;
    int exact = 0, level = 0;

    if (!PyArg_ParseTuple(args, "OL|ii", &skill, &exp_gain, &exact, &level)) {
        return NULL;
    }

    if (PyInt_Check(skill)) {
        skill_nr = PyInt_AsLong(skill);

        if (skill_nr >= NROFSKILLS) {
            PyErr_Format(PyExc_ValueError,
                    "Skill ID '%d' is invalid; 0-%d should be used.", skill_nr,
                    NROFSKILLS - 1);
            return NULL;
        }
    } else if (PyString_Check(skill)) {
        const char *skill_name = PyString_AsString(skill);

        for (skill_nr = 0; skill_nr < NROFSKILLS; skill_nr++) {
            if (strcmp(hooks->skills[skill_nr].name, skill_name) == 0) {
                break;
            }
        }

        if (skill_nr == NROFSKILLS) {
            PyErr_Format(PyExc_ValueError, "Skill '%s' does not exist.",
                    skill_name);
            return NULL;
        }
    } else {
        PyErr_SetString(PyExc_TypeError,
                "Invalid object type for 'skill' parameter.");
        return NULL;
    }

    if (self->pl->skill_ptr[skill_nr] == NULL) {
        PyErr_Format(AtrinikError, "Player %s does not have the skill '%s'.",
                self->pl->ob->name, hooks->skills[skill_nr].name);
        return NULL;
    }

    if (level) {
        int level_reach = self->pl->skill_ptr[skill_nr]->level + exp_gain;
        level_reach = MAX(1, MIN(MAXLEVEL, level_reach));
        exp_gain = hooks->level_exp(level_reach, 1.0) -
                self->pl->skill_ptr[skill_nr]->stats.exp;
    }

    hooks->add_exp(self->pl->ob, exp_gain, skill_nr, exact);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_BankDeposit(). */
static const char doc_Atrinik_Player_BankDeposit[] =
".. method:: BankDeposit(text).\n\n"
"Deposit money to bank.\n\n"
":param text: How much money to deposit, in string representation.\n"
":type text: str\n"
":returns: Tuple containing the status code (one of the BANK_xxx constants, "
"eg, :attr:`~Atrinik.BANK_SUCCESS`) and amount of money deposited as "
"integer.\n"
":rtype: tuple";

/**
 * Implements Atrinik.Player.Player.BankDeposit() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_BankDeposit(Atrinik_Player *self,
        PyObject *args)
{
    const char *text;

    if (!PyArg_ParseTuple(args, "s", &text)) {
        return NULL;
    }

    int64_t value;
    int ret = hooks->bank_deposit(self->pl->ob, text, &value);

    return Py_BuildValue("(iL)", ret, value);
}

/** Documentation for Atrinik_Player_BankWithdraw(). */
static const char doc_Atrinik_Player_BankWithdraw[] =
".. method:: BankWithdraw(text).\n\n"
"Withdraw money from bank.\n\n"
":param text: How much money to withdraw, in string representation.\n"
":type text: str\n"
":returns: Tuple containing the status code (one of the BANK_xxx constants, "
"eg, :attr:`~Atrinik.BANK_SUCCESS`) and amount of money withdrawn as "
"integer.\n"
":rtype: tuple";

/**
 * Implements Atrinik.Player.Player.BankWithdraw() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_BankWithdraw(Atrinik_Player *self,
        PyObject *args)
{
    const char *text;

    if (!PyArg_ParseTuple(args, "s", &text)) {
        return NULL;
    }

    int64_t value;
    int ret = hooks->bank_withdraw(self->pl->ob, text, &value);

    return Py_BuildValue("(iL)", ret, value);
}

/** Documentation for Atrinik_Player_BankBalance(). */
static const char doc_Atrinik_Player_BankBalance[] =
".. method:: BankBalance().\n\n"
"Figure out how much money player has in bank.\n\n"
":returns: Integer value of the money in bank.\n"
":rtype: int";

/**
 * Implements Atrinik.Player.Player.BankBalance() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Player_BankBalance(Atrinik_Player *self)
{
    return Py_BuildValue("L", hooks->bank_get_balance(self->pl->ob));
}

/** Documentation for Atrinik_Player_SwapApartments(). */
static const char doc_Atrinik_Player_SwapApartments[] =
".. method:: SwapApartments(oldmap, newmap, x, y).\n\n"
"Swaps *oldmap* apartment with *newmap* one.\n\nCopies old items from *oldmap* "
"to *newmap* at *x*, *y* and saves the map.\n\n"
":param oldmap: The old apartment map.\n"
":type oldmap: str\n"
":param newmap: The new apartment map.\n"
":type newmap: str\n"
":param x: X coordinate to copy the items to.\n"
":type x: int\n"
":param y: Y coordinate to copy the items to.\n"
":type y: int\n"
":returns: Whether the operation was successful or not.\n"
":rtype: bool";

/**
 * Implements Atrinik.Player.Player.SwapApartments() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_SwapApartments(Atrinik_Player *self,
        PyObject *args)
{
    const char *mapold, *mapnew;
    int x, y;

    if (!PyArg_ParseTuple(args, "ssii", &mapold, &mapnew, &x, &y)) {
        return NULL;
    }

    return Py_BuildBoolean(hooks->swap_apartments(mapold, mapnew, x, y,
            self->pl->ob));
}

/** Documentation for Atrinik_Player_ExecuteCommand(). */
static const char doc_Atrinik_Player_ExecuteCommand[] =
".. method:: ExecuteCommand(command).\n\n"
"Make player execute a command.\n\n"
":param command: Command to execute.\n"
":type command: str\n"
":returns: Return value of the command.\n"
":rtype: int\n"
":raises Atrinik.AtrinikError: If player is not in a state to execute "
"commands.";

/**
 * Implements Atrinik.Player.Player.ExecuteCommand() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_ExecuteCommand(Atrinik_Player *self,
        PyObject *args)
{
    const char *command;

    if (!PyArg_ParseTuple(args, "s", &command)) {
        return NULL;
    }

    if (self->pl->socket.state != ST_PLAYING) {
        PyErr_SetString(AtrinikError,
                "Player is not in a state to execute commands.");
        return NULL;
    }

    /* Make a copy of the command, since commands_handle() modifies the
     * string. */
    char *cp = strdup(command);
    hooks->commands_handle(self->pl->ob, cp);
    free(cp);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>player.FindMarkedObject()</h1>
 * Find marked object in player's inventory.
 * @return The marked object, or None if no object is marked. */

/** Documentation for Atrinik_Player_FindMarkedObject(). */
static const char doc_Atrinik_Player_FindMarkedObject[] =
".. method:: FindMarkedObject().\n\n"
"Find marked object in player's inventory.\n\n"
":returns: The marked object, or None if no object is marked\n"
":rtype: :class:`Atrinik.Object.Object` or None";

/**
 * Implements Atrinik.Player.Player.FindMarkedObject() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Player_FindMarkedObject(Atrinik_Player *self)
{
    return wrap_object(hooks->find_marked_object(self->pl->ob));
}

/** Documentation for Atrinik_Player_Sound(). */
static const char doc_Atrinik_Player_Sound[] =
".. method:: Sound(filename, type=Atrinik.CMD_SOUND_EFFECT, x=0, y=0, loop=0, "
"volume=0).\n\n"
"Play a sound to the specified player.\n\n"
":param filename: Sound file to play.\n"
":type filename: str\n"
":param type: Sound type to play, one of the CMD_SOUND_xxx constants, eg, "
":attr:`~Atrinik.CMD_SOUND_BACKGROUND`.\n"
":type type: int\n"
":param x: X position where the sound is playing from.\n"
":type x: int\n"
":param y: Y position where the sound is playing from.\n"
":type y: int\n"
":param loop: How many times to loop the sound, -1 to loop infinitely.\n"
":type loop: int\n"
":param volume: Volume adjustment.\n"
":type volume: int";

/**
 * Implements Atrinik.Player.Player.Sound() Python method.
 * @copydoc PyMethod_VARARGS_KEYWORDS
 */
static PyObject *Atrinik_Player_Sound(Atrinik_Player *self, PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {
        "filename", "type", "x", "y", "loop", "volume", NULL
    };
    const char *filename;
    int type = CMD_SOUND_EFFECT, x = 0, y = 0, loop = 0, volume = 0;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|iiiii", kwlist,
            &filename, &type, &x, &y, &loop, &volume)) {
        return NULL;
    }

    hooks->play_sound_player_only(self->pl, type, filename, x, y, loop, volume);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_Examine(). */
static const char doc_Atrinik_Player_Examine[] =
".. method:: Examine(obj, ret=False).\n\n"
"Makes player examine the specified object.\n\n"
":param obj: Object to examine.\n"
":type obj: :class:`Atrinik.Object.Object`\n"
":param ret: If True, instead of printing out the examine text to the player,"
"the examine text is returned as a string.\n"
":type ret: bool\n"
":returns: None, examine string in case *ret* was True.\n"
":rtype: None or str";

/**
 * Implements Atrinik.Player.Player.Examine() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_Examine(Atrinik_Player *self, PyObject *args)
{
    Atrinik_Object *obj;
    int ret = 0;

    if (!PyArg_ParseTuple(args, "O!|i", &Atrinik_ObjectType, &obj, &ret)) {
        return NULL;
    }

    StringBuffer *sb_capture = NULL;
    if (ret) {
        sb_capture = hooks->stringbuffer_new();
    }

    hooks->examine(self->pl->ob, obj->obj, sb_capture);

    if (ret) {
        char *cp = hooks->stringbuffer_finish(sb_capture);
        PyObject *retval = Py_BuildValue("s", cp);
        efree(cp);

        return retval;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_SendPacket(). */
static const char doc_Atrinik_Player_SendPacket[] =
".. method:: SendPacket(command, format, *args).\n\n"
"Constructs and sends a packet to the player's client.\n\n"
":param command: The command ID.\n"
":type command: int\n"
":param format: Format specifier. For example, 'Bs' would imply uint8_t + "
"string data, and the format specifier would need to be followed by an integer "
"that is within uint8_t data range and a string. Allowed format specifiers "
"are:\n\n"
"  * **b**: 8-bit signed int (int8_t)\n"
"  * **B**: 8-bit unsigned int (uint8_t)\n"
"  * **h**: 16-bit signed int (int16_t)\n"
"  * **H**: 16-bit unsigned int (uint16_t)\n"
"  * **i**: 32-bit signed int (int32_t)\n"
"  * **I**: 32-bit unsigned int (uint32_t)\n"
"  * **l**: 64-bit signed int (int64_t)\n"
"  * **L**: 64-bit unsigned int (uint64_t)\n"
"  * **s**: String (automatically NUL-terminated)\n"
"  * **x**: Bytes (NOT NUL-terminated)\n"
":type format: str\n"
":param \\*args: Rest of the arguments is converted into the packet data as "
"specified by the format string.\n"
":raises OverflowError: If *command* is not within a valid range.\n"
":raises ValueError: If an unrecognized character is encountered in "
"*format*.\n"
":raises TypeError: If an object's type in *args* does not match what was "
"specified in *format*.\n"
":raises OverflowError: If an integer specified in *args* is not within a "
"valid range.";

/**
 * Implements Atrinik.Player.Player.Examine() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_SendPacket(Atrinik_Player *self, PyObject *args)
{
    /* Must have at least 3 arguments. */
    if (PyTuple_Size(args) < 3) {
        PyErr_SetString(PyExc_TypeError, "Insufficient number of arguments.");
        return NULL;
    }

    /* The first argument must be an integer. */
    if (!PyInt_Check(PyTuple_GET_ITEM(args, 0))) {
        PyErr_SetString(PyExc_TypeError,
                "Illegal object type for 'cmd' function argument.");
        return NULL;
    }

    long cmd = PyLong_AsLong(PyTuple_GET_ITEM(args, 0));
    /* It also must be uint8. */
    if (cmd < 0 || cmd > UINT8_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                "Invalid value for 'cmd' function argument.");
        return NULL;
    }

    /* Get the format specifier. */
    char *format = PyString_AsString(PyTuple_GET_ITEM(args, 1));

    packet_struct *packet = hooks->packet_new(cmd, 256, 512);

#define CHECK_INT_RANGE(min, max) \
    if (PyErr_Occurred()) { \
        PyErr_Format(PyExc_OverflowError, \
                "Invalid integer value for '%c' format specifier.", \
                format[i]); \
        goto error; \
    } else if (val < min || val > max) { \
        PyErr_Format(PyExc_OverflowError, \
                "Invalid integer value for '%c' format specifier.", \
                format[i]); \
        goto error; \
    }
#define CHECK_UINT_RANGE(max) \
    if (PyErr_Occurred()) { \
        PyErr_Format(PyExc_OverflowError, \
                "Invalid integer value for '%c' format specifier.", \
                format[i]); \
        goto error; \
    } else if (val > max) { \
        PyErr_Format(PyExc_OverflowError, \
                "Invalid integer value for '%c' format specifier.", \
                format[i]); \
        goto error; \
    }

    for (size_t i = 0; format[i] != '\0'; i++) {
        PyObject *value = PyTuple_GetItem(args, 2 + i);
        if (value == NULL) {
            PyErr_SetString(PyExc_ValueError,
                    "Insufficient number of arguments.");
            goto error;
        }

        if (format[i] == 'b') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);
                CHECK_INT_RANGE(INT8_MIN, INT8_MAX);
                hooks->packet_append_int8(packet, val);
                continue;
            }
        } else if (format[i] == 'B') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);
                CHECK_INT_RANGE(0, UINT8_MAX);
                hooks->packet_append_uint8(packet, val);
                continue;
            }
        } else if (format[i] == 'h') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);
                CHECK_INT_RANGE(INT16_MIN, INT16_MAX);
                hooks->packet_append_int16(packet, val);
                continue;
            }
        } else if (format[i] == 'H') {
            if (PyInt_Check(value)) {
                long val = PyLong_AsLong(value);
                CHECK_INT_RANGE(0, UINT16_MAX);
                hooks->packet_append_uint16(packet, val);
                continue;
            }
        } else if (format[i] == 'i') {
            if (PyInt_Check(value)) {
                PY_LONG_LONG val = PyLong_AsLongLong(value);
                CHECK_INT_RANGE(INT32_MIN, INT32_MAX);
                hooks->packet_append_int32(packet, val);
                continue;
            }
        } else if (format[i] == 'I') {
            if (PyInt_Check(value)) {
                unsigned PY_LONG_LONG val = PyLong_AsUnsignedLongLong(value);
                CHECK_UINT_RANGE(UINT32_MAX);
                hooks->packet_append_uint32(packet, val);
                continue;
            }
        } else if (format[i] == 'l') {
            if (PyInt_Check(value)) {
                PY_LONG_LONG val = PyLong_AsLongLong(value);
                if (PyErr_Occurred()) {
                    PyErr_Format(PyExc_OverflowError,
                            "Invalid integer value for '%c' format specifier.",
                            format[i]);
                    goto error;
                }

                hooks->packet_append_int64(packet, val);
                continue;
            }
        } else if (format[i] == 'L') {
            if (PyInt_Check(value)) {
                unsigned PY_LONG_LONG val = PyLong_AsUnsignedLongLong(value);
                if (PyErr_Occurred()) {
                    PyErr_Format(PyExc_OverflowError,
                            "Invalid integer value for '%c' format specifier.",
                            format[i]);
                    goto error;
                }

                hooks->packet_append_uint64(packet, val);
                continue;
            }
        } else if (format[i] == 's') {
            if (PyString_Check(value)) {
                Py_ssize_t size;
                char *data = PyUnicode_AsUTF8AndSize(value, &size);
                hooks->packet_append_string_len_terminated(packet, data, size);
                continue;
            }
        } else if (format[i] == 'x') {
            if (PyBytes_Check(value)) {
                hooks->packet_append_data_len(packet,
                        (uint8_t *) PyBytes_AsString(value),
                        PyBytes_Size(value));
                continue;
            }
        } else {
            PyErr_Format(PyExc_ValueError,
                    "Illegal format specifier '%c'.", format[i]);
            goto error;
        }

        PyErr_Format(PyExc_TypeError,
                "Illegal object type for '%c' format specifier.", format[i]);
        goto error;
    }

#undef CHECK_INT_RANGE
#undef CHECK_UINT_RANGE

    hooks->socket_send_packet(&self->pl->socket, packet);

    Py_INCREF(Py_None);
    return Py_None;

error:
    hooks->packet_free(packet);
    return NULL;
}

/** Documentation for Atrinik_Player_DrawInfo(). */
static const char doc_Atrinik_Player_DrawInfo[] =
".. method:: DrawInfo(message, color=Atrinik.COLOR_ORANGE, "
"type=Atrinik.CHAT_TYPE_GAME, broadcast=False, name=None).\n\n"
"Sends a message to the player.\n\n"
":param message: The message to send.\n"
":type message: str\n"
":param color: Color to use for the message. Can be one of the COLOR_xxx "
"constants (eg, :attr:`~Atrinik.COLOR_RED`) or a regular HTML color notation "
"(eg, '00ff00')\n"
":type color: str\n"
":param type: One of the CHAT_TYPE_xxx constants, eg, :attr:"
"`~Atrinik.CHAT_TYPE_CHAT`.\n"
":type type: int\n"
":param broadcast: If True, the message will be broadcast to all players.\n"
":type broadcast: bool\n"
":param name: Player name that is the source of this message, if applicable. "
"If None and *type* is not :attr:`~Atrinik.CHAT_TYPE_GAME`, :attr:"
"`Atrinik.Player.Player.ob.name` will be used.\n"
":type name: str or None";

/**
 * Implements Atrinik.Player.Player.DrawInfo() Python method.
 * @copydoc PyMethod_VARARGS_KEYWORDS
 */
static PyObject *Atrinik_Player_DrawInfo(Atrinik_Player *self, PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {
        "message", "color", "type", "broadcast", "name", NULL
    };
    const char *message, *color, *name;
    uint8_t type, broadcast;

    color = COLOR_ORANGE;
    type = CHAT_TYPE_GAME;
    broadcast = 0;
    name = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|sbbz", kwlist,
            &message, &color, &type, &broadcast, &name)) {
        return NULL;
    }

    if (name == NULL && type != CHAT_TYPE_GAME) {
        name = self->pl->ob->name;
    }

    hooks->draw_info_type(type, name, color, broadcast ? NULL : self->pl->ob,
            message);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_FactionGetBounty(). */
static const char doc_Atrinik_Player_FactionGetBounty[] =
".. method:: FactionGetBounty(faction).\n\n"
"Acquires player's bounty for the specified faction.\n\n"
":param faction: The faction name.\n"
":type faction: str\n"
":returns: Player's bounty in the specified faction.\n"
":rtype: float\n"
":raises Atrinik.AtrinikError: If the specified faction doesn't exist.";

/**
 * Implements Atrinik.Player.Player.FactionGetBounty() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_FactionGetBounty(Atrinik_Player *self,
        PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    shstr *sh_name = hooks->find_string(name);
    if (sh_name == NULL) {
        PyErr_Format(AtrinikError, "No such faction: %s", name);
        return NULL;
    }

    faction_t faction = hooks->faction_find(sh_name);
    if (faction == NULL) {
        PyErr_Format(AtrinikError, "No such faction: %s", name);
        return NULL;
    }

    return Py_BuildValue("d", hooks->faction_get_bounty(faction, self->pl));
}

/** Documentation for Atrinik_Player_FactionClearBounty(). */
static const char doc_Atrinik_Player_FactionClearBounty[] =
".. method:: FactionClearBounty(faction).\n\n"
"Clear player's bounty for the specified faction.\n\n"
":param faction: The faction name.\n"
":type faction: str\n"
":raises Atrinik.AtrinikError: If the specified faction doesn't exist";

/**
 * Implements Atrinik.Player.Player.FactionClearBounty() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_FactionClearBounty(Atrinik_Player *self,
        PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    shstr *sh_name = hooks->find_string(name);
    if (sh_name == NULL) {
        PyErr_Format(AtrinikError, "No such faction: %s", name);
        return NULL;
    }

    faction_t faction = hooks->faction_find(sh_name);
    if (faction == NULL) {
        PyErr_Format(AtrinikError, "No such faction: %s", name);
        return NULL;
    }

    hooks->faction_clear_bounty(faction, self->pl);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_InsertCoins(). */
static const char doc_Atrinik_Player_InsertCoins[] =
".. method:: InsertCoins(value).\n\n"
"Gives coins of the specified value to the player.\n\n"
":param value: The value.\n"
":type value: int";

/**
 * Implements Atrinik.Player.Player.InsertCoins() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_InsertCoins(Atrinik_Player *self,
        PyObject *args)
{
    int64_t value;

    if (!PyArg_ParseTuple(args, "l", &value)) {
        return NULL;
    }

    hooks->shop_insert_coins(self->pl->ob, value);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_Save(). */
static const char doc_Atrinik_Player_Save[] =
".. method:: Save().\n\n"
"Saves the player.\n\n";

/**
 * Implements Atrinik.Player.Player.Save() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Player_Save(Atrinik_Player *self)
{
    hooks->player_save(self->pl->ob);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Player_Address(). */
static const char doc_Atrinik_Player_Address[] =
".. method:: Address(verbose=False).\n\n"
"Acquires the player's IP address.\n\n"
":param verbose: If True, will contain the port as well.\n"
":type verbose: bool\n"
":returns: The player's IP address.\n"
":rtype: str";

/**
 * Implements Atrinik.Player.Player.Address() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Player_Address(Atrinik_Player *self, PyObject *args)
{
    int verbose = 0;
    if (!PyArg_ParseTuple(args, "|i", &verbose)) {
        return NULL;
    }

    if (!verbose) {
        return Py_BuildValue("s", hooks->socket_get_addr(self->pl->socket.sc));
    }

    return Py_BuildValue("s", hooks->socket_get_str(self->pl->socket.sc));
}

/** Available Python methods for the AtrinikPlayer type. */
static PyMethodDef methods[] = {
    {"GetEquipment", (PyCFunction) Atrinik_Player_GetEquipment, METH_VARARGS,
            doc_Atrinik_Player_GetEquipment},
    {"CanCarry", (PyCFunction) Atrinik_Player_CanCarry, METH_O,
            doc_Atrinik_Player_CanCarry},
    {"AddExp", (PyCFunction) Atrinik_Player_AddExp, METH_VARARGS,
            doc_Atrinik_Player_AddExp},
    {"BankDeposit", (PyCFunction) Atrinik_Player_BankDeposit, METH_VARARGS,
            doc_Atrinik_Player_BankDeposit},
    {"BankWithdraw", (PyCFunction) Atrinik_Player_BankWithdraw, METH_VARARGS,
            doc_Atrinik_Player_BankWithdraw},
    {"BankBalance", (PyCFunction) Atrinik_Player_BankBalance, METH_NOARGS,
            doc_Atrinik_Player_BankBalance},
    {"SwapApartments", (PyCFunction) Atrinik_Player_SwapApartments,
            METH_VARARGS, doc_Atrinik_Player_SwapApartments},
    {"ExecuteCommand", (PyCFunction) Atrinik_Player_ExecuteCommand,
            METH_VARARGS, doc_Atrinik_Player_ExecuteCommand},
    {"FindMarkedObject", (PyCFunction) Atrinik_Player_FindMarkedObject,
            METH_NOARGS, doc_Atrinik_Player_FindMarkedObject},
    {"Sound", (PyCFunction) Atrinik_Player_Sound, METH_VARARGS | METH_KEYWORDS,
            doc_Atrinik_Player_Sound},
    {"Examine", (PyCFunction) Atrinik_Player_Examine, METH_VARARGS,
            doc_Atrinik_Player_Examine},
    {"SendPacket", (PyCFunction) Atrinik_Player_SendPacket, METH_VARARGS,
            doc_Atrinik_Player_SendPacket},
    {"DrawInfo", (PyCFunction) Atrinik_Player_DrawInfo,
            METH_VARARGS | METH_KEYWORDS, doc_Atrinik_Player_DrawInfo},
    {"FactionGetBounty", (PyCFunction) Atrinik_Player_FactionGetBounty,
            METH_VARARGS, doc_Atrinik_Player_FactionGetBounty},
    {"FactionClearBounty", (PyCFunction) Atrinik_Player_FactionClearBounty,
            METH_VARARGS, doc_Atrinik_Player_FactionClearBounty},
    {"InsertCoins", (PyCFunction) Atrinik_Player_InsertCoins, METH_VARARGS,
            doc_Atrinik_Player_InsertCoins},
    {"Save", (PyCFunction) Atrinik_Player_Save, METH_NOARGS,
            doc_Atrinik_Player_Save},
    {"Address", (PyCFunction) Atrinik_Player_Address, METH_VARARGS,
            doc_Atrinik_Player_Address},

    {NULL, NULL, 0, NULL}
};

/**
 * Get player's attribute.
 * @param pl Python player wrapper.
 * @param context Void pointer to the field ID.
 * @return Python object with the attribute value, NULL on failure.
 */
static PyObject *get_attribute(Atrinik_Player *pl, void *context)
{
    return generic_field_getter(context, pl->pl);
}

/**
 * Set attribute of a player.
 * @param whoptr Python player wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure.
 */
static int set_attribute(Atrinik_Player *pl, PyObject *value, void *context)
{
    fields_struct *field = context;

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
 * @return The new wrapper.
 */
static PyObject *Atrinik_Player_new(PyTypeObject *type, PyObject *args,
        PyObject *kwds)
{
    Atrinik_Player *pl = (Atrinik_Player *) type->tp_alloc(type, 0);
    if (pl != NULL) {
        pl->pl = NULL;
    }

    return (PyObject *) pl;
}

/**
 * Free a player wrapper.
 * @param pl The wrapper to free.
 */
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
 * @return Python object containing the name of the player.
 */
static PyObject *Atrinik_Player_str(Atrinik_Player *pl)
{
    return Py_BuildValue("s", pl->pl->ob->name);
}

static int Atrinik_Player_InternalCompare(Atrinik_Player *left,
        Atrinik_Player *right)
{
    return (left->pl < right->pl ? -1 : (left->pl == right->pl ? 0 : 1));
}

static PyObject *Atrinik_Player_RichCompare(Atrinik_Player *left,
        Atrinik_Player *right, int op)
{
    if (left == NULL || right == NULL ||
            !PyObject_TypeCheck((PyObject *) left, &Atrinik_PlayerType) ||
            !PyObject_TypeCheck((PyObject *) right, &Atrinik_PlayerType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op,
            Atrinik_Player_InternalCompare(left, right));
}

/**
 * This is filled in when we initialize our player type.
 */
static PyGetSetDef getseters[NUM_FIELDS + 1];

/**
 * Our actual Python PlayerType.
 */
PyTypeObject Atrinik_PlayerType = {
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
    "Atrinik Player class.\n\n"
    "To access object's player controller, you can use something like::\n\n"
    "    activator = Atrinik.WhoIsActivator()\n"
    "    player = activator.Controller()\n\n"
    "In the above example, player points to the player structure (which Python "
    "is wrapping) that is controlling the object *activator*. In this way, you "
    "can, for example, use something like this to get player's save bed, among "
    "other things::\n\n"
    "    print(Atrinik.WhoIsActivator().Controller().savebed_map)\n\n",
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
#ifdef Py_TPFLAGS_HAVE_FINALIZE
    , NULL
#endif
};

/**
 * Initialize the Atrinik.Player module.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure.
 */
int Atrinik_Player_init(PyObject *module)
{
    size_t i;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) get_attribute;
        def->set = (setter) set_attribute;
        def->doc = fields[i].doc;
        def->closure = &fields[i];
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
 * @return Python object wrapping the real player.
 */
PyObject *wrap_player(player *pl)
{
    /* Return None if no player was to be wrapped. */
    if (pl == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    Atrinik_Player *wrapper = PyObject_NEW(Atrinik_Player, &Atrinik_PlayerType);
    if (wrapper != NULL) {
        wrapper->pl = pl;
    }

    return (PyObject *) wrapper;
}
