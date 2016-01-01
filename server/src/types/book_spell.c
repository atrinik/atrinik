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
 * Handles code related to @ref BOOK_SPELL "spell books".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    /* Monsters cannot learn spells from spell books. */
    if (applier->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    if (op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS) {
        LOG(ERROR, "Spell book with an invalid ID (%d): %s",
            op->stats.sp, object_get_str(op));
        char *name = object_get_name_s(op, applier);
        draw_info_format(COLOR_WHITE, applier,
                         "The symbols in the %s make no sense.", name);
        efree(name);
        return OBJECT_METHOD_OK;
    }

    if (QUERY_FLAG(applier, FLAG_BLIND)) {
        draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
        return OBJECT_METHOD_OK;
    }

    player *pl = CONTR(applier);

    if (pl->skill_ptr[SK_LITERACY] == NULL) {
        draw_info(COLOR_WHITE, applier,
                  "You are unable to decipher the strange symbols.");
        return OBJECT_METHOD_OK;
    }

    if (pl->skill_ptr[SK_WIZARDRY_SPELLS] == NULL) {
        draw_info(COLOR_WHITE, applier,
                  "The arcane symbols have no meaning to you.");
        return OBJECT_METHOD_OK;
    }

    spell_struct *spell = &spells[op->stats.sp];

    if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) {
        char *name = object_get_base_name_s(op, applier);
        draw_info_format(COLOR_RED, applier, "The %s was %s!",
                         name,
                         QUERY_FLAG(op, FLAG_DAMNED) ? "damned" : "cursed");
        efree(name);
        spell_failure(applier, (spell->at->clone.level +
                                pl->skill_ptr[SK_WIZARDRY_SPELLS]->level) / 2);

        /* Damned spell book, small chance to forget the spell. */
        if (QUERY_FLAG(op, FLAG_DAMNED) && rndm_chance(15)) {
            object *tmp = player_find_spell(applier, spell);
            if (tmp != NULL) {
                draw_info_format(COLOR_RED, applier,
                                 "The wild magic burns the spell %s out of "
                                 "your mind!",
                                 tmp->name);
                object_remove(tmp, 0);
                object_destroy(tmp);
            }
        }

        op = decrease_ob_nr(op, 1);
        if (op != NULL && !QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            identify(op);
        }

        return OBJECT_METHOD_OK;
    }

    if (spell->at->clone.level > pl->skill_ptr[SK_LITERACY]->level + 15) {
        draw_info(COLOR_WHITE, applier,
                  "Try as hard as you might, you can't quite make sense "
                  "out of the symbols...");
        return OBJECT_METHOD_OK;
    }

    if (!QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        identify(op);
        draw_info_format(COLOR_WHITE, applier,
                         "The spellbook contains the spell %s.",
                         spell->at->clone.name);
    }

    if (player_find_spell(applier, spell) != NULL) {
        draw_info(COLOR_WHITE, applier, "You already know that spell.");
        return OBJECT_METHOD_OK;
    }

    if (spell->at->clone.level > pl->skill_ptr[SK_WIZARDRY_SPELLS]->level) {
        draw_info_format(COLOR_WHITE, applier,
                         "You need to be level %d in %s to learn this spell.",
                         spell->at->clone.level,
                         pl->skill_ptr[SK_WIZARDRY_SPELLS]->name);
        return OBJECT_METHOD_OK;
    }

    if (QUERY_FLAG(applier, FLAG_CONFUSED)) {
        draw_info(COLOR_RED, applier,
                  "In your confused state you mix up the wording "
                  "of the spell!");
        spell_failure(applier, (spell->at->clone.level +
                                pl->skill_ptr[SK_WIZARDRY_SPELLS]->level) / 2);
    } else {
        object *tmp = object_insert_into(arch_to_object(spell->at), applier, 0);
        if (tmp == NULL) {
            LOG(ERROR, "Failed to insert spell, op: %s, applier: %s",
                object_get_str(op), object_get_str(applier));
            return OBJECT_METHOD_OK;
        }

        draw_info_format(COLOR_WHITE, applier, "You succeed in learning %s.",
                         spell->at->clone.name);
    }

    decrease_ob_nr(op, 1);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the spell book type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(book_spell)
{
    OBJECT_METHODS(BOOK_SPELL)->apply_func = apply_func;
}
