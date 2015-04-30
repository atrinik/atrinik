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
 * Region management.
 *
 * A region is a group of maps. It includes a "parent" region.
 */

#include <global.h>
#include <toolkit_string.h>

/** First region. */
region_struct *first_region = NULL;

static region_struct *region_get(void);
static void region_free(region_struct *region);
static void region_add(region_struct *region);
static void region_assign_parents(void);

/**
 * Initializes regions from the regions file.
 */
void regions_init(void)
{
    FILE *fp;
    char filename[MAX_BUF];
    region_struct *region;
    char buf[HUGE_BUF * 4], msgbuf[HUGE_BUF], *key, *value, *end;

    /* Only do this once */
    if (first_region != NULL) {
        return;
    }

    snprintf(filename, sizeof(filename), "%s/regions.reg", settings.mapspath);
    fp = fopen(filename, "r");

    if (fp == NULL) {
        LOG(ERROR, "Can't open regions file: %s.", filename);
        exit(1);
    }

    region = NULL;

    while (fgets(buf, sizeof(buf), fp)) {
        key = buf;

        while (isspace(*key)) {
            key++;
        }

        end = strchr(buf, '\n');

        if (end != NULL) {
            *end = '\0';
        }

        /* Empty line or a comment */
        if (*key == '\0' || *key == '#') {
            continue;
        }

        value = strchr(key, ' ');

        if (value != NULL) {
            *value = '\0';
            value++;

            while (isspace(*value)) {
                value++;
            }
        }

        /* When there is no region allocated yet, we expect 'region xxx'. */
        if (region == NULL) {
            if (strcmp(key, "region") == 0 && !string_isempty(value)) {
                region = region_get();
                region->name = estrdup(value);
            } else {
                LOG(ERROR, "Parsing error: %s %s", buf,
                        value ? value : "");
                exit(1);
            }

            continue;
        } else if (value == NULL) {
            if (strcmp(key, "msg") == 0) {
                msgbuf[0] = '\0';

                while (fgets(buf, sizeof(buf), fp)) {
                    if (strcmp(buf, "endmsg\n") == 0) {
                        break;
                    }

                    snprintfcat(VS(msgbuf), "%s", buf);
                }

                if (msgbuf[0] != '\0') {
                    region->msg = estrdup(msgbuf);
                }
            } else if (strcmp(key, "end") == 0) {
                region_add(region);
                region = NULL;
            } else {
                LOG(ERROR, "Parsing error: %s %s", buf,
                        value ? value : "");
                exit(1);
            }

            continue;
        } else if (strcmp(key, "parent") == 0) {
            region->parent_name = estrdup(value);
        } else if (strcmp(key, "longname") == 0) {
            region->longname = estrdup(value);
        } else if (strcmp(key, "map_first") == 0) {
            region->map_first = estrdup(value);
        } else if (strcmp(key, "map_bg") == 0) {
            region->map_bg = estrdup(value);
        } else if (strcmp(key, "map_quest") == 0) {
            region->map_quest = KEYWORD_IS_TRUE(value);
        } else if (strcmp(key, "jail") == 0) {
            char path[MAX_BUF];
            int x, y;

            /* Jail entries are of the form: /path/to/map x y */
            if (sscanf(value, "%255[^ ] %d %d", path, &x, &y) != 3) {
                LOG(ERROR, "Parsing error: %s %s", buf,
                        value ? value : "");
                exit(1);
            }

            region->jailmap = estrdup(path);
            region->jailx = x;
            region->jaily = y;
        } else {
            LOG(ERROR, "Parsing error: %s %s", buf,
                    value ? value : "");
            exit(1);
        }
    }

    region_assign_parents();

    fclose(fp);

    if (region != NULL) {
        LOG(ERROR, "Region block without end: %s", region->name);
        exit(1);
    }
}

/**
 * Deinitializes all regions.
 */
void regions_free(void)
{
    region_struct *region, *next;

    for (region = first_region; region != NULL; region = next) {
        next = region->next;
        region_free(region);
    }
}

/**
 * Allocates and zeros a region struct.
 * @return Initialized region structure.
 */
static region_struct *region_get(void)
{
    return ecalloc(1, sizeof(region_struct));
}

/**
 * Frees the specified region and all the data associated with it.
 * @param region Region to free.
 */
static void region_free(region_struct *region)
{
    if (region == NULL) {
        return;
    }

    FREE_AND_NULL_PTR(region->name);
    FREE_AND_NULL_PTR(region->parent_name);
    FREE_AND_NULL_PTR(region->longname);
    FREE_AND_NULL_PTR(region->map_first);
    FREE_AND_NULL_PTR(region->map_bg);
    FREE_AND_NULL_PTR(region->msg);
    FREE_AND_NULL_PTR(region->jailmap);
    efree(region);
}

/**
 * Add the specified region to the linked list of all regions.
 * @param region Region to add.
 */
static void region_add(region_struct *region)
{
    region_struct *tmp;

    for (tmp = first_region;
            tmp != NULL && tmp->next != NULL;
            tmp = tmp->next) {
    }

    if (tmp == NULL) {
        first_region = region;
    } else {
        tmp->next = region;
    }
}

/**
 * Links children regions with their parent from the parent_name field.
 */
static void region_assign_parents(void)
{
    region_struct *region;

    for (region = first_region; region != NULL; region = region->next) {
        if (region->parent_name != NULL) {
            region->parent = region_find_by_name(region->parent_name);
        }
    }
}

/**
 * Finds a region by name.
 *
 * Used by the map parsing code.
 * @param region_name Name of region.
 * @return Matching region, NULL if it can't be found.
 */
region_struct *region_find_by_name(const char *region_name)
{
    region_struct *region;

    for (region = first_region; region != NULL; region = region->next) {
        if (strcmp(region->name, region_name) == 0) {
            return region;
        }
    }

    LOG(BUG, "Got no region for region %s.", region_name);
    return NULL;
}

/**
 * Find a region that has a generated client map, searching parents as well.
 * @param region Region to start at.
 * @return Region or NULL if none found.
 */
const region_struct *region_find_with_map(const region_struct *region)
{
    HARD_ASSERT(region != NULL);

    for ( ; region != NULL; region = region->parent) {
        if (region->map_first != NULL) {
            break;
        }
    }

    return region;
}

/**
 * Gets the longname of a region.
 *
 * The longname of a region is not a required field, any given region may want
 * to not set it and use the parent's one instead.
 * @param region Region we're searching the longname.
 * @return Long name of a region if found. Will also search recursively in
 * parents. NULL is never returned, instead a fake region name is returned.
 */
char *region_get_longname(const region_struct *region)
{
    if (region->longname) {
        return region->longname;
    } else if (region->parent) {
        return region_get_longname(region->parent);
    }

    LOG(BUG, "Region %s has no parent and no longname.",
            region->name);
    return "no region name";
}

/**
 * Gets a message for a region.
 * @param region Region. Can't be NULL.
 * @return Message of a region if found. Will also search recursively in
 * parents. NULL is never returned, instead a fake region message is returned.
 */
char *region_get_msg(const region_struct *region)
{
    if (region->msg) {
        return region->msg;
    } else if (region->parent) {
        return region_get_msg(region->parent);
    }

    LOG(BUG, "Region %s has no parent and no msg.",
            region->name);
    return "no region message";
}

/**
 * Attempts to jail the specified object.
 * @param op Object we want to jail.
 * @return 1 if jailed successfully, 0 otherwise.
 */
int region_enter_jail(object *op)
{
    region_struct *region;
    mapstruct *m;

    if (op->map->region == NULL) {
        return 0;
    }

    for (region = op->map->region; region != NULL; region = region->parent) {
        if (region->jailmap == NULL) {
            continue;
        }

        m = ready_map_name(region->jailmap, NULL, 0);

        if (m == NULL) {
            LOG(BUG, "Could not load map '%s' (%d,%d).",
                    region->jailmap, region->jailx, region->jaily);
            return 0;
        }

        return object_enter_map(op, NULL, m, region->jailx, region->jaily, 1);
    }

    LOG(BUG, "No suitable jailmap for region %s was found.",
            op->map->region->name);
    return 0;
}
