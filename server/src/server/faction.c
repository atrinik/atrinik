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
 * Handles faction related code.
 *
 * @author Alex Tokar
 */

#ifndef __CPROTO__

#include <global.h>
#include <faction.h>
#include <toolkit_string.h>

/**
 * Faction parent structure. Holds information about the parent faction,
 * data like spill percentage, etc.
 */
typedef struct faction_parent {
    /**
     * The actual faction parent. During factions file reading, 'name' is used,
     * and afterwards faction_assign_names() is called which sets the ptr
     * instead (by looking up the faction pointer from the name.
     */
    union {
        shstr *name; ///< Parent faction name.
        faction_t ptr; ///< Pointer to the parent faction.
    } faction;

    /**
     * When a player's reputation with this faction changes, how much reputation
     * spills over to this parent (as a percentage, eg, 50 for half of the
     * reputation).
     */
    int16_t spill;

    /**
     * Percentage of how much reputation from this parent affects friendliness
     * checks.
     */
    int16_t attention;

    /**
     * If true, will force spill value usage.
     */
    bool spill_force:1;
} faction_parent_t;

/**
 * Faction enemy structure. Holds information about a faction enemy.
 */
typedef struct faction_enemy {
    /**
     * The actual faction enemy. Behavior is same as faction_parent_t::faction;
     * during factions file reading, 'name' is used, and afterwards
     * faction_assign_names() is called which sets the ptr instead.
     */
    union {
        shstr *name; ///< Enemy faction name.
        faction_t ptr; ///< Pointer to the enemy faction.
    } faction;
} faction_enemy_t;

/**
 * The faction structure.
 */
struct faction {
    shstr *name; ///< Name of the faction.

    UT_hash_handle hh; ///< UT hash handle.

    faction_parent_t *parents; ///< Array of the faction's parents.
    size_t parents_num; ///< Number of entries in ::parents.

    faction_enemy_t *enemies; ///< Array of the faction's enemies.
    size_t enemies_num; ///< Number of entries in ::enemies.

    struct faction **children; ///< Pointers to child factions.
    size_t children_num; ///< Number of entries in ::children.

    /**
     * Percentage modifier of reputation gains/losses that spill from children
     * to this faction.
     */
    int16_t modifier;

    /**
     * Penalty for killing an NPC that belongs to this faction.
     */
    double penalty;

    /**
     * When below this threshold, NPCs that belong to this faction will attack
     * on sight.
     */
    double threshold;

    /**
     * Whether this faction is an alliance.
     */
    bool alliance:1;
};

/**
 * Hashtable of the factions.
 */
static faction_t factions;

/* Prototypes */

static faction_t faction_create(const char *name, faction_t parent);
static void faction_free(faction_t faction);
static void faction_add_parent(faction_t faction, shstr *name);
static void faction_assign_names(void);

TOOLKIT_API(DEPENDS(shstr));

TOOLKIT_INIT_FUNC(faction)
{
    char filename[HUGE_BUF];
    snprintf(VS(filename), "%s/factions", settings.libpath);

    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        LOG(ERROR, "Can't open factions file %s: %s (%d)", filename,
            strerror(errno), errno);
        exit(1);
    }

    factions = NULL;

    char buf[HUGE_BUF];
    uint64_t linenum = 0;
    faction_t faction_stack[50];
    size_t depth = 0;

    while (fgets(VS(buf), fp)) {
        linenum++;

        char *cp = string_skip_whitespace(buf), *end = strchr(cp, '\n');

        if (end != NULL) {
            *end = '\0';
        }

        char *cps[2];

        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        const char *key = cps[0], *value = cps[1], *error_str;
        faction_t faction = depth != 0 ? faction_stack[depth - 1] : NULL;

        if (strcmp(key, "faction") == 0) {
            if (depth == arraysize(faction_stack)) {
                error_str = "reached max faction stack size";
                goto error;
            }

            if (!string_isempty(value)) {
                shstr *name = find_string(value);

                if (name != NULL && faction_find(name) != NULL) {
                    error_str = "faction already exists";
                    goto error;
                }

                faction_stack[depth] = faction_create(value, faction);
                depth++;
            } else {
                error_str = "empty faction attribute";
                goto error;
            }
        } else if (faction == NULL) {
            error_str = "expected region attribute";
            goto error;
        } else if (string_isempty(value)) {
            if (strcmp(key, "end") == 0) {
                if (depth > 0) {
                    depth--;
                } else {
                    error_str = "premature end attribute";
                    goto error;
                }
            } else {
                error_str = "unknown attribute without a value";
                goto error;
            }
        } else if (strcmp(key, "modifier") == 0 ||
                strcmp(key, "spill") == 0 ||
                strcmp(key, "attention") == 0) {
            if (!string_endswith(value, "%")) {
                error_str = "value must end with a percent sign";
                goto error;
            }

            int value_int = atoi(value);

            if (value_int < INT16_MIN || value_int > INT16_MAX) {
                error_str = "invalid value";
                goto error;
            }

            if ((strcmp(key, "spill") == 0 || strcmp(key, "attention") == 0) &&
                    faction->parents_num == 0) {
                error_str = "faction has no parent";
                goto error;
            }

            if (strcmp(key, "modifier") == 0) {
                faction->modifier = (int16_t) value_int;
            } else {
                faction_parent_t *parent =
                        &faction->parents[faction->parents_num - 1];

                if (strcmp(key, "spill") == 0) {
                    parent->spill = (int16_t) value_int;
                } else if (strcmp(key, "attention") == 0) {
                    parent->attention = (int16_t) value_int;
                }
            }
        } else if (strcmp(key, "penalty") == 0) {
            faction->penalty = atof(value);
        } else if (strcmp(key, "threshold") == 0) {
            faction->threshold = atof(value);
        } else if (strcmp(key, "parent") == 0) {
            faction_add_parent(faction, value);
        } else if (strcmp(key, "enemy") == 0) {
            faction->enemies = erealloc(faction->enemies,
                    sizeof(*faction->enemies) * (faction->enemies_num + 1));
            faction->enemies[faction->enemies_num].faction.name =
                    add_string(value);
            faction->enemies_num++;
        } else if (strcmp(key, "alliance") == 0) {
            if (KEYWORD_IS_TRUE(value)) {
                faction->alliance = true;
            } else {
                error_str = "unknown value";
                goto error;
            }
        } else if (strcmp(key, "spill_force") == 0) {
            if (faction->parents_num == 0) {
                error_str = "faction has no parent";
                goto error;
            }

            faction_parent_t *parent =
                    &faction->parents[faction->parents_num - 1];
            if (KEYWORD_IS_TRUE(value)) {
                parent->spill_force = true;
            } else {
                error_str = "unknown value";
                goto error;
            }
        } else {
            error_str = "unknown attribute";
            goto error;
        }

        continue;

error:
        LOG(ERROR, "Error parsing %s, line %" PRIu64 ", %s: %s %s", filename,
            linenum, error_str, key, value != NULL ? value : "");
        exit(1);
    }

    fclose(fp);

    if (depth > 0) {
        LOG(ERROR, "Faction block without end: %s",
            faction_stack[depth - 1]->name);
        exit(1);
    }

    faction_assign_names();
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(faction)
{
    faction_t faction, tmp;

    HASH_ITER(hh, factions, faction, tmp) {
        faction_free(faction);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Create a new faction with the specified name, adding it to the ::factions
 * hash table.
 * @warning Make sure this is NEVER called with duplicate names.
 * @param name Name of the faction.
 * @param parent Faction's parent. May be NULL.
 * @return The created faction.
 */
static faction_t faction_create(const char *name, faction_t parent)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(name != NULL);

    faction_t faction = ecalloc(1, sizeof(*faction));
    faction->name = add_string(name);
    faction->modifier = 100;
    faction->penalty = -25.0;
    faction->threshold = -500.0;

    if (parent != NULL) {
        faction_add_parent(faction, parent->name);
    }

    HASH_ADD(hh, factions, name, sizeof(shstr *), faction);

    return faction;
}

/**
 * Frees data associated with the specified faction structure, removing it from
 * the ::factions hash table.
 * @param faction Faction to free.
 */
static void faction_free(faction_t faction)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);

    HASH_DEL(factions, faction);
    free_string_shared(faction->name);

    if (faction->parents != NULL) {
        efree(faction->parents);
    }

    if (faction->enemies != NULL) {
        efree(faction->enemies);
    }

    if (faction->children != NULL) {
        efree(faction->children);
    }

    efree(faction);
}

/**
 * Find a faction in the ::factions hash table, identified by its name.
 * @param name Name to find.
 * @return Faction if found, NULL otherwise.
 */
faction_t faction_find(shstr *name)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(name != NULL);

    faction_t faction;
    HASH_FIND(hh, factions, &name, sizeof(shstr *), faction);
    return faction;
}

/**
 * Add a parent to the specified faction.
 * @param faction Faction.
 * @param name Name of the parent.
 */
static void faction_add_parent(faction_t faction, const char *name)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(name != NULL);

    faction->parents = erealloc(faction->parents, sizeof(*faction->parents) *
            (faction->parents_num + 1));
    faction->parents[faction->parents_num].spill = 100;
    faction->parents[faction->parents_num].attention = 100;
    faction->parents[faction->parents_num].spill_force = false;
    faction->parents[faction->parents_num].faction.name = add_string(name);
    faction->parents_num++;
}

/**
 * Assign pointers to faction parents and enemies.
 */
static void faction_assign_names(void)
{
    TOOLKIT_PROTECT();

    faction_t faction, tmp;

    HASH_ITER(hh, factions, faction, tmp) {
        /* Assign parents. */
        for (size_t i = 0; i < faction->parents_num; i++) {
            faction_t parent = faction_find(faction->parents[i].faction.name);

            if (parent == NULL) {
                LOG(ERROR, "Could not find parent faction %s for faction %s.",
                    faction->parents[i].faction.name, faction->name);
                exit(1);
            }

            free_string_shared(faction->parents[i].faction.name);
            faction->parents[i].faction.ptr = parent;

            parent->children = erealloc(parent->children,
                    sizeof(*parent->children) * (parent->children_num + 1));
            parent->children[parent->children_num] = faction;
            parent->children_num++;
        }

        /* Assign enemies. */
        for (size_t i = 0; i < faction->enemies_num; i++) {
            faction_t enemy = faction_find(faction->enemies[i].faction.name);

            if (enemy == NULL) {
                LOG(ERROR, "Could not find enemy faction %s for faction %s.",
                    faction->enemies[i].faction.name, faction->name);
                exit(1);
            }

            free_string_shared(faction->enemies[i].faction.name);
            faction->enemies[i].faction.ptr = enemy;
        }
    }
}

/**
 * Updates player's reputation with the specified faction and all its parents.
 * @param faction Faction.
 * @param pl Player.
 * @param reputation Reputation value to add.
 * @param override_spill If true, spill value of parents will not be considered.
 */
static void _faction_update(faction_t faction, player *pl, double reputation,
        bool override_spill)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    player_faction_update(pl, faction->name, reputation);

    for (size_t i = 0; i < faction->parents_num; i++) {
        faction_t parent = faction->parents[i].faction.ptr;
        double new_reputation = reputation;

        if (!override_spill || faction->parents[i].spill_force) {
            new_reputation *= (double) faction->parents[i].spill / 100.0;
        }

        new_reputation *= (double) parent->modifier / 100.0;

        if (fabs(new_reputation) < 0.00000001) {
            continue;
        }

        _faction_update(parent, pl, new_reputation, override_spill);
    }
}

/**
 * Update player's reputation with the specified faction.
 * @param faction Faction.
 * @param pl Player.
 * @param reputation Reputation value to add.
 */
void faction_update(faction_t faction, player *pl, double reputation)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    _faction_update(faction, pl, reputation, false);
}

/**
 * Update player's reputation with the specified faction, triggered by
 * killing a member of the faction.
 * @param faction Faction.
 * @param pl Player.
 */
void faction_update_kill(faction_t faction, player *pl)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    _faction_update(faction, pl, faction->penalty, true);
}

/**
 * Checks whether the specified object is a friend of the specified faction.
 * @param faction Faction.
 * @param op Object to check.
 * @param check_enemies If true, check faction's enemies as well.
 * @param attention Parent's attention value.
 * @return Whether the object is a friend of the faction.
 */
static bool _faction_is_friend(faction_t faction, object *op,
        bool check_enemies, double attention)
{
    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(op != NULL);

    double reputation;

    if (op->type == PLAYER) {
        reputation = player_faction_reputation(CONTR(op), faction->name);
    } else {
        reputation = 0;

        if (faction->name == object_get_value(op, "faction")) {
            return true;
        }
    }

    if (check_enemies) {
        for (size_t i = 0; i < faction->enemies_num; i++) {
            if (_faction_is_friend(faction->enemies[i].faction.ptr, op, false,
                    attention)) {
                reputation -= fabs(faction->threshold) + 1.0;
                break;
            }
        }
    }

    if (reputation <= faction->threshold) {
        return false;
    }

    for (size_t i = 0; i < faction->parents_num; i++) {
        if (faction->parents[i].attention == 0) {
            continue;
        }

        faction_t parent = faction->parents[i].faction.ptr;
        double new_attention = (double) faction->parents[i].attention / 100.0;

        if (!_faction_is_friend(parent, op, true, new_attention)) {
            return false;
        }
    }

    if (!check_enemies) {
        return reputation >= fabs(faction->threshold);
    }

    return true;
}

/**
 * Checks whether the specified object is a friend of the given faction.
 * @param faction Faction.
 * @param op Object to check.
 * @return Whether the object is a friend of the faction.
 */
bool faction_is_friend(faction_t faction, object *op)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(op != NULL);

    return _faction_is_friend(faction, op, true, 1.0);
}

/**
 * Acquire the first faction alliance, recursively iterating its primary
 * parents.
 * @param faction Faction.
 * @return Faction with the alliance flag. Can be NULL.
 */
static faction_t faction_get_alliance(faction_t faction)
{
    HARD_ASSERT(faction != NULL);

    if (faction->alliance) {
        return faction;
    }

    if (faction->parents_num == 0) {
        return NULL;
    }

    return faction_get_alliance(faction->parents[0].faction.ptr);
}

/**
 * Checks whether the two specified factions are in an alliance.
 *
 * In order to be in an alliance, the first parent with an alliance flag must
 * be the same for both factions.
 * @param faction First faction.
 * @param faction2 Second faction.
 * @return True if the two factions are in an alliance, false otherwise.
 */
bool faction_is_alliance(faction_t faction, faction_t faction2)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(faction2 != NULL);

    if (faction == faction2) {
        return true;
    }

    faction_t faction_alliance = faction_get_alliance(faction);
    faction_t faction_alliance2 = faction_get_alliance(faction2);

    if (faction_alliance == NULL || faction_alliance2 == NULL) {
        return false;
    }

    return faction_alliance == faction_alliance2;
}

/**
 * Helper function for faction_get_bounty().
 * @param faction Faction.
 * @param pl Player.
 * @return Bounty.
 */
static double _faction_get_bounty(faction_t faction, player *pl)
{
    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    double bounty = -player_faction_reputation(pl, faction->name);
    if (bounty < 0.0) {
        bounty = 0.0;
    }

    for (size_t i = 0; i < faction->children_num; i++) {
        SOFT_ASSERT_RC(faction->children[i]->parents_num > 0, 0.0,
                "Child faction %s has no parents!", faction->children[i]->name);

        /* Skip child factions that do not have this faction as their primary
         * parent. */
        if (faction->children[i]->parents[0].faction.ptr != faction) {
            continue;
        }

        double bounty_child = _faction_get_bounty(faction->children[i], pl);
        if (bounty_child > bounty) {
            bounty = bounty_child;
        }
    }

    return bounty;
}

/**
 * Acquire player's bounty for the specified faction.
 *
 * If the faction is part of an alliance, the entire alliance will be checked
 * instead.
 * @param faction Faction.
 * @param pl Player.
 * @return Bounty.
 */
double faction_get_bounty(faction_t faction, player *pl)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    faction_t alliance = faction_get_alliance(faction);
    if (alliance != NULL) {
        faction = alliance;
    }

    return _faction_get_bounty(faction, pl);
}

/**
 * Helper function for faction_clear_bounty().
 * @param faction Faction.
 * @param pl Player.
 */
static void _faction_clear_bounty(faction_t faction, player *pl)
{
    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    double bounty = player_faction_reputation(pl, faction->name);
    if (bounty < 0.0) {
        player_faction_update(pl, faction->name, -bounty);
    }

    for (size_t i = 0; i < faction->children_num; i++) {
        SOFT_ASSERT(faction->children[i]->parents_num > 0,
                "Child faction %s has no parents!", faction->children[i]->name);

        /* Skip child factions that do not have this faction as their primary
         * parent. */
        if (faction->children[i]->parents[0].faction.ptr != faction) {
            continue;
        }

        _faction_clear_bounty(faction->children[i], pl);
    }
}

/**
 * Clear player's bounty for the specified faction.
 *
 * If the faction is part of an alliance, the bounty will be cleared in the
 * entire alliance.
 * @param faction Faction.
 * @param pl Player.
 */
void faction_clear_bounty(faction_t faction, player *pl)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    faction_t alliance = faction_get_alliance(faction);
    if (alliance != NULL) {
        faction = alliance;
    }

    return _faction_clear_bounty(faction, pl);
}

#endif
