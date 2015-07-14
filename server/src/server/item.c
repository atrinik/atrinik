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
    "",
    "",
    "two ",
    "three ",
    "four ",
    "five ",
    "six ",
    "seven ",
    "eight ",
    "nine ",
    "ten ",
    "eleven ",
    "twelve ",
    "thirteen ",
    "fourteen ",
    "fifteen ",
    "sixteen ",
    "seventeen ",
    "eighteen ",
    "nineteen ",
    "twenty "
};

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
 * Returns the text representation of the given number in a static buffer.
 *
 * The buffer might be overwritten with the next call.
 *
 * It is currently only used by the query_short_name() function.
 * @param i The number.
 * @return Text representation of the given number.
 */
static char *get_number(int i)
{
    if (i <= 20) {
        return numbers[i];
    } else {
        static char buf[MAX_BUF];
        snprintf(VS(buf), "%d", i);
        return buf;
    }
}

/**
 * Builds a textual representation of the object's material (if applicable).
 * @param op Object to get the material of.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object material.
 */
StringBuffer *object_get_material(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    /* Named objects do not show the material name. */
    if (QUERY_FLAG(op, FLAG_IS_NAMED)) {
        return sb;
    }

    if (!IS_LIVE(op) && op->type != BASE_INFO &&
            op->item_race < NROF_ITEM_RACES) {
        stringbuffer_append_string(sb, item_races[op->item_race]);
    }

    if (op->material_real > 0 && op->material_real < NUM_MATERIALS_REAL &&
            QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        stringbuffer_append_string(sb, material_real[op->material_real].name);
    }

    return sb;
}

/**
 * Builds a textual representation of the object's title (if applicable).
 * @param op Object to get the title of.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object title.
 */
StringBuffer *object_get_title(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    switch (op->type) {
    case CONTAINER:
        if (op->title != NULL && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            stringbuffer_append_printf(sb, " %s", op->title);
        }

        if (op->sub_type >= ST1_CONTAINER_NORMAL_party) {
            if (op->sub_type == ST1_CONTAINER_CORPSE_party) {
                if (op->slaying != NULL) {
                    if (caller == NULL || caller->type != PLAYER) {
                        stringbuffer_append_string(sb, " (bounty of a party)");
                    } else if (CONTR(caller)->party != NULL &&
                            CONTR(caller)->party->name == op->slaying) {
                        stringbuffer_append_string(sb,
                                " (bounty of your party");

                        /* A searched bounty */
                        if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                            stringbuffer_append_string(sb, ", searched");

                            if (op->inv == NULL) {
                                stringbuffer_append_string(sb, ", empty");
                            }
                        }

                        stringbuffer_append_string(sb, ")");
                    } else {
                        /* It's a different party */
                        stringbuffer_append_string(sb,
                                " (bounty of another party)");
                    }
                } else if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                    stringbuffer_append_string(sb, " (searched");

                    if (op->inv == NULL) {
                        stringbuffer_append_string(sb, ", empty");
                    }

                    stringbuffer_append_string(sb, ")");
                }
            }
        } else if (op->sub_type >= ST1_CONTAINER_NORMAL_player) {
            if (op->sub_type == ST1_CONTAINER_CORPSE_player) {
                if (op->slaying != NULL) {
                    stringbuffer_append_printf(sb, " (bounty of %s",
                            op->slaying);

                    /* A searched bounty */
                    if ((caller != NULL && caller->name == op->slaying) &&
                            QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                        stringbuffer_append_string(sb, ", searched");

                        if (op->inv == NULL) {
                            stringbuffer_append_string(sb, ", empty");
                        }
                    }

                    stringbuffer_append_string(sb, ")");
                } else if (QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
                    stringbuffer_append_string(sb, " (searched");

                    if (op->inv == NULL) {
                        stringbuffer_append_string(sb, ", empty");
                    }

                    stringbuffer_append_string(sb, ")");
                }
            }
        }

        break;

    case SCROLL:
    case WAND:
    case ROD:
    case POTION:
    case BOOK_SPELL:
        if (QUERY_FLAG(op, FLAG_IDENTIFIED) ||
                QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
            if (op->title == NULL) {
                stringbuffer_append_string(sb, " of ");

                if (op->stats.sp >= 0 && op->stats.sp < NROFREALSPELLS) {
                    stringbuffer_append_string(sb, spells[op->stats.sp].name);
                } else {
                    stringbuffer_append_string(sb, "nothing");
                }
            } else {
                stringbuffer_append_printf(sb, " %s", op->title);
            }

            if (op->type != BOOK_SPELL) {
                stringbuffer_append_printf(sb, " (lvl %" PRId8 ")", op->level);
            }
        }

        break;

    case SKILL:
    case AMULET:
    case RING:
        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            /* If ring has a title, full description isn't so useful. */
            if (op->title == NULL) {
                stringbuffer_append_string(sb, " ");
                size_t len = stringbuffer_length(sb);
                sb = object_get_description(op, caller, NULL);
                if (stringbuffer_length(sb) == len) {
                    stringbuffer_seek(sb, len - 1);
                }
            } else {
                stringbuffer_append_printf(sb, " %s", op->title);
            }
        }

        break;

    default:
        if (op->magic != 0 && (!need_identify(op) ||
                QUERY_FLAG(op, FLAG_BEEN_APPLIED) ||
                QUERY_FLAG(op, FLAG_IDENTIFIED))) {
            if (!IS_LIVE(op) && op->type != BASE_INFO) {
                stringbuffer_append_printf(sb, " %+" PRId8, op->magic);
            }
        }

        if (op->title != NULL && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            stringbuffer_append_printf(sb, " %s", op->title);
        }

        if ((op->type == ARROW || op->type == WEAPON) && op->slaying != NULL &&
                QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            stringbuffer_append_printf(sb, " %s", op->slaying);
        }
    }

    return sb;
}

/**
 * Builds a verbose textual representation of the name of the given object.
 * @param op Object to get the name of.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object name.
 */
StringBuffer *object_get_name(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    sb = object_get_short_name(op, caller, sb);

    if (QUERY_FLAG(op, FLAG_ONE_DROP)) {
        stringbuffer_append_string(sb, " (one-drop)");
    } else if (QUERY_FLAG(op, FLAG_QUEST_ITEM)) {
        stringbuffer_append_string(sb, " (quest)");
    }

    if (QUERY_FLAG(op, FLAG_INV_LOCKED)) {
        stringbuffer_append_string(sb, " *");
    }

    if (op->type == CONTAINER && QUERY_FLAG(op, FLAG_APPLIED)) {
        if (op->attacked_by != NULL && op->attacked_by->type == PLAYER) {
            stringbuffer_append_string(sb, " (open)");
        } else {
            stringbuffer_append_string(sb, " (ready)");
        }
    }

    if (QUERY_FLAG(op, FLAG_IDENTIFIED) || QUERY_FLAG(op, FLAG_APPLIED)) {
        if (QUERY_FLAG(op, FLAG_PERM_DAMNED)) {
            stringbuffer_append_string(sb, " (perm. damned)");
        } else if (QUERY_FLAG(op, FLAG_DAMNED)) {
            stringbuffer_append_string(sb, " (damned)");
        } else if (QUERY_FLAG(op, FLAG_PERM_CURSED)) {
            stringbuffer_append_string(sb, " (perm. cursed)");
        } else if (QUERY_FLAG(op, FLAG_CURSED)) {
            stringbuffer_append_string(sb, " (cursed)");
        }
    }

    if (QUERY_FLAG(op, FLAG_IS_MAGICAL) && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        stringbuffer_append_string(sb, " (magical)");
    }

    if (QUERY_FLAG(op, FLAG_APPLIED)) {
        switch (op->type) {
        case BOW:
        case WAND:
        case ROD:
            stringbuffer_append_string(sb, " (readied)");
            break;

        case WEAPON:
            stringbuffer_append_string(sb, " (wielded)");
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
        case PANTS:
            stringbuffer_append_string(sb, " (worn)");
            break;

        case CONTAINER:
            stringbuffer_append_string(sb, " (active)");
            break;

        case SKILL:
        case SKILL_ITEM:
        default:
            stringbuffer_append_string(sb, " (applied)");
        }
    }

    if (QUERY_FLAG(op, FLAG_UNPAID)) {
        stringbuffer_append_string(sb, " (unpaid)");
    }

    return sb;
}

/**
 * Like object_get_name(), but object status information (eg, worn/cursed/etc)
 * is not included.
 * @param op Object to get the name of.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object name.
 */
StringBuffer *object_get_short_name(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    if (op->name == NULL) {
        return sb;
    }

    if (op->nrof != 0) {
        if (op->nrof <= 20) {
            stringbuffer_append_string(sb, numbers[op->nrof]);
        } else {
            stringbuffer_append_printf(sb, "%" PRIu32 " ", op->nrof);
        }
    }

    sb = object_get_material(op, caller, sb);
    stringbuffer_append_string(sb, op->name);
    sb = object_get_title(op, caller, sb);

    return sb;
}

/**
 * Builds an object's name, but only includes the name, title (if any)
 * and material information (if any).
 * @param op Object to get the name of.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object name.
 */
StringBuffer *object_get_material_name(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    sb = object_get_material(op, caller, sb);
    stringbuffer_append_string(sb, op->name);

    if (op->title != NULL && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        stringbuffer_append_printf(sb, " %s", op->title);
    }

    return sb;
}


/**
 * Like object_get_name(), but neither object count nor object status
 * information is included.
 * @param op Object to get the name of.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object name.
 */
StringBuffer *object_get_base_name(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    sb = object_get_material(op, caller, sb);
    stringbuffer_append_string(sb, op->name);
    sb = object_get_title(op, caller, sb);

    return sb;
}

/**
 * Builds a description of the object's terrain flags.
 * @param op Object's terrain to describe.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object terrain.
 */
StringBuffer *object_get_description_terrain(const object *op,
        const object *caller, StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    if (op->terrain_flag == 0) {
        return sb;
    }

    stringbuffer_append_string(sb, "(");
    size_t old_len = stringbuffer_length(sb);

    if (op->terrain_flag & TERRAIN_AIRBREATH) {
        stringbuffer_append_string(sb, "air breathing, ");
    }

    if (op->terrain_flag & TERRAIN_WATERWALK) {
        stringbuffer_append_string(sb, "water walking, ");
    }

    if (op->terrain_flag & TERRAIN_FIREWALK) {
        stringbuffer_append_string(sb, "fire walking, ");
    }

    if (op->terrain_flag & TERRAIN_CLOUDWALK) {
        stringbuffer_append_string(sb, "cloud walking, ");
    }

    if (op->terrain_flag & TERRAIN_WATERBREATH) {
        stringbuffer_append_string(sb, "water breathing, ");
    }

    if (op->terrain_flag & TERRAIN_FIREBREATH) {
        stringbuffer_append_string(sb, "fire breathing, ");
    }

    if (op->terrain_flag & TERRAIN_WATER_SHALLOW) {
        stringbuffer_append_string(sb, "shallow water walking, ");
    }

    size_t len = stringbuffer_length(sb);
    if (len - old_len >= 2) {
        stringbuffer_seek(sb, len - 2);
    } else {
        stringbuffer_append_string(sb, "unknown terrain");
    }

    stringbuffer_append_string(sb, ") ");

    return sb;
}

/**
 * Builds a description of the object's attack types.
 * @param op Object's attack types to describe.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object attack types.
 */
StringBuffer *object_get_description_attacks(const object *op,
        const object *caller, StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    bool got_one = false;
    for (int i = 0; i < NROFATTACKS; i++) {
        if (op->attack[i] == 0) {
            continue;
        }

        if (!got_one) {
            stringbuffer_append_string(sb, "(Attacks: ");
        }

        if (got_one) {
            stringbuffer_append_string(sb, ", ");
        }

        stringbuffer_append_printf(sb, "%s +%" PRIu8 "%%", attack_name[i],
                op->attack[i]);
        got_one = true;
    }

    if (got_one) {
        stringbuffer_append_string(sb, ") ");
    }

    return sb;
}

/**
 * Builds a description of the object's protections.
 * @param op Object's protections to describe.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the object protections.
 */
StringBuffer *object_get_description_protections(const object *op,
        const object *caller, StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    bool got_one = false;
    for (int i = 0; i < NROFATTACKS; i++) {
        if (op->protection[i] == 0) {
            continue;
        }

        if (!got_one) {
            stringbuffer_append_string(sb, "(Protections: ");
        }

        if (got_one) {
            stringbuffer_append_string(sb, ", ");
        }

        stringbuffer_append_printf(sb, "%s %+" PRId8 "%%", attack_name[i],
                op->protection[i]);
        got_one = true;
    }

    if (got_one) {
        stringbuffer_append_string(sb, ") ");
    }

    return sb;
}

/**
 * Builds a description of the object's spell path.
 * @param op Object's spell path to describe.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @param path The spell path flags.
 * @param name Name of the spell path.
 * @return StringBuffer instance that contains the spell path description.
 */
StringBuffer *object_get_description_path(const object *op, const
        object *caller, StringBuffer *sb, const uint32_t path, const char *name)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    if (path == 0) {
        return sb;
    }

    stringbuffer_append_printf(sb, "(%s: ", name);

    bool got_one = false;
    for (int i = 0; i < NRSPELLPATHS; i++) {
        if (!(path & (1 << i))) {
            continue;
        }

        if (got_one) {
            stringbuffer_append_string(sb, ", ");
        } else {
            got_one = true;
        }

        stringbuffer_append_string(sb, spellpathnames[i]);
    }

    stringbuffer_append_string(sb, ") ");

    return sb;
}

/**
 * Builds a description of the given object.
 *
 * If the object is a monster, lots of information about its abilities will be
 * generated.
 *
 * If it is an item, lots of information about which abilities will be gained
 * about its use will be generated.
 *
 * If it is a player, generates the current abilities of the player, which are
 * usually gained by the items applied.
 * @param op Object that should be described.
 * @param caller Object calling this. Can be NULL.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the description.
 */
StringBuffer *object_get_description(const object *op, const object *caller,
        StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    if (sb == NULL) {
        sb = stringbuffer_new();
    }

    size_t old_len = stringbuffer_length(sb);
    bool identified = false, more_info = false;

    if (op->type == PLAYER) {
        sb = object_get_description_terrain(op, caller, sb);

        if (CONTR(op)->digestion != 0) {
            if (CONTR(op)->digestion > 0) {
                stringbuffer_append_printf(sb, "(sustenance%+d) ",
                        CONTR(op)->digestion);
            } else {
                stringbuffer_append_printf(sb, "(hunger%+d) ",
                        -CONTR(op)->digestion);
            }
        }

        if (CONTR(op)->gen_client_hp != 0) {
            stringbuffer_append_printf(sb, "(hp reg. %3.1f) ",
                    CONTR(op)->gen_client_hp / 10.0);
        }

        if (CONTR(op)->gen_client_sp != 0) {
            stringbuffer_append_printf(sb, "(mana reg. %3.1f) ",
                    CONTR(op)->gen_client_sp / 10.0);
        }
    } else if (QUERY_FLAG(op, FLAG_MONSTER)) {
        sb = object_get_description_terrain(op, caller, sb);

        if (QUERY_FLAG(op, FLAG_UNDEAD)) {
            stringbuffer_append_string(sb, "(undead) ");
        }

        if (QUERY_FLAG(op, FLAG_CAN_PASS_THRU)) {
            stringbuffer_append_string(sb, "(pass through doors) ");
        }

        if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) {
            stringbuffer_append_string(sb, "(see invisible) ");
        }

        if (QUERY_FLAG(op, FLAG_USE_WEAPON)) {
            stringbuffer_append_string(sb, "(wield weapon) ");
        }

        if (QUERY_FLAG(op, FLAG_USE_BOW)) {
            stringbuffer_append_string(sb, "(archer) ");
        }

        if (QUERY_FLAG(op, FLAG_USE_ARMOUR)) {
            stringbuffer_append_string(sb, "(wear armour) ");
        }

        if (QUERY_FLAG(op, FLAG_CAST_SPELL)) {
            stringbuffer_append_string(sb, "(spellcaster) ");
        }

        if (QUERY_FLAG(op, FLAG_FRIENDLY)) {
            stringbuffer_append_string(sb, "(friendly) ");
        }

        if (QUERY_FLAG(op, FLAG_UNAGGRESSIVE)) {
            stringbuffer_append_string(sb, "(unaggressive) ");
        }

        if (QUERY_FLAG(op, FLAG_HITBACK)) {
            stringbuffer_append_string(sb, "(hitback) ");
        }

        if (FABS(op->speed) > MIN_ACTIVE_SPEED) {
            switch ((int) ((FABS(op->speed)) * 15)) {
            case 0:
                stringbuffer_append_string(sb, "(very slow movement) ");
                break;

            case 1:
                stringbuffer_append_string(sb, "(slow movement) ");
                break;

            case 2:
                stringbuffer_append_string(sb, "(normal movement) ");
                break;

            case 3:
            case 4:
                stringbuffer_append_string(sb, "(fast movement) ");
                break;

            case 5:
            case 6:
                stringbuffer_append_string(sb, "(very fast movement) ");
                break;

            case 7:
            case 8:
            case 9:
            case 10:
                stringbuffer_append_string(sb, "(extremely fast movement) ");
                break;

            default:
                stringbuffer_append_string(sb, "(lightning fast movement) ");
                break;
            }
        }
    } else {
        /* We only need calculate this once */
        if (QUERY_FLAG(op, FLAG_IDENTIFIED) ||
                QUERY_FLAG(op, FLAG_BEEN_APPLIED) ||
                !need_identify(op)) {
            identified = true;
        }

        if (identified) {
            sb = object_get_description_terrain(op, caller, sb);

            /* Deal with special cases */
            switch (op->type) {
            case WAND:
            case ROD:
                stringbuffer_append_printf(sb, "(delay%+2.1fs) ",
                        (double) op->last_grace / MAX_TICKS);
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
            case PANTS:
                if (ARMOUR_SPEED(op) != 0) {
                    stringbuffer_append_printf(sb, "(speed cap %1.2f) ",
                            ARMOUR_SPEED(op) / 10.0);
                }

                if (ARMOUR_SPELLS(op) != 0) {
                    stringbuffer_append_printf(sb, "(armour mana reg %d) ",
                            -ARMOUR_SPELLS(op));
                }

            case WEAPON:
            case RING:
            case AMULET:
            case FORCE:
                more_info = 1;

            case BOW:
            case ARROW:
                if (op->type == BOW) {
                    stringbuffer_append_printf(sb, "(delay%+2.1fs) ",
                            (double) op->stats.sp / MAX_TICKS);
                } else if (op->type == ARROW) {
                    stringbuffer_append_printf(sb, "(delay%+2.1fs) ",
                            (double) op->last_grace / MAX_TICKS);
                }

                if (op->last_sp != 0 && !IS_ARMOR(op)) {
                    stringbuffer_append_printf(sb, "(range%+d) ", op->last_sp);
                }

                if (op->stats.wc != 0) {
                    stringbuffer_append_printf(sb, "(wc%+d) ", op->stats.wc);
                }

                if (op->stats.dam != 0) {
                    stringbuffer_append_printf(sb, "(dam%+d) ", op->stats.dam);
                }

                if (op->stats.ac != 0) {
                    stringbuffer_append_printf(sb, "(ac%+d) ", op->stats.ac);
                }

                if (op->type == WEAPON) {
                    stringbuffer_append_printf(sb, "(%3.2f sec) ",
                            (double) op->last_grace / MAX_TICKS);

                    if (op->level > 0) {
                        stringbuffer_append_printf(sb, "(improved %d/%d) ",
                                op->last_eat, op->level);
                    }
                }

                break;

            case FOOD:
            case FLESH:
            case DRINK:
            {
                stringbuffer_append_printf(sb, "(food%+d) ", op->stats.food);

                if (op->stats.hp != 0) {
                    stringbuffer_append_printf(sb, "(hp%+d) ", op->stats.hp);
                }

                if (op->stats.sp != 0) {
                    stringbuffer_append_printf(sb, "(mana%+d) ", op->stats.sp);
                }

                break;
            }

            case POTION:
                if (op->last_sp) {
                    stringbuffer_append_printf(sb, "(range%+d) ", op->last_sp);
                }

                break;

            case BOOK:
                if (op->level) {
                    stringbuffer_append_printf(sb, "(lvl %d) ", op->level);
                }

                if (op->msg != NULL) {
                    if (QUERY_FLAG(op, FLAG_NO_SKILL_IDENT)) {
                        stringbuffer_append_string(sb, "(read) ");
                    } else {
                        stringbuffer_append_string(sb, "(unread) ");
                    }
                }

            default:
                return sb;
            }

            for (int attr = 0; attr < NUM_STATS; attr++) {
                int8_t val = get_attr_value(&op->stats, attr);
                if (val != 0) {
                    stringbuffer_append_printf(sb, "(%s%+" PRId8 ") ",
                            short_stat_name[attr], val);
                }
            }
        }
    }

    /* Some special info for some identified items */
    if (identified && more_info) {
        if (op->stats.sp != 0) {
            stringbuffer_append_printf(sb, "(mana reg.%+3.1f) ",
                    op->stats.sp * 0.4);
        }

        if (op->stats.hp != 0) {
            stringbuffer_append_printf(sb, "(hp reg.%+3.1f) ",
                    op->stats.hp * 0.4);
        }

        if (op->stats.food != 0) {
            if (op->stats.food > 0) {
                stringbuffer_append_printf(sb, "(sustenance%+d) ",
                        op->stats.food);
            } else {
                stringbuffer_append_printf(sb, "(hunger%+d) ",
                        -op->stats.food);
            }
        }

        if (op->stats.exp != 0) {
            stringbuffer_append_printf(sb, "(speed%+" PRId64 ") ",
                    op->stats.exp);
        }
    }

    /* Here we deal with all the special flags */
    if (identified || QUERY_FLAG(op, FLAG_MONSTER) || op->type == PLAYER) {
        if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) {
            stringbuffer_append_string(sb, "(see invisible) ");
        }

        if (QUERY_FLAG(op, FLAG_MAKE_ETHEREAL)) {
            stringbuffer_append_string(sb, "(makes ethereal) ");
        }

        if (QUERY_FLAG(op, FLAG_IS_ETHEREAL)) {
            stringbuffer_append_string(sb, "(ethereal) ");
        }

        if (QUERY_FLAG(op, FLAG_MAKE_INVISIBLE)) {
            stringbuffer_append_string(sb, "(makes invisible) ");
        }

        if (QUERY_FLAG(op, FLAG_IS_INVISIBLE)) {
            stringbuffer_append_string(sb, "(invisible) ");
        }

        if (QUERY_FLAG(op, FLAG_XRAYS)) {
            stringbuffer_append_string(sb, "(xray-vision) ");
        }

        if (QUERY_FLAG(op, FLAG_SEE_IN_DARK)) {
            stringbuffer_append_string(sb, "(infravision) ");
        }

        if (QUERY_FLAG(op, FLAG_LIFESAVE)) {
            stringbuffer_append_string(sb, "(lifesaving) ");
        }

        if (QUERY_FLAG(op, FLAG_REFL_SPELL)) {
            stringbuffer_append_string(sb, "(reflect spells) ");
        }

        if (QUERY_FLAG(op, FLAG_REFL_MISSILE)) {
            stringbuffer_append_string(sb, "(reflect missiles) ");
        }

        if (QUERY_FLAG(op, FLAG_STEALTH)) {
            stringbuffer_append_string(sb, "(stealth) ");
        }

        if (QUERY_FLAG(op, FLAG_FLYING)) {
            stringbuffer_append_string(sb, "(flying) ");
        }
    }

    if (identified) {
        if (op->slaying != NULL) {
            stringbuffer_append_printf(sb, "(slay %s) ", op->slaying);
        }

        sb = object_get_description_attacks(op, caller, sb);
        sb = object_get_description_protections(op, caller, sb);

        sb = object_get_description_path(op, caller, sb, op->path_attuned,
                "Attuned");
        sb = object_get_description_path(op, caller, sb, op->path_repelled,
                "Repelled");
        sb = object_get_description_path(op, caller, sb, op->path_denied,
                "Denied");

        if (op->stats.maxhp != 0 && op->type != ROD && op->type != WAND) {
            stringbuffer_append_printf(sb, "(hp%+d) ", op->stats.maxhp);
        }

        if (op->stats.maxsp != 0) {
            stringbuffer_append_printf(sb, "(mana%+d) ", op->stats.maxsp);
        }

        if (op->item_power != 0) {
            stringbuffer_append_printf(sb, "(item power%+d) ", op->item_power);
        }
    }

    size_t len = stringbuffer_length(sb);
    if (len > old_len) {
        stringbuffer_seek(sb, len - 1);
    }

    return sb;
}

/**
 * Get object's name using object_get_name(), along with its description
 * (if any).
 * @param tmp Object to get description of.
 * @param caller Caller.
 * @param sb StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return StringBuffer instance that contains the description.
 */
StringBuffer *object_get_name_description(const object *op,
        const object *caller, StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);

    sb = object_get_name(op, caller, sb);

    switch (op->type) {
    case RING:
    case SKILL:
    case WEAPON:
    case ARMOUR:
    case BRACERS:
    case HELMET:
    case PANTS:
    case SHIELD:
    case BOOTS:
    case GLOVES:
    case AMULET:
    case GIRDLE:
    case POTION:
    case BOW:
    case ARROW:
    case CLOAK:
    case FOOD:
    case DRINK:
    case WAND:
    case ROD:
    case FLESH:
    case BOOK:
    case CONTAINER:
    {
        if ((op->type != AMULET && op->type != RING) || op->title != NULL) {
            StringBuffer *sb2 = object_get_description(op, caller, NULL);
            if (stringbuffer_length(sb2) > 1) {
                stringbuffer_append_string(sb, " ");
                stringbuffer_append_stringbuffer(sb, sb2);
            }
            stringbuffer_free(sb2);
        }

        break;
    }

    default:
        break;
    }

    return sb;
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
    case BOOK_SPELL:

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

            if (op->type != BOOK_SPELL) {
                sprintf(buf2, " (lvl %d)", op->level);
                safe_strcat(buf, buf2, &len, sizeof(buf));
            }
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
        case PANTS:
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
    case BOOK_SPELL:

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

            if (op->type != BOOK_SPELL) {
                sprintf(buf2, " (lvl %d)", op->level);
                safe_strcat(buf, buf2, &len, sizeof(buf));
            }
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
            case PANTS:

                if (ARMOUR_SPEED(op)) {
                    sprintf(buf, "(speed cap %1.2f)", ARMOUR_SPEED(op) / 10.0);
                    strcat(retbuf, buf);
                }

                /* Do this in all cases - otherwise it gets confusing - does
                 * that
                 * item have no penalty, or is it not fully identified for
                 * example. */
                if (ARMOUR_SPELLS(op)) {
                    sprintf(buf, "(armour mana reg %d)", -ARMOUR_SPELLS(op));
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
 * @return True if this object needs identification, false otherwise. */
bool need_identify(const object *op)
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
    case PANTS:
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
    case BOOK_SPELL:
        return true;
    }

    return false;
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
