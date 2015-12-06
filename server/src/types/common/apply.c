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
 *
 * Apply-related functions.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Handle generic object applying.
 *
 * @param op Object being applied.
 * @param applier Who is applying the object.
 * @param aflags Apply flags.
 * @return One of @ref OBJECT_METHOD_xxx.
 */
int
common_object_apply (object *op, object *applier, int aflags)
{
    if (op->msg != NULL) {
        draw_info(COLOR_WHITE, applier, op->msg);
        return OBJECT_METHOD_OK;
    }

    return OBJECT_METHOD_UNHANDLED;
}

/**
 * Check if the specified object matches the type of the one being applied,
 * and whether it's applied.
 *
 * @param op Object being applied.
 * @param tmp Object to check.
 * @return True if the object matches, false otherwise.
 */
static bool
object_apply_item_check_type (object *op, object *tmp)
{
    if (!QUERY_FLAG(tmp, FLAG_APPLIED)) {
        return false;
    }

    if (op->type == tmp->type &&
        QUERY_FLAG(op, FLAG_IS_THROWN) == QUERY_FLAG(tmp, FLAG_IS_THROWN)) {
        return true;
    }

    if (OBJECT_IS_RANGED(op) && OBJECT_IS_RANGED(tmp)) {
        return true;
    }

    if (OBJECT_IS_AMMO(op) && OBJECT_IS_AMMO(tmp)) {
        return true;
    }

    return false;
}

/**
 * Generic handler for applying equipment-like items, such as armour, weapons,
 * trinkets, jewelry, etc.
 *
 * @param op Object being applied.
 * @param applier Who is applying the object.
 * @param aflags Apply flags.
 * @return One of @ref OBJECT_METHOD_xxx.
 */
int
object_apply_item (object *op, object *applier, int aflags)
{
    SOFT_ASSERT_RC(op != NULL, OBJECT_METHOD_UNHANDLED,
                   "Object to apply is NULL.");
    SOFT_ASSERT_RC(applier != NULL, OBJECT_METHOD_UNHANDLED,
                   "Applier is NULL.");

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_UNHANDLED;
    }

    /* The item must be in the applier's inventory in order to be equipped. */
    if (op->env != applier) {
        return OBJECT_METHOD_ERROR;
    }

    int basic_aflag = aflags & APPLY_BASIC_FLAGS;

    if (!QUERY_FLAG(op, FLAG_APPLIED)) {
        if (QUERY_FLAG(op, FLAG_QUEST_ITEM)) {
            draw_info(COLOR_WHITE, applier,
                      "You feel you should turn it in first...");
            return OBJECT_METHOD_ERROR;
        }
    } else {
        /* Always apply, so no reason to unapply. */
        if (basic_aflag == APPLY_ALWAYS) {
            return OBJECT_METHOD_OK;
        }

        if (!(aflags & APPLY_IGNORE_CURSE) &&
            (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))) {
            draw_info_format(COLOR_WHITE, applier,
                             "No matter how hard you try, you just can't "
                             "remove it!");
            return OBJECT_METHOD_ERROR;
        }

        if (QUERY_FLAG(op, FLAG_PERM_CURSED)) {
            SET_FLAG(op, FLAG_CURSED);
        }

        if (QUERY_FLAG(op, FLAG_PERM_DAMNED)) {
            SET_FLAG(op, FLAG_DAMNED);
        }

        CLEAR_FLAG(op, FLAG_APPLIED);
        esrv_update_item(UPD_FLAGS, op);
        char *name = object_get_name_s(op, applier);

        switch (op->type) {
        case WEAPON:
            CLEAR_FLAG(applier, FLAG_READY_WEAPON);
            draw_info_format(COLOR_WHITE, applier, "You unwield %s.", name);
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
            draw_info_format(COLOR_WHITE, applier, "You unwear %s.", name);
            break;

        case BOW:
        case WAND:
        case ROD:
        case SPELL:
        case SKILL:
        case ARROW:
        case CONTAINER:
            draw_info_format(COLOR_WHITE, applier, "You unready %s.", name);
            break;

        default:
            draw_info_format(COLOR_WHITE, applier, "You unapply %s.", name);
            break;
        }

        efree(name);
        living_update(applier);

        if (!(aflags & APPLY_NO_MERGE)) {
            object_merge(op);
        }

        return OBJECT_METHOD_OK;
    }

    if (basic_aflag == APPLY_ALWAYS_UNAPPLY) {
        return OBJECT_METHOD_OK;
    }

    if (op->type != TRINKET) {
        bool ring_left = false;

        /* This goes through and checks to see if the player already has
         * something of that type applied - if so, unapply it. */
        FOR_INV_PREPARE(applier, tmp) {
            if (tmp == op || !object_apply_item_check_type(op, tmp)) {
                continue;
            }

            if (tmp->type == RING && !ring_left) {
                ring_left = true;
                continue;
            }

            int ret = object_apply(tmp, applier, APPLY_ALWAYS_UNAPPLY);
            if (ret != OBJECT_METHOD_OK) {
                return OBJECT_METHOD_ERROR;
            }
        } FOR_INV_FINISH();
    }

    if (!QUERY_FLAG(op, FLAG_CAN_STACK)) {
        op = object_stack_get_reinsert(op, 1);
    }

    char *name = object_get_name_s(op, applier);

    switch (op->type) {
    case WEAPON:
        if (!QUERY_FLAG(applier, FLAG_USE_WEAPON)) {
            draw_info_format(COLOR_WHITE, applier, "You can't use %s.", name);
            efree(name);
            return OBJECT_METHOD_ERROR;
        }

        draw_info_format(COLOR_WHITE, applier, "You wield %s.", name);
        SET_FLAG(op, FLAG_APPLIED);
        SET_FLAG(applier, FLAG_READY_WEAPON);
        break;

    case SHIELD:
    case ARMOUR:
    case HELMET:
    case BOOTS:
    case GLOVES:
    case GIRDLE:
    case BRACERS:
    case CLOAK:
    case PANTS:
        if (!QUERY_FLAG(applier, FLAG_USE_ARMOUR)) {
            draw_info_format(COLOR_WHITE, applier, "You can't use %s.", name);
            efree(name);
            return OBJECT_METHOD_ERROR;
        }

    case RING:
    case AMULET:
    case TRINKET:
        draw_info_format(COLOR_WHITE, applier, "You wear %s.", name);
        SET_FLAG(op, FLAG_APPLIED);
        break;

    case WAND:
    case ROD:
    case BOW:
    case SPELL:
    case SKILL:
    case ARROW:
    case CONTAINER:
        if (op->type == SPELL &&
            SKILL_LEVEL(CONTR(applier), SK_WIZARDRY_SPELLS) < op->level) {
            draw_info_format(COLOR_WHITE, applier,
                             "Your wizardry spells skill is too low to use %s.",
                             name);
            efree(name);
            return OBJECT_METHOD_ERROR;
        }

        draw_info_format(COLOR_WHITE, applier, "You ready %s.", name);
        SET_FLAG(op, FLAG_APPLIED);

        if (op->type == BOW) {
            draw_info_format(COLOR_WHITE, applier,
                             "You will now fire %s with %s.",
                             op->race != NULL ? op->race : "nothing",
                             name);
        }

        break;

    default:
        draw_info_format(COLOR_WHITE, applier, "You apply %s.", name);
    }

    efree(name);

    if (!QUERY_FLAG(op, FLAG_APPLIED)) {
        SET_FLAG(op, FLAG_APPLIED);
    }

    living_update(applier);
    SET_FLAG(op, FLAG_BEEN_APPLIED);

    if (QUERY_FLAG(op, FLAG_PERM_CURSED)) {
        SET_FLAG(op, FLAG_CURSED);
    }

    if (QUERY_FLAG(op, FLAG_PERM_DAMNED)) {
        SET_FLAG(op, FLAG_DAMNED);
    }

    esrv_update_item(UPD_FLAGS, op);

    if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) {
        draw_info(COLOR_WHITE, applier, "Oops, it feels deadly cold!");
    }

    return OBJECT_METHOD_OK;
}
