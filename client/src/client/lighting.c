/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/**
 * @file
 * Software-rendered, per-pixel map lighting.
 */

#include <global.h>
#include <lighting.h>

static SDL_Surface *lightmap;
static Uint32 alpha_pixels[UINT8_MAX + 1];
static uint16_t *light_samples;
static size_t light_samples_num;
static bool lighting_active;

#define LIGHT_SAMPLE_PRESENT (UINT16_C(1) << 8)
#define LIGHT_SAMPLE_ALPHA(_sample) ((uint8_t)((_sample) & UINT8_MAX))

static bool lighting_surface_create(int width, int height) {
    if (lightmap != NULL && lightmap->w == width && lightmap->h == height) {
        return true;
    }

    if (lightmap != NULL) {
        SDL_FreeSurface(lightmap);
        lightmap = NULL;
    }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    const Uint32 rmask = 0xff000000;
    const Uint32 gmask = 0x00ff0000;
    const Uint32 bmask = 0x0000ff00;
    const Uint32 amask = 0x000000ff;
#else
    const Uint32 rmask = 0x000000ff;
    const Uint32 gmask = 0x0000ff00;
    const Uint32 bmask = 0x00ff0000;
    const Uint32 amask = 0xff000000;
#endif

    lightmap = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, rmask, gmask, bmask, amask);
    if (lightmap == NULL) {
        LOG(ERROR, "Could not create map lightmap: %s", SDL_GetError());
        return false;
    }

    for (int alpha = 0; alpha <= UINT8_MAX; alpha++) {
        alpha_pixels[alpha] = SDL_MapRGBA(lightmap->format, 0, 0, 0, alpha);
    }

    size_t samples_num = (size_t)width * (size_t)height;
    light_samples = erealloc(light_samples, samples_num * sizeof(*light_samples));
    light_samples_num = samples_num;

    SDL_SetAlpha(lightmap, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
    return true;
}

bool lighting_begin(int width, int height) {
    HARD_ASSERT(width > 0);
    HARD_ASSERT(height > 0);
    HARD_ASSERT(!lighting_active);

    if (!lighting_surface_create(width, height)) {
        return false;
    }

    memset(light_samples, 0, light_samples_num * sizeof(*light_samples));
    lighting_active = true;
    return true;
}

/** Evaluate an edge at doubled coordinates, preserving pixel-center precision. */
static int64_t
lighting_edge(const lighting_vertex_t *a, const lighting_vertex_t *b, int x2, int y2) {
    return (int64_t)(x2 - a->x * 2) * (b->y - a->y) - (int64_t)(y2 - a->y * 2) * (b->x - a->x);
}

static void lighting_draw_triangle(const lighting_vertex_t *a,
                                   const lighting_vertex_t *b,
                                   const lighting_vertex_t *c) {
    int64_t area = lighting_edge(a, b, c->x * 2, c->y * 2);
    if (area == 0) {
        return;
    }

    int orientation = area < 0 ? -1 : 1;
    area *= orientation;

    int min_x = MAX(0, MIN(a->x, MIN(b->x, c->x)));
    int max_x = MIN(lightmap->w - 1, MAX(a->x, MAX(b->x, c->x)));
    int min_y = MAX(0, MIN(a->y, MIN(b->y, c->y)));
    int max_y = MIN(lightmap->h - 1, MAX(a->y, MAX(b->y, c->y)));

    int64_t row_weight_a = orientation * lighting_edge(b, c, min_x * 2 + 1, min_y * 2 + 1);
    int64_t row_weight_b = orientation * lighting_edge(c, a, min_x * 2 + 1, min_y * 2 + 1);
    int64_t row_weight_c = orientation * lighting_edge(a, b, min_x * 2 + 1, min_y * 2 + 1);
    int64_t step_x_a = orientation * 2 * (c->y - b->y);
    int64_t step_x_b = orientation * 2 * (a->y - c->y);
    int64_t step_x_c = orientation * 2 * (b->y - a->y);
    int64_t step_y_a = orientation * -2 * (c->x - b->x);
    int64_t step_y_b = orientation * -2 * (a->x - c->x);
    int64_t step_y_c = orientation * -2 * (b->x - a->x);

    for (int y = min_y; y <= max_y; y++) {
        int64_t weight_a = row_weight_a;
        int64_t weight_b = row_weight_b;
        int64_t weight_c = row_weight_c;

        for (int x = min_x; x <= max_x; x++) {
            if (weight_a >= 0 && weight_b >= 0 && weight_c >= 0) {
                int64_t level =
                    (weight_a * a->level + weight_b * b->level + weight_c * c->level + area / 2) /
                    area;
                uint8_t alpha = UINT8_MAX - (uint8_t)MIN((int64_t)UINT8_MAX, level);
                light_samples[(size_t)y * (size_t)lightmap->w + (size_t)x] =
                    LIGHT_SAMPLE_PRESENT | alpha;
            }

            weight_a += step_x_a;
            weight_b += step_x_b;
            weight_c += step_x_c;
        }

        row_weight_a += step_y_a;
        row_weight_b += step_y_b;
        row_weight_c += step_y_c;
    }
}

void lighting_draw_quad(const lighting_vertex_t vertices[4]) {
    HARD_ASSERT(vertices != NULL);
    HARD_ASSERT(lightmap != NULL);
    HARD_ASSERT(lighting_active);

    int min_x = MIN(MIN(vertices[0].x, vertices[1].x), MIN(vertices[2].x, vertices[3].x));
    int max_x = MAX(MAX(vertices[0].x, vertices[1].x), MAX(vertices[2].x, vertices[3].x));
    int min_y = MIN(MIN(vertices[0].y, vertices[1].y), MIN(vertices[2].y, vertices[3].y));
    int max_y = MAX(MAX(vertices[0].y, vertices[1].y), MAX(vertices[2].y, vertices[3].y));
    if (max_x < 0 || min_x >= lightmap->w || max_y < 0 || min_y >= lightmap->h) {
        return;
    }

    lighting_draw_triangle(&vertices[0], &vertices[1], &vertices[2]);
    lighting_draw_triangle(&vertices[0], &vertices[2], &vertices[3]);
}

/**
 * Extend the sampled map field through unsampled screen pixels.
 *
 * Ground masks can extend outside their owning diamond, and projected terrain
 * can leave gaps between sampled rows. Extrapolating the nearest edge samples
 * lights those pixels like the surrounding map without treating the unsampled
 * area as fully bright. Elevated sprites are drawn after this lightmap.
 */
static void lighting_extrapolate(void) {
    int first_sampled_row = -1;
    int previous_sampled_row = -1;

    for (int y = 0; y < lightmap->h; y++) {
        uint16_t *samples = light_samples + (size_t)y * (size_t)lightmap->w;
        int first_sample = -1;
        int previous_sample = -1;

        for (int x = 0; x < lightmap->w; x++) {
            if (!(samples[x] & LIGHT_SAMPLE_PRESENT)) {
                continue;
            }

            if (first_sample == -1) {
                first_sample = x;
                for (int fill_x = 0; fill_x < x; fill_x++) {
                    samples[fill_x] = samples[x];
                }
            } else if (x > previous_sample + 1) {
                int first_alpha = LIGHT_SAMPLE_ALPHA(samples[previous_sample]);
                int last_alpha = LIGHT_SAMPLE_ALPHA(samples[x]);
                int distance = x - previous_sample;

                for (int fill_x = previous_sample + 1; fill_x < x; fill_x++) {
                    int alpha = first_alpha +
                                (last_alpha - first_alpha) * (fill_x - previous_sample) / distance;
                    samples[fill_x] = LIGHT_SAMPLE_PRESENT | (uint8_t)alpha;
                }
            }

            previous_sample = x;
        }

        if (first_sample == -1) {
            continue;
        }

        for (int fill_x = previous_sample + 1; fill_x < lightmap->w; fill_x++) {
            samples[fill_x] = samples[previous_sample];
        }

        if (first_sampled_row == -1) {
            first_sampled_row = y;
            for (int fill_y = 0; fill_y < y; fill_y++) {
                memcpy(light_samples + (size_t)fill_y * (size_t)lightmap->w,
                       samples,
                       (size_t)lightmap->w * sizeof(*samples));
            }
        } else if (y > previous_sampled_row + 1) {
            uint16_t *first = light_samples + (size_t)previous_sampled_row * (size_t)lightmap->w;
            int distance = y - previous_sampled_row;

            for (int fill_y = previous_sampled_row + 1; fill_y < y; fill_y++) {
                uint16_t *destination = light_samples + (size_t)fill_y * (size_t)lightmap->w;

                for (int fill_x = 0; fill_x < lightmap->w; fill_x++) {
                    int first_alpha = LIGHT_SAMPLE_ALPHA(first[fill_x]);
                    int last_alpha = LIGHT_SAMPLE_ALPHA(samples[fill_x]);
                    int alpha = first_alpha + (last_alpha - first_alpha) *
                                                  (fill_y - previous_sampled_row) / distance;
                    destination[fill_x] = LIGHT_SAMPLE_PRESENT | (uint8_t)alpha;
                }
            }
        }

        previous_sampled_row = y;
    }

    if (first_sampled_row == -1) {
        for (size_t i = 0; i < light_samples_num; i++) {
            light_samples[i] = LIGHT_SAMPLE_PRESENT | UINT8_MAX;
        }
        return;
    }

    uint16_t *last = light_samples + (size_t)previous_sampled_row * (size_t)lightmap->w;
    for (int y = previous_sampled_row + 1; y < lightmap->h; y++) {
        memcpy(light_samples + (size_t)y * (size_t)lightmap->w,
               last,
               (size_t)lightmap->w * sizeof(*last));
    }
}

void lighting_render(SDL_Surface *destination) {
    HARD_ASSERT(destination != NULL);
    HARD_ASSERT(lightmap != NULL);
    HARD_ASSERT(lighting_active);

    lighting_extrapolate();

    if (SDL_LockSurface(lightmap) != 0) {
        LOG(ERROR, "Could not lock map lightmap: %s", SDL_GetError());
        lighting_active = false;
        return;
    }

    for (int y = 0; y < lightmap->h; y++) {
        Uint32 *pixels = (Uint32 *)((Uint8 *)lightmap->pixels + y * lightmap->pitch);
        const uint16_t *samples = light_samples + (size_t)y * (size_t)lightmap->w;

        for (int x = 0; x < lightmap->w; x++) {
            pixels[x] = alpha_pixels[LIGHT_SAMPLE_ALPHA(samples[x])];
        }
    }

    SDL_UnlockSurface(lightmap);
    lighting_active = false;
    SDL_BlitSurface(lightmap, NULL, destination, NULL);
}

void lighting_deinit(void) {
    lighting_active = false;

    if (lightmap != NULL) {
        SDL_FreeSurface(lightmap);
        lightmap = NULL;
    }

    if (light_samples != NULL) {
        efree(light_samples);
        light_samples = NULL;
        light_samples_num = 0;
    }
}
