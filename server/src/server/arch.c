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
 * Arch related functions.
 */

#ifndef __CPROTO__

#include <global.h>
#include <arch.h>
#include <artifact.h>
#include <loader.h>
#include <toolkit_string.h>

/**
 * The archetype hash table.
 */
archetype_t *arch_table = NULL;
/**
 * Pointers to cached archetypes. Initialized from #arch_names.
 */
archetype_t *arches[ARCH_MAX] = {NULL};

/**
 * True if doing arch initialization.
 */
bool arch_in_init = false;

/**
 * Used to create archetype's clone object in arch_new(), to avoid
 * a lot of calls to object_get().
 *
 * Is allocated - and gets destroyed - in arch_init().
 */
static object *clone_op;

/**
 * Used to initialize the #arches array.
 */
static const char *const arch_names[ARCH_MAX] = {
    "waypoint", "empty_archetype", "base_info", "level_up", "ring_normal",
    "ring_generic", "amulet_normal", "amulet_generic", "inv_group_next",
    "inv_group_prev", "inv_start", "inv_end"
};

/* Prototypes */
static void arch_free(archetype_t *at);
static void arch_load(void);

/**
 * Initializes the archetypes hash table (read from file).
 *
 * #arches are also initialized.
 */
void arch_init(void)
{
    HARD_ASSERT(arch_table == NULL);

    clone_op = get_object();
    arch_in_init = true;
    arch_load();
    arch_in_init = false;
    object_destroy(clone_op);

    for (int i = 0; i < ARCH_MAX; i++) {
        arches[i] = arch_find(arch_names[i]);

        if (arches[i] == NULL) {
            LOG(ERROR, "Could not find required archetype %s.", arch_names[i]);
            exit(1);
        }
    }
}

/**
 * Frees all memory allocated to archetypes.
 *
 * After calling this, it's possible to call arch_init() again to
 * reload data.
 */
void arch_deinit(void)
{
    archetype_t *at, *tmp, *tail, *tail_tmp;
    HASH_ITER(hh, arch_table, at, tmp) {
        HASH_DEL(arch_table, at);

        for (tail = at; tail != NULL; tail = tail_tmp) {
            tail_tmp = tail->more;
            arch_free(tail);
        }
    }
}

/**
 * Allocates, initializes and returns the pointer to an archetype structure.
 * @return New archetype structure, never NULL.
 */
static archetype_t *arch_new(void)
{
    HARD_ASSERT(arch_in_init == true);

    archetype_t *new = ecalloc(1, sizeof(*new));
    memcpy(&new->clone, clone_op, sizeof(new->clone));

    return new;
}

/**
 * Frees the specified archetype structure.
 * @param at Archetype to free.
 */
static void arch_free(archetype_t *at)
{
    HARD_ASSERT(at != NULL);

    FREE_AND_CLEAR_HASH(at->name);
    object_destroy_inv(&at->clone);
    FREE_AND_CLEAR_HASH(at->clone.name);
    FREE_AND_CLEAR_HASH(at->clone.title);
    FREE_AND_CLEAR_HASH(at->clone.race);
    FREE_AND_CLEAR_HASH(at->clone.slaying);
    FREE_AND_CLEAR_HASH(at->clone.msg);
    FREE_AND_CLEAR_HASH(at->clone.custom_name);
    FREE_AND_CLEAR_HASH(at->clone.glow);
    free_key_values(&at->clone);
    efree(at);
}

/**
 * Reads/parses the archetype-file, and copies into a linked list
 * of archetype structures.
 * @param fp Opened file descriptor which will be used to read the
 * archetypes.
 */
static void arch_pass_first(FILE *fp)
{
    HARD_ASSERT(fp != NULL);

    archetype_t *at = arch_new();
    at->clone.arch = at;

    void *buffer = create_loader_buffer(fp);
    archetype_t *prev = NULL, *last_more = NULL;

    for (int rc = load_object_fp(fp, &at->clone, MAP_STYLE);
         rc != LL_EOF;
         rc = load_object_buffer(buffer, &at->clone, MAP_STYLE)) {
        switch (rc) {
        /* A new archetype, add it to the arch table. */
        case LL_NORMAL:
            arch_add(at);
            prev = last_more = at;
            break;

        /* Another part of the previous archetype, link it correctly. */
        case LL_MORE:
            at->head = prev;
            at->clone.head = &prev->clone;

            if (last_more != NULL) {
                last_more->more = at;
                last_more->clone.more = &at->clone;
            }

            last_more = at;
            break;
        }

        at = arch_new();
        at->clone.arch = at;
    }

    delete_loader_buffer(buffer);
    efree(at);
}

/**
 * Reads the archetype file once more, and links all pointers between
 * archetypes and treasure lists. Must be called after first_arch_pass().
 * @param fp File from which to read. Won't be rewinded.
 * @param filename Filename fp is being read from.
 */
static void arch_pass_second(FILE *fp, const char *filename)
{
    char buf[HUGE_BUF];
    uint64_t linenum = 0;
    const char *key = NULL, *value = NULL, *error_str = NULL;
    archetype_t *at = NULL;
    bool is_more = false;

    while (fgets(VS(buf), fp) != NULL) {
        linenum++;

        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        key = cps[0];
        value = cps[1];

        if (is_more) {
            if (strcmp(key, "end") == 0) {
                is_more = false;
            }
        } else if (strcmp(key, "Object") == 0) {
            at = arch_find(value);

            if (at == NULL) {
                error_str = "failed to find arch";
                goto error;
            }
        } else if (strcmp(key, "More") == 0) {
            is_more = true;
        } else if (at == NULL) {
            error_str = "expected Object attribute";
            goto error;
        } else if (strcmp(key, "end") == 0) {
            at = NULL;
        } else if (strcmp(key, "other_arch") == 0) {
            if (at->clone.other_arch == NULL) {
                archetype_t *other = arch_find(value);
                if (other == NULL) {
                    error_str = "failed to find other_arch";
                    goto error;
                }
                at->clone.other_arch = other;
            }
        } else if (strcmp(key, "randomitems") == 0) {
            treasurelist *tl = find_treasurelist(value);
            if (tl == NULL) {
                error_str = "failed to find treasure list";
                goto error;
            }
            at->clone.randomitems = tl;
        } else if (strcmp(key, "arch") == 0) {
            object *inv = arch_get(value);
            load_object_fp(fp, inv, 0);
            insert_ob_in_ob(inv, &at->clone);
        }
    }

    char filename2[MAX_BUF];
    snprintf(VS(filename2), "%s/artifacts", settings.libpath);
    fp = fopen(filename2, "rb");

    if (fp == NULL) {
        LOG(ERROR, "Can't open %s: %s (%d)", filename2, strerror(errno), errno);
        exit(1);
    }

    linenum = 0;
    filename = filename2;

    while (fgets(VS(buf), fp) != NULL) {
        linenum++;

        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        key = cps[0];
        value = cps[1];

        if (strcmp(key, "artifact") == 0) {
            at = arch_find(value);
            if (at == NULL) {
                error_str = "failed to find artifact";
                goto error;
            }
        } else if (at == NULL) {
            // Ignore
        } else if (strcmp(key, "end") == 0) {
            at = NULL;
        } else if (strcmp(key, "def_arch") == 0) {
            archetype_t *other = arch_find(value);
            if (other == NULL) {
                error_str = "failed to find def_arch";
                goto error;
            }
            at->clone.other_arch = other->clone.other_arch;
            at->clone.randomitems = other->clone.randomitems;
        } else if (strcmp(key, "other_arch") == 0) {
            archetype_t *other = arch_find(value);
            if (other == NULL) {
                error_str = "failed to find other_arch";
                goto error;
            }
            at->clone.other_arch = other;
        } else if (strcmp(key, "randomitems") == 0) {
            treasurelist *tl = find_treasurelist(value);
            if (tl == NULL) {
                error_str = "failed to find treasure list";
                goto error;
            }
            at->clone.randomitems = tl;
        }
    }

    fclose(fp);
    return;

error:
    LOG(ERROR, "Error parsing %s, line %" PRIu64 ", %s: %s %s", filename,
            linenum, error_str != NULL ? error_str : "", key != NULL ? key : "",
            value != NULL ? value : "");
    exit(1);
}

/**
 * Loads all archetypes, artifacts and treasures.
 *
 * Reads and parses the archetype file using arch_pass_first(), then initializes
 * the artifacts and treasures. Afterwards, calls arch_pass_second(), which
 * does the second pass initialization of archetypes and artifacts.
 */
static void arch_load(void)
{
    TIMER_START(1);

    char filename[MAX_BUF];
    snprintf(VS(filename), "%s/archetypes", settings.libpath);
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL) {
        LOG(ERROR, "Can't open archetype file %s: %s (%d)", filename,
                strerror(errno), errno);
        exit(1);
    }

    arch_pass_first(fp);
    rewind(fp);

    /* If not called before, reads all artifacts from file */
    artifact_init();
    load_treasures();

    arch_pass_second(fp, filename);
    fclose(fp);

    TIMER_UPDATE(1);
    LOG(DEVEL, "Archetype loading took %f seconds", TIMER_GET(1));
}

/**
 * Adds an archetype to the hashtable.
 *
 * Must be called within archetypes initialization time-frame
 * (arch_in_init == true).
 * @param at Archetype to add.
 */
void arch_add(archetype_t *at)
{
    HARD_ASSERT(at != NULL);
    HARD_ASSERT(arch_in_init == true);

    SOFT_ASSERT(at->name != NULL, "Archetype has no name.");

    HASH_ADD_KEYPTR(hh, arch_table, at->name, strlen(at->name), at);
}

/**
 * Finds, using the hashtable, which archetype matches the given name.
 * @param name Archetype name to find. Can be NULL.
 * @return Pointer to the found archetype, otherwise NULL.
 */
archetype_t *arch_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    archetype_t *at;
    HASH_FIND_STR(arch_table, name, at);
    return at;
}

/**
 * Finds which archetype matches the given name, and returns a new object
 * containing a copy of the archetype.
 *
 * If the archetype cannot be found, object_create_singularity() is used to
 * create a singularity. Thus the return value is never NULL.
 * @param name Archetype name. Can be NULL.
 * @return Object of specified archetype, or a singularity. Will never be
 * NULL.
 */
object *arch_get(const char *name)
{
    archetype_t *at = arch_find(name);
    if (at == NULL) {
        return object_create_singularity(name);
    }
    return arch_to_object(at);
}

/**
 * Creates and returns a new object which is a copy of the given archetype.
 * @param at Archetype from which to get an object.
 * @return New object, never NULL.
 */
object *arch_to_object(archetype_t *at)
{
    HARD_ASSERT(at != NULL);

    object *op = get_object();
    copy_object_with_inv(&at->clone, op);
    op->arch = at;
    return op;
}

/**
 * Clones the specified archetype.
 * @param at The archetype to clone.
 * @return New archetype.
 * @warning The archetype's name is not cloned.
 */
archetype_t *arch_clone(archetype_t *at)
{
    HARD_ASSERT(at != NULL);
    HARD_ASSERT(arch_in_init == true);

    archetype_t *new = ecalloc(1, sizeof(*new));
    memcpy(&new->clone, &at->clone, sizeof(new->clone));
    ADD_REF_NOT_NULL_HASH(new->clone.name);
    ADD_REF_NOT_NULL_HASH(new->clone.title);
    ADD_REF_NOT_NULL_HASH(new->clone.race);
    ADD_REF_NOT_NULL_HASH(new->clone.slaying);
    ADD_REF_NOT_NULL_HASH(new->clone.msg);
    new->clone.arch = new;
    return new;
}

#endif
