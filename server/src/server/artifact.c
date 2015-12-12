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
 * Everything concerning artifacts.
 */

#ifndef __CPROTO__

#include <global.h>
#include <loader.h>
#include <toolkit_string.h>
#include <arch.h>
#include <artifact.h>

/**
 * How many times to try to generate an artifact from an artifact list.
 */
#define ARTIFACT_TRIES 2

/* Prototypes */
static void artifact_list_free(artifact_list_t *al);
static void artifact_load(void);

/**
 * Initializes artifacts code.
 */
void artifact_init(void)
{
    artifact_load();
}

/**
 * Deinitializes artifacts code.
 */
void artifact_deinit(void)
{
    artifact_list_t *al, *tmp;
    LL_FOREACH_SAFE(first_artifactlist, al, tmp) {
        artifact_list_free(al);
    }
}

/**
 * Allocate and return the pointer to an empty artifact structure.
 * @return New structure.
 */
static artifact_t *artifact_new(void)
{
    artifact_t *art = ecalloc(1, sizeof(*art));
    return art;
}

/**
 * Frees an artifact structures.
 * @param art Artifact to free.
 */
static void artifact_free(artifact_t *art)
{
    HARD_ASSERT(art != NULL);

    free_string_shared(art->def_at_name);

    SHSTR_LIST_CLEAR(art->allowed);

    if (art->parse_text != NULL) {
        efree(art->parse_text);
    }

    efree(art);
}

/**
 * Allocate and return the pointer to an empty artifact_list_t structure.
 * @return New structure.
 */
static artifact_list_t *artifact_list_new(void)
{
    artifact_list_t *al = ecalloc(1, sizeof(*al));
    return al;
}

/**
 * Frees the specified artifact list, its artifacts, and all linked artifact
 * lists.
 * @param al Artifact list.
 */
static void artifact_list_free(artifact_list_t *al)
{
    artifact_t *art, *tmp;
    LL_FOREACH_SAFE(al->items, art, tmp) {
        artifact_free(art);
    }

    efree(al);
}

/**
 * Builds up the lists of artifacts from the file in the libpath.
 */
void artifact_load(void)
{
    char filename[MAX_BUF];
    snprintf(VS(filename), "%s/artifacts", settings.libpath);
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        LOG(ERROR, "Can't open %s.", filename);
        exit(1);
    }

    char buf[HUGE_BUF];
    uint64_t linenum = 0;
    artifact_t *art = NULL;
    shstr *name = NULL;
    bool allowed_none = false;

    while (fgets(VS(buf), fp) != NULL) {
        linenum++;

        char *cp = string_skip_whitespace(buf), *end = strchr(cp, '\n');
        if (end != NULL) {
            *end = '\0';
        }

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        const char *key = cps[0], *value = cps[1], *error_str = NULL;

        /* We have a single artifact */
        if (strcmp(key, "Allowed") == 0) {
            if (art != NULL) {
                error_str = "duplicated Allowed attribute";
                goto error;
            }

            art = artifact_new();
            allowed_none = false;

            if (strcmp(value, "all") == 0) {
                continue;
            }

            if (strcmp(value, "none") == 0) {
                allowed_none = true;
                continue;
            }

            char word[MAX_BUF], *word2;
            size_t pos = 0;
            while (string_get_word(value, &pos, ',', VS(word), 0)) {
                word2 = word;
                if (*word == '!') {
                    word2++;
                    art->disallowed = true;
                }
                SHSTR_LIST_PREPEND(art->allowed, word2);
            }
        } else if (art == NULL) {
            error_str = "expected Allowed attribute";
            goto error;
        } else if (strcmp(key, "t_style") == 0) {
            if (!string_isdigit(value)) {
                error_str = "t_style attribute expects a number";
                goto error;
            }

            art->t_style = atoi(value);
        } else if (strcmp(key, "chance") == 0) {
            if (!string_isdigit(value)) {
                error_str = "chance attribute expects a number";
                goto error;
            }

            int val = atoi(value);
            if (val < 0 || val > UINT16_MAX) {
                error_str = "invalid value for chance attribute";
                goto error;
            }

            art->chance = (uint16_t) val;
        } else if (strcmp(key, "difficulty") == 0) {
            if (!string_isdigit(value)) {
                error_str = "difficulty attribute expects a number";
                goto error;
            }

            int val = atoi(value);
            if (val < 0 || val > UINT8_MAX) {
                error_str = "invalid value for difficulty attribute";
                goto error;
            }

            art->difficulty = (uint8_t) val;
        } else if (strcmp(key, "artifact") == 0) {
            if (name != NULL) {
                error_str = "duplicated artifact attribute";
                goto error;
            }

            name = add_string(value);
        } else if (strcmp(key, "copy_artifact") == 0) {
            if (KEYWORD_IS_TRUE(value)) {
                art->copy_artifact = true;
            } else if (KEYWORD_IS_FALSE(value)) {
                art->copy_artifact = false;
            } else {
                error_str = "invalid value for copy_artifact attribute";
                goto error;
            }
        } else if (strcmp(key, "def_arch") == 0) {
            if (art->def_at != NULL) {
                error_str = "duplicated def_arch attribute";
                goto error;
            }

            archetype_t *at = arch_find(value);
            if (at == NULL) {
                error_str = "unknown archetype";
                goto error;
            }

            art->def_at = arch_clone(at);
            art->def_at_name = add_string(value);
        } else if (strcmp(key, "Object") == 0) {
            if (name == NULL) {
                error_str = "artifact is missing name";
                goto error;
            }

            if (art->def_at == NULL) {
                error_str = "artifact is missing def_arch";
                goto error;
            }

            if (art->chance == 0) {
                error_str = "artifact has no chance set";
                goto error;
            }

            long old_pos = ftell(fp);
            if (old_pos == -1) {
                LOG(ERROR, "ftell() failed: %s (%d)", strerror(errno), errno);
                error_str = "general failure";
                goto error;
            }

            if (load_object_fp(fp,
                               &art->def_at->clone,
                               MAP_STYLE) != LL_NORMAL) {
                error_str = "could not load object";
                goto error;
            }

            /* This should never happen, because arch name is normally set
             * by the Flex loader only if it encounters "object xxx" */
            if (art->def_at->name != NULL) {
                error_str = "artifact def_at already has a name";
                goto error;
            }

            art->def_at->name = name;

            long file_pos = ftell(fp);
            if (file_pos == -1) {
                LOG(ERROR, "ftell() failed: %s (%d)", strerror(errno), errno);
                error_str = "general failure";
                goto error;
            }

            if (fseek(fp, old_pos, SEEK_SET) != 0) {
                LOG(ERROR, "Could not fseek() to %ld: %s (%d)", old_pos,
                        strerror(errno), errno);
                error_str = "general failure";
                goto error;
            }

            StringBuffer *sb = stringbuffer_new();
            while (fgets(VS(buf), fp) != NULL) {
                stringbuffer_append_string(sb, buf);

                long pos = ftell(fp);
                if (pos == -1) {
                    LOG(ERROR, "ftell() failed: %s (%d)", strerror(errno),
                            errno);
                    error_str = "general failure";
                    goto error;
                }

                if (pos == file_pos) {
                    break;
                }

                if (pos > file_pos) {
                    LOG(ERROR, "fgets() read too much data, at: %ld, should "
                            "be: %ld", pos, file_pos);
                    error_str = "general failure";
                    goto error;
                }
            }

            /* Flex loader needs an extra NUL at the end. */
            stringbuffer_append_char(sb, '\0');
            art->parse_text = stringbuffer_finish(sb);

            /* Determine the type; if 'Allowed none', then type is 0. */
            uint8_t type = allowed_none ? 0 : art->def_at->clone.type;

            /* Add it to the appropriate artifact list */
            artifact_list_t *al = artifact_list_find(type);
            if (al == NULL) {
                al = artifact_list_new();
                al->type = type;
                LL_PREPEND(first_artifactlist, al);
            }

            LL_PREPEND(al->items, art);
            arch_add(art->def_at);

            art = NULL;
            name = NULL;
        } else {
            error_str = "unrecognized attribute";
            goto error;
        }

        continue;
error:
        LOG(ERROR, "Error parsing %s, line %" PRIu64 ", %s: %s %s", filename,
                linenum, error_str != NULL ? error_str : "",
                key != NULL ? key : "", value != NULL ? value : "");
        exit(1);
    }

    fclose(fp);

    if (art != NULL) {
        LOG(ERROR, "Artifacts file has no end: %s", filename);
        exit(1);
    }

    artifact_list_t *al;
    LL_FOREACH(first_artifactlist, al) {
        if (al->type == 0) {
            continue;
        }

        LL_FOREACH(al->items, art) {
            al->total_chance += art->chance;
        }
    }
}

/**
 * Searches the artifact lists and returns one that has the same type of
 * objects on it.
 * @param type Type to search for.
 * @return NULL if no suitable list found.
 */
artifact_list_t *artifact_list_find(uint8_t type)
{
    for (artifact_list_t *al = first_artifactlist; al != NULL; al = al->next) {
        if (al->type == type) {
            return al;
        }
    }

    return NULL;
}

/**
 * Find an artifact by its name and type (as there are several lists of
 * artifacts, depending on their types).
 * @param name Name of the artifact to find.
 * @param type Type of the artifact to find.
 * @return The artifact if found, NULL otherwise.
 */
artifact_t *artifact_find_type(const char *name, uint8_t type)
{
    HARD_ASSERT(name != NULL);

    artifact_list_t *al = artifact_list_find(type);
    if (al == NULL) {
        return NULL;
    }

    for (artifact_t *art = al->items; art != NULL; art = art->next) {
        if (strcmp(art->def_at->name, name) == 0) {
            return art;
        }
    }

    return NULL;
}

/**
 * Fixes the given object, giving it the abilities and titles it should
 * have due to the artifact template.
 * @param art The artifact.
 * @param op The object to change.
 */
void artifact_change_object(artifact_t *art, object *op)
{
    if (art->copy_artifact) {
        copy_object_with_inv(&art->def_at->clone, op);
        return;
    }

    int64_t tmp_value = op->value;
    op->value = 0;

    if (load_object(art->parse_text, op, MAP_ARTIFACT) != LL_NORMAL) {
        LOG(ERROR, "load_object() error, art: %s, object: %s",
            art->def_at->name, object_get_str(op));
    }

    FREE_AND_ADD_REF_HASH(op->artifact, art->def_at->name);

    /* This will solve the problem to adjust the value for different
     * items of same artification. Also we can safely use negative
     * values. */
    op->value += tmp_value;

    if (op->value < 0) {
        op->value = 0;
    }
}

/**
 * Checks if op can be combined with art, depending on 'Allowed xxx' from
 * the artifacts file (stored in artifact::allowed), the difficulty, etc.
 * @param art Artifact.
 * @param op The object to check.
 * @param difficulty Difficulty.
 * @param t_style Treasure style value to check.
 * @return Whether the object can be combined with the artifact.
 */
static bool artifact_can_combine(artifact_t *art, object *op, int difficulty,
        int t_style)
{
    if (difficulty < art->difficulty) {
        return false;
    }

    if (t_style == -1 && art->t_style != 0 && art->t_style != T_STYLE_UNSET) {
        return false;
    }

    if (t_style != 0 && art->t_style != 0 && art->t_style != t_style &&
            art->t_style != T_STYLE_UNSET) {
        return false;
    }

    /* 'Allowed all' */
    if (art->allowed == NULL) {
        return true;
    }

    if (op->arch == NULL) {
        return false;
    }

    bool ret = art->disallowed;
    SHSTR_LIST_FOR_PREPARE(art->allowed, name) {
        if (op->arch->name == name) {
            return !ret;
        }
    } SHSTR_LIST_FOR_FINISH();

    return ret;
}

/**
 * Decides randomly which artifact the object should be turned into.
 * Makes sure that the item can become that artifact (means magic, difficulty,
 * and Allowed fields properly). Then calls artifact_change_object() in order
 * to actually create the artifact.
 * @param op Object.
 * @param difficulty Difficulty.
 * @param t_style Treasure style.
 * @param a_chance Artifact chance.
 * @return Whether the object was turned into an artifact.
 */
bool artifact_generate(object *op, int difficulty, int t_style, int a_chance)
{
    artifact_list_t *al = artifact_list_find(op->type);
    if (al == NULL) {
        return 0;
    }

    /* Now we overrule unset to 0 */
    if (t_style == T_STYLE_UNSET) {
        t_style = 0;
    }

    for (int i = 0; i < ARTIFACT_TRIES; i++) {
        int roll = rndm(0, al->total_chance - 1);

        artifact_t *art;
        for (art = al->items; art != NULL; art = art->next) {
            roll -= art->chance;

            if (roll < 0) {
                break;
            }
        }

        if (art == NULL || roll >= 0) {
            LOG(ERROR, "Could not find artifact type %d", op->type);
            return false;
        }

        if (!artifact_can_combine(art, op, difficulty, t_style)) {
            continue;
        }

        artifact_change_object(art, op);
        return true;
    }

    /* If we are here then we failed to generate an artifact by chance. */
    if (a_chance > 0) {
        for (artifact_t *art = al->items; art != NULL; art = art->next) {
            if (art->chance <= 0) {
                continue;
            }

            if (!artifact_can_combine(art, op, difficulty, t_style)) {
                continue;
            }

            artifact_change_object(art, op);
            return true;
        }
    }

    return false;
}

#endif
