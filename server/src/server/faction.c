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

#include <global.h>
#include <faction.h>
#include <toolkit_string.h>

/**
 * The faction structure.
 */
struct faction {
    shstr *name;

    union {
        shstr *name;

        faction_t ptr;
    } *parents;

    size_t parents_num;

    int16_t modifier;

    UT_hash_handle hh;
};

static faction_t factions;

/* Prototypes */

static faction_t faction_create(const char *name, faction_t parent);
static void faction_free(faction_t faction);
static void faction_add_parent(faction_t faction, shstr *name);
static faction_t faction_find(shstr *name);
static void faction_assign_parents(void);

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
    faction_t faction[50];
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

        if (strcmp(key, "faction") == 0) {
            if (depth == arraysize(faction)) {
                error_str = "reached max faction stack size";
                goto error;
            }

            if (!string_isempty(value)) {
                faction[depth] = faction_create(value,
                        depth > 0 ? faction[depth - 1] : NULL);
                depth++;
            } else {
                error_str = "empty faction attribute";
                goto error;
            }
        } else if (depth == 0) {
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
        } else if (strcmp(key, "modifier") == 0) {
            if (!string_endswith(value, "%")) {
                error_str = "modifier value must end with a percent sign";
                goto error;
            }

            int modifier = atoi(value);

            if (modifier < INT16_MIN || modifier > INT16_MAX) {
                error_str = "invalid modifier value";
                goto error;
            }

            faction[depth - 1]->modifier = (int16_t) modifier;
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
        LOG(ERROR, "Faction block without end: %s", faction[depth - 1]->name);
        exit(1);
    }

    faction_assign_parents();
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
    HARD_ASSERT(name != NULL);

    faction_t faction = ecalloc(1, sizeof(*faction));
    faction->name = add_string(name);

    if (parent != NULL) {
        faction_add_parent(faction, parent->name);
    }

    HASH_ADD(hh, factions, name, sizeof(*faction->name), faction);

    return faction;
}

static void faction_free(faction_t faction)
{
    free_string_shared(faction->name);

    if (faction->parents != NULL) {
        efree(faction->parents);
    }

    efree(faction);
}

static faction_t faction_find(shstr *name)
{
    faction_t faction;
    HASH_FIND(hh, factions, &name, sizeof(*name), faction);
    return faction;
}

static void faction_add_parent(faction_t faction, shstr *name)
{
    faction->parents = erealloc(faction->parents, sizeof(*faction->parents) *
            (faction->parents_num + 1));
    faction->parents[faction->parents_num].name = add_string(name);
    faction->parents_num++;
}

static void faction_assign_parents(void)
{
    faction_t faction, tmp;

    HASH_ITER(hh, factions, faction, tmp) {
        for (size_t i = 0; i < faction->parents_num; i++) {
            faction_t parent = faction_find(faction->parents[i].name);

            if (parent == NULL) {
                LOG(ERROR, "Could not find parent faction %s for faction %s.",
                    faction->parents[i].name, faction->name);
                exit(1);
            }

            free_string_shared(faction->parents[i].name);
            faction->parents[i].ptr = parent;
        }
    }
}
