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
 * Item related functions */

#include <global.h>

/** Word representations of numbers used by get_number() */
static char numbers[21][20] = {
    "no",
    "",
    "two",
    "three",
    "four",
    "five",
    "six",
    "seven",
    "eight",
    "nine",
    "ten",
    "eleven",
    "twelve",
    "thirteen",
    "fourteen",
    "fifteen",
    "sixteen",
    "seventeen",
    "eighteen",
    "nineteen",
    "twenty"
};

/** Tens */
static char numbers_10[10][20] = {
    "zero",
    "ten",
    "twenty",
    "thirty",
    "forty",
    "fifty",
    "sixty",
    "seventy",
    "eighty",
    "ninety"
};

/** Levels as a full name and not a number. */
static char levelnumbers[21][20] = {
    "zeroth",
    "first",
    "second",
    "third",
    "fourth",
    "fifth",
    "sixth",
    "seventh",
    "eighth",
    "ninth",
    "tenth",
    "eleventh",
    "twelfth",
    "thirteenth",
    "fourteenth",
    "fifteenth",
    "sixteenth",
    "seventeenth",
    "eighteen",
    "nineteen",
    "twentieth"
};

/** Tens for levels */
static char levelnumbers_10[11][20] = {
    "zeroth",
    "tenth",
    "twentieth",
    "thirtieth",
    "fortieth",
    "fiftieth",
    "sixtieth",
    "seventieth",
    "eightieth",
    "ninetieth"
};

static char *describe_attack(object *op, int newline);
static char *get_number(int i);

/**
 * Generates the visible naming for attack forms.
 * Returns a static array of the description. This can return a
 * big buffer.
 * @param op Object to get the attack forms for.
 * @param newline If true, don't put parens around the description
 * but do put a newline at the end. Useful when dumping to files.
 * @return Static buffer with the attack forms. */
static char *describe_attack(object *op, int newline)
{
    static char buf[VERY_BIG_BUF];
    char buf1[VERY_BIG_BUF];
    int tmpvar, flag = 1;

    buf[0] = '\0';

    for (tmpvar = 0; tmpvar < NROFATTACKS; tmpvar++) {
        if (op->attack[tmpvar]) {
            if (flag && !newline) {
                strncat(buf, "(Attacks: ", sizeof(buf) - strlen(buf) - 1);
            }

            if (!newline) {
                if (!flag) {
                    strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
                }

                snprintf(buf1, sizeof(buf1), "%s %+d%%", attack_name[tmpvar], op->attack[tmpvar]);
            } else {
                snprintf(buf1, sizeof(buf1), "%s %+d%%\n", attack_name[tmpvar], op->attack[tmpvar]);
            }

            flag = 0;
            strncat(buf, buf1, sizeof(buf) - strlen(buf) - 1);
        }
    }

    if (!newline && !flag) {
        strncat(buf, ") ", sizeof(buf) - strlen(buf) - 1);
    }

    return buf;
}

/**
 * Generates the visible naming for protections.
 * Returns a static array of the description. This can return a
 * big buffer.
 * @param op Object to get the protections for.
 * @param newline If true, don't put parens around the description
 * but do put a newline at the end. Useful when dumping to files.
 * @return Static buffer with the protections. */
char *describe_protections(object *op, int newline)
{
    static char buf[VERY_BIG_BUF];
    char buf1[VERY_BIG_BUF];
    int tmpvar, flag = 1;

    buf[0] = '\0';

    for (tmpvar = 0; tmpvar < NROFATTACKS; tmpvar++) {
        if (op->protection[tmpvar]) {
            if (flag && !newline) {
                strncat(buf, "(Protections: ", sizeof(buf) - strlen(buf) - 1);
            }

            if (!newline) {
                if (!flag) {
                    strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
                }

                snprintf(buf1, sizeof(buf1), "%s %+d%%", attack_name[tmpvar], op->protection[tmpvar]);
            } else {
                snprintf(buf1, sizeof(buf1), "%s %d%%\n", attack_name[tmpvar], op->protection[tmpvar]);
            }

            flag = 0;
            strncat(buf, buf1, sizeof(buf) - strlen(buf) - 1);
        }
    }

    if (!newline && !flag) {
        strncat(buf, ") ", sizeof(buf) - strlen(buf) - 1);
    }

    return buf;
}

/**
 * Formats the item's weight.
 * @param op Object to get the weight of.
 * @return The text representation of the object's weight in a static buffer. */
char *query_weight(object *op)
{
    static char buf[10];
    int i = op->nrof ? (int) op->nrof * op->weight : op->weight + op->carrying;

    if (op->weight < 0) {
        return "      ";
    }

    if (i % 1000) {
        snprintf(buf, sizeof(buf), "%6.1f", (float) i / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "%4d  ", i / 1000);
    }

    return buf;
}

/**
 * Formats a level.
 * @param i Level to format.
 * @return Word representation of the level. */
char *get_levelnumber(int i)
{
    static char buf[MAX_BUF];

    if (i > 99) {
        snprintf(buf, sizeof(buf), "%d.", i);
        return buf;
    }

    if (i < 21) {
        return levelnumbers[i];
    }

    if (!(i % 10)) {
        return levelnumbers_10[i / 10];
    }

    strcpy(buf, numbers_10[i / 10]);
    strcat(buf, levelnumbers[i % 10]);

    return buf;
}

/**
 * Returns the text representation of the given number
 * in a static buffer. The buffer might be overwritten at the next
 * call.
 *
 * It is currently only used by the query_name() function.
 * @param i The number.
 * @return Text representation of the given number. */
static char *get_number(int i)
{
    if (i <= 20) {
        return numbers[i];
    } else {
        static char buf[MAX_BUF];
        snprintf(buf, sizeof(buf), "%d", i);
        return buf;
    }
}

/**
 * This function is similar to query_name(), but doesn't
 * contain any information about the object status (worn/cursed/etc).
 * @param op Object to get the name from.
 * @param caller Object calling this.
 * @return The short name of the object. */
char *query_short_name(object *op, object *caller)
{
    static char buf[HUGE_BUF];
    char buf2[HUGE_BUF];
    size_t len = 0;

    buf[0] = '\0';

    if (!op || !op->name) {
        return buf;
    }

    if (op->nrof) {
        safe_strcat(buf, get_number(op->nrof), &len, sizeof(buf));

        if (op->nrof != 1) {
            safe_strcat(buf, " ", &len, sizeof(buf));
        }

        if (!QUERY_FLAG(op, FLAG_IS_NAMED)) {
            /* Add the item race name */
            if (!IS_LIVE(op) && op->type != BASE_INFO) {
                safe_strcat(buf, item_races[op->item_race], &len, sizeof(buf));
            }

            if (op->material_real && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
                safe_strcat(buf, material_real[op->material_real].name, &len, sizeof(buf));
            }
        }

        safe_strcat(buf, op->name, &len, sizeof(buf));

        if (op->nrof != 1) {
            char *buf3 = strstr(buf, " of ");

            if (buf3) {
                strcpy(buf2, buf3);
                /* Also changes value in buf */
                *buf3 = '\0';
            }

            len = strlen(buf);

            /* If buf3 is set, then this was a string that contained
             * something of something (potion of dexterity.)  The part before
             * the of gets made plural, so now we need to copy the rest
             * (after and including the " of "), to the buffer string. */
            if (buf3) {
                safe_strcat(buf, buf2, &len, sizeof(buf));
            }
        }
    } else {
        /* If nrof is 0, the object is not mergable, and thus, op->name
         * should contain the name to be used. */

        if (!QUERY_FLAG(op, FLAG_IS_NAMED)) {
            if (!IS_LIVE(op) && op->type != BASE_INFO) {
                safe_strcat(buf, item_races[op->item_race], &len, sizeof(buf));
            }

            if (op->material_real && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
                safe_strcat(buf, material_real[op->material_real].name, &len, sizeof(buf));
            }
        }

        safe_strcat(buf, op->name, &len, sizeof(buf));
    }

    switch (op->type) {
    case CONTAINER:

        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            if (op->title) {
                safe_strcat(buf, " ", &len, sizeof(buf));
                safe_strcat(buf, op->title, &len, sizeof(buf));
            }
        }

        if (op->sub_type >= ST1_CONTAINER_NORMAL_party) {
            if (op->sub_type == ST1_CONTAINER_CORPSE_party) {
                if (op->slaying) {
                    if (!caller || caller->type != PLAYER) {
                        safe_strcat(buf, " (bounty of a party)", &len, sizeof(buf));
                    } else if (CONTR(caller)->party && CONTR(caller)->party->name == op->slaying) {
                        safe_strcat(buf, " (bounty of your party", &len, sizeof(buf));

                        /* A searched bounty */
                        if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                            safe_strcat(buf, ", searched", &len, sizeof(buf));
                        }

                        safe_strcat(buf, ")", &len, sizeof(buf));
                    } else {
                        /* It's a different party */
                        safe_strcat(buf, " (bounty of another party)", &len, sizeof(buf));
                    }
                } else if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                    safe_strcat(buf, " (searched)", &len, sizeof(buf));
                }
            }
        } else if (op->sub_type >= ST1_CONTAINER_NORMAL_player) {
            if (op->sub_type == ST1_CONTAINER_CORPSE_player) {
                if (op->slaying) {
                    safe_strcat(buf, " (bounty of ", &len, sizeof(buf));
                    safe_strcat(buf, op->slaying, &len, sizeof(buf));

                    /* A searched bounty */
                    if ((caller && caller->name == op->slaying) && QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                        safe_strcat(buf, ", searched", &len, sizeof(buf));
                    }

                    safe_strcat(buf, ")", &len, sizeof(buf));
                } else if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                    safe_strcat(buf, " (searched)", &len, sizeof(buf));
                }
            }
        }

        break;

    case SCROLL:
    case WAND:
    case ROD:
    case POTION:

        if (QUERY_FLAG(op, FLAG_IDENTIFIED) || QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
            if (!op->title) {
                if (op->stats.sp != SP_NO_SPELL) {
                    safe_strcat(buf, " of ", &len, sizeof(buf));
                    safe_strcat(buf, spells[op->stats.sp].name, &len, sizeof(buf));
                } else {
                    safe_strcat(buf, " of nothing", &len, sizeof(buf));
                }
            } else {
                safe_strcat(buf, " ", &len, sizeof(buf));
                safe_strcat(buf, op->title, &len, sizeof(buf));
            }

            sprintf(buf2, " (lvl %d)", op->level);
            safe_strcat(buf, buf2, &len, sizeof(buf));
        }

        break;

    case SKILL:
    case AMULET:
    case RING:

        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            if (!op->title) {
                /* If ring has a title, full description isn't so useful */
                char *s = describe_item(op);

                if (s[0]) {
                    safe_strcat(buf, " ", &len, sizeof(buf));
                    safe_strcat(buf, s, &len, sizeof(buf));
                }
            } else {
                safe_strcat(buf, " ", &len, sizeof(buf));
                safe_strcat(buf, op->title, &len, sizeof(buf));
            }
        }

        break;

    default:

        if (op->magic && (!need_identify(op) || QUERY_FLAG(op, FLAG_BEEN_APPLIED) || QUERY_FLAG(op, FLAG_IDENTIFIED))) {
            if (!IS_LIVE(op) && op->type != BASE_INFO) {
                sprintf(buf2, " %+d", op->magic);
                safe_strcat(buf, buf2, &len, sizeof(buf));
            }
        }

        if (op->title && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            safe_strcat(buf, " ", &len, sizeof(buf));
            safe_strcat(buf, op->title, &len, sizeof(buf));
        }

        if ((op->type == ARROW || op->type == WEAPON) && op->slaying && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            safe_strcat(buf, " ", &len, sizeof(buf));
            safe_strcat(buf, op->slaying, &len, sizeof(buf));
        }
    }

    return buf;
}

/**
 * Returns a character pointer pointing to a static buffer which
 * contains a verbose textual representation of the name of the
 * given object.
 *
 * Uses 5 buffers that it will cycle through. In this way,
 * you can make several calls to query_name before the bufs start getting
 * overwritten. This may be a bad thing (it may be easier to assume the value
 * returned is good forever). However, it makes printing statements that
 * use several names much easier (don't need to store them to temp variables).
 * @param op Object to get the name from.
 * @param caller Object calling this.
 * @return Full name of the object, with things like worn/cursed/etc. */
char *query_name(object *op, object *caller)
{
    static char buf[5][HUGE_BUF];
    static int use_buf = 0;
    size_t len = 0;

    use_buf++;
    use_buf %= 5;

    if (!op || !op->name) {
        buf[use_buf][0] = 0;
        return buf[use_buf];
    }

    safe_strcat(buf[use_buf], query_short_name(op, caller), &len, HUGE_BUF);

    if (QUERY_FLAG(op, FLAG_ONE_DROP)) {
        safe_strcat(buf[use_buf], " (one-drop)", &len, HUGE_BUF);
    } else if (QUERY_FLAG(op, FLAG_QUEST_ITEM)) {
        safe_strcat(buf[use_buf], " (quest)", &len, HUGE_BUF);
    }

    if (QUERY_FLAG(op, FLAG_INV_LOCKED)) {
        safe_strcat(buf[use_buf], " *", &len, HUGE_BUF);
    }

    if (op->type == CONTAINER && QUERY_FLAG(op, FLAG_APPLIED)) {
        if (op->attacked_by && op->attacked_by->type == PLAYER) {
            safe_strcat(buf[use_buf], " (open)", &len, HUGE_BUF);
        } else {
            safe_strcat(buf[use_buf], " (ready)", &len, HUGE_BUF);
        }
    }

    if (QUERY_FLAG(op, FLAG_IDENTIFIED) || QUERY_FLAG(op, FLAG_APPLIED)) {
        if (QUERY_FLAG(op, FLAG_PERM_DAMNED)) {
            safe_strcat(buf[use_buf], " (perm. damned)", &len, HUGE_BUF);
        } else if (QUERY_FLAG(op, FLAG_DAMNED)) {
            safe_strcat(buf[use_buf], " (damned)", &len, HUGE_BUF);
        } else if (QUERY_FLAG(op, FLAG_PERM_CURSED)) {
            safe_strcat(buf[use_buf], " (perm. cursed)", &len, HUGE_BUF);
        } else if (QUERY_FLAG(op, FLAG_CURSED)) {
            safe_strcat(buf[use_buf], " (cursed)", &len, HUGE_BUF);
        }
    }

    if (QUERY_FLAG(op, FLAG_IS_MAGICAL) && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        safe_strcat(buf[use_buf], " (magical)", &len, HUGE_BUF);
    }

    if (QUERY_FLAG(op, FLAG_APPLIED)) {
        switch (op->type) {
        case BOW:
        case WAND:
        case ROD:
            safe_strcat(buf[use_buf], " (readied)", &len, HUGE_BUF);
            break;

        case WEAPON:
            safe_strcat(buf[use_buf], " (wielded)", &len, HUGE_BUF);
            break;

        case ARMOUR:
        case HELMET:
        case SHIELD:
        case RING:
        case BOOTS:
        case GLOVES:
        case AMULET:
        case GIRDLE:
        case BRACERS:
        case CLOAK:
            safe_strcat(buf[use_buf], " (worn)", &len, HUGE_BUF);
            break;

        case CONTAINER:
            safe_strcat(buf[use_buf], " (active)", &len, HUGE_BUF);
            break;

        case SKILL:
        case SKILL_ITEM:
        default:
            safe_strcat(buf[use_buf], " (applied)", &len, HUGE_BUF);
        }
    }

    if (QUERY_FLAG(op, FLAG_UNPAID)) {
        safe_strcat(buf[use_buf], " (unpaid)", &len, HUGE_BUF);
    }

    return buf[use_buf];
}

/**
 * Queries an object's name, but only includes the name, title (if any)
 * and material information (if any).
 * @param op Object.
 * @return The object's name. */
char *query_material_name(object *op)
{
    static char buf[MAX_BUF];
    size_t len;

    buf[0] = '\0';
    len = 0;

    if (!op->name) {
        return "(null)";
    }

    if (!QUERY_FLAG(op, FLAG_IS_NAMED)) {
        /* Add the item race name */
        if (!IS_LIVE(op) && op->type != BASE_INFO) {
            safe_strcat(buf, item_races[op->item_race], &len, sizeof(buf));
        }

        if (op->material_real && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            safe_strcat(buf, material_real[op->material_real].name, &len, sizeof(buf));
        }
    }

    safe_strcat(buf, op->name, &len, sizeof(buf));

    if (op->title && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        safe_strcat(buf, " ", &len, sizeof(buf));
        safe_strcat(buf, op->title, &len, sizeof(buf));
    }

    return buf;
}

/**
 * Returns a character pointer pointing to a static buffer which contains
 * a verbose textual representation of the name of the given object.
 *
 * The buffer will be overwritten at the next call.
 *
 * This is a lot like query_name(), but we don't include the item
 * count or item status.  Used for inventory sorting and sending to
 * client.
 * @param op Object to get the base name from.
 * @param caller Object calling this.
 * @return The base name of the object. */
char *query_base_name(object *op, object *caller)
{
    static char buf[MAX_BUF];
    char buf2[32];
    size_t len;

    buf[0] = '\0';

    if (op->name == NULL) {
        return "(null)";
    }

    if (!QUERY_FLAG(op, FLAG_IS_NAMED)) {
        /* Add the item race name */
        if (!IS_LIVE(op) && op->type != BASE_INFO) {
            strcpy(buf, item_races[op->item_race]);
        }

        if (op->material_real && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            strcat(buf, material_real[op->material_real].name);
        }
    }

    strcat(buf, op->name);

    /* To speed things up */
    if (!op->weight && !op->title && !is_magical(op)) {
        return buf;
    }

    len = strlen(buf);

    switch (op->type) {
    case CONTAINER:

        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            if (op->title) {
                safe_strcat(buf, " ", &len, sizeof(buf));
                safe_strcat(buf, op->title, &len, sizeof(buf));
            }
        }

        if (op->sub_type >= ST1_CONTAINER_NORMAL_party) {
            if (op->sub_type == ST1_CONTAINER_CORPSE_party) {
                if (op->slaying) {
                    if (!caller || caller->type != PLAYER) {
                        safe_strcat(buf, " (bounty of a party)", &len, sizeof(buf));
                    } else if (CONTR(caller)->party && CONTR(caller)->party->name == op->slaying) {
                        safe_strcat(buf, " (bounty of your party", &len, sizeof(buf));

                        /* A searched bounty */
                        if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                            safe_strcat(buf, ", searched", &len, sizeof(buf));
                        }

                        safe_strcat(buf, ")", &len, sizeof(buf));
                    } else {
                        /* It's a different party */
                        safe_strcat(buf, " (bounty of another party)", &len, sizeof(buf));
                    }
                } else if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                    safe_strcat(buf, " (searched)", &len, sizeof(buf));
                }
            }
        } else if (op->sub_type >= ST1_CONTAINER_NORMAL_player) {
            if (op->sub_type == ST1_CONTAINER_CORPSE_player) {
                if (op->slaying) {
                    safe_strcat(buf, " (bounty of ", &len, sizeof(buf));
                    safe_strcat(buf, op->slaying, &len, sizeof(buf));

                    /* A searched bounty */
                    if ((caller && caller->name == op->slaying) && QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                        safe_strcat(buf, ", searched", &len, sizeof(buf));
                    }

                    safe_strcat(buf, ")", &len, sizeof(buf));
                } else if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                    safe_strcat(buf, " (searched)", &len, sizeof(buf));
                }
            }
        }

        break;

    case SCROLL:
    case WAND:
    case ROD:
    case POTION:

        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            if (!op->title) {
                if (op->stats.sp != SP_NO_SPELL) {
                    safe_strcat(buf, " of ", &len, sizeof(buf));
                    safe_strcat(buf, spells[op->stats.sp].name, &len, sizeof(buf));
                } else {
                    safe_strcat(buf, " of nothing", &len, sizeof(buf));
                }
            } else {
                safe_strcat(buf, " ", &len, sizeof(buf));
                safe_strcat(buf, op->title, &len, sizeof(buf));
            }

            sprintf(buf2, " (lvl %d)", op->level);
            safe_strcat(buf, buf2, &len, sizeof(buf));
        }

        break;

    case SKILL:
    case AMULET:
    case RING:

        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            if (!op->title) {
                /* If ring has a title, full description isn't so useful */
                char *s = describe_item(op);

                if (s[0]) {
                    safe_strcat (buf, " ", &len, sizeof(buf));
                    safe_strcat (buf, s, &len, sizeof(buf));
                }
            } else {
                safe_strcat(buf, " ", &len, sizeof(buf));
                safe_strcat(buf, op->title, &len, sizeof(buf));
            }
        }

        break;

    default:

        if (op->magic && (!need_identify(op) || QUERY_FLAG(op, FLAG_BEEN_APPLIED) || QUERY_FLAG(op, FLAG_IDENTIFIED))) {
            if (!IS_LIVE(op) && op->type != BASE_INFO) {
                sprintf(buf2, " %+d", op->magic);
                safe_strcat(buf, buf2, &len, sizeof(buf));
            }
        }

        if (op->title && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            safe_strcat(buf, " ", &len, sizeof(buf));
            safe_strcat(buf, op->title, &len, sizeof(buf));
        }

        if ((op->type == ARROW || op->type == WEAPON) && op->slaying && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            safe_strcat(buf, " ", &len, sizeof(buf));
            safe_strcat(buf, op->slaying, &len, sizeof(buf));
        }
    }

    return buf;
}

/**
 * Describe terrain flags of a given object.
 * @param op The object.
 * @param retbuf Character buffer to store the described terrains. */
static void describe_terrain(object *op, char *retbuf)
{
    if (op->terrain_flag & TERRAIN_AIRBREATH) {
        strcat(retbuf, "(air breathing)");
    }

    if (op->terrain_flag & TERRAIN_WATERWALK) {
        strcat(retbuf, "(water walking)");
    }

    if (op->terrain_flag & TERRAIN_FIREWALK) {
        strcat(retbuf, "(fire walking)");
    }

    if (op->terrain_flag & TERRAIN_CLOUDWALK) {
        strcat(retbuf, "(cloud walking)");
    }

    if (op->terrain_flag & TERRAIN_WATERBREATH) {
        strcat(retbuf, "(water breathing)");
    }

    if (op->terrain_flag & TERRAIN_FIREBREATH) {
        strcat(retbuf, "(fire breathing)");
    }
}

/**
 * Returns a pointer to a static buffer which contains a
 * description of the given object.
 *
 * If it is a monster, lots of information about its abilities
 * will be returned.
 *
 * If it is an item, lots of information about which abilities
 * will be gained about its user will be returned.
 *
 * If it is a player, it writes out the current abilities
 * of the player, which is usually gained by the items applied.
 *
 * Used to describe <b>every</b> object in the game, including
 * description of every flag, etc.
 * @param op Object that should be described.
 * @return The described information. */
char *describe_item(object *op)
{
    int attr, val, more_info = 0, id_true = 0;
    char buf[MAX_BUF];
    static char retbuf[VERY_BIG_BUF * 3];

    retbuf[0] = '\0';

    /* We start with players */
    if (op->type == PLAYER) {
        describe_terrain(op, retbuf);

        if (CONTR(op)->digestion) {
            if (CONTR(op)->digestion > 0) {
                sprintf(buf, "(sustenance%+d)", CONTR(op)->digestion);
            } else if (CONTR(op)->digestion < 0) {
                sprintf(buf, "(hunger%+d)", -CONTR(op)->digestion);
            }

            strcat(retbuf, buf);
        }

        if (CONTR(op)->gen_client_hp) {
            sprintf(buf, "(hp reg. %3.1f)", (float) CONTR(op)->gen_client_hp / 10);
            strcat(retbuf, buf);
        }

        if (CONTR(op)->gen_client_sp) {
            sprintf(buf, "(mana reg. %3.1f)", (float) CONTR(op)->gen_client_sp / 10);
            strcat(retbuf, buf);
        }
    } else if (QUERY_FLAG(op, FLAG_MONSTER)) {
        /* And then monsters */

        describe_terrain(op, retbuf);

        if (QUERY_FLAG(op, FLAG_UNDEAD)) {
            strcat(retbuf, "(undead)");
        }

        if (QUERY_FLAG(op, FLAG_CAN_PASS_THRU)) {
            strcat(retbuf, "(pass through doors)");
        }

        if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) {
            strcat(retbuf, "(see invisible)");
        }

        if (QUERY_FLAG(op, FLAG_USE_WEAPON)) {
            strcat(retbuf, "(wield weapon)");
        }

        if (QUERY_FLAG(op, FLAG_USE_BOW)) {
            strcat(retbuf, "(archer)");
        }

        if (QUERY_FLAG(op, FLAG_USE_ARMOUR)) {
            strcat(retbuf, "(wear armour)");
        }

        if (QUERY_FLAG(op, FLAG_CAST_SPELL)) {
            strcat(retbuf, "(spellcaster)");
        }

        if (QUERY_FLAG(op, FLAG_FRIENDLY)) {
            strcat(retbuf, "(friendly)");
        }

        if (QUERY_FLAG(op, FLAG_UNAGGRESSIVE)) {
            strcat(retbuf, "(unaggressive)");
        }

        if (QUERY_FLAG(op, FLAG_HITBACK)) {
            strcat(retbuf, "(hitback)");
        }

        if (FABS(op->speed) > MIN_ACTIVE_SPEED) {
            switch ((int) ((FABS(op->speed)) * 15)) {
            case 0:
                strcat(retbuf, "(very slow movement)");
                break;

            case 1:
                strcat(retbuf, "(slow movement)");
                break;

            case 2:
                strcat(retbuf, "(normal movement)");
                break;

            case 3:
            case 4:
                strcat(retbuf, "(fast movement)");
                break;

            case 5:
            case 6:
                strcat(retbuf, "(very fast movement)");
                break;

            case 7:
            case 8:
            case 9:
            case 10:
                strcat(retbuf, "(extremely fast movement)");
                break;

            default:
                strcat(retbuf, "(lightning fast movement)");
                break;
            }
        }
    } else {
        /* Here we handle items */

        /* We only need calculate this once */
        if (QUERY_FLAG(op, FLAG_IDENTIFIED) || QUERY_FLAG(op, FLAG_BEEN_APPLIED) || !need_identify(op)) {
            id_true = 1;
        }

        /* We only need to show the full details of an item if it is identified
         * */
        if (id_true) {
            /* Terrain flags have no double use... If valid, show them */
            if (op->terrain_type) {
                describe_terrain(op, retbuf);
            }

            /* Deal with special cases */
            switch (op->type) {
            case WAND:
            case ROD:
                sprintf(buf, "(delay%+2.1fs)", ((float) op->last_grace / MAX_TICKS));
                strcat(retbuf, buf);
                break;

                /* Armour type objects */
            case ARMOUR:
            case HELMET:
            case SHIELD:
            case BOOTS:
            case GLOVES:
            case GIRDLE:
            case BRACERS:
            case CLOAK:

                if (ARMOUR_SPEED(op)) {
                    sprintf(buf, "(speed cap %1.2f)", ARMOUR_SPEED(op) / 10.0);
                    strcat(retbuf, buf);
                }

                /* Do this in all cases - otherwise it gets confusing - does
                 * that
                 * item have no penalty, or is it not fully identified for
                 * example. */
                if (ARMOUR_SPELLS(op)) {
                    sprintf(buf, "(armour mana reg %d)", -1 * ARMOUR_SPELLS(op));
                    strcat(retbuf, buf);
                }

            case WEAPON:
            case RING:
            case AMULET:
            case FORCE:
                more_info = 1;

            case BOW:
            case ARROW:

                if (op->type == BOW) {
                    sprintf(buf, "(delay%+2.1fs)", ((float) op->stats.sp / MAX_TICKS));
                    strcat(retbuf, buf);
                } else if (op->type == ARROW) {
                    sprintf(buf, "(delay%+2.1fs)", ((float) op->last_grace / MAX_TICKS));
                    strcat(retbuf, buf);
                }

                if (op->last_sp && !IS_ARMOR(op)) {
                    sprintf(buf, "(range%+d)", op->last_sp);
                    strcat(retbuf, buf);
                }

                if (op->stats.wc) {
                    sprintf(buf, "(wc%+d)", op->stats.wc);
                    strcat(retbuf, buf);
                }

                if (op->stats.dam) {
                    sprintf(buf, "(dam%+d)", op->stats.dam);
                    strcat(retbuf, buf);
                }

                if (op->stats.ac) {
                    sprintf(buf, "(ac%+d)", op->stats.ac);
                    strcat(retbuf, buf);
                }

                if (op->type == WEAPON) {
                    sprintf(buf, "(%3.2f sec)", ((float) op->last_grace / MAX_TICKS));
                    strcat(retbuf, buf);

                    if (op->level > 0) {
                        sprintf(buf, "(improved %d/%d)", op->last_eat, op->level);
                        strcat(retbuf, buf);
                    }
                }

                break;

            case FOOD:
            case FLESH:
            case DRINK:
            {
                int curse_multiplier = 1;

                sprintf(buf, "(food%s%d)", op->stats.food >= 0 ? "+" : "", op->stats.food);
                strcat(retbuf, buf);

                if (QUERY_FLAG(op, FLAG_CURSED)) {
                    curse_multiplier = 2;
                }

                if (QUERY_FLAG(op, FLAG_DAMNED)) {
                    curse_multiplier = 3;
                }

                if (op->stats.hp) {
                    snprintf(buf, sizeof(buf), "(hp%s%d)", curse_multiplier == 1 ? "+" : "", op->stats.hp * curse_multiplier);
                    strcat(retbuf, buf);
                }

                if (op->stats.sp) {
                    snprintf(buf, sizeof(buf), "(mana%s%d)", curse_multiplier == 1 ? "+" : "", op->stats.sp * curse_multiplier);
                    strcat(retbuf, buf);
                }

                break;
            }

            case POTION:

                if (op->last_sp) {
                    sprintf(buf, "(range%+d)", op->last_sp);
                    strcat(retbuf, buf);
                }

                break;

            case BOOK:

                if (op->level) {
                    sprintf(buf, "(lvl %d)", op->level);
                    strcat(retbuf, buf);
                }

                if (op->msg) {
                    if (QUERY_FLAG(op, FLAG_NO_SKILL_IDENT)) {
                        strcat(retbuf, "(read)");
                    } else {
                        strcat(retbuf, "(unread)");
                    }
                }

            default:
                return retbuf;
            }

            /* These count for every "normal" item player deals with - mostly
             * equipment */
            for (attr = 0; attr < NUM_STATS; attr++) {
                if ((val = get_attr_value(&(op->stats), attr)) != 0) {
                    sprintf(buf, "(%s%+d)", short_stat_name[attr], val);
                    strcat(retbuf, buf);
                }
            }
        }
    }

    /* Some special info for some identified items */
    if (id_true && more_info) {
        if (op->stats.sp) {
            sprintf(buf, "(mana reg.%+3.1f)", 0.4f * op->stats.sp);
            strcat(retbuf, buf);
        }

        if (op->stats.hp) {
            sprintf(buf, "(hp reg.%+3.1f)", 0.4f * op->stats.hp);
            strcat(retbuf, buf);
        }

        if (op->stats.food) {
            if (op->stats.food > 0) {
                sprintf(buf, "(sustenance%+d)", op->stats.food);
            } else if (op->stats.food < 0) {
                sprintf(buf, "(hunger%+d)", -op->stats.food);
            }

            strcat(retbuf, buf);
        }

        if (op->stats.exp) {
            sprintf(buf, "(speed %+"PRId64 ")", op->stats.exp);
            strcat(retbuf, buf);
        }
    }

    /* Here we deal with all the special flags */
    if (id_true || QUERY_FLAG(op, FLAG_MONSTER) || op->type == PLAYER) {
        if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) {
            strcat(retbuf, "(see invisible)");
        }

        if (QUERY_FLAG(op, FLAG_MAKE_ETHEREAL)) {
            strcat(retbuf, "(makes ethereal)");
        }

        if (QUERY_FLAG(op, FLAG_IS_ETHEREAL)) {
            strcat(retbuf, "(ethereal)");
        }

        if (QUERY_FLAG(op, FLAG_MAKE_INVISIBLE)) {
            strcat(retbuf, "(makes invisible)");
        }

        if (QUERY_FLAG(op, FLAG_IS_INVISIBLE)) {
            strcat(retbuf, "(invisible)");
        }

        if (QUERY_FLAG(op, FLAG_XRAYS)) {
            strcat(retbuf, "(xray-vision)");
        }

        if (QUERY_FLAG(op, FLAG_SEE_IN_DARK)) {
            strcat(retbuf, "(infravision)");
        }

        if (QUERY_FLAG(op, FLAG_LIFESAVE)) {
            strcat(retbuf, "(lifesaving)");
        }

        if (QUERY_FLAG(op, FLAG_REFL_SPELL)) {
            strcat(retbuf, "(reflect spells)");
        }

        if (QUERY_FLAG(op, FLAG_REFL_MISSILE)) {
            strcat(retbuf, "(reflect missiles)");
        }

        if (QUERY_FLAG(op, FLAG_STEALTH)) {
            strcat(retbuf, "(stealth)");
        }

        if (QUERY_FLAG(op, FLAG_FLYING)) {
            strcat(retbuf, "(levitate)");
        }
    }

    if (id_true) {
        if (op->slaying != NULL) {
            strcat(retbuf, "(slay ");
            strcat(retbuf, op->slaying);
            strcat(retbuf, ")");
        }

        strcat(retbuf, describe_attack(op, 0));
        strcat(retbuf, describe_protections(op, 0));

        DESCRIBE_PATH(retbuf, op->path_attuned, "Attuned");
        DESCRIBE_PATH(retbuf, op->path_repelled, "Repelled");
        DESCRIBE_PATH(retbuf, op->path_denied, "Denied");

        if (op->stats.maxhp && (op->type != ROD && op->type != WAND)) {
            sprintf(buf, "(hp%+d)", op->stats.maxhp);
            strcat(retbuf, buf);
        }

        if (op->stats.maxsp) {
            sprintf(buf, "(mana%+d)", op->stats.maxsp);
            strcat(retbuf, buf);
        }

        if (op->item_power) {
            sprintf(buf, "(item power%+d)", op->item_power);
            strcat(retbuf, buf);
        }
    }

    return retbuf;
}

/**
 * Checks if given object should need identification.
 * @param op Object to check.
 * @return 1 if this object needs identification, 0 otherwise. */
int need_identify(object *op)
{
    switch (op->type) {
    case RING:
    case WAND:
    case ROD:
    case SCROLL:
    case FOOD:
    case POTION:
    case BOW:
    case ARROW:
    case WEAPON:
    case ARMOUR:
    case SHIELD:
    case HELMET:
    case AMULET:
    case BOOTS:
    case GLOVES:
    case BRACERS:
    case GIRDLE:
    case CONTAINER:
    case DRINK:
    case FLESH:
    case INORGANIC:
    case CLOAK:
    case GEM:
    case JEWEL:
    case NUGGET:
    case PEARL:
    case POWER_CRYSTAL:
    case BOOK:
    case LIGHT_APPLY:
    case LIGHT_REFILL:
        return 1;
    }

    return 0;
}

/**
 * Identify an object. Basically sets FLAG_IDENTIFIED on the object along
 * with other things.
 * @param op Object to identify. */
void identify(object *op)
{
    if (!op) {
        return;
    }

    SET_FLAG(op, FLAG_IDENTIFIED);

    /* The shop identifies items before they hit the ground */
    if (op->map) {
        update_object(op, UP_OBJ_FACE);
    } else {
        esrv_send_item(op);
    }
}

/**
 * Check if an object marked with FLAG_IS_TRAPPED still has a known trap
 * in it.
 * @param op The object to check. */
void set_trapped_flag(object *op)
{
    object *tmp;
    uint32_t flag;

    if (!op) {
        return;
    }

    /* Player and monsters are not marked. */
    if (op->type == PLAYER || op->type == MONSTER) {
        return;
    }

    flag = QUERY_FLAG(op, FLAG_IS_TRAPPED);
    CLEAR_FLAG(op, FLAG_IS_TRAPPED);

    for (tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        /* Must be a rune AND visible */
        if (tmp->type == RUNE && tmp->stats.Cha <= 1) {
            SET_FLAG(op, FLAG_IS_TRAPPED);
            return;
        }
    }

    if (QUERY_FLAG(op, FLAG_IS_TRAPPED) != flag) {
        if (op->env) {
            esrv_update_item(UPD_FLAGS, op);
        } else {
            update_object(op, UP_OBJ_FACE);
        }
    }
}
