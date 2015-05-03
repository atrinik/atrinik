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

typedef struct faction_parent {
    union {
        shstr *name;

        faction_t ptr;
    } faction;

    int16_t spill;

    int16_t attention;
} faction_parent_t;

typedef struct faction_enemy {
    union {
        shstr *name;

        faction_t ptr;
    } faction;
} faction_enemy_t;

/**
 * The faction structure.
 */
struct faction {
    shstr *name;

    faction_parent_t *parents;
    size_t parents_num;

    faction_enemy_t *enemies;
    size_t enemies_num;

    int16_t modifier;

    double penalty;

    double threshold;

    bool alliance:1;

    UT_hash_handle hh;
};

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
        HASH_DEL(factions, faction);
        faction_free(faction);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

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

static void faction_free(faction_t faction)
{
    TOOLKIT_PROTECT();

    free_string_shared(faction->name);

    if (faction->parents != NULL) {
        efree(faction->parents);
    }

    if (faction->enemies != NULL) {
        efree(faction->enemies);
    }

    efree(faction);
}

faction_t faction_find(shstr *name)
{
    TOOLKIT_PROTECT();

    faction_t faction;
    HASH_FIND(hh, factions, &name, sizeof(shstr *), faction);
    return faction;
}

static void faction_add_parent(faction_t faction, const char *name)
{
    TOOLKIT_PROTECT();

    faction->parents = erealloc(faction->parents, sizeof(*faction->parents) *
            (faction->parents_num + 1));
    faction->parents[faction->parents_num].spill = 100;
    faction->parents[faction->parents_num].attention = 100;
    faction->parents[faction->parents_num].faction.name = add_string(name);
    faction->parents_num++;
}

static void faction_assign_names(void)
{
    TOOLKIT_PROTECT();

    faction_t faction, tmp;

    HASH_ITER(hh, factions, faction, tmp) {
        for (size_t i = 0; i < faction->parents_num; i++) {
            faction_t parent = faction_find(faction->parents[i].faction.name);

            if (parent == NULL) {
                LOG(ERROR, "Could not find parent faction %s for faction %s.",
                    faction->parents[i].faction.name, faction->name);
                exit(1);
            }

            free_string_shared(faction->parents[i].faction.name);
            faction->parents[i].faction.ptr = parent;
        }

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

        if (!override_spill) {
            new_reputation *= (double) faction->parents[i].spill / 100.0;
        }

        new_reputation *= (double) parent->modifier / 100.0;

        if (fabs(new_reputation) < 0.00000001) {
            continue;
        }

        _faction_update(parent, pl, new_reputation, override_spill);
    }
}

void faction_update(faction_t faction, player *pl, double reputation)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    _faction_update(faction, pl, reputation, false);
}

void faction_update_kill(faction_t faction, player *pl)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(pl != NULL);

    _faction_update(faction, pl, faction->penalty, true);
}

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

bool faction_is_friend(faction_t faction, object *op)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(op != NULL);

    return _faction_is_friend(faction, op, true, 1.0);
}

static faction_t faction_get_topmost_alliance(faction_t faction)
{
    if (faction->alliance) {
        return faction;
    }

    if (faction->parents_num == 0) {
        return NULL;
    }

    return faction_get_topmost_alliance(faction->parents[0].faction.ptr);
}

bool faction_is_alliance(faction_t faction, faction_t faction2)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(faction != NULL);
    HARD_ASSERT(faction2 != NULL);

    if (faction == faction2) {
        return true;
    }

    faction_t faction_alliance = faction_get_topmost_alliance(faction);
    faction_t faction_alliance2 = faction_get_topmost_alliance(faction2);

    return faction_alliance == faction_alliance2;
}

#endif
