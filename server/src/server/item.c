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
 * Item related functions
 */

#include <global.h>
#include <player.h>
#include <object.h>

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
 * Builds a textual representation of the object's material (if applicable).
 * @param op
 * Object to get the material of.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object material.
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
 * Wrapper for object_get_material() that returns a string.
 * @param op
 * Object to get the material of.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the object material. Must be freed.
 */
char *object_get_material_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_material(op, caller, NULL));
}

/**
 * Builds a textual representation of the object's title (if applicable).
 * @param op
 * Object to get the title of.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object title.
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
    case TRINKET:
        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            /* If ring has a title, full description isn't so useful. */
            if (op->title == NULL) {
                stringbuffer_append_string(sb, " ");
                size_t len = stringbuffer_length(sb);
                sb = object_get_description(op, caller, sb);
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
 * Wrapper for object_get_title() that returns a string.
 * @param op
 * Object to get the title of.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the object title. Must be freed.
 */
char *object_get_title_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_title(op, caller, NULL));
}

/**
 * Builds a verbose textual representation of the name of the given object.
 * @param op
 * Object to get the name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object name.
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

    if (QUERY_FLAG(op, FLAG_SOULBOUND)) {
        stringbuffer_append_string(sb, " (soulbound)");
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
        case TRINKET:
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
 * Wrapper for object_get_name() that returns a string.
 * @param op
 * Object to get the name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the object name. Must be freed.
 */
char *object_get_name_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_name(op, caller, NULL));
}

/**
 * Like object_get_name(), but object status information (eg, worn/cursed/etc)
 * is not included.
 * @param op
 * Object to get the name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object name.
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
 * Wrapper for object_get_short_name() that returns a string.
 * @param op
 * Object to get the short name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the short object name. Must be freed.
 */
char *object_get_short_name_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_short_name(op, caller, NULL));
}

/**
 * Builds an object's name, but only includes the name, title (if any)
 * and material information (if any).
 * @param op
 * Object to get the name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object name.
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
 * Wrapper for object_get_material_name() that returns a string.
 * @param op
 * Object to get the material name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the material object name. Must be freed.
 */
char *object_get_material_name_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_material_name(op, caller, NULL));
}

/**
 * Like object_get_name(), but neither object count nor object status
 * information is included.
 * @param op
 * Object to get the name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object name.
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
 * Wrapper for object_get_base_name() that returns a string.
 * @param op
 * Object to get the base name of.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the base object name. Must be freed.
 */
char *object_get_base_name_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_base_name(op, caller, NULL));
}

/**
 * Builds a description of the object's terrain flags.
 * @param op
 * Object's terrain to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object terrain.
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
 * Wrapper for object_get_description_terrain() that returns a string.
 * @param op
 * Object's terrain to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the terrain description. Must be freed.
 */
char *object_get_description_terrain_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_description_terrain(op, caller,
            NULL));
}

/**
 * Builds a description of the object's attack types.
 * @param op
 * Object's attack types to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object attack types.
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
 * Wrapper for object_get_description_attacks() that returns a string.
 * @param op
 * Object's attacks to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the attacks description. Must be freed.
 */
char *object_get_description_attacks_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_description_attacks(op, caller,
            NULL));
}

/**
 * Builds a description of the object's protections.
 * @param op
 * Object's protections to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the object protections.
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
 * Wrapper for object_get_description_protections() that returns a string.
 * @param op
 * Object's protections to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the protections description. Must be freed.
 */
char *object_get_description_protections_s(const object *op,
        const object *caller)
{
    return stringbuffer_finish(object_get_description_protections(op, caller,
            NULL));
}

/**
 * Builds a description of the object's spell path.
 * @param op
 * Object's spell path to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @param path
 * The spell path flags.
 * @param name
 * Name of the spell path.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the spell path description.
 */
StringBuffer *object_get_description_path(const object *op, const
        object *caller, const uint32_t path, const char *name, StringBuffer *sb)
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
 * Wrapper for object_get_description_path() that returns a string.
 * @param op
 * Object's path to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @param path
 * The spell path flags.
 * @param name
 * Name of the spell path.
 * @return
 * String containing the path description. Must be freed.
 */
char *object_get_description_path_s(const object *op, const object *caller,
        const uint32_t path, const char *name)
{
    return stringbuffer_finish(object_get_description_path(op, caller, path,
            name, NULL));
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
 * @param op
 * Object that should be described.
 * @param caller
 * Object calling this. Can be NULL.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the description.
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
            case TRINKET:
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

        if (op->block != 0) {
            stringbuffer_append_printf(sb, "(block%+d) ", op->block);
        }

        if (op->absorb != 0) {
            stringbuffer_append_printf(sb, "(absorb%+d%%) ", op->absorb);
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

        sb = object_get_description_path(op, caller, op->path_attuned,
                "Attuned", sb);
        sb = object_get_description_path(op, caller, op->path_repelled,
                "Repelled", sb);
        sb = object_get_description_path(op, caller, op->path_denied,
                "Denied", sb);

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
 * Wrapper for object_get_description() that returns a string.
 * @param op
 * Object to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the description. Must be freed.
 */
char *object_get_description_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_description(op, caller, NULL));
}

/**
 * Get object's name using object_get_name(), along with its description
 * (if any).
 * @param tmp
 * Object to get description of.
 * @param caller
 * Caller.
 * @param sb
 * StringBuffer instance to append to. If NULL, a new instance will
 * be created.
 * @return
 * StringBuffer instance that contains the description.
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
    case TRINKET:
    {
        if ((op->type != AMULET && op->type != RING && op->type != TRINKET) ||
            op->title != NULL) {
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
 * Wrapper for object_get_name_description() that returns a string.
 * @param op
 * Object to describe.
 * @param caller
 * Object calling this. Can be NULL.
 * @return
 * String containing the object's name and its description. Must be
 * freed.
 */
char *object_get_name_description_s(const object *op, const object *caller)
{
    return stringbuffer_finish(object_get_name_description(op, caller, NULL));
}

/**
 * Checks if given object should need identification.
 * @param op
 * Object to check.
 * @return
 * True if this object needs identification, false otherwise.
 */
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
    case TRINKET:
        return true;
    }

    return false;
}

/**
 * Identify an object. Basically sets FLAG_IDENTIFIED on the object along
 * with other things.
 * @param op
 * Object to identify.
 */
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
 * @param op
 * The object to check.
 */
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
        if (tmp->type == RUNE && tmp->stats.Int <= 1) {
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
