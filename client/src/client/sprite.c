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
 * Sprite related functions. */

#include <global.h>
#include <toolkit_string.h>

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
 * Initialize the sprite system. */
void sprite_init_system(void)
{
    FormatHolder = SDL_CreateRGBSurface(SDL_SRCALPHA, 1, 1, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    SDL_SetAlpha(FormatHolder, SDL_SRCALPHA, 255);
}

/**
 * Load sprite file.
 * @param fname Sprite filename.
 * @param flags Flags for the sprite.
 * @return NULL if failed, the sprite otherwise. */
sprite_struct *sprite_load_file(char *fname, uint32_t flags)
{
    sprite_struct *sprite = sprite_tryload_file(fname, flags, NULL);

    if (sprite == NULL) {
        LOG(BUG, "Can't load sprite %s", fname);
        return NULL;
    }

    return sprite;
}

/**
 * Try to load a sprite image file.
 * @param fname Sprite filename
 * @param flag Flags
 * @param rwop Pointer to memory for the image
 * @return The sprite if success, NULL otherwise */
sprite_struct *sprite_tryload_file(char *fname, uint32_t flag, SDL_RWops *rwop)
{
    sprite_struct *sprite;
    SDL_Surface *bitmap;
    uint32_t ckflags, tmp = 0;

    if (fname) {
        if (!(bitmap = IMG_Load_wrapper(fname))) {
            return NULL;
        }
    } else {
        bitmap = IMG_LoadPNG_RW(rwop);
    }

    if (!(sprite = emalloc(sizeof(sprite_struct)))) {
        return NULL;
    }

    memset(sprite, 0, sizeof(sprite_struct));

    ckflags = SDL_SRCCOLORKEY | SDL_ANYFORMAT | SDL_RLEACCEL;

    if (bitmap->format->palette) {
        SDL_SetColorKey(bitmap, ckflags, (tmp = bitmap->format->colorkey));
    } else if (flag & SURFACE_FLAG_COLKEY_16M) {
        /* We force a true color png to colorkey. Default colkey is black (0). */
        SDL_SetColorKey(bitmap, ckflags, 0);
    }

    surface_borders_get(bitmap, &sprite->border_up, &sprite->border_down, &sprite->border_left, &sprite->border_right, tmp);

    /* We store our original bitmap */
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
 * @param sprite Sprite to free. */
void sprite_free_sprite(sprite_struct *sprite)
{
    if (!sprite) {
        return;
    }

    if (sprite->bitmap) {
        SDL_FreeSurface(sprite->bitmap);
    }

    efree(sprite);
}

static sprite_cache_t *sprite_cache_find(const char *name)
{
    HARD_ASSERT(name != NULL);

    sprite_cache_t *cache;
    HASH_FIND_STR(sprites_cache, name, cache);

    if (cache != NULL) {
        cache->last_used = time(NULL);
    }

    return cache;
}

static sprite_cache_t *sprite_cache_new(const char *name)
{
    HARD_ASSERT(name != NULL);

    sprite_cache_t *cache = ecalloc(1, sizeof(*cache));
    cache->name = estrdup(name);
    cache->last_used = time(NULL);
    return cache;
}

static void sprite_cache_add(sprite_cache_t *cache)
{
    HARD_ASSERT(cache != NULL);

    HASH_ADD_KEYPTR(hh, sprites_cache, cache->name, strlen(cache->name), cache);
}

static void sprite_cache_remove(sprite_cache_t *cache)
{
    HARD_ASSERT(cache != NULL);

    HASH_DEL(sprites_cache, cache);
}

static void sprite_cache_free(sprite_cache_t *cache)
{
    HARD_ASSERT(cache != NULL);

    efree(cache->name);
    SDL_FreeSurface(cache->surface);
    efree(cache);
}

void sprite_cache_free_all(void)
{
    sprite_cache_t *cache, *tmp;
    HASH_ITER(hh, sprites_cache, cache, tmp) {
        sprite_cache_remove(cache);
        sprite_cache_free(cache);
    }
}

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

        struct timeval tv2;
        if (gettimeofday(&tv2, NULL) == 0 && tv2.tv_usec - tv1.tv_usec >=
                SPRITE_CACHE_GC_MAX_TIME) {
            break;
        }
    }
}

/**
 * Creates a red version of the specified sprite surface. Used for the
 * infravision effect.
 * @param surface Surface.
 * @return New surface.
 */
static SDL_Surface *sprite_effect_red(SDL_Surface *surface)
{
    SDL_Surface *tmp = SDL_ConvertSurface(surface, FormatHolder->format,
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
 * Creates a gray version of the specified sprite surface. Used for the
 * invisible effect.
 * @param surface Surface.
 * @return New surface.
 */
static SDL_Surface *sprite_effect_gray(SDL_Surface *surface)
{
    SDL_Surface *tmp = SDL_ConvertSurface(surface, FormatHolder->format,
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
 * Creates somewhat gray version of the specified sprite surface. Used for the
 * fog of war effect.
 * @param surface Surface.
 * @return New surface.
 */
static SDL_Surface *sprite_effect_fow(SDL_Surface *surface)
{
    SDL_Surface *tmp = SDL_ConvertSurface(surface, FormatHolder->format,
            FormatHolder->flags);
    if (tmp == NULL) {
        return NULL;
    }

    for (int y = 0; y < tmp->h; y++) {
        for (int x = 0; x < tmp->w; x++) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(getpixel(tmp, x, y), tmp->format, &r, &g, &b, &a);
            r = g = b = (Uint8) ((0.212671 * r + 0.715160 * g + 0.072169 * b) *
                    0.34);
            b += 16;
            putpixel(tmp, x, y, SDL_MapRGBA(tmp->format, r, g, b, a));
        }
    }

    SDL_Surface *ret = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(tmp);
    return ret;
}

/**
 * Creates a glowing effect for the specified sprite surface.
 * @param surface Surface.
 * @param color Glow color.
 * @param speed Animation speed of the glow.
 * @param state Current animation state of the glow.
 * @return New surface.
 */
static SDL_Surface *sprite_effect_glow(SDL_Surface *surface,
        const SDL_Color *color, double speed, double state)
{
    SDL_Surface *tmp = SDL_CreateRGBSurface(surface->flags, surface->w + 2,
            surface->h + 2, surface->format->BitsPerPixel,
            surface->format->Rmask, surface->format->Gmask,
            surface->format->Bmask, surface->format->Amask);
    if (tmp == NULL) {
        return NULL;
    }

    uint8_t *grid = ecalloc(1, sizeof(*grid) * tmp->w * tmp->h);

    for (int x = 0; x < surface->w; x++) {
        for (int y = 0; y < surface->h; y++) {
            Uint32 pixel = getpixel(surface, x, y);
            if (pixel == surface->format->colorkey) {
                continue;
            }

            putpixel(tmp, x + 1, y + 1, pixel);

            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
            if (a < 127) {
                continue;
            }

            grid[tmp->w * (y + 1) + (x + 1)] = 1;
        }
    }

    speed = MAX(1.0, speed);
    state = MAX(1.0, state);
    double mod = (speed - state - speed / 2.0) / (speed / 2.0);
    Uint8 alpha = 200.0 * fabs(mod);

    double rgb[3], hsv[3];
    rgb[0] = color->r / 255.0;
    rgb[1] = color->g / 255.0;
    rgb[2] = color->b / 255.0;
    colorspace_rgb2hsv(rgb, hsv);

    Uint32 pixels[10];
    for (size_t i = 0; i < arraysize(pixels); i++) {
        double hsv2[3], rgb2[3];
        memcpy(&hsv2, hsv, sizeof(hsv2));
        hsv2[1] += (10 - rndm(1, 20)) * 0.01;
        hsv2[2] += (10 - rndm(1, 20)) * 0.01;
        hsv2[1] = MIN(1.0, MAX(0.0, hsv2[1]));
        hsv2[2] = MIN(1.0, MAX(0.0, hsv2[2]));
        colorspace_hsv2rgb(hsv2, rgb2);

        pixels[i] = SDL_MapRGBA(tmp->format, rgb2[0] * 255.0, rgb2[1] * 255.0,
                rgb2[2] * 255.0, alpha);
    }

    hsv[1] += 0.10;
    hsv[1] = MIN(1.0, hsv[2]);
    hsv[2] -= 0.25;
    hsv[2] = MAX(0.0, hsv[2]);
    colorspace_hsv2rgb(hsv, rgb);
    Uint32 edge_color = SDL_MapRGBA(tmp->format, rgb[0] * 255.0, rgb[1] * 255.0,
            rgb[2] * 255.0, MAX(0, alpha - 25));

    for (int x = 0; x < tmp->w; x++) {
        for (int y = 0; y < tmp->h; y++) {
            if (grid[tmp->w * y + x] != 1) {
                continue;
            }

            bool has_neighbors = false;
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }

                    int tx = x + dx;
                    int ty = y + dy;

                    if (tx < 0 || tx >= tmp->w || ty < 0 || ty >= tmp->h) {
                        continue;
                    }

                    if (grid[tmp->w * ty + tx] == 1) {
                        has_neighbors = true;
                        dx = 2;
                        break;
                    }
                }
            }

            if (!has_neighbors) {
                continue;
            }

            for (int off = 1; off <= 2; off++) {
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int tx = x + dx * off;
                        int ty = y + dy * off;

                        if (tx < 0 || tx >= tmp->w || ty < 0 || ty >= tmp->h) {
                            continue;
                        }

                        uint8_t *point = &grid[tmp->w * ty + tx];
                        if (*point == 0 || *point == 3) {
                            Uint32 pixel;
                            if (off == 1) {
                                pixel = pixels[rndm(0, arraysize(pixels) - 1)];
                                *point = 2;
                            } else {
                                pixel = edge_color;
                                *point = 3;
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
}

static SDL_Surface *sprite_effects_create(SDL_Surface *surface,
        const sprite_effects_t *effects)
{
#define FREE_TMP_SURFACE()      \
    do {                        \
        if (tmp != NULL) {      \
        SDL_FreeSurface(tmp);   \
        }                       \
        tmp = surface;          \
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
        snprintf(VS(buf), "rectangle:500,500,%d",
                dark_alpha[effects->dark_level]);
        SDL_BlitSurface(texture_surface(texture_get(TEXTURE_TYPE_SOFTWARE,
                buf)), NULL, surface, NULL);
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

    if ((effects->zoom_x != 0 && effects->zoom_x != 100) ||
            (effects->zoom_y != 0 && effects->zoom_y != 100) ||
            effects->rotate != 0) {
        bool smooth;
        if (effects->rotate == 0 && (effects->zoom_x == 0 ||
                abs(effects->zoom_x) == 100) && (effects->zoom_y == 0 ||
                abs(effects->zoom_y) == 100)) {
            smooth = false;
        } else {
            smooth = setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH);
        }

        surface = rotozoomSurfaceXY(surface, effects->rotate,
                effects->zoom_x != 0 ? effects->zoom_x / 100.0 : 1.0,
                effects->zoom_y ? effects->zoom_y / 100.0 : 1.0, smooth);
        if (surface == NULL) {
            goto done;
        }

        FREE_TMP_SURFACE();
    }

    if (effects->glow[0] != '\0') {
        SDL_Color color;
        if (text_color_parse(effects->glow, &color)) {
            surface = sprite_effect_glow(surface, &color, effects->glow_speed,
                    effects->glow_state);
            if (surface == NULL) {
                goto done;
            }

            FREE_TMP_SURFACE();
        }
    }

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

void surface_show(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, SDL_Surface *src)
{
    SDL_Rect dstrect;

    dstrect.x = x;
    dstrect.y = y;

    SDL_BlitSurface(src, srcrect, surface, &dstrect);
}

void surface_show_fill(SDL_Surface *surface, int x, int y, SDL_Rect *srcsize, SDL_Surface *src, SDL_Rect *box)
{
    SDL_Rect dest, srcrect;

    for (dest.x = 0; dest.x < box->w; dest.x += srcsize ? srcsize->w : src->w) {
        for (dest.y = 0; dest.y < box->h; dest.y += srcsize ? srcsize->h : src->h) {
            srcrect.x = srcsize ? MAX(0, srcsize->x) : 0;
            srcrect.y = srcsize ? MAX(0, srcsize->y) : 0;
            srcrect.w = MIN(srcsize ? srcsize->w : src->w, box->w - dest.x);
            srcrect.h = MIN(srcsize ? srcsize->h : src->h, box->h - dest.y);
            surface_show(surface, x + dest.x, y + dest.y, &srcrect, src);
        }
    }
}

void surface_show_effects(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect,
        SDL_Surface *src, const sprite_effects_t *effects)
{
    HARD_ASSERT(surface != NULL);

    if (src == NULL) {
        return;
    }

    if (effects != NULL && SPRITE_EFFECTS_NEED_RENDERING(effects)) {
        if (BIT_QUERY(effects->flags, SPRITE_FLAG_DARK) &&
                effects->dark_level == DARK_LEVELS) {
            return;
        }

        char name[HUGE_BUF];
        snprintf(VS(name), "%p;%u;%u;%s;%u;%u;%d;%d;%d;%s;%u;%u",
                src, effects->flags, effects->dark_level,
                effect_overlay_identifier(), effects->alpha, effects->stretch,
                effects->zoom_x, effects->zoom_y, effects->rotate,
                effects->glow, effects->glow_speed, effects->glow_state);

        SDL_Surface *old_src = src;
        sprite_cache_t *cache = sprite_cache_find(name);
        if (cache != NULL) {
            src = cache->surface;
        } else {
            SDL_Surface *tmp = sprite_effects_create(src, effects);
            if (tmp != NULL) {
                src = tmp;

                cache = sprite_cache_new(name);
                cache->surface = src;
                sprite_cache_add(cache);
            }
        }

        if (effects->stretch != 0) {
            y -= src->h - old_src->h;
        }

        if (effects->glow[0] != '\0') {
            y -= 1;
            x -= 1;
        }
    }

    surface_show(surface, x, y, srcrect, src);
}

/**
 * Get pixel from an SDL surface at specified X/Y position
 * @param surface SDL surface to get the pixel from.
 * @param x X position.
 * @param y Y position.
 * @return The pixel. */
Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* The address to the pixel we want to retrieve */
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
 * Puts a pixel to specified X/Y position on SDL surface.
 * @param surface The surface.
 * @param x X position.
 * @param y Y position.
 * @param pixel Pixel to put. */
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* The address to the pixel we want to set */
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
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_left(SDL_Surface *surface, int *pos, uint32_t ckey)
{
    int x, y;

    for (x = 0; x < surface->w; x++) {
        for (y = 0; y < surface->h; y++) {
            if (getpixel(surface, x, y) != ckey) {
                *pos = x;
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Calculate the right border in the surface - this is the position of
 * the first pixel from the right that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_right(SDL_Surface *surface, int *pos, uint32_t ckey)
{
    int x, y;

    for (x = surface->w - 1; x >= 0; x--) {
        for (y = 0; y < surface->h; y++) {
            if (getpixel(surface, x, y) != ckey) {
                *pos = (surface->w - 1) - x;
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Calculate the top border in the surface - this is the position of
 * the first pixel from the top that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_top(SDL_Surface *surface, int *pos, uint32_t ckey)
{
    int x, y;

    for (y = 0; y < surface->h; y++) {
        for (x = 0; x < surface->w; x++) {
            if (getpixel(surface, x, y) != ckey) {
                *pos = y;
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Calculate the bottom border in the surface - this is the position of
 * the first pixel from the bottom that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_bottom(SDL_Surface *surface, int *pos, uint32_t color)
{
    int x, y;

    for (y = surface->h - 1; y >= 0; y--) {
        for (x = 0; x < surface->w; x++) {
            if (getpixel(surface, x, y) != color) {
                *pos = (surface->h - 1) - y;
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Get borders from SDL_surface. The borders indicate the first pixel
 * from the border's side that does not match 'color'.
 * @param surface Surface to get borders from.
 * @param[out] top Where to store the top border.
 * @param[out] bottom Where to store the bottom border.
 * @param[out] left Where to store the left border.
 * @param[out] right Where to store the right border.
 * @param color Color to check for.
 * @return 1 if the borders were found, 0 otherwise (image is all filled
 * with 'color' color). */
int surface_borders_get(SDL_Surface *surface, int *top, int *bottom, int *left, int *right, uint32_t color)
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
 * Check for possible sprite collision.
 *
 * Used to make the player overlapping objects transparent.
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param sprite1
 * @param sprite2
 * @return  */
int sprite_collision(int x, int y, int x2, int y2, sprite_struct *sprite1, sprite_struct *sprite2)
{
    int left1, left2;
    int right1, right2;
    int top1, top2;
    int bottom1, bottom2;

    left1 = x + sprite1->border_left;
    left2 = x2 + sprite2->border_left;

    right1 = x + sprite1->bitmap->w - sprite1->border_right;
    right2 = x2 + sprite2->bitmap->w - sprite2->border_right;

    top1 = y + sprite1->border_up;
    top2 = y2 + sprite2->border_down;

    bottom1 = y + sprite1->bitmap->h - sprite1->border_down;
    bottom2 = y2 + sprite2->bitmap->h - sprite2->border_down;

    if (bottom1 < top2) {
        return 0;
    }

    if (top1 > bottom2) {
        return 0;
    }

    if (right1 < left2) {
        return 0;
    }

    if (left1 > right2) {
        return 0;
    }

    return 1;
}

/**
 * Pans the surface.
 * @param surface Surface.
 * @param box Coordinates. */
void surface_pan(SDL_Surface *surface, SDL_Rect *box)
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
 * Draw a single frame.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param w Width of the frame.
 * @param h Height of the frame. */
void draw_frame(SDL_Surface *surface, int x, int y, int w, int h)
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
 * @param surface Surface to use.
 * @param x X start of the border.
 * @param y Y start of the border.
 * @param w Maximum border width.
 * @param h Maximum border height.
 * @param color Color to use for the border.
 * @param size Border's size. */
void border_create(SDL_Surface *surface, int x, int y, int w, int h, int color, int size)
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

void border_create_line(SDL_Surface *surface, int x, int y, int w, int h, uint32_t color)
{
    SDL_Rect dst;

    dst.x = x;
    dst.y = y;
    dst.w = w;
    dst.h = h;
    SDL_FillRect(surface, &dst, color);
}

void border_create_sdl_color(SDL_Surface *surface, SDL_Rect *coords, int thickness, SDL_Color *color)
{
    uint32_t color_mapped;

    color_mapped = SDL_MapRGB(surface->format, color->r, color->g, color->b);

    BORDER_CREATE_TOP(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, thickness);
    BORDER_CREATE_BOTTOM(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, thickness);
    BORDER_CREATE_LEFT(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, thickness);
    BORDER_CREATE_RIGHT(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, thickness);
}

void border_create_color(SDL_Surface *surface, SDL_Rect *coords, int thickness, const char *color_notation)
{
    SDL_Color color;

    if (!text_color_parse(color_notation, &color)) {
        LOG(BUG, "Invalid color: %s", color_notation);
        return;
    }

    border_create_sdl_color(surface, coords, thickness, &color);
}

void border_create_texture(SDL_Surface *surface, SDL_Rect *coords, int thickness, SDL_Surface *texture)
{
    SDL_Rect box;

    box.w = coords->w;
    box.h = thickness;
    surface_show_fill(surface, coords->x, coords->y, NULL, texture, &box);
    surface_show_fill(surface, coords->x, coords->y + coords->h - thickness, NULL, texture, &box);

    box.w = thickness;
    box.h = coords->h;
    surface_show_fill(surface, coords->x, coords->y, NULL, texture, &box);
    surface_show_fill(surface, coords->x + coords->w - thickness, coords->y, NULL, texture, &box);
}

void rectangle_create(SDL_Surface *surface, int x, int y, int w, int h, const char *color_notation)
{
    SDL_Color color;

    if (!text_color_parse(color_notation, &color)) {
        LOG(BUG, "Invalid color: %s", color_notation);
        return;
    }

    border_create_line(surface, x, y, w, h, SDL_MapRGB(surface->format, color.r, color.g, color.b));
}

/**
 * Changes alpha value of the specified surface. If the surface is per-pixel
 * alpha, changes every pixel on the surface to match the specified alpha value.
 * @param surface Surface to change alpha value of.
 * @param alpha Alpha value to set.
 */
void surface_set_alpha(SDL_Surface *surface, uint8_t alpha)
{
    SDL_PixelFormat *fmt = surface->format;

    if (fmt->Amask == 0) {
        SDL_SetAlpha(surface, SDL_SRCALPHA, alpha);
    } else {
        int x, y;
        uint8_t bpp = fmt->BytesPerPixel;
        float scale = alpha / 255.0f;

        SDL_LockSurface(surface);

        for (y = 0; y < surface->h; y++) {
            for (x = 0; x < surface->w; x++) {
                uint8_t r, g, b, a;
                uint32_t *pixel_ptr = (Uint32 *) ((Uint8 *) surface->pixels + y * surface->pitch + x * bpp);

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
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param corners_x Array of X corner coordinates.
 * @param corners_y Array of Y corner coordinates.
 * @param corners_num Number of corner coordinate entries.
 * @return 1 if the coordinates are in the polygon, 0 otherwise.
 */
int polygon_check_coords(double x, double y,
        double corners_x[], double corners_y[], int corners_num)
{
    int i, j, odd_nodes;

    j = corners_num - 1;
    odd_nodes = 0;

    for (i = 0; i < corners_num; i++) {
        if (((corners_y[i] < y && corners_y[j] >= y) ||
                (corners_y[j] < y && corners_y[i] >= y)) &&
                (corners_x[i] <= x || corners_x[j] <= x)) {
            odd_nodes ^= (corners_x[i] + (y - corners_y[i]) / (corners_y[j] -
                    corners_y[i]) * (corners_x[j] - corners_x[i]) < x);
        }

        j = i;
    }

    return odd_nodes;
}
