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
 * Object management.
 */

#include <global.h>
#include <region_map.h>
#include <toolkit/packet.h>

/**
 * Pool for objects.
 */
static mempool_struct *pool_object;

/**
 * Initialize the object system.
 */
void object_init(void)
{
    toolkit_import(mempool);

    pool_object = mempool_create("objects", NROF_ITEMS, sizeof(object),
            MEMPOOL_ALLOW_FREEING, NULL, NULL, NULL, NULL);
    objects_init();
}

/**
 * Deinitialize the object system.
 */
void object_deinit(void)
{
    objects_deinit();
}

/**
 * Frees all objects in a list.
 * @param op
 * Start of the list.
 */
void objects_free(object *op)
{
    object *next;

    while (op) {
        if (op->itype == TYPE_SPELL) {
            spells_remove(op);
        } else if (op->itype == TYPE_SKILL) {
            skills_remove(op);
        } else if (op->itype == TYPE_FORCE || op->itype == TYPE_POISONING) {
            widget_active_effects_remove(cur_widget[ACTIVE_EFFECTS_ID], op);
        }

        if (op->inv) {
            objects_free(op->inv);
        }

        next = op->next;
        mempool_return(pool_object, op);
        op = next;
    }
}

/**
 * Find an object inside another object, but not inside inventories.
 * @param op
 * Object to search in.
 * @param tag
 * ID of the object we're looking for.
 * @return
 * Matching object if found, NULL otherwise.
 */
object *object_find_object_inv(object *op, tag_t tag)
{
    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->next) {
        if (tmp->tag == tag) {
            return op;
        }
    }

    return NULL;
}

/**
 * Find an object inside another object by its tag.
 * @param op
 * Object to search in.
 * @param tag
 * ID of the object we're looking for.
 * @return
 * Matching object if found, NULL otherwise.
 */
object *object_find_object(object *op, tag_t tag)
{
    for ( ; op != NULL; op = op->next) {
        if (op->tag == tag) {
            return op;
        } else if (op->inv != NULL) {
            object *tmp = object_find_object(op->inv, tag);
            if (tmp != NULL) {
                return tmp;
            }
        }
    }

    return NULL;
}

/**
 * Attempts to find an object by its tag, wherever it may be.
 * @param tag
 * Tag to look for.
 * @return
 * Matching object if found, NULL otherwise.
 */
object *object_find(tag_t tag)
{
    /* In interface GUI. */
    if (cpl.interface != NULL) {
        object *op = object_find_object(cpl.interface->inv, tag);
        if (op != NULL) {
            return op;
        }
    }

    /* Below the player. */
    if (cpl.below != NULL) {
        object *op = object_find_object(cpl.below, tag);
        if (op != NULL) {
            return op;
        }
    }

    /* Open container. */
    if (cpl.sack != NULL) {
        object *op = object_find_object(cpl.sack, tag);
        if (op != NULL) {
            return op;
        }
    }

    /* Last attempt, inside the player. */
    return object_find_object(cpl.ob, tag);
}

/**
 * Remove an object.
 * @param op
 * What to remove.
 */
void object_remove(object *op)
{
    if (op == NULL || op == cpl.ob || op == cpl.below) {
        return;
    }

    if (op->itype == TYPE_SPELL) {
        spells_remove(op);
    } else if (op->itype == TYPE_SKILL) {
        skills_remove(op);
    } else if (op->itype == TYPE_FORCE || op->itype == TYPE_POISONING) {
        widget_active_effects_remove(cur_widget[ACTIVE_EFFECTS_ID], op);
    }

    object_redraw(op);

    if (op->inv != NULL) {
        object_remove_inventory(op);
    }

    if (op->prev != NULL) {
        op->prev->next = op->next;
    } else if (op->env != NULL) {
        op->env->inv = op->next;
    }

    if (op->next != NULL) {
        op->next->prev = op->prev;
    }

    if (op->itype == TYPE_REGION_MAP) {
        region_map_fow_update(MapData.region_map);
        minimap_redraw_flag = 1;
    }

    mempool_return(pool_object, op);
}

/**
 * Remove all items in object's inventory.
 * @param op
 * The object to remove inventory of.
 */
void object_remove_inventory(object *op)
{
    if (!op) {
        return;
    }

    if (op == cpl.sack) {
        cpl.sack = NULL;
    }

    object_redraw(op);

    for (object *tmp = op->inv, *next; tmp != NULL; tmp = next) {
        next = tmp->next;

        if (tmp == cpl.sack) {
            continue;
        }

        object_remove(tmp);
    }
}

/**
 * Adds an object to inventory of 'env'.
 * @param env
 * Which object to add to.
 * @param op
 * Object to add.
 * @param bflag
 * If 1, the object will be added to the end of the
 * inventory instead of the start.
 */
static void object_add(object *env, object *op, int bflag)
{
    object *tmp;

    if (!op) {
        return;
    }

    if (!bflag) {
        op->next = env->inv;

        if (op->next) {
            op->next->prev = op;
        }

        op->prev = NULL;
        env->inv = op;
        op->env = env;
    } else {
        for (tmp = env->inv; tmp && tmp->next; tmp = tmp->next) {
        }

        op->next = NULL;
        op->prev = tmp;
        op->env = env;

        if (!tmp) {
            env->inv = op;
        } else {
            if (tmp->next) {
                tmp->next->prev = op;
            }

            tmp->next = op;
        }
    }
}

/**
 * Transfer the entire inventory of 'op' into 'to'.
 * @param op
 * Object to transfer the inventory of.
 * @param to
 * Object to receive the items.
 */
void object_transfer_inventory(object *op, object *to)
{
    for (object *tmp = op->inv, *next; tmp != NULL; tmp = next) {
        next = tmp->next;

        if (tmp->prev != NULL) {
            tmp->prev->next = tmp->next;
        } else if (tmp->env != NULL) {
            tmp->env->inv = tmp->next;
        }

        if (tmp->next != NULL) {
            tmp->next->prev = tmp->prev;
        }

        object_add(to, tmp, 1);
    }
}

/**
 * Creates a new object and inserts it into 'env'.
 * @param env
 * Which object to insert the created object into. Can be NULL
 * not to insert the created object anywhere.
 * @param tag
 * The object's ID.
 * @param bflag
 * If 1, the object will be added to the end of the
 * inventory instead of the start.
 * @return
 * The created object.
 */
object *object_create(object *env, tag_t tag, int bflag)
{
    object *op = mempool_get(pool_object);

    op->tag = tag;

    if (env != NULL) {
        object_add(env, op, bflag);
    }

    object_redraw(op);

    return op;
}

/**
 * Toggle the locked status of an object.
 * @param op
 * Object.
 */
void toggle_locked(object *op)
{
    packet_struct *packet;

    /* If object is on the ground, don't lock it. */
    if (!op || !op->env || op->env->tag == 0) {
        return;
    }

    packet = packet_new(SERVER_CMD_ITEM_LOCK, 8, 0);
    packet_append_uint32(packet, op->tag);
    socket_send_packet(packet);
}

/**
 * Update the marked object.
 * @param op
 * The object.
 */
void object_send_mark(object *op)
{
    packet_struct *packet;

    /* If object is on the ground, don't mark it. */
    if (!op || !op->env || op->env->tag == 0) {
        return;
    }

    if (cpl.mark_count == op->tag) {
        cpl.mark_count = 0;
    } else {
        cpl.mark_count = op->tag;
    }

    object_redraw(op);

    packet = packet_new(SERVER_CMD_ITEM_MARK, 8, 0);
    packet_append_uint32(packet, op->tag);
    socket_send_packet(packet);
}

void object_redraw(object *op)
{
    object *env;

    HARD_ASSERT(op != NULL);

    if (op->env == NULL) {
        return;
    }

    env = op->env;

    if (env == cpl.sack) {
        env = cpl.sack->env;
    }

    if (env == cpl.interface) {
        interface_redraw();
    } else if (env == cpl.below) {
        widget_redraw_type_id(INVENTORY_ID, "below");
    } else {
        widget_redraw_type_id(INVENTORY_ID, "main");
        /* TODO: This could be more sophisticated... */
        WIDGET_REDRAW_ALL(QUICKSLOT_ID);
    }
}

/**
 * Deinitialize the various objects of ::cpl structure.
 */
void objects_deinit(void)
{
    objects_free(cpl.below);
    objects_free(cpl.ob);
}

/**
 * Initializes the various objects of ::cpl structure.
 */
void objects_init(void)
{
    cpl.ob = mempool_get(pool_object);
    cpl.below = mempool_get(pool_object);

    cpl.below->weight = -111;
}

/**
 * Animate one object.
 * @param ob
 * The object to animate.
 * @return
 * 1 if the object changed face, 0 otherwise.
 */
int object_animate(object *ob)
{
    bool ret = false;

    if (ob->glow_speed > 1) {
        ob->glow_state++;

        if (ob->glow_state > ob->glow_speed) {
            ob->glow_state = 0;
        }

        ret = true;
    }

    if (ob->animation_id > 0) {
        check_animation_status(ob->animation_id);
    }

    if (ob->animation_id > 0 && ob->anim_speed) {
        ob->last_anim++;

        if (ob->last_anim >= ob->anim_speed) {
            if (++ob->anim_state >= animations[ob->animation_id].frame) {
                ob->anim_state = 0;
            }

            if (ob->direction > animations[ob->animation_id].facings) {
                ob->face = animations[ob->animation_id].faces[ob->anim_state];
            } else {
                ob->face = animations[ob->animation_id].faces[animations[ob->animation_id].frame * ob->direction + ob->anim_state];
            }

            ob->last_anim = 0;

            ret = true;
        }
    }

    return ret;
}

/**
 * Animate the inventory of an object.
 * @param op
 * The object, such as cpl.ob, cpl.below, etc.
 */
static void animate_inventory(object *op)
{
    object *tmp;

    for (tmp = op->inv; tmp != NULL; tmp = tmp->next) {
        if (!object_animate(tmp)) {
            continue;
        }

        /* Applied item inside the player, redraw the player doll -- most items
         * that can be applied are visible in the player doll. */
        if (op == cpl.ob && tmp->flags & CS_FLAG_APPLIED) {
            WIDGET_REDRAW_ALL(PDOLL_ID);
        }

        object_redraw(tmp);
    }
}

/**
 * Animate all possible objects.
 */
void animate_objects(void)
{
    animate_inventory(cpl.ob);
    animate_inventory(cpl.below);

    if (cpl.sack != NULL) {
        animate_inventory(cpl.sack);
    }

    if (cpl.interface != NULL) {
        animate_inventory(cpl.interface);
    }
}

/**
 * Draw the object, centering it. Animation offsets are taken into
 * account for perfect centering, even with different image sizes in
 * animation.
 *
 * @param surface
 * Surface to render on.
 * @param tmp
 * Object to show.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @param w
 * Maximum width.
 * @param h
 * Maximum height.
 * @param fit
 * Whether to fit the object into the maximum width/height by
 * zooming it as necessary.
 */
void object_show_centered (SDL_Surface *surface,
                           object      *tmp,
                           int          x,
                           int          y,
                           int          w,
                           int          h,
                           bool         fit)
{
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(tmp != NULL);

    if (FaceList[tmp->face].sprite == NULL) {
        return;
    }

    /* Will be used for coordinate calculations. */
    uint16_t face = tmp->face;

    /* If the item is animated, try to use the first animation face for
     * coordinate calculations to prevent 'jumping' of the animation. */
    if (tmp->animation_id > 0) {
        check_animation_status(tmp->animation_id);

        if (animations[tmp->animation_id].num_animations) {
            uint16_t face_id;

            face_id = animations[tmp->animation_id].frame * tmp->direction;

            if (FaceList[animations[tmp->animation_id].faces[face_id]].sprite) {
                face = animations[tmp->animation_id].faces[face_id];
            }
        }
    }

    int border_left = FaceList[face].sprite->border_left;
    int border_up = FaceList[face].sprite->border_up;
    if (tmp->glow[0] != '\0') {
        border_left -= SPRITE_GLOW_SIZE * 2;
        border_up -= SPRITE_GLOW_SIZE * 2;
    }

    int xlen = FaceList[face].sprite->bitmap->w - border_left -
               FaceList[face].sprite->border_right;
    int ylen = FaceList[face].sprite->bitmap->h - border_up -
               FaceList[face].sprite->border_down;
    if (tmp->glow[0] != '\0') {
        xlen += SPRITE_GLOW_SIZE * 2;
        ylen += SPRITE_GLOW_SIZE * 2;
    }

    double zoom_x = 1.0, zoom_y = 1.0;
    if (fit) {
        int xlen2 = xlen, ylen2 = ylen;

        if (xlen2 != w) {
            double factor = (double) w / xlen2;
            xlen2 *= factor;
            ylen2 *= factor;
        }

        if (ylen2 != h) {
            double factor = (double) h / ylen2;
            xlen2 *= factor;
            ylen2 *= factor;
        }

        if (xlen2 != xlen) {
            zoom_x = ((double) xlen2 + 0.5) / xlen;
            xlen = xlen2;
            border_left *= zoom_x;
        }

        if (ylen2 != ylen) {
            zoom_y = ((double) ylen2 + 0.5) / ylen;
            ylen = ylen2;
            border_up *= zoom_y;
        }
    }

    SDL_Rect box;
    if (xlen > w) {
        box.w = w;
        int temp = (xlen - w) / 2;
        box.x = border_left + temp;
        border_left = 0;
    } else {
        box.w = xlen;
        box.x = border_left;
        border_left = (w - xlen) / 2;
    }

    if (ylen > h) {
        box.h = h;
        int temp = (ylen - h) / 2;
        box.y = border_up + temp;
        border_up = 0;
    } else {
        box.h = ylen;
        box.y = border_up;
        border_up = (h - ylen) / 2;
    }

    if (face != tmp->face) {
        int temp = border_left - box.x;

        box.x = 0;
        box.w = FaceList[tmp->face].sprite->bitmap->w * zoom_x;
        border_left = temp;

        temp = border_up - box.y + (FaceList[face].sprite->bitmap->h * zoom_y -
                                    FaceList[tmp->face].sprite->bitmap->h *
                                    zoom_y);
        box.y = 0;
        box.h = FaceList[tmp->face].sprite->bitmap->h * zoom_y;
        border_up = temp;

        if (border_left < 0) {
            box.x = -border_left;
            box.w = FaceList[tmp->face].sprite->bitmap->w * zoom_x +
                    border_left;

            if (box.w > w) {
                box.w = w;
            }

            border_left = 0;
        } else {
            if (box.w + border_left > w) {
                box.w -= ((box.w + border_left) - w);
            }
        }

        if (border_up < 0) {
            box.y = -border_up;
            box.h = FaceList[tmp->face].sprite->bitmap->h * zoom_y + border_up;

            if (box.h > h) {
                box.h = h;
            }

            border_up = 0;
        } else {
            if (box.h + border_up > h) {
                box.h -= ((box.h + border_up) - h);
            }
        }
    }

    sprite_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    snprintf(VS(effects.glow), "%s", tmp->glow);
    effects.glow_speed = tmp->glow_speed;
    effects.glow_state = tmp->glow_state;
    effects.zoom_x = zoom_x * 100.0;
    effects.zoom_y = zoom_y * 100.0;

    if (effects.glow[0] != '\0') {
        BIT_SET(effects.flags, SPRITE_FLAG_DARK);
        effects.dark_level = 0;
    }

    surface_show_effects(surface, x + border_left, y + border_up, &box,
                         FaceList[tmp->face].sprite->bitmap, &effects);
}
