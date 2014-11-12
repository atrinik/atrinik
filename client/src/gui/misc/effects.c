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
 * Map effects handling.
 *
 * @author Alex Tokar */

#include <global.h>

/** Linked list of possible effects. */
static effect_struct *effects = NULL;
/** Current effect. */
static effect_struct *current_effect = NULL;
/** RGBA as lowercase overlay color names. */
static const char *overlay_cols[] = {"r", "g", "b", "a"};

/**
 * Initialize effects from file. */
void effects_init(void)
{
    FILE *fp;
    char buf[MAX_BUF], *cp;
    effect_struct *effect = NULL;
    effect_sprite_def *sprite_def = NULL;
    effect_overlay *overlay = NULL;

    /* Try to deinitialize all effects first. */
    effects_deinit();

    fp = server_file_open_name(SERVER_FILE_EFFECTS);

    if (!fp) {
        return;
    }

    /* Read the file... */
    while (fgets(buf, sizeof(buf) - 1, fp)) {
        /* Ignore comments and blank lines. */
        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }

        /* End a previous 'effect xxx' or 'sprite xxx' block. */
        if (!strcmp(buf, "end\n")) {
            /* Inside effect block. */
            if (effect) {
                /* Inside sprite block. */
                if (sprite_def) {
                    /* Add this sprite to the linked list. */
                    sprite_def->next = effect->sprite_defs;
                    effect->sprite_defs = sprite_def;
                    /* Update total chance value. */
                    effect->chance_total += sprite_def->chance;
                    sprite_def = NULL;
                }
                /* Overlay block, just set it as the effect's overlay. */
                else if (overlay) {
                    effect->overlay = overlay;
                    overlay = NULL;
                }
                /* Inside effect block. */
                else {
                    /* Add this effect to the linked list of effects. */
                    effect->next = effects;
                    effects = effect;
                    effect = NULL;
                }
            }

            continue;
        }

        cp = strrchr(buf, '\n');

        /* Eliminate newline. */
        if (cp) {
            *cp = '\0';
        }

        /* Parse definitions inside sprite block. */
        if (sprite_def) {
            if (!strncmp(buf, "chance ", 7)) {
                sprite_def->chance = atoi(buf + 7);
            }
            else if (!strncmp(buf, "weight ", 7)) {
                sprite_def->weight = atof(buf + 7);
            }
            else if (!strncmp(buf, "weight_mod ", 11)) {
                sprite_def->weight_mod = atof(buf + 11);
            }
            else if (!strncmp(buf, "delay ", 6)) {
                sprite_def->delay = atoi(buf + 6);
            }
            else if (!strncmp(buf, "wind ", 5)) {
                sprite_def->wind = atoi(buf + 5);
            }
            else if (!strncmp(buf, "wiggle ", 7)) {
                sprite_def->wiggle = atof(buf + 7);
            }
            else if (!strncmp(buf, "wind_mod ", 9)) {
                sprite_def->wind_mod = atof(buf + 9);
            }
            else if (!strncmp(buf, "x ", 2)) {
                sprite_def->x = atoi(buf + 2);
            }
            else if (!strncmp(buf, "y ", 2)) {
                sprite_def->y = atoi(buf + 2);
            }
            else if (!strncmp(buf, "xpos ", 5)) {
                sprite_def->xpos = atoi(buf + 5);
            }
            else if (!strncmp(buf, "ypos ", 5)) {
                sprite_def->ypos = atoi(buf + 5);
            }
            else if (!strncmp(buf, "reverse ", 8)) {
                sprite_def->reverse = atoi(buf + 8);
                sprite_def->y_mod = -sprite_def->y_mod;
            }
            else if (!strncmp(buf, "y_rndm ", 7)) {
                sprite_def->y_rndm = atof(buf + 7);
            }
            else if (!strncmp(buf, "x_mod ", 6)) {
                sprite_def->x_mod = atof(buf + 6);
            }
            else if (!strncmp(buf, "y_mod ", 6)) {
                sprite_def->y_mod = atof(buf + 6);
            }
            else if (!strncmp(buf, "x_check_mod ", 12)) {
                sprite_def->x_check_mod = atoi(buf + 12);
            }
            else if (!strncmp(buf, "y_check_mod ", 12)) {
                sprite_def->y_check_mod = atoi(buf + 12);
            }
            else if (!strncmp(buf, "kill_sides ", 11)) {
                sprite_def->kill_side_left = sprite_def->kill_side_right = atoi(buf + 11);
            }
            else if (!strncmp(buf, "kill_side_left ", 15)) {
                sprite_def->kill_side_left = atoi(buf + 15);
            }
            else if (!strncmp(buf, "kill_side_right ", 16)) {
                sprite_def->kill_side_right = atoi(buf + 16);
            }
            else if (!strncmp(buf, "zoom ", 5)) {
                sprite_def->zoom = atoi(buf + 5);
            }
            else if (!strncmp(buf, "warp_sides ", 11)) {
                sprite_def->warp_sides = atoi(buf + 11);
            }
            else if (!strncmp(buf, "ttl ", 4)) {
                sprite_def->ttl = atoi(buf + 4);
            }
            else if (!strncmp(buf, "sound_file ", 11)) {
                strncpy(sprite_def->sound_file, buf + 11, sizeof(sprite_def->sound_file) - 1);
                sprite_def->sound_file[sizeof(sprite_def->sound_file) - 1] = '\0';
            }
            else if (!strncmp(buf, "sound_volume ", 13)) {
                sprite_def->sound_volume = atoi(buf + 13);
            }
        }
        else if (!strcmp(buf, "overlay")) {
            size_t col;

            overlay = ecalloc(1, sizeof(effect_overlay));

            for (col = 0; col < arraysize(overlay_cols); col++) {
                overlay->col[col].val = -1;
                overlay->col[col].mod[4] = 1.0;
            }
        }
        else if (overlay) {
            size_t col;

            for (col = 0; col < arraysize(overlay_cols); col++) {
                if (!strncmp(buf, overlay_cols[col], strlen(overlay_cols[col]))) {
                    if (!strncmp(buf + 1, "_rndm_min ", 10)) {
                        overlay->col[col].rndm_min = atoi(buf + 11);
                    }
                    else if (!strncmp(buf + 1, "_rndm_max ", 10)) {
                        overlay->col[col].rndm_max = atoi(buf + 11);
                    }
                    else if (!strncmp(buf + 1, "_mod1 ", 6)) {
                        overlay->col[col].mod[0] = atof(buf + 7);
                    }
                    else if (!strncmp(buf + 1, "_mod2 ", 6)) {
                        overlay->col[col].mod[1] = atof(buf + 7);
                    }
                    else if (!strncmp(buf + 1, "_mod3 ", 6)) {
                        overlay->col[col].mod[2] = atof(buf + 7);
                    }
                    else if (!strncmp(buf + 1, "_mod4 ", 6)) {
                        overlay->col[col].mod[3] = atof(buf + 7);
                    }
                    else if (!strncmp(buf + 1, "_mod5 ", 6)) {
                        overlay->col[col].mod[4] = atof(buf + 7);
                    }
                    else {
                        overlay->col[col].val = atoi(buf + 2);
                    }
                }
            }
        }
        /* Parse definitions inside effect block. */
        else if (effect) {
            if (!strncmp(buf, "wind_chance ", 12)) {
                effect->wind_chance = atof(buf + 12);
            }
            else if (!strncmp(buf, "sprite_chance ", 14)) {
                effect->sprite_chance = atof(buf + 14);
            }
            else if (!strncmp(buf, "delay ", 6)) {
                effect->delay = atoi(buf + 6);
            }
            else if (!strncmp(buf, "wind_blow_dir ", 14)) {
                effect->wind_blow_dir = atoi(buf + 14);
            }
            else if (!strncmp(buf, "max_sprites ", 12)) {
                effect->max_sprites = atoi(buf + 12);
            }
            else if (!strncmp(buf, "wind_mod ", 9)) {
                effect->wind_mod = atof(buf + 9);
            }
            else if (!strncmp(buf, "sprites_per_move ", 17)) {
                effect->sprites_per_move = atoi(buf + 17);
            }
            else if (!strncmp(buf, "sound_effect ", 13)) {
                strncpy(effect->sound_effect, buf + 13, sizeof(effect->sound_effect) - 1);
                effect->sound_effect[sizeof(effect->sound_effect) - 1] = '\0';
            }
            else if (!strncmp(buf, "sound_volume ", 13)) {
                effect->sound_volume = atoi(buf + 13);
            }
            /* Start of sprite block. */
            else if (!strncmp(buf, "sprite ", 7)) {
                sprite_def = ecalloc(1, sizeof(*sprite_def));
                /* Store the sprite ID and name. */
                sprite_def->id = get_bmap_id(buf + 7);
                sprite_def->name = estrdup(buf + 7);
                /* Initialize default values. */
                sprite_def->chance = 1;
                sprite_def->weight = 1.0;
                sprite_def->weight_mod = 2.0;
                sprite_def->wind = 1;
                sprite_def->wiggle = 1.0;
                sprite_def->wind_mod = 1.0;
                sprite_def->x = -1;
                sprite_def->y = -1;
                sprite_def->reverse = 0;
                sprite_def->y_rndm = 60.0;
                sprite_def->x_mod = 1.0;
                sprite_def->y_mod = 1.0;
                sprite_def->x_check_mod = 1;
                sprite_def->y_check_mod = 1;
                sprite_def->kill_side_left = 1;
                sprite_def->kill_side_right = 0;
                sprite_def->zoom = 0;
                sprite_def->warp_sides = 1;
                sprite_def->ttl = 0;
                sprite_def->sound_volume = 100;
            }
        }
        /* Start of effect block. */
        else if (!strncmp(buf, "effect ", 7)) {
            effect = ecalloc(1, sizeof(effect_struct));
            /* Store the effect unique name. */
            strncpy(effect->name, buf + 7, sizeof(effect->name) - 1);
            effect->name[sizeof(effect->name) - 1] = '\0';
            /* Initialize default values. */
            effect->wind_chance = 0.98;
            effect->sprite_chance = 60.0;
            effect->wind_blow_dir = WIND_BLOW_RANDOM;
            effect->wind_mod = 1.0;
            effect->max_sprites = -1;
            effect->sprites_per_move = 1;
            effect->sound_channel = -1;
            effect->sound_volume = 100;
        }
    }

    /* Close the file. */
    fclose(fp);
}

/**
 * Deinitialize ::effects linked list. */
void effects_deinit(void)
{
    effect_struct *effect, *effect_next;
    effect_sprite_def *sprite_def, *sprite_def_next;

    /* Deinitialize all effects. */
    for (effect = effects; effect; effect = effect_next) {
        effect_next = effect->next;

        /* Deinitialize the effect's sprite definitions. */
        for (sprite_def = effect->sprite_defs; sprite_def; sprite_def = sprite_def_next) {
            sprite_def_next = sprite_def->next;
            effect_sprite_def_free(sprite_def);
        }

        /* Deinitialize the shown sprites and the actual effect. */
        effect_sprites_free(effect);
        effect_free(effect);
    }

    effects = current_effect = NULL;
}

/**
 * Makes sure all sprite definitions have correct sprite IDs and their
 * images are properly loaded. */
void effects_reinit(void)
{
    effect_struct *effect;
    effect_sprite_def *sprite_def;

    for (effect = effects; effect; effect = effect->next) {
        for (sprite_def = effect->sprite_defs; sprite_def; sprite_def = sprite_def->next) {
            sprite_def->id = get_bmap_id(sprite_def->name);
        }
    }
}

/**
 * Deinitialize shown sprites of a single effect.
 * @param effect The effect to have shown sprites deinitialized. */
void effect_sprites_free(effect_struct *effect)
{
    effect_sprite *tmp, *next;

    for (tmp = effect->sprites; tmp; tmp = next) {
        next = tmp->next;
        effect_sprite_free(tmp);
    }

    effect->sprites = effect->sprites_end = NULL;

    if (effect->sound_channel != -1) {
#ifdef HAVE_SDL_MIXER
        Mix_HaltChannel(effect->sound_channel);
#endif
        effect->sound_channel = -1;
    }
}

/**
 * Deinitialize a single effect.
 * @param effect Effect that will be freed. */
void effect_free(effect_struct *effect)
{
    if (effect->overlay) {
        efree(effect->overlay);
    }

    efree(effect);
}

/**
 * Deinitialize a single sprite definition.
 * @param sprite_def Sprite definition that will be freed. */
void effect_sprite_def_free(effect_sprite_def *sprite_def)
{
    efree(sprite_def->name);
    efree(sprite_def);
}

/**
 * Deinitialize a single shown sprite.
 * @param sprite Sprite that will be freed. */
void effect_sprite_free(effect_sprite *sprite)
{
    efree(sprite);
}

/**
 * Remove a single shown sprite from the linked list and free it.
 * @param sprite Sprite to remove and free. */
void effect_sprite_remove(effect_sprite *sprite)
{
    if (!sprite || !current_effect) {
        return;
    }

    if (sprite->prev) {
        sprite->prev->next = sprite->next;
    }
    else {
        current_effect->sprites = sprite->next;
    }

    if (sprite->next) {
        sprite->next->prev = sprite->prev;
    }
    else {
        current_effect->sprites_end = sprite->prev;
    }

    effect_sprite_free(sprite);
}

/**
 * Allocate a new sprite object and add it to the link of currently shown
 * sprites.
 *
 * A random sprite definition object will be chosen.
 * @param effect Effect this is being done for.
 * @return The created sprite. */
static effect_sprite *effect_sprite_create(effect_struct *effect)
{
    int roll;
    effect_sprite_def *tmp;
    effect_sprite *sprite;

    if (!effect->sprite_defs) {
        return NULL;
    }

    /* Choose which sprite to use. */
    roll = rndm(1, effect->chance_total) - 1;

    for (tmp = effect->sprite_defs; tmp; tmp = tmp->next) {
        roll -= tmp->chance;

        if (roll < 0) {
            break;
        }
    }

    if (!tmp) {
        return NULL;
    }

    /* Allocate a new sprite. */
    sprite = ecalloc(1, sizeof(*sprite));
    sprite->def = tmp;

    /* Add it to the linked list. */
    if (!effect->sprites) {
        effect->sprites = effect->sprites_end = sprite;
    }
    else {
        effect->sprites_end->next = sprite;
        sprite->prev = effect->sprites_end;
        effect->sprites_end = sprite;
    }

    return sprite;
}

/**
 * Try to play effect sprites. */
void effect_sprites_play(void)
{
    effect_sprite *tmp, *next;
    int num_sprites = 0;
    int x_check, y_check;
    uint32 ticks;

    /* No current effect or not playing, quit. */
    if (!current_effect || cpl.state != ST_PLAY) {
        return;
    }

    ticks = SDL_GetTicks();

    for (tmp = current_effect->sprites; tmp; tmp = next) {
        next = tmp->next;

        if (!FaceList[tmp->def->id].sprite) {
            continue;
        }

        /* Check if the sprite should be removed due to ttl being up. */
        if (tmp->def->ttl && ticks - tmp->created_tick > tmp->def->ttl) {
            effect_sprite_remove(tmp);
            continue;
        }

        x_check = y_check = 0;

        if (tmp->def->x_check_mod) {
            x_check = FaceList[tmp->def->id].sprite->bitmap->w;
        }

        if (tmp->def->y_check_mod) {
            y_check = FaceList[tmp->def->id].sprite->bitmap->h;
        }

        if (tmp->def->warp_sides) {
            if (tmp->x + x_check < 0) {
                tmp->x = cur_widget[MAP_ID]->w;
                continue;
            }
            else if (tmp->x - x_check > cur_widget[MAP_ID]->w) {
                tmp->x = -x_check;
                continue;
            }
        }

        /* Off-screen? */
        if ((tmp->def->kill_side_left && tmp->x + x_check < 0) || (tmp->def->kill_side_right && tmp->x - x_check > cur_widget[MAP_ID]->h) || tmp->y + y_check < 0 || tmp->y - y_check > cur_widget[MAP_ID]->h) {
            effect_sprite_remove(tmp);
            continue;
        }

        /* Show the sprite. */
        map_sprite_show(cur_widget[MAP_ID]->surface, tmp->x, tmp->y, NULL, FaceList[tmp->def->id].sprite, 0, 0, 0, 0, tmp->def->zoom, tmp->def->zoom, 0);
        num_sprites++;

        /* Move it if there is no delay configured or if enough time has passed.
         * */
        if (!tmp->def->delay || !tmp->delay_ticks || ticks - tmp->delay_ticks > tmp->def->delay) {
            int ypos = tmp->def->weight * tmp->def->weight_mod;

            if (tmp->def->reverse) {
                ypos = -ypos;
            }

            tmp->y += ypos;
            tmp->x += (-1.0 + 3.0 * RANDOM() / (RAND_MAX + 1.0)) * tmp->def->wiggle;

            /* Apply wind. */
            if (tmp->def->wind && current_effect->wind_blow_dir != WIND_BLOW_NONE) {
                tmp->x += ((double) current_effect->wind / tmp->def->weight + tmp->def->weight * tmp->def->weight_mod * ((-1.0 + 2.0 * RANDOM() / (RAND_MAX + 1.0)) * tmp->def->wind_mod));
            }

            tmp->delay_ticks = ticks;
            map_redraw_flag = 1;
        }
    }

    /* Change wind direction... */
    if (current_effect->wind_blow_dir == WIND_BLOW_RANDOM && current_effect->wind_chance != 1.0 && (current_effect->wind_chance == 0.0 || RANDOM() / (RAND_MAX + 1.0) >= current_effect->wind_chance)) {
        current_effect->wind += (-2.0 + 4.0 * RANDOM() / (RAND_MAX + 1.0)) * current_effect->wind_mod;
    }

    if (current_effect->wind_blow_dir == WIND_BLOW_LEFT) {
        current_effect->wind = -1.0 * current_effect->wind_mod;
    }
    else if (current_effect->wind_blow_dir == WIND_BLOW_RIGHT) {
        current_effect->wind = 1.0 * current_effect->wind_mod;
    }

    if ((current_effect->max_sprites == -1 || num_sprites < current_effect->max_sprites) && (!current_effect->delay || !current_effect->delay_ticks || ticks - current_effect->delay_ticks > current_effect->delay) && RANDOM() / (RAND_MAX + 1.0) >= (100.0 - current_effect->sprite_chance) / 100.0) {
        int i;
        effect_sprite *sprite;

        for (i = 0; i < current_effect->sprites_per_move; i++) {
            /* Add the sprite. */
            sprite = effect_sprite_create(current_effect);

            if (!sprite) {
                return;
            }

            /* Invalid sprite. */
            if (sprite->def->id == -1 || !FaceList[sprite->def->id].sprite) {
                logger_print(LOG(INFO), "Invalid sprite ID %d", sprite->def->id);
                effect_sprite_remove(sprite);
                return;
            }

            if (sprite->def->x != -1) {
                sprite->x = sprite->def->x;
            }
            else {
                /* Calculate where to put the sprite. */
                sprite->x = (double) cur_widget[MAP_ID]->w * RANDOM() / (RAND_MAX + 1.0) * sprite->def->x_mod;
            }

            if (sprite->def->reverse) {
                sprite->y = cur_widget[MAP_ID]->h - FaceList[sprite->def->id].sprite->bitmap->h;
            }
            else if (sprite->def->y != -1) {
                sprite->y = sprite->def->y;
            }

            sprite->y += sprite->def->y_rndm * (RANDOM() / (RAND_MAX + 1.0) * sprite->def->y_mod);

            sprite->x += sprite->def->xpos;
            sprite->y += sprite->def->ypos;

            if (sprite->def->sound_file[0] != '\0') {
                sound_play_effect(sprite->def->sound_file, sprite->def->sound_volume);
            }

            sprite->created_tick = ticks;
            map_redraw_flag = 1;
        }

        current_effect->delay_ticks = ticks;
    }
}

/**
 * Start an effect identified by its name.
 * @param name Name of the effect to start. 'none' is a reserved effect name
 * and will stop any currently playing effect.
 * @return 1 if the effect was started, 0 otherwise. */
int effect_start(const char *name)
{
    effect_struct *tmp;

    /* Stop playing any effect. */
    if (!strcmp(name, "none")) {
        effect_stop();
        return 1;
    }

    /* Already playing the same effect? */
    if (current_effect && !strcmp(current_effect->name, name)) {
        return 1;
    }

    /* Find the effect... */
    for (tmp = effects; tmp; tmp = tmp->next) {
        /* Found it? */
        if (!strcmp(tmp->name, name)) {
            /* Stop current effect (if any) */
            effect_stop();
            /* Reset wind direction. */
            tmp->wind = 0;
            /* Load it up. */
            current_effect = tmp;

            /* Does this effect have an overlay? If so, we need to free
             * old dark levels, as they are rendered without the image
             * overlay. */
            if (current_effect->overlay) {
                size_t i, dark;

                for (i = 0; i < MAX_FACE_TILES; i++) {
                    if (!FaceList[i].sprite) {
                        continue;
                    }

                    if (FaceList[i].sprite->effect) {
                        SDL_FreeSurface(FaceList[i].sprite->effect);
                        FaceList[i].sprite->effect = NULL;
                    }

                    /* Free all dark levels. */
                    for (dark = 0; dark < DARK_LEVELS; dark++) {
                        if (FaceList[i].sprite->dark_level[dark]) {
                            SDL_FreeSurface(FaceList[i].sprite->dark_level[dark]);
                            FaceList[i].sprite->dark_level[dark] = NULL;
                        }
                    }
                }
            }

            if (current_effect->sound_effect[0] != '\0') {
                current_effect->sound_channel = sound_play_effect_loop(current_effect->sound_effect, current_effect->sound_volume, -1);
            }

            return 1;
        }
    }

    return 0;
}

/**
 * Used for debugging effects code using /d_effect command.
 * @param type What debugging command to run. */
void effect_debug(const char *type)
{
    if (!strcmp(type, "num")) {
        uint32 num = 0;
        uint64 bytes;
        double kbytes;
        effect_sprite *tmp;

        if (!current_effect) {
            draw_info(COLOR_RED, "No effect is currently playing.");
            return;
        }

        for (tmp = current_effect->sprites; tmp; tmp = tmp->next) {
            num++;
        }

        bytes = ((uint64) sizeof(effect_sprite)) * num;
        kbytes = (double) bytes / 1024;

        draw_info_format(COLOR_WHITE, "Visible sprites: [green]%d[/green] using [green]%"FMT64U "[/green] bytes ([green]%2.2f[/green] KB)", num, bytes, kbytes);
    }
    else if (!strcmp(type, "sizeof")) {
        draw_info(COLOR_WHITE, "Information about various data structures used by effects:\n");
        draw_info_format(COLOR_WHITE, "Size of a single sprite definition: [green]%"FMT64U "[/green]", (uint64) sizeof(effect_sprite_def));
        draw_info_format(COLOR_WHITE, "Size of a single visible sprite: [green]%"FMT64U "[/green]", (uint64) sizeof(effect_sprite));
        draw_info_format(COLOR_WHITE, "Size of a single effect structure: [green]%"FMT64U "[/green]", (uint64) sizeof(effect_struct));
        draw_info_format(COLOR_WHITE, "Size of a single overlay: [green]%"FMT64U "[/green]", (uint64) sizeof(effect_overlay));
    }
    else {
        draw_info_format(COLOR_RED, "No such debug option '%s'.", type);
    }
}

/**
 * Stop currently playing effect. */
void effect_stop(void)
{
    if (!current_effect) {
        return;
    }

    effect_sprites_free(current_effect);
    current_effect = NULL;
}

/**
 * Check whether there is an overlay on the active effect (if any).
 * @return 1 if there is an overlay, 0 otherwise. */
uint8 effect_has_overlay(void)
{
    if (!current_effect) {
        return 0;
    }

    return current_effect->overlay ? 1 : 0;
}

/**
 * Add an effect overlay to a sprite.
 * @param sprite The sprite to add overlay to. */
void effect_scale(sprite_struct *sprite)
{
    int j, k, r, g, b, a, idx;
    Uint8 vals[4];
    SDL_Surface *temp = SDL_ConvertSurface(sprite->bitmap, FormatHolder->format, FormatHolder->flags);

    for (k = 0; k < temp->h; k++) {
        for (j = 0; j < temp->w; j++) {
            SDL_GetRGBA(getpixel(temp, j, k), temp->format, &vals[0], &vals[1], &vals[2], &vals[3]);

            idx = 0;
            EFFECT_SCALE_ADJUST(r, current_effect->overlay);
            EFFECT_SCALE_ADJUST(g, current_effect->overlay);
            EFFECT_SCALE_ADJUST(b, current_effect->overlay);
            EFFECT_SCALE_ADJUST(a, current_effect->overlay);

            putpixel(temp, j, k, SDL_MapRGBA(temp->format, r, g, b, a));
        }
    }

    sprite->effect = SDL_DisplayFormatAlpha(temp);
    SDL_FreeSurface(temp);
}
