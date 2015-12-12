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
 * This file contains animation related code. */

#include <global.h>

Animations *animations = NULL;
int num_animations = 0, animations_allocated;

/**
 * Free all animations loaded */
void free_all_anim(void)
{
    int i;

    if (animations) {
        for (i = 0; i <= num_animations; i++) {
            FREE_AND_CLEAR_HASH(animations[i].name);
            efree(animations[i].faces);
        }

        efree(animations);
    }
}

/**
 * Initialize animations structure, read the animations
 * data from a file. */
void init_anim(void)
{
    char buf[MAX_BUF];
    FILE *fp;
    static int anim_init = 0;
    int num_frames = 0, faces[MAX_ANIMATIONS], i;

    if (anim_init) {
        return;
    }

    animations_allocated = 9;
    num_animations = 0;

    /* Make a default.  New animations start at one, so if something
     * thinks it is animated but hasn't set the animation_id properly,
     * it will have a default value that should be pretty obvious. */
    animations = emalloc(10 * sizeof(Animations));
    /* set the name so we don't try to dereference null.
     * Put # at start so it will be first in alphabetical
     * order. */
    animations[0].name = NULL;
    FREE_AND_COPY_HASH(animations[0].name, "###none" );
    animations[0].num_animations = 1;
    animations[0].faces = emalloc(sizeof(Fontindex));
    animations[0].faces[0] = 0;
    animations[0].facings = 0;

    snprintf(buf, sizeof(buf), "%s/animations", settings.libpath);

    if ((fp = fopen(buf, "r")) == NULL) {
        LOG(ERROR, "Can not open animations file %s.", buf);
        exit(1);
    }

    while (fgets(buf, MAX_BUF - 1, fp) != NULL) {
        if (*buf == '#') {
            continue;
        }

        /* Kill the newline */
        buf[strlen(buf) - 1] = '\0';

        if (!strncmp(buf, "anim ", 5)) {
            if (num_frames) {
                LOG(ERROR, "Didn't get a mina before %s.", buf);
                exit(1);
            }

            num_animations++;

            if (num_animations == animations_allocated) {
                animations = erealloc(animations, sizeof(Animations) * (animations_allocated + 10));
                animations_allocated += 10;
            }

            animations[num_animations].name = NULL;
            FREE_AND_COPY_HASH(animations[num_animations].name, buf + 5);
            /* for bsearch */
            animations[num_animations].num = num_animations;
            animations[num_animations].facings = 1;
        } else if (!strncmp(buf, "mina", 4)) {
            animations[num_animations].faces = emalloc(sizeof(Fontindex) * num_frames);

            for (i = 0; i < num_frames; i++) {
                animations[num_animations].faces[i] = faces[i];
            }

            animations[num_animations].num_animations = num_frames;

            if (num_frames % animations[num_animations].facings) {
                LOG(DEBUG, "Animation %s frame numbers (%d) is not a multiple of facings (%d)", STRING_SAFE(animations[num_animations].name), num_frames, animations[num_animations].facings);
            }

            num_frames = 0;
        } else if (!strncmp(buf, "facings", 7)) {
            if (!(animations[num_animations].facings = atoi(buf + 7))) {
                LOG(DEBUG, "Animation %s has 0 facings, line=%s", STRING_SAFE(animations[num_animations].name), buf);
                animations[num_animations].facings = 1;
            }

            if (animations[num_animations].facings != 9 && animations[num_animations].facings != 25) {
                LOG(DEBUG, "Animation %s has invalid facings parameter (%d - allowed are 9 or 25 only).", STRING_SAFE(animations[num_animations].name), animations[num_animations].facings);
                animations[num_animations].facings = 1;
            }
        } else {
            if (!(faces[num_frames++] = find_face(buf, 0))) {
                LOG(BUG, "Could not find face %s for animation %s", buf, STRING_SAFE(animations[num_animations].name));
            }
        }
    }

    fclose(fp);
}

/**
 * Compare two animations.
 *
 * Used for bsearch in find_animation().
 * @param a First animation to compare
 * @param b Second animation to compare
 * @return Return value of strcmp for the animation names */
static int anim_compare(const void *a, const void *b)
{
    return strcmp(((const Animations *) a)->name, ((const Animations *) b)->name);
}

/**
 * Tries to find the animation ID that matches name.
 * @param name Animation name to find
 * @return ID of the animation if found, 0 otherwise (animation 0 is
 * initialized as the 'bug' face). */
int find_animation(const char *name)
{
    Animations search, *match;

    search.name = (char *) name;

    match = bsearch(&search, animations, (num_animations + 1), sizeof(Animations), anim_compare);

    if (match) {
        return match->num;
    }

    LOG(BUG, "Unable to find animation %s", STRING_SAFE(name));
    return 0;
}

/**
 * Update the object's animation state.
 * @param op Object. */
void animate_object(object *op)
{
    if (op->animation_id == 0 || NUM_ANIMATIONS(op) == 0 || op->head != NULL) {
        return;
    }

    op->state += 1;

    /* If beyond drawable states, reset */
    if (op->state >= NUM_ANIMATIONS(op) / NUM_FACINGS(op)) {
        op->state = 0;
    }
}

/**
 * Animates one step of object.
 * @param op Object to animate. */
void animate_turning(object *op)
{
    SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction) + op->state);
    update_object(op, UP_OBJ_FACE);
}
