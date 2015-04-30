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

enum {
    SA_OPER_TYPE_HOUR, ///< In-game hours.
    SA_OPER_TYPE_MINUTE ///< In-game minutes.
};

enum {
    SA_OPER_NONE,
    SA_OPER_ADD,
    SA_OPER_SUB,
    SA_OPER_MUL,
    SA_OPER_DIV,
    SA_OPER_MOD
};

enum {
    SA_OPER2_EQ,
    SA_OPER2_LT,
    SA_OPER2_GT,
    SA_OPER2_LE,
    SA_OPER2_GE
};

typedef struct sound_ambient_match {
    struct sound_ambient_match *next;

    union {
        struct sound_ambient_match *group;

        struct {
            uint8_t type;

            uint8_t operation;

            uint16_t num;

            uint8_t operation2;

            uint16_t num2;
        } operation;
    } data;

    int is_group : 1;

    int is_and : 1;
} __attribute__((packed)) sound_ambient_match_t;

static void sound_ambient_match_print_rec(sound_ambient_match_t *match,
        char *buf, size_t size)
{
    sound_ambient_match_t *tmp;

    for (tmp = match; tmp != NULL; tmp = tmp->next) {
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

static int sound_ambient_match_rec(sound_ambient_match_t *match)
{
    sound_ambient_match_t *tmp;
    int ret, value;
    timeofday_t tod;

    if (match == NULL) {
        return 1;
    }

    get_tod(&tod);

    for (tmp = match; tmp != NULL; tmp = tmp->next) {
        if (tmp->is_group) {
            ret = sound_ambient_match_rec(tmp->data.group);
        } else {
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
                ret = 0;
                break;
            }
        }

        if (ret && !tmp->is_and) {
            return 1;
        } else if (!ret && tmp->is_and) {
            return 0;
        }
    }

    return 0;
}

static void sound_ambient_match_free(sound_ambient_match_t *match)
{
    sound_ambient_match_t *tmp, *next;

    for (tmp = match; tmp != NULL; tmp = next) {
        next = tmp->next;

        if (tmp->is_group) {
            sound_ambient_match_free(tmp->data.group);
        }

        efree(tmp);
    }
}

const char *sound_ambient_match_str(object *ob)
{
    static char buf[HUGE_BUF];

    buf[0] = '\0';
    sound_ambient_match_print_rec(ob->custom_attrset, VS(buf));

    return buf;
}

int sound_ambient_match(object *ob)
{
    return sound_ambient_match_rec(ob->custom_attrset);
}

void sound_ambient_match_parse(object *ob, const char *str)
{
    char word[64], *cp;
    sound_ambient_match_t *match, *match_stack[10], *tmp;
    size_t pos, stack_id, word_num;
    int is_group_end, is_in_group;

    HARD_ASSERT(ob != NULL);
    HARD_ASSERT(str != NULL);

    if (ob->type != SOUND_AMBIENT) {
        LOG(BUG, "Called on incorrect object type: %d", ob->type);
        return;
    }

    sound_ambient_match_free(ob->custom_attrset);
    ob->custom_attrset = NULL;

    pos = 0;
    match = NULL;
    memset(match_stack, 0, 10 * sizeof(*match_stack));
    stack_id = 0;
    is_in_group = 0;
    is_group_end = 0;
    word_num = 0;

    while (string_get_word(str, &pos, ' ', word, sizeof(word), 0)) {
        cp = word;

        while (string_startswith(cp, "(")) {
            cp++;
            stack_id++;

            tmp = ecalloc(1, sizeof(sound_ambient_match_t));
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
            is_in_group++;
        }

        while (string_endswith(word, ")")) {
            word[strlen(word) - 1] = '\0';
            is_group_end++;
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

            if (ob->custom_attrset == NULL) {
                if (match_stack[0] != NULL) {
                    ob->custom_attrset = match_stack[0];
                } else {
                    ob->custom_attrset = match;
                }
            }

            if (match_stack[stack_id] != NULL) {
                if (match_stack[stack_id]->is_group && is_in_group &&
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

        if (is_group_end) {
            if (stack_id > 0) {
                stack_id -= is_group_end;
            }

            is_in_group -= is_group_end;
            is_group_end = 0;
            match = NULL;
        }

        word_num++;
    }
}

/**
 * Initialize ambient sound effect object.
 * @param ob The object to initialize.
 */
void sound_ambient_init(object *ob)
{
    MapSpace *msp;

    HARD_ASSERT(ob != NULL);
    HARD_ASSERT(ob->type == SOUND_AMBIENT);

    /* Must be on map... */
    if (ob->map == NULL) {
        LOG(BUG, "Ambient sound effect object not on map.");
        return;
    }

    if (string_isempty(ob->race)) {
        LOG(BUG, "Ambient sound effect object is missing sound "
                "effect filename.");
        return;
    }

    msp = GET_MAP_SPACE_PTR(ob->map, ob->x, ob->y);
    msp->sound_ambient = ob;
    msp->sound_ambient_count = ob->count;
}

/**
 * Deinitialize ambient sound effect object.
 * @param ob The object to deinitialize.
 */
void sound_ambient_deinit(object *ob)
{
    HARD_ASSERT(ob != NULL);
    HARD_ASSERT(ob->type == SOUND_AMBIENT);

    sound_ambient_match_free(ob->custom_attrset);
    ob->custom_attrset = NULL;
}

/**
 * Initialize the ambient sound type object methods. */
void object_type_init_sound_ambient(void)
{
}
