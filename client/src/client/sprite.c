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
 * Sprite related functions.
 */

#include <global.h>
#include <toolkit/string.h>
#include <toolkit/colorspace.h>

/**
 * Structure used to cache sprite surfaces that have had special effects
 * rendered on them.
 */
typedef struct sprite_cache {
    char *name; ///< Name of the sprite. Used for hash table lookups.
    SDL_Surface *surface; ///< The sprite's surface.
    time_t last_used; ///< Last time the sprite was used.
    UT_hash_handle hh; ///< Hash handle.
} sprite_cache_t;

/** Format holder for red_scale(), fow_scale() and grey_scale() functions. */
SDL_Surface *FormatHolder;

/** Darkness alpha values. */
static int dark_alpha[DARK_LEVELS] = {
    0, 44, 80, 117, 153, 190, 226
};

/**
 * The sprite cache hash table.
 */
static sprite_cache_t *sprites_cache = NULL;

/**
 * Initialize the sprite system.
 */
void
sprite_init_system (void)
{
    FormatHolder = SDL_CreateRGBSurface(SDL_SRCALPHA,
                                        1,
                                        1,
                                        32,
                                        0xFF000000,
                                        0x00FF0000,
                                        0x0000FF00,
                                        0x000000FF);
    SDL_SetAlpha(FormatHolder, SDL_SRCALPHA, 255);
}

/**
 * Load sprite file.
 *
 * @param fname
 * Sprite filename.
 * @param flags
 * Flags for the sprite.
 * @return
 * NULL if failed, the sprite otherwise.
 */
sprite_struct *
sprite_load_file (char *fname, uint32_t flags)
{
    sprite_struct *sprite = sprite_tryload_file(fname, flags, NULL);
    if (sprite == NULL) {
        LOG(ERROR, "Can't load sprite %s", fname);
        return NULL;
    }

    return sprite;
}

/**
 * Try to load a sprite image file.
 *
 * @param fname
 * Sprite filename
 * @param flag
 * Flags
 * @param rwop
 * Pointer to memory for the image
 * @return
 * The sprite if success, NULL otherwise
 */
sprite_struct *
sprite_tryload_file (char *fname, uint32_t flag, SDL_RWops *rwop)
{
    SDL_Surface *bitmap;
    if (fname != NULL) {
        bitmap = IMG_Load_wrapper(fname);
        if (bitmap == NULL) {
            return NULL;
        }
    } else {
        bitmap = IMG_LoadPNG_RW(rwop);
    }

    sprite_struct *sprite = ecalloc(1, sizeof(*sprite));
    if (sprite == NULL) {
        return NULL;
    }

    uint32_t ckflags = SDL_SRCCOLORKEY | SDL_ANYFORMAT | SDL_RLEACCEL;
    uint32_t ckey = 0;

    if (bitmap->format->palette) {
        ckey = bitmap->format->colorkey;
        SDL_SetColorKey(bitmap, ckflags, ckey);
    } else if (flag & SURFACE_FLAG_COLKEY_16M) {
        /* Force a true color png to colorkey. Default ckey is black (0). */
        SDL_SetColorKey(bitmap, ckflags, 0);
    }

    surface_borders_get(bitmap,
                        &sprite->border_up,
                        &sprite->border_down,
                        &sprite->border_left,
                        &sprite->border_right,
                        ckey);
    sprite->bitmap = bitmap;

    if (flag & SURFACE_FLAG_DISPLAYFORMATALPHA) {
        sprite->bitmap = SDL_DisplayFormatAlpha(bitmap);
        SDL_FreeSurface(bitmap);
    } else if (flag & SURFACE_FLAG_DISPLAYFORMAT) {
        sprite->bitmap = SDL_DisplayFormat(bitmap);
        SDL_FreeSurface(bitmap);
    }

    return sprite;
}

/**
 * Free a sprite.
 *
 * @param sprite
 * Sprite to free.
 */
void
sprite_free_sprite (sprite_struct *sprite)
{
    if (sprite == NULL) {
        return;
    }

    if (sprite->bitmap != NULL) {
        SDL_FreeSurface(sprite->bitmap);
    }

    efree(sprite);
}

/**
 * Find a sprite in the sprite cache.
 *
 * @param name
 * Name of the sprite to find.
 * @return
 * Sprite if found, NULL otherwise.
 */
static sprite_cache_t *
sprite_cache_find (const char *name)
{
    HARD_ASSERT(name != NULL);

    sprite_cache_t *cache;
    HASH_FIND_STR(sprites_cache, name, cache);

    if (cache != NULL) {
        cache->last_used = time(NULL);
    }

    return cache;
}

/**
 * Create a new sprite cache entry.
 *
 * @param name
 * Name of the cache entry.
 * @return
 * Created sprite entry.
 */
static sprite_cache_t *
sprite_cache_create (const char *name)
{
    HARD_ASSERT(name != NULL);

    sprite_cache_t *cache = ecalloc(1, sizeof(*cache));
    cache->name = estrdup(name);
    cache->last_used = time(NULL);
    return cache;
}

/**
 * Add a sprite cache entry to the sprite cache.
 *
 * @param cache
 * Cache entry to add.
 */
static void
sprite_cache_add (sprite_cache_t *cache)
{
    HARD_ASSERT(cache != NULL);
    HASH_ADD_KEYPTR(hh, sprites_cache, cache->name, strlen(cache->name), cache);
}

/**
 * Remove a sprite entry from the sprite cache.
 *
 * @param cache
 * Cache entry to remove.
 */
static void
sprite_cache_remove (sprite_cache_t *cache)
{
    HARD_ASSERT(cache != NULL);
    HASH_DEL(sprites_cache, cache);
}

/**
 * Free the specified sprite cache entry.
 * @param cache
 */
static void
sprite_cache_free (sprite_cache_t *cache)
{
    HARD_ASSERT(cache != NULL);

    efree(cache->name);
    SDL_FreeSurface(cache->surface);
    efree(cache);
}

/**
 * Free all the sprite cache entries.
 */
void sprite_cache_free_all(void)
{
    sprite_cache_t *cache, *tmp;
    HASH_ITER(hh, sprites_cache, cache, tmp) {
        sprite_cache_remove(cache);
        sprite_cache_free(cache);
    }
}

/**
 * Free unused sprite cache entries.
 */
void sprite_cache_gc(void)
{
    if (!rndm_chance(SPRITE_CACHE_GC_CHANCE)) {
        return;
    }

    time_t now = time(NULL);

    struct timeval tv1;
    gettimeofday(&tv1, NULL);

    sprite_cache_t *cache, *tmp;
    HASH_ITER(hh, sprites_cache, cache, tmp) {
        if (now - cache->last_used >= SPRITE_CACHE_GC_FREE_TIME) {
            sprite_cache_remove(cache);
            sprite_cache_free(cache);
        }

        /* Avoid executing this loop for too long. */
        struct timeval tv2;
        if (gettimeofday(&tv2, NULL) == 0 &&
            tv2.tv_usec - tv1.tv_usec >= SPRITE_CACHE_GC_MAX_TIME) {
            break;
        }
    }
}

/**
 * Creates a red version of the specified sprite surface.
 *
 * Used for the infravision effect.
 *
 * @param surface
 * Surface.
 * @return
 * New surface.
 */
static SDL_Surface *
sprite_effect_red (SDL_Surface *surface)
{
    SDL_Surface *tmp = SDL_ConvertSurface(surface,
                                          FormatHolder->format,
                                          FormatHolder->flags);
    if (tmp == NULL) {
        return NULL;
    }

    for (int y = 0; y < tmp->h; y++) {
        for (int x = 0; x < tmp->w; x++) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(getpixel(tmp, x, y), tmp->format, &r, &g, &b, &a);
            r = (Uint8) (0.212671 * r + 0.715160 * g + 0.072169 * b);
            g = b = 0;
            putpixel(tmp, x, y, SDL_MapRGBA(tmp->format, r, g, b, a));
        }
    }

    SDL_Surface *ret = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(tmp);
    return ret;
}

/**
 * Creates a gray version of the specified sprite surface.
 *
 * Used for the invisible effect.
 *
 * @param surface
 * Surface.
 * @return
 * New surface.
 */
static SDL_Surface *
sprite_effect_gray (SDL_Surface *surface)
{
    SDL_Surface *tmp = SDL_ConvertSurface(surface,
                                          FormatHolder->format,
                                          FormatHolder->flags);
    if (tmp == NULL) {
        return NULL;
    }

    for (int y = 0; y < tmp->h; y++) {
        for (int x = 0; x < tmp->w; x++) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(getpixel(tmp, x, y), tmp->format, &r, &g, &b, &a);
            r = g = b = (Uint8) (0.212671 * r + 0.715160 * g + 0.072169 * b);
            putpixel(tmp, x, y, SDL_MapRGBA(tmp->format, r, g, b, a));
        }
    }

    SDL_Surface *ret = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(tmp);
    return ret;
}

/**
 * Creates somewhat gray version of the specified sprite surface.
 *
 * Used for the fog of war effect.
 *
 * @param surface
 * Surface.
 * @return
 * New surface.
 */
static SDL_Surface *
sprite_effect_fow (SDL_Surface *surface)
{
    SDL_Surface *tmp = SDL_ConvertSurface(surface,
                                          FormatHolder->format,
                                          FormatHolder->flags);
    if (tmp == NULL) {
        return NULL;
    }

    for (int y = 0; y < tmp->h; y++) {
        for (int x = 0; x < tmp->w; x++) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(getpixel(tmp, x, y), tmp->format, &r, &g, &b, &a);
            r = (Uint8) ((0.212671 * r + 0.715160 * g + 0.072169 * b) * 0.34);
            g = b = r;
            b += 16;
            putpixel(tmp, x, y, SDL_MapRGBA(tmp->format, r, g, b, a));
        }
    }

    SDL_Surface *ret = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(tmp);
    return ret;
}

/**
 * Creates a glow effect for the specified sprite surface.
 *
 * @param surface
 * Surface.
 * @param color
 * Glow color.
 * @param speed
 * Animation speed of the glow.
 * @param state
 * Current animation state of the glow.
 * @return
 * New surface.
 */
static SDL_Surface *
sprite_effect_glow (SDL_Surface     *surface,
                    const SDL_Color *color,
                    double           speed,
                    double           state)
{
    SDL_Surface *tmp = SDL_CreateRGBSurface(surface->flags,
                                            surface->w + SPRITE_GLOW_SIZE * 2,
                                            surface->h + SPRITE_GLOW_SIZE * 2,
                                            surface->format->BitsPerPixel,
                                            surface->format->Rmask,
                                            surface->format->Gmask,
                                            surface->format->Bmask,
                                            surface->format->Amask);
    if (tmp == NULL) {
        return NULL;
    }

#define GLOW_GRID_PIXEL_NONE    0 ///< No data.
#define GLOW_GRID_PIXEL_VISIBLE 1 ///< A visible pixel.
#define GLOW_GRID_PIXEL_GLOW    2 ///< Added glow pixel.
#define GLOW_GRID_PIXEL_OUTLINE 3 ///< Added glow outline pixel.

    /* Create a 2D grid representation of the sprite's pixel surface for
     * storing information about the processing state, such as which
     * coordinates contain visible pixels. */
    uint8_t *grid = ecalloc(1, sizeof(*grid) * tmp->w * tmp->h);

    for (int x = 0; x < surface->w; x++) {
        for (int y = 0; y < surface->h; y++) {
            Uint32 pixel = getpixel(surface, x, y);
            if (pixel == surface->format->colorkey) {
                /* Transparent pixel. */
                continue;
            }

            putpixel(tmp, x + SPRITE_GLOW_SIZE, y + SPRITE_GLOW_SIZE, pixel);

            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
            if (a < 127) {
                /* Avoid outlining pixels with low alpha values, such as
                 * shadows or already existing glow effects. */
                continue;
            }

            int idx = tmp->w * (y + SPRITE_GLOW_SIZE) + (x + SPRITE_GLOW_SIZE);
            grid[idx] = GLOW_GRID_PIXEL_VISIBLE;
        }
    }

    /* Figure out the alpha value based on the animation speed and the current
     * animation state for a fade-out/pulsing effect. */
    speed = MAX(1.0, speed);
    state = MAX(1.0, state);
    double mod = (speed - state - speed / 2.0) / (speed / 2.0);
    Uint8 alpha = 200.0 * fabs(mod);

    /* It's much easier to work in HSV for this. */
    double rgb[3], hsv[3];
    rgb[0] = color->r / 255.0;
    rgb[1] = color->g / 255.0;
    rgb[2] = color->b / 255.0;
    colorspace_rgb2hsv(rgb, hsv);

    /* Create some random variations of the specified color. */
    Uint32 pixels[10];
    for (size_t i = 0; i < arraysize(pixels); i++) {
        double hsv2[3], rgb2[3];
        memcpy(&hsv2, hsv, sizeof(hsv2));
        hsv2[1] += (10 - rndm(1, 20)) * 0.01;
        hsv2[2] += (10 - rndm(1, 20)) * 0.01;
        hsv2[1] = MIN(1.0, MAX(0.0, hsv2[1]));
        hsv2[2] = MIN(1.0, MAX(0.0, hsv2[2]));
        colorspace_hsv2rgb(hsv2, rgb2);

        pixels[i] = SDL_MapRGBA(tmp->format,
                                rgb2[0] * 255.0,
                                rgb2[1] * 255.0,
                                rgb2[2] * 255.0,
                                alpha);
    }

    hsv[1] += 0.10;
    hsv[1] = MIN(1.0, hsv[1]);
    hsv[2] -= 0.25;
    hsv[2] = MAX(0.0, hsv[2]);
    colorspace_hsv2rgb(hsv, rgb);

    /* Acquire the color to use for the glow's outline. */
    Uint32 edge_color = SDL_MapRGBA(tmp->format,
                                    rgb[0] * 255.0,
                                    rgb[1] * 255.0,
                                    rgb[2] * 255.0,
                                    MAX(0, alpha - 25));

    /* Iterate the pixels in the sprite's surface. */
    for (int x = 0; x < tmp->w; x++) {
        for (int y = 0; y < tmp->h; y++) {
            if (grid[tmp->w * y + x] != GLOW_GRID_PIXEL_VISIBLE) {
                /* Transparent pixel, or an already processed one. */
                continue;
            }

            /* Scan adjacent pixels to see if there's a visible pixel. */
            bool has_neighbors = false;
            for (int dx = -1; dx <= 1 && !has_neighbors; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) {
                        /* Skip self */
                        continue;
                    }

                    int tx = x + dx;
                    int ty = y + dy;
                    if (tx < 0 || tx >= tmp->w || ty < 0 || ty >= tmp->h) {
                        continue;
                    }

                    if (grid[tmp->w * ty + tx] == GLOW_GRID_PIXEL_VISIBLE) {
                        has_neighbors = true;
                        break;
                    }
                }
            }

            if (!has_neighbors) {
                /* No visible neighboring pixels, move on. */
                continue;
            }

            /* Add glow pixels where applicable. */
            for (int off = 1; off <= 2; off++) {
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int tx = x + dx * off;
                        int ty = y + dy * off;
                        if (tx < 0 || tx >= tmp->w || ty < 0 || ty >= tmp->h) {
                            continue;
                        }

                        uint8_t *point = &grid[tmp->w * ty + tx];
                        /* Only adjust pixels that don't have a visible pixel,
                         * or if they have been added a glow outline before. */
                        if (*point == GLOW_GRID_PIXEL_NONE ||
                            *point == GLOW_GRID_PIXEL_OUTLINE) {
                            Uint32 pixel;
                            if (off == 1) {
                                /* Glow pixel processing. */
                                pixel = pixels[rndm(0, arraysize(pixels) - 1)];
                                *point = GLOW_GRID_PIXEL_GLOW;
                            } else {
                                /* Glow outline pixel processing. */
                                pixel = edge_color;
                                *point = GLOW_GRID_PIXEL_OUTLINE;
                            }

                            putpixel(tmp, tx, ty, pixel);
                        }
                    }
                }
            }
        }
    }

    efree(grid);

    SDL_Surface *ret = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(tmp);
    return ret;

#undef GLOW_GRID_PIXEL_NONE
#undef GLOW_GRID_PIXEL_VISIBLE
#undef GLOW_GRID_PIXEL_GLOW
#undef GLOW_GRID_PIXEL_OUTLINE
}

/**
 * Create a new sprite surface based on 'surface', applying the specified
 * 'effects'.
 *
 * @param surface
 * Surface to use as the base.
 * @param effects
 * Effects to apply.
 * @return
 * New surface, NULL on failure.
 */
static SDL_Surface *
sprite_effects_create (SDL_Surface *surface, const sprite_effects_t *effects)
{
#define FREE_TMP_SURFACE()      \
do {                            \
    if (tmp != NULL) {          \
        SDL_FreeSurface(tmp);   \
    }                           \
    tmp = surface;              \
} while (0)

    SDL_Surface *tmp = NULL;

    if (BIT_QUERY(effects->flags, SPRITE_FLAG_EFFECTS)) {
        surface = effect_sprite_overlay(surface);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    }

    if (BIT_QUERY(effects->flags, SPRITE_FLAG_DARK)) {
        surface = SDL_DisplayFormatAlpha(surface);
        if (surface == NULL) {
            goto done;
        }

        char buf[MAX_BUF];
        snprintf(VS(buf),
                 "rectangle:500,500,%d",
                 dark_alpha[effects->dark_level]);
        SDL_BlitSurface(texture_surface(texture_get(TEXTURE_TYPE_SOFTWARE,
                                                    buf)),
                        NULL,
                        surface,
                        NULL);
        FREE_TMP_SURFACE();
    } else if (BIT_QUERY(effects->flags, SPRITE_FLAG_FOW)) {
        surface = sprite_effect_fow(surface);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    } else if (BIT_QUERY(effects->flags, SPRITE_FLAG_RED)) {
        surface = sprite_effect_red(surface);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    } else if (BIT_QUERY(effects->flags, SPRITE_FLAG_GRAY)) {
        surface = sprite_effect_gray(surface);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    }

    /* Apply tile-stretching. */
    if (effects->stretch != 0) {
        Sint8 n = (effects->stretch >> 24) & 0xFF;
        Sint8 e = (effects->stretch >> 16) & 0xFF;
        Sint8 w = (effects->stretch >> 8) & 0xFF;
        Sint8 s = effects->stretch & 0xFF;

        surface = tile_stretch(surface, n, e, s, w);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    }

    /* Apply zoom and/or rotate effects. */
    if ((effects->zoom_x != 0 && effects->zoom_x != 100) ||
        (effects->zoom_y != 0 && effects->zoom_y != 100) ||
        effects->rotate != 0) {
        bool smooth;
        /* Figure out whether to use smoothing. */
        if (effects->rotate == 0 &&
            (effects->zoom_x == 0 || abs(effects->zoom_x) == 100) &&
            (effects->zoom_y == 0 || abs(effects->zoom_y) == 100)) {
            smooth = false;
        } else {
            smooth = setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH);
        }

        double zoom_x = effects->zoom_x != 0 ? effects->zoom_x / 100.0 : 1.0;
        double zoom_y = effects->zoom_y != 0 ? effects->zoom_y / 100.0 : 1.0;
        surface = rotozoomSurfaceXY(surface,
                                    effects->rotate,
                                    zoom_x,
                                    zoom_y,
                                    smooth);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    }

    /* Apply glow effects. */
    if (effects->glow[0] != '\0') {
        SDL_Color color;
        if (text_color_parse(effects->glow, &color)) {
            surface = sprite_effect_glow(surface,
                                         &color,
                                         effects->glow_speed,
                                         effects->glow_state);
            if (surface == NULL) {
                goto done;
            }

            FREE_TMP_SURFACE();
        }
    }

    /* Alpha transparency. */
    if (effects->alpha != 0) {
        surface = SDL_DisplayFormatAlpha(surface);
        if (surface == NULL) {
            goto done;
        }

        surface_set_alpha(surface, effects->alpha);
        FREE_TMP_SURFACE();
    }

    SOFT_ASSERT_RC(tmp != NULL, NULL, "Generated NULL surface!");

done:
    return tmp;
#undef FREE_TMP_SURFACE
}

/**
 * Render the specified surface.
 *
 * @param surface
 * Surface on which to render.
 * @param x
 * X rendering position.
 * @param y
 * Y rendering position.
 * @param srcrect
 * Limit which parts of the source surface to render. Can be NULL.
 * @param src
 * Source surface to render.
 */
void
surface_show (SDL_Surface *surface,
              int          x,
              int          y,
              SDL_Rect    *srcrect,
              SDL_Surface *src)
{
    SDL_Rect dstrect;
    dstrect.x = x;
    dstrect.y = y;
    SDL_BlitSurface(src, srcrect, surface, &dstrect);
}

/**
 * Render the specified surface until the specified 'box' is completely filled.
 *
 * Used for rendering tile-able textures.
 *
 * @param surface
 * Surface on which to render.
 * @param x
 * X rendering position.
 * @param y
 * Y rendering position.
 * @param srcrect
 * Limit which parts of the source surface to render. Can be NULL.
 * @param src
 * Source surface to render.
 * @param box
 * Specifies maximum width and height to render.
 */
void
surface_show_fill (SDL_Surface *surface,
                   int          x,
                   int          y,
                   SDL_Rect    *srcsize,
                   SDL_Surface *src,
                   SDL_Rect    *box)
{
    int w = srcsize != NULL ? srcsize->w : src->w;
    int h = srcsize != NULL ? srcsize->h : src->h;
    for (int tx = 0; tx < box->w; tx += w) {
        for (int ty = 0; ty < box->h; ty += h) {
            SDL_Rect srcrect;
            srcrect.x = srcsize ? MAX(0, srcsize->x) : 0;
            srcrect.y = srcsize ? MAX(0, srcsize->y) : 0;
            srcrect.w = MIN(w, box->w - tx);
            srcrect.h = MIN(h, box->h - ty);
            surface_show(surface, x + tx, y + ty, &srcrect, src);
        }
    }
}

/**
 * Render a surface, applying the specified effects.
 *
 * @param surface
 * Surface on which to render.
 * @param x
 * X rendering position.
 * @param y
 * Y rendering position.
 * @param srcrect
 * Limit which parts of the source surface to render. Can be NULL.
 * @param src
 * Source surface to render.
 * @param effects
 * Effects to apply.
 */
void
surface_show_effects (SDL_Surface            *surface,
                      int                     x,
                      int                     y,
                      SDL_Rect               *srcrect,
                      SDL_Surface            *src,
                      const sprite_effects_t *effects)
{
    HARD_ASSERT(surface != NULL);

    if (src == NULL) {
        return;
    }

    if (effects != NULL && SPRITE_EFFECTS_NEED_RENDERING(effects)) {
        /* Maximum darkness; do not render at all. */
        if (BIT_QUERY(effects->flags, SPRITE_FLAG_DARK) &&
            effects->dark_level == DARK_LEVELS) {
            return;
        }

        /* Construct a cache entry string. */
        char name[HUGE_BUF];
        snprintf(VS(name),
                 "%p;%u;%u;%s;%u;%u;%d;%d;%d;%s;%u;%u",
                 src,
                 effects->flags,
                 effects->dark_level,
                 effect_overlay_identifier(),
                 effects->alpha,
                 effects->stretch,
                 effects->zoom_x,
                 effects->zoom_y,
                 effects->rotate,
                 effects->glow,
                 effects->glow_speed,
                 effects->glow_state);

        /* Try to find the sprite we need in the cache, otherwise,
         * render it out and add it to the cache. */
        SDL_Surface *old_src = src;
        sprite_cache_t *cache = sprite_cache_find(name);
        if (cache != NULL) {
            src = cache->surface;
        } else {
            SDL_Surface *tmp = sprite_effects_create(src, effects);
            if (tmp != NULL) {
                src = tmp;

                cache = sprite_cache_create(name);
                cache->surface = src;
                sprite_cache_add(cache);
            }
        }

        if (effects->stretch != 0) {
            y -= src->h - old_src->h;
        }

        if (effects->glow[0] != '\0') {
            y -= SPRITE_GLOW_SIZE;
            x -= SPRITE_GLOW_SIZE;
        }
    }

    surface_show(surface, x, y, srcrect, src);
}

/**
 * Get pixel value from an SDL surface at the specified X/Y position.
 *
 * @param surface
 * SDL surface to get the pixel from.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @return
 * The pixel.
 */
Uint32
getpixel (SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* The address to the pixel we want to retrieve. */
    Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp) {
    case 1:
        return *p;

    case 2:
        return *(Uint16 *) p;

    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            return p[0] << 16 | p[1] << 8 | p[2];
        } else {
            return p[0] | p[1] << 8 | p[2] << 16;
        }

    case 4:
        return *(Uint32 *) p;
    }

    return 0;
}

/**
 * Put a pixel value to the specified X/Y position on an SDL surface.
 *
 * @param surface
 * The surface.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @param pixel
 * Pixel to put.
 */
void
putpixel (SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* The address to the pixel we want to set. */
    Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *) p = pixel;
        break;

    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }

        break;

    case 4:
        *(Uint32 *) p = pixel;
        break;
    }
}

/**
 * Calculate the left border in the surface - this is the position of
 * the first pixel from the left that that does not match 'color'.
 *
 * @param surface
 * Surface.
 * @param[out] pos
 * Where to store the position.
 * @param color
 * Color to check for.
 * @return
 * True if the border was found, false otherwise.
 */
static bool
surface_border_get_left (SDL_Surface *surface, int *pos, uint32_t color)
{
    for (int x = 0; x < surface->w; x++) {
        for (int y = 0; y < surface->h; y++) {
            if (getpixel(surface, x, y) != color) {
                *pos = x;
                return true;
            }
        }
    }

    return false;
}

/**
 * Calculate the right border in the surface - this is the position of
 * the first pixel from the right that that does not match 'color'.
 *
 * @param surface
 * Surface.
 * @param[out] pos
 * Where to store the position.
 * @param color
 * Color to check for.
 * @return
 * True if the border was found, false otherwise.
 */
static bool
surface_border_get_right(SDL_Surface *surface, int *pos, uint32_t color)
{
    for (int x = surface->w - 1; x >= 0; x--) {
        for (int y = 0; y < surface->h; y++) {
            if (getpixel(surface, x, y) != color) {
                *pos = (surface->w - 1) - x;
                return true;
            }
        }
    }

    return false;
}

/**
 * Calculate the top border in the surface - this is the position of
 * the first pixel from the top that that does not match 'color'.
 *
 * @param surface
 * Surface.
 * @param[out] pos
 * Where to store the position.
 * @param color
 * Color to check for.
 * @return
 * True if the border was found, false otherwise.
 */
static bool
surface_border_get_top (SDL_Surface *surface, int *pos, uint32_t color)
{
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            if (getpixel(surface, x, y) != color) {
                *pos = y;
                return true;
            }
        }
    }

    return false;
}

/**
 * Calculate the bottom border in the surface - this is the position of
 * the first pixel from the bottom that that does not match 'color'.
 *
 * @param surface
 * Surface.
 * @param[out] pos
 * Where to store the position.
 * @param color
 * Color to check for.
 * @return
 * True if the border was found, false otherwise.
 */
static bool
surface_border_get_bottom(SDL_Surface *surface, int *pos, uint32_t color)
{
    for (int y = surface->h - 1; y >= 0; y--) {
        for (int x = 0; x < surface->w; x++) {
            if (getpixel(surface, x, y) != color) {
                *pos = (surface->h - 1) - y;
                return true;
            }
        }
    }

    return false;
}

/**
 * Get borders from SDL_surface. The borders indicate the first pixel
 * from the border's side that does not match 'color'.
 *
 * @param surface
 * Surface to get borders from.
 * @param[out] top
 * Where to store the top border.
 * @param[out] bottom
 * Where to store the bottom border.
 * @param[out] left
 * Where to store the left border.
 * @param[out] right
 * Where to store the right border.
 * @param color
 * Color to check for.
 * @return
 * 1 if the borders were found, 0 otherwise (image is all filled with 'color'
 * color).
 */
int
surface_borders_get (SDL_Surface *surface,
                     int         *top,
                     int         *bottom,
                     int         *left,
                     int         *right,
                     uint32_t     color)
{
    *top = 0;
    *bottom = 0;
    *left = 0;
    *right = 0;

    /* If the border was not found, it means the surface is completely
     * filled with 'color' color. */
    if (!surface_border_get_top(surface, top, color)) {
        return 0;
    }

    surface_border_get_bottom(surface, bottom, color);
    surface_border_get_left(surface, left, color);
    surface_border_get_right(surface, right, color);

    return 1;
}

/**
 * Pans the surface.
 *
 * @param surface
 * Surface.
 * @param box
 * Coordinates.
 */
void
surface_pan (SDL_Surface *surface, SDL_Rect *box)
{
    if (box->x >= surface->w - box->w) {
        box->x = (Sint16) (surface->w - box->w);
    }

    if (box->x < 0) {
        box->x = 0;
    }

    if (box->y >= surface->h - box->h) {
        box->y = (Sint16) (surface->h - box->h);
    }

    if (box->y < 0) {
        box->y = 0;
    }
}

/**
 * Draw a border frame.
 *
 * @param surface
 * Surface to draw on.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @param w
 * Width of the frame.
 * @param h
 * Height of the frame.
 */
void
draw_frame (SDL_Surface *surface, int x, int y, int w, int h)
{
    SDL_Rect box;

    box.x = x;
    box.y = y;
    box.h = h;
    box.w = 1;
    SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x60, 0x60, 0x60));
    box.x = x + w;
    box.h++;
    SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x55, 0x55, 0x55));
    box.x = x;
    box.y += h;
    box.w = w;
    box.h = 1;
    SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x60, 0x60, 0x60));
    box.x++;
    box.y = y;
    SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x55, 0x55, 0x55));
}

/**
 * Create a border around the specified coordinates.
 *
 * @param surface
 * Surface to use.
 * @param x
 * X start of the border.
 * @param y
 * Y start of the border.
 * @param w
 * Maximum border width.
 * @param h
 * Maximum border height.
 * @param color
 * Color to use for the border.
 * @param size
 * Border's size.
 */
void
border_create (SDL_Surface *surface,
               int          x,
               int          y,
               int          w,
               int          h,
               int          color,
               int          size)
{
    SDL_Rect box;

    /* Left border. */
    box.x = x;
    box.y = y;
    box.h = h;
    box.w = size;
    SDL_FillRect(surface, &box, color);

    /* Right border. */
    box.x = x + w - size;
    SDL_FillRect(surface, &box, color);

    /* Top border. */
    box.x = x + size;
    box.y = y;
    box.w = w - size * 2;
    box.h = size;
    SDL_FillRect(surface, &box, color);

    /* Bottom border. */
    box.y = y + h - size;
    SDL_FillRect(surface, &box, color);
}

/**
 * Render a line (essentially a rectangle) of the specified width/height and
 * color.
 *
 * @param surface
 * Surface to render on.
 * @param x
 * Starting X coordinate.
 * @param y
 * Starting Y coordinate.
 * @param w
 * Width of the line.
 * @param h
 * Height of the line.
 * @param color
 * Color of the line.
 */
void
border_create_line (SDL_Surface *surface,
                    int          x,
                    int          y,
                    int          w,
                    int          h,
                    uint32_t     color)
{
    SDL_Rect dst;

    dst.x = x;
    dst.y = y;
    dst.w = w;
    dst.h = h;
    SDL_FillRect(surface, &dst, color);
}

/**
 * Render a border of the specified SDL color and thickness.
 *
 * @param surface
 * Surface to render on.
 * @param coords
 * Coordinates to render at.
 * @param thickness
 * Border thickness.
 * @param color
 * Border color.
 */
void
border_create_sdl_color (SDL_Surface *surface,
                         SDL_Rect    *coords,
                         int          thickness,
                         SDL_Color   *color)
{
    uint32_t color_mapped = SDL_MapRGB(surface->format,
                                       color->r,
                                       color->g,
                                       color->b);

    BORDER_CREATE_TOP(surface,
                      coords->x,
                      coords->y,
                      coords->w,
                      coords->h,
                      color_mapped,
                      thickness);
    BORDER_CREATE_BOTTOM(surface,
                         coords->x,
                         coords->y,
                         coords->w,
                         coords->h,
                         color_mapped,
                         thickness);
    BORDER_CREATE_LEFT(surface,
                       coords->x,
                       coords->y,
                       coords->w,
                       coords->h,
                       color_mapped,
                       thickness);
    BORDER_CREATE_RIGHT(surface,
                        coords->x,
                        coords->y,
                        coords->w,
                        coords->h,
                        color_mapped,
                        thickness);
}

/**
 * Render a border of the specified color and thickness.
 *
 * @param surface
 * Surface to render on.
 * @param coords
 * Coordinates to render at.
 * @param thickness
 * Border thickness.
 * @param color_notation
 * Border color, eg, "ff0000".
 */
void
border_create_color (SDL_Surface *surface,
                     SDL_Rect    *coords,
                     int          thickness,
                     const char  *color_notation)
{
    SDL_Color color;
    if (!text_color_parse(color_notation, &color)) {
        LOG(ERROR, "Invalid color: %s", color_notation);
        return;
    }

    border_create_sdl_color(surface, coords, thickness, &color);
}

/**
 * Render a border using the specified texture.
 *
 * The texture should be tile-able.
 *
 * @param surface
 * Surface to render on.
 * @param coords
 * Coordinates to render at.
 * @param thickness
 * Border thickness.
 * @param texture
 * Border texture.
 */
void
border_create_texture (SDL_Surface *surface,
                       SDL_Rect    *coords,
                       int          thickness,
                       SDL_Surface *texture)
{
    SDL_Rect box;

    box.w = coords->w;
    box.h = thickness;
    surface_show_fill(surface,
                      coords->x,
                      coords->y,
                      NULL,
                      texture,
                      &box);
    surface_show_fill(surface,
                      coords->x,
                      coords->y + coords->h - thickness,
                      NULL,
                      texture,
                      &box);

    box.w = thickness;
    box.h = coords->h;
    surface_show_fill(surface,
                      coords->x,
                      coords->y,
                      NULL,
                      texture,
                      &box);
    surface_show_fill(surface,
                      coords->x + coords->w - thickness,
                      coords->y,
                      NULL,
                      texture,
                      &box);
}

/**
 * Create a rectangle of the specified size and color.
 *
 * @param surface
 * Surface to render on.
 * @param x
 * X coordinate to render at.
 * @param y
 * Y coordinate to render at.
 * @param w
 * Width of the rectangle.
 * @param h
 * Height of the rectangle.
 * @param color_notation
 * Color of the rectangle, eg, "ff0000".
 */
void
rectangle_create (SDL_Surface *surface,
                  int          x,
                  int          y,
                  int          w,
                  int          h,
                  const char  *color_notation)
{
    SDL_Color color;
    if (!text_color_parse(color_notation, &color)) {
        LOG(BUG, "Invalid color: %s", color_notation);
        return;
    }

    border_create_line(surface,
                       x,
                       y,
                       w,
                       h,
                       SDL_MapRGB(surface->format, color.r, color.g, color.b));
}

/**
 * Changes alpha value of the specified surface.
 *
 * If the surface is per-pixel alpha, changes every pixel on the surface
 * to match the specified alpha value.
 *
 * @param surface
 * Surface to change alpha value of.
 * @param alpha
 * Alpha value to set.
 */
void
surface_set_alpha (SDL_Surface *surface, uint8_t alpha)
{
    SDL_PixelFormat *fmt = surface->format;

    if (fmt->Amask == 0) {
        SDL_SetAlpha(surface, SDL_SRCALPHA, alpha);
    } else {
        Uint8 bpp = fmt->BytesPerPixel;
        double scale = alpha / 255.0f;

        SDL_LockSurface(surface);

        for (int y = 0; y < surface->h; y++) {
            for (int x = 0; x < surface->w; x++) {
                Uint8 r, g, b, a;
                Uint32 *pixel_ptr = (Uint32 *) ((Uint8 *) surface->pixels + y *
                                                surface->pitch + x * bpp);
                SDL_GetRGBA(*pixel_ptr, fmt, &r, &g, &b, &a);
                *pixel_ptr = SDL_MapRGBA(fmt, r, g, b, scale * a);
            }

        }

        SDL_UnlockSurface(surface);
    }
}

/**
 * Checks whether the given coordinates are within the specified polygon.
 *
 * The arrays corners_x/corners_y should contain every single corner point of
 * the polygon that you want to test.
 *
 * @param x
 * X coordinate.
 * @param y
 * Y coordinate.
 * @param corners_x
 * Array of X corner coordinates.
 * @param corners_y
 * Array of Y corner coordinates.
 * @param corners_num
 * Number of corner coordinate entries.
 * @return
 * 1 if the coordinates are in the polygon, 0 otherwise.
 */
int
polygon_check_coords (double x,
                      double y,
                      double corners_x[],
                      double corners_y[],
                      int    corners_num)
{
    int j = corners_num - 1;
    int odd_nodes = 0;

    for (int i = 0; i < corners_num; i++) {
        if (((corners_y[i] < y && corners_y[j] >= y) ||
             (corners_y[j] < y && corners_y[i] >= y)) &&
            (corners_x[i] <= x || corners_x[j] <= x)) {
            odd_nodes ^= (corners_x[i] + (y - corners_y[i]) /
                          (corners_y[j] - corners_y[i]) *
                          (corners_x[j] - corners_x[i]) < x);
        }

        j = i;
    }

    return odd_nodes;
}
