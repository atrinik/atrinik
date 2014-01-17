/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
            free(animations[i].faces);
        }

        free(animations);
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
    animations = malloc(10 * sizeof(Animations));
    /* set the name so we don't try to dereference null.
     * Put # at start so it will be first in alphabetical
     * order. */
    animations[0].name = NULL;
    FREE_AND_COPY_HASH(animations[0].name, "###none" );
    animations[0].num_animations = 1;
    animations[0].faces = malloc(sizeof(Fontindex));
    animations[0].faces[0] = 0;
    animations[0].facings = 0;

    snprintf(buf, sizeof(buf), "%s/animations", settings.libpath);

    if ((fp = fopen(buf, "r")) == NULL) {
        logger_print(LOG(ERROR), "Can not open animations file %s.", buf);
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
                logger_print(LOG(ERROR), "Didn't get a mina before %s.", buf);
                exit(1);
            }

            num_animations++;

            if (num_animations == animations_allocated) {
                animations = realloc(animations, sizeof(Animations) * (animations_allocated + 10));
                animations_allocated += 10;
            }

            animations[num_animations].name = NULL;
            FREE_AND_COPY_HASH(animations[num_animations].name, buf + 5);
            /* for bsearch */
            animations[num_animations].num = num_animations;
            animations[num_animations].facings = 1;
        }
        else if (!strncmp(buf, "mina", 4)) {
            animations[num_animations].faces = malloc(sizeof(Fontindex) * num_frames);

            for (i = 0; i < num_frames; i++) {
                animations[num_animations].faces[i] = faces[i];
            }

            animations[num_animations].num_animations = num_frames;

            if (num_frames % animations[num_animations].facings) {
                logger_print(LOG(DEBUG), "Animation %s frame numbers (%d) is not a multiple of facings (%d)", STRING_SAFE(animations[num_animations].name), num_frames, animations[num_animations].facings);
            }

            num_frames = 0;
        }
        else if (!strncmp(buf, "facings", 7)) {
            if (!(animations[num_animations].facings = atoi(buf + 7))) {
                logger_print(LOG(DEBUG), "Animation %s has 0 facings, line=%s", STRING_SAFE(animations[num_animations].name), buf);
                animations[num_animations].facings = 1;
            }

            if (animations[num_animations].facings != 9 && animations[num_animations].facings != 25) {
                logger_print(LOG(DEBUG), "Animation %s has invalid facings parameter (%d - allowed are 9 or 25 only).", STRING_SAFE(animations[num_animations].name), animations[num_animations].facings);
                animations[num_animations].facings = 1;
            }
        }
        else {
            if (!(faces[num_frames++] = find_face(buf, 0))) {
                logger_print(LOG(BUG), "Could not find face %s for animation %s", buf, STRING_SAFE(animations[num_animations].name));
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
static int anim_compare(Animations *a, Animations *b)
{
    return strcmp(a->name, b->name);
}

/**
 * Tries to find the animation ID that matches name.
 * @param name Animation name to find
 * @return ID of the animation if found, 0 otherwise (animation 0 is
 * initialized as the 'bug' face). */
int find_animation(char *name)
{
    Animations search, *match;

    search.name = name;

    match = (Animations *) bsearch(&search, animations, (num_animations + 1), sizeof(Animations), (void *) (int (*)())anim_compare);

    if (match) {
        return match->num;
    }

    logger_print(LOG(BUG), "Unable to find animation %s", STRING_SAFE(name));
    return 0;
}

/**
 * Updates the face variable of an object.
 * If the object is the head of a multi object, all objects are animated.
 * @param op Object to animate
 * @param count State count of the animation */
void animate_object(object *op, int count)
{
    int numfacing, numanim;
    /* Max animation state object should be drawn in */
    int max_state;
    /* starting index # to draw from */
    int base_state;
    int dir;
    New_Face *old_face;

    numanim = NUM_ANIMATIONS(op);
    numfacing = NUM_FACINGS(op);

    if (!op->animation_id || !numanim || op->head) {
        return;
    }

    /* An animation is not only changed by anim_speed.
     * If we turn the object by a teleporter for example, the direction & facing
     * can
     * change - outside the normal animation loop.
     * We have then to change the frame and not increase the state */
    if ((!QUERY_FLAG(op, FLAG_SLEEP) && !QUERY_FLAG(op, FLAG_PARALYZED))) {
        op->state += count;
    }

    if (!count) {
        if (op->type == PLAYER) {
            if (!CONTR(op)->anim_flags && op->anim_moving_dir == op->anim_moving_dir_last && op->anim_last_facing == op->anim_last_facing_last) {
                return;
            }
        }
        else {
            /* Object needs no update for moving */
            if (op->anim_enemy_dir == op->anim_enemy_dir_last && op->anim_moving_dir == op->anim_moving_dir_last && op->anim_last_facing == op->anim_last_facing_last) {
                return;
            }
        }
    }

    dir = op->direction;

    /* If object is turning, then max animation state is half through the
     * animations.  Otherwise, we can use all the animations. */
    max_state= numanim / numfacing;
    base_state = 0;

    /* 0: "stay" "non direction" face
     * 1-8: point of the compass the object is facing. */
    if (numfacing == 9) {
        base_state = dir * (numanim / 9);

        /* If beyond drawable states, reset */
        if (op->state >= max_state) {
            op->state = 0;
        }
    }
    /* that's the new extended animation: base_state is */
    /* 0:     that's the dying anim - "non direction" facing */
    /* 1-8:   guard/stand_still anim frames */
    /* 9-16:  move anim frames */
    /* 17-24: close fight anim frames */
    /* TODO: allow different number of faces in each frame */
    else if (numfacing >= 25) {
        if (op->type == PLAYER) {
            /* Check flags - perhaps we have hit something in close fight */
            if ((CONTR(op)->anim_flags & PLAYER_AFLAG_ADDFRAME || CONTR(op)->anim_flags & PLAYER_AFLAG_ENEMY) && !(CONTR(op)->anim_flags & PLAYER_AFLAG_FIGHT)) {
                /* Do swing animation, starting at frame 0 */
                op->state = 0;

                if (CONTR(op)->anim_flags & PLAYER_AFLAG_ENEMY) {
                    /* So we do one more swing */
                    CONTR(op)->anim_flags |= PLAYER_AFLAG_ADDFRAME;
                    /* So we do one swing */
                    CONTR(op)->anim_flags |= PLAYER_AFLAG_FIGHT;
                }
                else {
                    /* Only do ADDFRAME if we are still fighting something */
                    if (op->enemy && is_melee_range(op, op->enemy)) {
                        CONTR(op)->anim_flags |= PLAYER_AFLAG_FIGHT;
                    }

                    /* We do our additional frame*/
                    CONTR(op)->anim_flags &=~PLAYER_AFLAG_ADDFRAME;
                }

                /* Clear enemy, set fight */
                CONTR(op)->anim_flags &=~PLAYER_AFLAG_ENEMY;
            }

            /* Now setup the best animation for our action */
            if (CONTR(op)->anim_flags & PLAYER_AFLAG_FIGHT) {
                op->anim_enemy_dir_last = op->anim_enemy_dir;

                /* Test of moving when swing */
                if (op->anim_moving_dir != -1) {
                    /* Face in moving direction */
                    dir = op->anim_moving_dir;
                    op->anim_moving_dir_last = op->anim_moving_dir;
                }
                else {
                    /* Face to last direction we had done something */
                    if (op->anim_enemy_dir != -1) {
                        dir = op->anim_enemy_dir;
                    }
                    else {
                        dir = op->anim_last_facing;
                    }
                }

                /* If we have no idea where we faced, we face to enemy */
                if (!dir || dir == -1) {
                    dir = 4;
                }

                op->anim_last_facing = dir;
                op->anim_last_facing_last = -1;
                dir += 16;
            }
            /* Test of moving */
            else if (op->anim_moving_dir != -1) {
                /* Face in moving direction */
                dir = op->anim_moving_dir;
                op->anim_moving_dir_last = op->anim_moving_dir;
                op->anim_enemy_dir_last = -1;

                /* Same spot will be mapped to south dir */
                if (!dir) {
                    dir = 4;
                }

                op->anim_last_facing = dir;
                op->anim_last_facing_last = -1;
                dir += 8;
            }
            /* If nothing to do: object is doing nothing. Use original facing */
            else {
                /* Face to last direction we had done something */
                if (op->anim_enemy_dir != -1) {
                    dir = op->anim_enemy_dir;
                }
                else {
                    dir = op->anim_last_facing;
                }

                op->anim_last_facing_last = dir;

                /* Same spot will be mapped to south dir */
                if (!dir || dir == -1) {
                    op->anim_last_facing = dir = 4;
                }
            }

            base_state = dir * (numanim / numfacing);

            /* If beyond drawable states, reset */
            if (op->state >= max_state) {
                op->state = 0;
                /* Always clear fighting flag */
                CONTR(op)->anim_flags &= ~PLAYER_AFLAG_FIGHT;
            }
        }
        /* Monster and non player animations */
        else {
            /* Mob has targeted an enemy and faces him. When me move, we strafe
             * sidewards */
            if (op->anim_enemy_dir != -1 && (!QUERY_FLAG(op, FLAG_RUN_AWAY) && !QUERY_FLAG(op, FLAG_SCARED))) {
                /* Face to the enemy position */
                dir = op->anim_enemy_dir;
                op->anim_enemy_dir_last = op->anim_enemy_dir;
                op->anim_moving_dir_last = -1;

                /* Same spot will be mapped to south dir */
                if (!dir) {
                    dir = 4;
                }

                op->anim_last_facing = dir;
                op->anim_last_facing_last = -1;
                dir += 16;
            }
            /* Test of moving */
            else if (op->anim_moving_dir != -1) {
                /* Face in moving direction */
                dir = op->anim_moving_dir;
                op->anim_moving_dir_last = op->anim_moving_dir;
                op->anim_enemy_dir_last = -1;

                /* Same spot will be mapped to south dir */
                if (!dir) {
                    dir = 4;
                }

                op->anim_last_facing = dir;
                op->anim_last_facing_last = -1;
                dir += 8;
            }
            else {
                /* Face to last direction we had done something */
                dir = op->anim_last_facing;
                op->anim_last_facing_last = dir;

                /* Same spot will be mapped to south dir */
                if (!dir || dir == -1) {
                    op->anim_last_facing = dir = 4;
                }
            }

            base_state = dir * (numanim / numfacing);

            /* If beyond drawable states, reset */
            if (op->state >= max_state) {
                op->state = 0;
            }
        }
    }
    else {
        /* If beyond drawable states, reset */
        if (op->state >= max_state) {
            op->state = 0;
        }
    }

    old_face = op->face;
    SET_ANIMATION(op, op->state + base_state);

    if (op->face != old_face) {
        update_object(op, UP_OBJ_FACE);
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
