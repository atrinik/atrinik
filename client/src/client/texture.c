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
 * Texture management.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * All the textures. */
static texture_struct *textures[TEXTURE_TYPE_NUM];

/**
 * Free texture's data (ie, its surface).
 * @param tmp Texture. */
static void texture_data_free(texture_struct *tmp)
{
    if (tmp->surface) {
        SDL_FreeSurface(tmp->surface);
        tmp->surface = NULL;
    }
}

/**
 * (Re-)create texture's data (the surface).
 * @param tmp Texture.
 * @return 1 on success, 0 on failure. */
static int texture_data_new(texture_struct *tmp)
{
    if (tmp->type == TEXTURE_TYPE_SOFTWARE) {
        SDL_Surface *surface;

        if (strcmp(tmp->name, TEXTURE_FALLBACK_NAME) == 0) {
            SDL_Rect box;

            surface = SDL_CreateRGBSurface(0, 20, 20, 32, 0, 0, 0, 0);
            lineRGBA(surface, 0, 0, surface->w, surface->h, 255, 0, 0, 255);
            lineRGBA(surface, surface->w, 0, 0, surface->h, 255, 0, 0, 255);
            box.x = 0;
            box.y = 0;
            box.w = surface->w;
            box.h = surface->h;
            border_create_color(surface, &box, 1, "950000");
        }
        else if (strncmp(tmp->name, "rectangle:", 10) == 0) {
            int w, h, alpha;

            alpha = 255;

            if (sscanf(tmp->name + 10, "%d,%d,%d", &w, &h, &alpha) >= 2) {
                char *cp;

                surface = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);

                if (alpha != 255) {
                    SDL_SetSurfaceAlphaMod(surface, alpha);
                }

                cp = strchr(tmp->name + 10, ';');

                if (cp) {
                    SDL_Rect box;

                    box.w = w;
                    box.h = h;
                    text_show(surface, FONT_ARIAL11, cp + 1, 0, 0, COLOR_WHITE, TEXT_MARKUP, &box);
                }
            }
            else {
                logger_print(LOG(BUG), "Invalid parameters for rectangle texture: %s", tmp->name);
                return 0;
            }
        }
        else {
            logger_print(LOG(BUG), "Invalid name for software texture: %s", tmp->name);
            return 0;
        }

        texture_data_free(tmp);
        tmp->surface = surface;
    }
    else if (tmp->type == TEXTURE_TYPE_CLIENT) {
        char path[HUGE_BUF];
        SDL_Surface *surface;

        snprintf(path, sizeof(path), "textures/%s.png", tmp->name);
        surface = IMG_Load_wrapper(path);

        if (!surface) {
            logger_print(LOG(BUG), "Could not load texture: %s", path);
            return 0;
        }

        texture_data_free(tmp);
        tmp->surface = surface;
    }

    return 1;
}

/**
 * Free a texture.
 * @param tmp Texture to free. */
static void texture_free(texture_struct *tmp)
{
    free(tmp->name);
    texture_data_free(tmp);
    free(tmp);
}

/**
 * Allocate a new texture structure.
 * @param type Type of the texture, one of ::texture_type_t.
 * @param name Name of the texture.
 * @return The allocated texture; NULL on failure. */
static texture_struct *texture_new(texture_type_t type, const char *name)
{
    texture_struct *tmp;

    tmp = calloc(1, sizeof(*tmp));
    tmp->name = strdup(name);
    tmp->type = type;
    tmp->last_used = time(NULL);

    if (!texture_data_new(tmp)) {
        texture_free(tmp);
        return NULL;
    }

    HASH_ADD_KEYPTR(hh, textures[type], tmp->name, strlen(tmp->name), tmp);

    return tmp;
}

/**
 * Initialize the texture API. */
void texture_init(void)
{
    texture_type_t type;

    for (type = 0; type < TEXTURE_TYPE_NUM; type++) {
        textures[type] = NULL;
    }

    texture_new(TEXTURE_TYPE_SOFTWARE, TEXTURE_FALLBACK_NAME);
}

/**
 * Deinitialize the texture API. */
void texture_deinit(void)
{
    texture_type_t type;
    texture_struct *curr, *tmp;

    for (type = 0; type < TEXTURE_TYPE_NUM; type++) {
        HASH_ITER(hh, textures[type], curr, tmp)
        {
            HASH_DEL(textures[type], curr);
            texture_free(curr);
        }
    }
}

/**
 * Reload all textures. */
void texture_reload(void)
{
    texture_type_t type;
    texture_struct *curr, *tmp;

    for (type = 0; type < TEXTURE_TYPE_NUM; type++) {
        HASH_ITER(hh, textures[type], curr, tmp)
        {
            texture_data_new(curr);
        }
    }
}

/**
 * Garbage-collect textures. */
void texture_gc(void)
{
    time_t now;
    struct timeval tv1, tv2;
    int done;
    texture_type_t type;
    texture_struct *curr, *tmp;

    if (!rndm_chance(TEXTURE_GC_CHANCE)) {
        return;
    }

    now = time(NULL);
    gettimeofday(&tv1, NULL);
    done = 0;

    for (type = 0; type < TEXTURE_TYPE_NUM && !done; type++) {
        HASH_ITER(hh, textures[type], curr, tmp)
        {
            if (curr->surface && now - curr->last_used >= TEXTURE_GC_FREE_TIME) {
                texture_data_free(curr);
            }

            if (gettimeofday(&tv2, NULL) == 0 && tv2.tv_usec - tv1.tv_usec >= TEXTURE_GC_MAX_TIME) {
                done = 1;
                break;
            }
        }
    }
}

/**
 * Find specified texture in the hash table, allocating it if necessary.
 * @param type Type of the texture to look for.
 * @param name Name of the texture.
 * @return The texture; never NULL. */
texture_struct *texture_get(texture_type_t type, const char *name)
{
    texture_struct *tmp;

    HASH_FIND_STR(textures[type], name, tmp);

    if (!tmp) {
        tmp = texture_new(type, name);

        if (!tmp) {
            tmp = texture_get(TEXTURE_TYPE_SOFTWARE, TEXTURE_FALLBACK_NAME);
        }
    }

    return tmp;
}

/**
 * Acquire texture's surface.
 * @param texture Texture.
 * @return Texture's surface, never NULL. */
SDL_Surface *texture_surface(texture_struct *texture)
{
    texture->last_used = time(NULL);

    /* No surface, which means that the texture's surface was freed by
     * the garbage collector, so re-create it. */
    if (!texture->surface) {
        /* If we could not load up the texture's surface for some reason,
         * use the fallback texture surface. */
        if (!texture_data_new(texture)) {
            return texture_get(TEXTURE_TYPE_SOFTWARE, TEXTURE_FALLBACK_NAME)->surface;
        }
    }

    return texture->surface;
}
