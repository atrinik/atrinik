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
 * Handles @ref SOUND_AMBIENT "ambient sound effect" objects.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <object.h>
#include <object_methods.h>
#include <sound_ambient.h>

/**
 * @anchor SA_OPER_TYPE_xxx
 *
 * Rule types.
 */
enum {
    SA_OPER_TYPE_HOUR, ///< In-game hours.
    SA_OPER_TYPE_MINUTE, ///< In-game minutes.
};

/**
 * @anchor SA_OPER_xxx
 *
 * Basic rule operations.
 */
enum {
    SA_OPER_NONE, ///< No operation.
    SA_OPER_ADD, ///< Addition.
    SA_OPER_SUB, ///< Subtraction.
    SA_OPER_MUL, ///< Multiplication.
    SA_OPER_DIV, ///< Division.
    SA_OPER_MOD, ///< Modulo.
};

/**
 * @anchor SA_OPER2_xxx
 *
 * Test rule operations.
 */
enum {
    SA_OPER2_EQ, ///< Test for equality.
    SA_OPER2_LT, ///< Test for less than.
    SA_OPER2_GT, ///< Test for greater than.
    SA_OPER2_LE, ///< Test for less than or equal to.
    SA_OPER2_GE, ///< Test for greater than or equal to.
};

/**
 * Structure used to represent the rules of an ambient sound object.
 */
typedef struct sound_ambient_match {
    struct sound_ambient_match *next; ///< Next match rule in a linked list.

    union {
        struct sound_ambient_match *group; ///< Group of rules.

        struct {
            uint8_t type; ///< One of @ref SA_OPER_TYPE_xxx "operation types".

            uint8_t operation; ///< One of @ref SA_OPER_xxx "basic operations".

            uint16_t num; ///< Arbitrary number for basic operations.

            uint8_t operation2; ///< One of @ref SA_OPER2_xxx "test operations".

            uint16_t num2; ///< Arbitrary number for test operations.
        } operation;
    } data; ///< Data about the rule.

    int is_group:1; ///< Whether the data union points to a group of rules.

    int is_and:1; ///< Whether this is an AND rule.
} __attribute__((packed)) sound_ambient_match_t;

/**
 * Frees the specified ambient sound match structure.
 *
 * @param match
 * Structure to free.
 */
static void
sound_ambient_match_free (sound_ambient_match_t *match)
{
    HARD_ASSERT(match != NULL);

    for (sound_ambient_match_t *tmp = match, *next; tmp != NULL; tmp = next) {
        next = tmp->next;

        if (tmp->is_group) {
            sound_ambient_match_free(tmp->data.group);
        }

        efree(tmp);
    }
}

/** @copydoc object_methods_t::init_func */
static void
init_func (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->type == SOUND_AMBIENT);

    /* Must be on map... */
    if (op->map == NULL) {
        LOG(ERROR,
            "Ambient sound effect object not on map: %s",
            object_get_str(op));
        return;
    }

    if (string_isempty(op->race)) {
        LOG(ERROR,
            "Ambient sound effect object is missing sound effect filename: %s",
            object_get_str(op));
        return;
    }

    MapSpace *msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);
    msp->sound_ambient = op;
    msp->sound_ambient_count = op->count;
}

/** @copydoc object_methods_t::deinit_func */
static void
deinit_func (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->type == SOUND_AMBIENT);

    sound_ambient_match_free(op->custom_attrset);
    op->custom_attrset = NULL;
}

/**
 * Initialize the ambient sound type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(sound_ambient)
{
    OBJECT_METHODS(SOUND_AMBIENT)->init_func = init_func;
    OBJECT_METHODS(SOUND_AMBIENT)->deinit_func = deinit_func;
}

/**
 * Used by sound_ambient_match_str() to create a string representation
 * of the ambient sound match structure.
 *
 * @param match
 * The match to create a string representation of. Can be NULL.
 * @param buf
 * Where to store the string. Must be NUL-terminated.
 * @param size
 * Maximum size of 'buf'.
 */
static void
sound_ambient_match_print_rec (sound_ambient_match_t *match,
                               char                  *buf,
                               size_t                 size)
{
    HARD_ASSERT(buf != NULL);

    for (sound_ambient_match_t *tmp = match; tmp != NULL; tmp = tmp->next) {
        if (tmp != match) {
            snprintfcat(buf, size, " ");
        }

        if (tmp->is_group) {
            snprintfcat(buf, size, "(");
            sound_ambient_match_print_rec(tmp->data.group, buf, size);
            snprintfcat(buf, size, ")");
        } else {
            switch (tmp->data.operation.type) {
            case SA_OPER_TYPE_HOUR:
                snprintfcat(buf, size, "hour");
                break;

            case SA_OPER_TYPE_MINUTE:
                snprintfcat(buf, size, "minute");
                break;
            }

            switch (tmp->data.operation.operation) {
            case SA_OPER_NONE:
                break;

            case SA_OPER_ADD:
                snprintfcat(buf, size, " + ");
                break;

            case SA_OPER_SUB:
                snprintfcat(buf, size, " - ");
                break;

            case SA_OPER_MUL:
                snprintfcat(buf, size, " * ");
                break;

            case SA_OPER_DIV:
                snprintfcat(buf, size, " / ");
                break;

            case SA_OPER_MOD:
                snprintfcat(buf, size, " %% ");
                break;
            }

            if (tmp->data.operation.operation != SA_OPER_NONE) {
                snprintfcat(buf, size, "%u", tmp->data.operation.num);
            }

            switch (tmp->data.operation.operation2) {
            case SA_OPER2_EQ:
                snprintfcat(buf, size, " == ");
                break;

            case SA_OPER2_LT:
                snprintfcat(buf, size, " < ");
                break;

            case SA_OPER2_GT:
                snprintfcat(buf, size, " > ");
                break;

            case SA_OPER2_LE:
                snprintfcat(buf, size, " <= ");
                break;

            case SA_OPER2_GE:
                snprintfcat(buf, size, " >= ");
                break;
            }

            snprintfcat(buf, size, "%u", tmp->data.operation.num2);
        }

        if (tmp->next != NULL) {
            snprintfcat(buf, size, " %s", tmp->is_and ? "&&" : "||");
        }
    }
}

/**
 * Match the rules in the specified ambient sound match structure.
 *
 * @param match
 * The sound match structure. Can be NULL.
 * @return
 * True if the rules match, false otherwise.
 */
static bool
sound_ambient_match_rec (sound_ambient_match_t *match)
{
    if (match == NULL) {
        return true;
    }

    timeofday_t tod;
    get_tod(&tod);

    for (sound_ambient_match_t *tmp = match; tmp != NULL; tmp = tmp->next) {
        bool ret;
        if (tmp->is_group) {
            ret = sound_ambient_match_rec(tmp->data.group);
        } else {
            int value;
            switch (tmp->data.operation.type) {
            case SA_OPER_TYPE_HOUR:
                value = tod.hour;
                break;

            case SA_OPER_TYPE_MINUTE:
                value = tod.minute;
                break;

            default:
                value = 0;
                break;
            }

            switch (tmp->data.operation.operation) {
            case SA_OPER_ADD:
                value += tmp->data.operation.num;
                break;

            case SA_OPER_SUB:
                value -= tmp->data.operation.num;
                break;

            case SA_OPER_MUL:
                value *= tmp->data.operation.num;
                break;

            case SA_OPER_DIV:
                value /= tmp->data.operation.num;
                break;

            case SA_OPER_MOD:
                value %= tmp->data.operation.num;
                break;
            }

            switch (tmp->data.operation.operation2) {
            case SA_OPER2_EQ:
                ret = value == tmp->data.operation.num2;
                break;

            case SA_OPER2_LT:
                ret = value < tmp->data.operation.num2;
                break;

            case SA_OPER2_GT:
                ret = value > tmp->data.operation.num2;
                break;

            case SA_OPER2_LE:
                ret = value <= tmp->data.operation.num2;
                break;

            case SA_OPER2_GE:
                ret = value >= tmp->data.operation.num2;
                break;

            default:
                ret = false;
                break;
            }
        }

        if (ret && !tmp->is_and) {
            return true;
        } else if (!ret && tmp->is_and) {
            return false;
        }
    }

    return false;
}

/**
 * Acquire a string representation of the specified ambient sound object
 * matching rules.
 *
 * @param op
 * Ambient sound object.
 * @return
 * String representation.
 */
const char *
sound_ambient_match_str (object *op)
{
    HARD_ASSERT(op != NULL);

    static char buf[HUGE_BUF];
    buf[0] = '\0';
    sound_ambient_match_print_rec(op->custom_attrset, VS(buf));

    return buf;
}

/**
 * Check if all the rules specified in the ambient sound object are met.
 *
 * @param op
 * Ambient sound object.
 * @return
 * True if all the rules are met, false otherwise.
 */
bool
sound_ambient_match (object *op)
{
    HARD_ASSERT(op != NULL);
    return sound_ambient_match_rec(op->custom_attrset);
}

/**
 * Parse string representation of ambient sound rules into the specified
 * object.
 *
 * @param op
 * Ambient sound object.
 * @param str
 * The string to parse.
 */
void
sound_ambient_match_parse (object *op, const char *str)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(str != NULL);

    if (op->type != SOUND_AMBIENT) {
        LOG(BUG, "Called on incorrect object type: %d", op->type);
        return;
    }

    sound_ambient_match_free(op->custom_attrset);
    op->custom_attrset = NULL;

    sound_ambient_match_t *match = NULL;
    sound_ambient_match_t *match_stack[10];
    memset(match_stack, 0, 10 * sizeof(*match_stack));
    size_t stack_id = 0;
    size_t group_num = 0;
    size_t group_end_num = 0;
    size_t word_num = 0;

    char word[64];
    size_t pos = 0;
    while (string_get_word(str, &pos, ' ', word, sizeof(word), 0)) {
        char *cp = word;

        while (string_startswith(cp, "(")) {
            cp++;
            stack_id++;

            sound_ambient_match_t *tmp =
                ecalloc(1, sizeof(sound_ambient_match_t));
            tmp->is_group = 1;

            if (match_stack[stack_id - 1] != NULL) {
                if (match_stack[stack_id - 1]->is_group &&
                        match_stack[stack_id - 1]->data.group == NULL) {
                    match_stack[stack_id - 1]->data.group = tmp;
                } else {
                    match_stack[stack_id - 1]->next = tmp;
                }
            }

            match_stack[stack_id] = tmp;
            match_stack[stack_id - 1] = tmp;
            group_num++;
        }

        while (string_endswith(word, ")")) {
            word[strlen(word) - 1] = '\0';
            group_end_num++;
        }

        if (strcmp(cp, "&&") == 0) {
            match_stack[stack_id]->is_and = 1;
            match = NULL;
            continue;
        } else if (strcmp(cp, "||") == 0) {
            match = NULL;
            continue;
        }

        if (match == NULL) {
            match = ecalloc(1, sizeof(*match));
            word_num = 0;

            if (op->custom_attrset == NULL) {
                if (match_stack[0] != NULL) {
                    op->custom_attrset = match_stack[0];
                } else {
                    op->custom_attrset = match;
                }
            }

            if (match_stack[stack_id] != NULL) {
                if (match_stack[stack_id]->is_group &&
                    group_num != 0 &&
                    match_stack[stack_id]->data.group == NULL) {
                    match_stack[stack_id]->data.group = match;
                } else {
                    match_stack[stack_id]->next = match;
                }
            }

            match_stack[stack_id] = match;
        }

        if (word_num == 0) {
            if (strcmp(cp, "hour") == 0) {
                match->data.operation.type = SA_OPER_TYPE_HOUR;
            } else if (strcmp(cp, "minute") == 0) {
                match->data.operation.type = SA_OPER_TYPE_MINUTE;
            }
        } else if (word_num == 1) {
            if (strcmp(cp, "+") == 0) {
                match->data.operation.operation = SA_OPER_ADD;
            } else if (strcmp(cp, "-") == 0) {
                match->data.operation.operation = SA_OPER_SUB;
            } else if (strcmp(cp, "*") == 0) {
                match->data.operation.operation = SA_OPER_MUL;
            } else if (strcmp(cp, "/") == 0) {
                match->data.operation.operation = SA_OPER_DIV;
            } else if (strcmp(cp, "%") == 0) {
                match->data.operation.operation = SA_OPER_MOD;
            } else {
                word_num += 2;
            }
        } else if (word_num == 2) {
            match->data.operation.num = atoi(cp);
        }

        if (word_num == 3) {
            if (strcmp(cp, "==") == 0) {
                match->data.operation.operation2 = SA_OPER2_EQ;
            } else if (strcmp(cp, "<") == 0) {
                match->data.operation.operation2 = SA_OPER2_LT;
            } else if (strcmp(cp, ">") == 0) {
                match->data.operation.operation2 = SA_OPER2_GT;
            } else if (strcmp(cp, "<=") == 0) {
                match->data.operation.operation2 = SA_OPER2_LE;
            } else if (strcmp(cp, ">=") == 0) {
                match->data.operation.operation2 = SA_OPER2_GE;
            }
        } else if (word_num == 4) {
            match->data.operation.num2 = atoi(cp);
        }

        if (group_end_num != 0) {
            if (stack_id > 0) {
                stack_id -= group_end_num;
            }

            group_num -= group_end_num;
            group_end_num = 0;
            match = NULL;
        }

        word_num++;
    }
}
