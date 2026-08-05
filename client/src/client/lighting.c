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

typedef struct lighting_sprite_cache_key {
    SDL_Surface *source;
    int32_t source_x;
    int32_t source_y;
    int32_t source_w;
    int32_t source_h;
    uint64_t illumination_signature;
    uint8_t mode;
    uint8_t surface_alpha;
} lighting_sprite_cache_key;

typedef struct lighting_sprite_cache_entry {
    lighting_sprite_cache_key key;
    SDL_Surface *surface;
    size_t bytes;
    struct lighting_sprite_cache_entry *lru_previous;
    struct lighting_sprite_cache_entry *lru_next;
    UT_hash_handle hh;
} lighting_sprite_cache_entry;

typedef struct lighting_context {
    SDL_Surface *lightmap;
    uint16_t *light_samples;
    uint16_t *structure_samples;
    uint16_t *structure_blur_row;
    uint8_t *structure_rows_valid;
    size_t light_samples_num;
    int width;
    int height;
    bool active;
    bool cache_valid;
    bool surface_valid;
    bool update_needed;
    uint64_t cache_key;
    uint64_t pending_cache_key;
    lighting_sprite_cache_entry *sprite_cache;
    lighting_sprite_cache_entry *sprite_cache_lru_oldest;
    lighting_sprite_cache_entry *sprite_cache_lru_newest;
    size_t sprite_cache_bytes;
} lighting_context;

static lighting_context lighting_contexts[MAP2_LEVELS];
static lighting_context *lighting_context_current = &lighting_contexts[MAP2_DEPTH_INDEX(0)];
static SDL_Surface *lighting_lit_surface;
static int *structure_column_bottom;
static uint8_t *structure_column_illumination;
static Uint32 alpha_pixels[UINT8_MAX + 1];

static void lighting_sprite_cache_clear(lighting_context *context);

/** Release an optional compositing surface while retaining sampled light. */
static void lighting_context_free_surface(lighting_context *context) {
    if (context->lightmap != NULL) {
        SDL_FreeSurface(context->lightmap);
        context->lightmap = NULL;
        context->surface_valid = false;
    }
}

static void lighting_context_free(lighting_context *context) {
    lighting_context_free_surface(context);

    free(context->light_samples);
    free(context->structure_samples);
    free(context->structure_blur_row);
    free(context->structure_rows_valid);
    lighting_sprite_cache_clear(context);
    memset(context, 0, sizeof(*context));
}

#define lightmap (lighting_context_current->lightmap)
#define light_samples (lighting_context_current->light_samples)
#define structure_samples (lighting_context_current->structure_samples)
#define structure_blur_row (lighting_context_current->structure_blur_row)
#define structure_rows_valid (lighting_context_current->structure_rows_valid)
#define light_samples_num (lighting_context_current->light_samples_num)
#define lighting_width (lighting_context_current->width)
#define lighting_height (lighting_context_current->height)
#define lighting_active (lighting_context_current->active)
#define lighting_cache_valid (lighting_context_current->cache_valid)
#define lighting_update_needed (lighting_context_current->update_needed)
#define lighting_cache_key (lighting_context_current->cache_key)
#define lighting_pending_cache_key (lighting_context_current->pending_cache_key)

#define LIGHT_SAMPLE_PRESENT (UINT16_C(1) << 8)
#define LIGHT_SAMPLE_ALPHA(_sample) ((uint8_t)((_sample) & UINT8_MAX))
#define LIGHT_STRUCTURE_BLUR_RADIUS 24
#define LIGHTING_SPRITE_CACHE_MAX_BYTES (8 * 1024 * 1024)

static void lighting_sprite_cache_clear(lighting_context *context) {
    lighting_sprite_cache_entry *entry, *next;
    HASH_ITER(hh, context->sprite_cache, entry, next) {
        HASH_DEL(context->sprite_cache, entry);
        SDL_FreeSurface(entry->surface);
        free(entry);
    }

    context->sprite_cache_bytes = 0;
    context->sprite_cache_lru_oldest = NULL;
    context->sprite_cache_lru_newest = NULL;
}

/** Append an unlinked entry to the newest end of the cache's LRU list. */
static void lighting_sprite_cache_append(lighting_context *context,
                                         lighting_sprite_cache_entry *entry) {
    entry->lru_previous = context->sprite_cache_lru_newest;
    entry->lru_next = NULL;
    if (context->sprite_cache_lru_newest != NULL) {
        context->sprite_cache_lru_newest->lru_next = entry;
    } else {
        context->sprite_cache_lru_oldest = entry;
    }
    context->sprite_cache_lru_newest = entry;
}

/** Move an existing entry to the newest end of the cache's LRU list. */
static void lighting_sprite_cache_touch(lighting_context *context,
                                        lighting_sprite_cache_entry *entry) {
    if (context->sprite_cache_lru_newest == entry) {
        return;
    }

    if (entry->lru_previous != NULL) {
        entry->lru_previous->lru_next = entry->lru_next;
    } else {
        context->sprite_cache_lru_oldest = entry->lru_next;
    }

    HARD_ASSERT(entry->lru_next != NULL);
    entry->lru_next->lru_previous = entry->lru_previous;
    lighting_sprite_cache_append(context, entry);
}

/** Make room for one lit sprite without discarding the whole warm cache. */
static void lighting_sprite_cache_reserve(lighting_context *context, size_t bytes) {
    while (context->sprite_cache != NULL &&
           context->sprite_cache_bytes + bytes > LIGHTING_SPRITE_CACHE_MAX_BYTES) {
        lighting_sprite_cache_entry *oldest = context->sprite_cache_lru_oldest;
        HARD_ASSERT(oldest != NULL);
        context->sprite_cache_lru_oldest = oldest->lru_next;
        if (context->sprite_cache_lru_oldest != NULL) {
            context->sprite_cache_lru_oldest->lru_previous = NULL;
        } else {
            context->sprite_cache_lru_newest = NULL;
        }
        HASH_DEL(context->sprite_cache, oldest);
        context->sprite_cache_bytes -= oldest->bytes;
        SDL_FreeSurface(oldest->surface);
        free(oldest);
    }
}

bool lighting_select_level(int depth) {
    if (depth < -MAP2_MAX_DEPTH || depth > MAP2_MAX_DEPTH || lighting_active) {
        return false;
    }

    lighting_context_current = &lighting_contexts[MAP2_DEPTH_INDEX(depth)];
    return true;
}

void lighting_set_level_mask(uint16_t mask) {
    HARD_ASSERT(!lighting_active);

    for (size_t i = 0; i < arraysize(lighting_contexts); i++) {
        if (!(mask & (UINT16_C(1) << i))) {
            lighting_context_free(&lighting_contexts[i]);
        }
    }

    lighting_context_current = &lighting_contexts[MAP2_DEPTH_INDEX(0)];
}

void lighting_level_scroll(int dz) {
    HARD_ASSERT(!lighting_active);

    if (dz == 0) {
        return;
    }

    lighting_context shifted[MAP2_LEVELS] = {0};
    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        int source_depth = depth + dz;
        if (source_depth >= -MAP2_MAX_DEPTH && source_depth <= MAP2_MAX_DEPTH) {
            size_t destination = MAP2_DEPTH_INDEX(depth);
            size_t source = MAP2_DEPTH_INDEX(source_depth);
            shifted[destination] = lighting_contexts[source];
            memset(&lighting_contexts[source], 0, sizeof(lighting_contexts[source]));
        }
    }

    for (size_t i = 0; i < arraysize(lighting_contexts); i++) {
        lighting_context_free(&lighting_contexts[i]);
        lighting_contexts[i] = shifted[i];

        /* Only relative depth zero is ever composited through a standalone
         * screen-sized lightmap. Release a shifted surface immediately even
         * when smooth lighting is currently disabled and no begin call will
         * otherwise visit this context. */
        if (i != MAP2_DEPTH_INDEX(0)) {
            lighting_context_free_surface(&lighting_contexts[i]);
        }
    }

    lighting_context_current = &lighting_contexts[MAP2_DEPTH_INDEX(0)];
}

/** Create one 32-bit RGBA surface used by the software lighting pipeline. */
static SDL_Surface *lighting_rgba_surface_create(int width, int height) {
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

    return SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, rmask, gmask, bmask, amask);
}

static bool lighting_surface_create(int width, int height) {
    bool size_changed = lighting_width != width || lighting_height != height;
    if (size_changed) {
        if (lightmap != NULL) {
            SDL_FreeSurface(lightmap);
            lightmap = NULL;
        }

        size_t samples_num = (size_t)width * (size_t)height;
        light_samples = xreallocarray(light_samples, samples_num, sizeof(*light_samples));
        structure_samples =
            xreallocarray(structure_samples, samples_num, sizeof(*structure_samples));
        structure_blur_row =
            xreallocarray(structure_blur_row, (size_t)width, sizeof(*structure_blur_row));
        structure_rows_valid =
            xreallocarray(structure_rows_valid, (size_t)height, sizeof(*structure_rows_valid));
        light_samples_num = samples_num;
        lighting_width = width;
        lighting_height = height;
        lighting_cache_valid = false;
        lighting_context_current->surface_valid = false;
    }

    bool needs_lightmap = lighting_context_current == &lighting_contexts[MAP2_DEPTH_INDEX(0)];
    if (!needs_lightmap) {
        if (lightmap != NULL) {
            SDL_FreeSurface(lightmap);
            lightmap = NULL;
            lighting_context_current->surface_valid = false;
        }
        return true;
    }

    if (lightmap == NULL) {
        lightmap = lighting_rgba_surface_create(width, height);
        if (lightmap == NULL) {
            LOG(ERROR, "Could not create map lightmap: %s", SDL_GetError());
            return false;
        }

        for (int alpha = 0; alpha <= UINT8_MAX; alpha++) {
            alpha_pixels[alpha] = SDL_MapRGBA(lightmap->format, 0, 0, 0, alpha);
        }

        SDL_SetAlpha(lightmap, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
        lighting_context_current->surface_valid = false;
    }
    return true;
}

bool lighting_begin(int width, int height, uint64_t cache_key) {
    HARD_ASSERT(width > 0);
    HARD_ASSERT(height > 0);
    HARD_ASSERT(!lighting_active);

    if (!lighting_surface_create(width, height)) {
        return false;
    }

    lighting_update_needed = !lighting_cache_valid || lighting_cache_key != cache_key;
    lighting_pending_cache_key = cache_key;
    if (lighting_update_needed) {
        memset(light_samples, 0, light_samples_num * sizeof(*light_samples));
    }

    lighting_active = true;
    return true;
}

void lighting_clear_sprite_cache(void) {
    for (size_t i = 0; i < arraysize(lighting_contexts); i++) {
        lighting_sprite_cache_clear(&lighting_contexts[i]);
    }
}

bool lighting_needs_update(void) {
    HARD_ASSERT(lighting_active);
    return lighting_update_needed;
}

/** Evaluate an edge at doubled coordinates, preserving pixel-center precision. */
static int64_t
lighting_edge(const lighting_vertex_t *a, const lighting_vertex_t *b, int x2, int y2) {
    return (int64_t)(x2 - a->x * 2) * (b->y - a->y) - (int64_t)(y2 - a->y * 2) * (b->x - a->x);
}

/** Rasterize one half of a quad while interpolating its four corner levels. */
static void lighting_draw_triangle(const lighting_vertex_t vertices[4], bool first_half) {
    const lighting_vertex_t *a = &vertices[0];
    const lighting_vertex_t *b = &vertices[first_half ? 1 : 2];
    const lighting_vertex_t *c = &vertices[first_half ? 2 : 3];
    int64_t area = lighting_edge(a, b, c->x * 2, c->y * 2);
    if (area == 0) {
        return;
    }

    int orientation = area < 0 ? -1 : 1;
    area *= orientation;

    int min_x = MAX(0, MIN(a->x, MIN(b->x, c->x)));
    int max_x = MIN(lighting_width - 1, MAX(a->x, MAX(b->x, c->x)));
    int min_y = MAX(0, MIN(a->y, MIN(b->y, c->y)));
    int max_y = MIN(lighting_height - 1, MAX(a->y, MAX(b->y, c->y)));

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
                /* Recover the quad's logical U/V coordinates from this
                 * triangle's barycentric weights, then blend all four light
                 * samples. This removes the visible gradient crease created
                 * by treating the two halves as unrelated planes. */
                int64_t u = first_half ? weight_b + weight_c : weight_b;
                int64_t v = first_half ? weight_c : weight_b + weight_c;
                int64_t inverse_u = area - u;
                int64_t inverse_v = area - v;
                int64_t divisor = area * area;
                int64_t level =
                    (vertices[0].level * inverse_u * inverse_v + vertices[1].level * u * inverse_v +
                     vertices[2].level * u * v + vertices[3].level * inverse_u * v + divisor / 2) /
                    divisor;
                uint8_t alpha = UINT8_MAX - (uint8_t)MIN((int64_t)UINT8_MAX, level);
                light_samples[(size_t)y * (size_t)lighting_width + (size_t)x] =
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
    HARD_ASSERT(light_samples != NULL);
    HARD_ASSERT(lighting_width > 0);
    HARD_ASSERT(lighting_height > 0);
    HARD_ASSERT(lighting_active);
    HARD_ASSERT(lighting_update_needed);

    int min_x = MIN(MIN(vertices[0].x, vertices[1].x), MIN(vertices[2].x, vertices[3].x));
    int max_x = MAX(MAX(vertices[0].x, vertices[1].x), MAX(vertices[2].x, vertices[3].x));
    int min_y = MIN(MIN(vertices[0].y, vertices[1].y), MIN(vertices[2].y, vertices[3].y));
    int max_y = MAX(MAX(vertices[0].y, vertices[1].y), MAX(vertices[2].y, vertices[3].y));
    if (max_x < 0 || min_x >= lighting_width || max_y < 0 || min_y >= lighting_height) {
        return;
    }

    lighting_draw_triangle(vertices, true);
    lighting_draw_triangle(vertices, false);
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

    for (int y = 0; y < lighting_height; y++) {
        uint16_t *samples = light_samples + (size_t)y * (size_t)lighting_width;
        int first_sample = -1;
        int previous_sample = -1;

        for (int x = 0; x < lighting_width; x++) {
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

        for (int fill_x = previous_sample + 1; fill_x < lighting_width; fill_x++) {
            samples[fill_x] = samples[previous_sample];
        }

        if (first_sampled_row == -1) {
            first_sampled_row = y;
            for (int fill_y = 0; fill_y < y; fill_y++) {
                memcpy(light_samples + (size_t)fill_y * (size_t)lighting_width,
                       samples,
                       (size_t)lighting_width * sizeof(*samples));
            }
        } else if (y > previous_sampled_row + 1) {
            uint16_t *first = light_samples + (size_t)previous_sampled_row * (size_t)lighting_width;
            int distance = y - previous_sampled_row;

            for (int fill_y = previous_sampled_row + 1; fill_y < y; fill_y++) {
                uint16_t *destination = light_samples + (size_t)fill_y * (size_t)lighting_width;

                for (int fill_x = 0; fill_x < lighting_width; fill_x++) {
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

    uint16_t *last = light_samples + (size_t)previous_sampled_row * (size_t)lighting_width;
    for (int y = previous_sampled_row + 1; y < lighting_height; y++) {
        memcpy(light_samples + (size_t)y * (size_t)lighting_width,
               last,
               (size_t)lighting_width * sizeof(*last));
    }
}

/** Apply one horizontal box-blur pass to a light sample row. */
static void lighting_blur_row(const uint16_t *source, uint16_t *destination) {
    const int radius = LIGHT_STRUCTURE_BLUR_RADIUS;
    const uint32_t diameter = radius * 2 + 1;
    uint32_t sum = 0;

    for (int offset = -radius; offset <= radius; offset++) {
        int source_x = MAX(0, MIN(lighting_width - 1, offset));
        sum += LIGHT_SAMPLE_ALPHA(source[source_x]);
    }

    for (int x = 0; x < lighting_width; x++) {
        destination[x] = LIGHT_SAMPLE_PRESENT | (uint8_t)((sum + diameter / 2) / diameter);

        int outgoing_x = MAX(0, MIN(lighting_width - 1, x - radius));
        int incoming_x = MAX(0, MIN(lighting_width - 1, x + radius + 1));
        sum -= LIGHT_SAMPLE_ALPHA(source[outgoing_x]);
        sum += LIGHT_SAMPLE_ALPHA(source[incoming_x]);
    }
}

/** Lazily soften one horizontal light row used by a large structure. */
static void lighting_blur_structure_row(int y) {
    HARD_ASSERT(y >= 0 && y < lighting_height);

    if (structure_rows_valid[y]) {
        return;
    }

    const uint16_t *source = light_samples + (size_t)y * (size_t)lighting_width;
    uint16_t *destination = structure_samples + (size_t)y * (size_t)lighting_width;
    lighting_blur_row(source, structure_blur_row);
    lighting_blur_row(structure_blur_row, destination);

    structure_rows_valid[y] = 1;
}

/** Sample the structural field through a vertical triangular filter. */
static uint8_t lighting_structure_darkness(int x, int y) {
    const int radius = LIGHT_STRUCTURE_BLUR_RADIUS;
    uint32_t total = 0;
    uint32_t weights = 0;

    for (int offset = -radius; offset <= radius; offset++) {
        int sample_y = MAX(0, MIN(lighting_height - 1, y + offset));
        uint32_t weight = (uint32_t)(radius + 1 - abs(offset));
        lighting_blur_structure_row(sample_y);
        total += LIGHT_SAMPLE_ALPHA(
                     structure_samples[(size_t)sample_y * (size_t)lighting_width + (size_t)x]) *
                 weight;
        weights += weight;
    }

    return (uint8_t)((total + weights / 2) / weights);
}

void lighting_render(SDL_Surface *destination) {
    HARD_ASSERT(lighting_width > 0);
    HARD_ASSERT(lighting_height > 0);
    HARD_ASSERT(lighting_active);

    bool samples_updated = lighting_update_needed;
    if (samples_updated) {
        lighting_extrapolate();
        memset(structure_rows_valid, 0, (size_t)lighting_height * sizeof(*structure_rows_valid));
        lighting_cache_key = lighting_pending_cache_key;
        lighting_cache_valid = true;
        lighting_context_current->surface_valid = false;
    }
    lighting_active = false;
    if (destination == NULL) {
        return;
    }
    HARD_ASSERT(lightmap != NULL);
    if (lighting_context_current->surface_valid) {
        SDL_BlitSurface(lightmap, NULL, destination, NULL);
        return;
    }

    if (SDL_LockSurface(lightmap) != 0) {
        LOG(ERROR, "Could not lock map lightmap: %s", SDL_GetError());
        lighting_cache_valid = false;
        return;
    }

    for (int y = 0; y < lighting_height; y++) {
        Uint32 *pixels = (Uint32 *)((Uint8 *)lightmap->pixels + y * lightmap->pitch);
        const uint16_t *samples = light_samples + (size_t)y * (size_t)lighting_width;

        for (int x = 0; x < lighting_width; x++) {
            pixels[x] = alpha_pixels[LIGHT_SAMPLE_ALPHA(samples[x])];
        }
    }

    SDL_UnlockSurface(lightmap);
    lighting_context_current->surface_valid = true;
    SDL_BlitSurface(lightmap, NULL, destination, NULL);
}

/** Ensure the reusable smoothly lit sprite surface is large enough. */
static bool lighting_lit_surface_create(int width, int height) {
    if (lighting_lit_surface != NULL && lighting_lit_surface->w >= width &&
        lighting_lit_surface->h >= height) {
        return true;
    }

    if (lighting_lit_surface != NULL) {
        SDL_FreeSurface(lighting_lit_surface);
        lighting_lit_surface = NULL;
    }

    lighting_lit_surface = lighting_rgba_surface_create(width, height);
    if (lighting_lit_surface == NULL) {
        LOG(ERROR, "Could not create smoothly lit sprite surface: %s", SDL_GetError());
        return false;
    }

    structure_column_bottom =
        xreallocarray(structure_column_bottom, (size_t)width, sizeof(*structure_column_bottom));
    structure_column_illumination = xreallocarray(structure_column_illumination,
                                                  (size_t)width,
                                                  sizeof(*structure_column_illumination));
    SDL_SetAlpha(lighting_lit_surface, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
    return true;
}

/** Get a source pixel's intrinsic alpha, excluding whole-surface opacity. */
static uint8_t lighting_source_alpha(SDL_Surface *source, Uint32 pixel, bool has_colorkey) {
    if (has_colorkey && pixel == source->format->colorkey) {
        return SDL_ALPHA_TRANSPARENT;
    }

    uint8_t red, green, blue, alpha;
    SDL_GetRGBA(pixel, source->format, &red, &green, &blue, &alpha);
    return alpha;
}

/** Copy the active portion of the reusable lit-sprite surface. */
static SDL_Surface *lighting_lit_surface_copy(int width, int height) {
    SDL_Surface *copy = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                             width,
                                             height,
                                             lighting_lit_surface->format->BitsPerPixel,
                                             lighting_lit_surface->format->Rmask,
                                             lighting_lit_surface->format->Gmask,
                                             lighting_lit_surface->format->Bmask,
                                             lighting_lit_surface->format->Amask);
    if (copy == NULL) {
        return NULL;
    }

    if (SDL_LockSurface(lighting_lit_surface) != 0) {
        SDL_FreeSurface(copy);
        return NULL;
    }
    if (SDL_LockSurface(copy) != 0) {
        SDL_UnlockSurface(lighting_lit_surface);
        SDL_FreeSurface(copy);
        return NULL;
    }

    size_t row_bytes = (size_t)width * (size_t)copy->format->BytesPerPixel;
    for (int row = 0; row < height; row++) {
        memcpy((Uint8 *)copy->pixels + row * copy->pitch,
               (Uint8 *)lighting_lit_surface->pixels + row * lighting_lit_surface->pitch,
               row_bytes);
    }

    SDL_UnlockSurface(copy);
    SDL_UnlockSurface(lighting_lit_surface);
    SDL_SetAlpha(copy, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
    return copy;
}

static uint64_t lighting_signature_byte(uint64_t signature, uint8_t value) {
    signature ^= value;
    signature *= UINT64_C(1099511628211);
    return signature;
}

/** Draw a sprite through the cached continuous light field. */
void lighting_show_surface(SDL_Surface *destination,
                           int x,
                           int y,
                           SDL_Rect *srcrect,
                           SDL_Surface *source,
                           int sample_y,
                           lighting_surface_mode_t mode) {
    HARD_ASSERT(destination != NULL);
    HARD_ASSERT(source != NULL);

    if (!lighting_cache_valid) {
        surface_show(destination, x, y, srcrect, source);
        return;
    }

    SDL_Rect source_rect = {
        .x = srcrect != NULL ? srcrect->x : 0,
        .y = srcrect != NULL ? srcrect->y : 0,
        .w = srcrect != NULL ? srcrect->w : source->w,
        .h = srcrect != NULL ? srcrect->h : source->h,
    };
    if (source_rect.w <= 0 || source_rect.h <= 0) {
        surface_show(destination, x, y, srcrect, source);
        return;
    }

    bool has_colorkey = (source->flags & SDL_SRCCOLORKEY) != 0;
    bool has_surface_alpha = (source->flags & SDL_SRCALPHA) != 0;
    if (!lighting_lit_surface_create(source_rect.w, source_rect.h)) {
        surface_show(destination, x, y, srcrect, source);
        return;
    }

    bool source_locked = false;
    uint64_t illumination_signature = UINT64_C(14695981039346656037);

    if (mode == LIGHTING_SURFACE_STRUCTURE) {
        if (SDL_LockSurface(source) != 0) {
            LOG(ERROR, "Could not lock smoothly lit sprite: %s", SDL_GetError());
            surface_show(destination, x, y, srcrect, source);
            return;
        }
        source_locked = true;

        /* A wall sprite is a vertical projection of an isometric ground edge.
         * Find that edge from the lowest opaque pixel in each source column,
         * then project every pixel in the column onto it. */
        int max_bottom = -1;
        for (int source_x = 0; source_x < source_rect.w; source_x++) {
            int bottom = -1;

            for (int source_y = source_rect.h - 1; source_y >= 0; source_y--) {
                Uint32 source_pixel =
                    getpixel(source, source_rect.x + source_x, source_rect.y + source_y);
                if (lighting_source_alpha(source, source_pixel, has_colorkey) !=
                    SDL_ALPHA_TRANSPARENT) {
                    bottom = source_y;
                    break;
                }
            }

            structure_column_bottom[source_x] = bottom;
            max_bottom = MAX(max_bottom, bottom);
        }

        for (int source_x = 0; source_x < source_rect.w; source_x++) {
            int bottom = structure_column_bottom[source_x];
            if (bottom < 0) {
                structure_column_illumination[source_x] = UINT8_MAX;
                continue;
            }

            int light_x = MAX(0, MIN(lighting_width - 1, x + source_x));
            int light_y = MAX(0, MIN(lighting_height - 1, sample_y - max_bottom + bottom));
            uint8_t darkness = lighting_structure_darkness(light_x, light_y);
            structure_column_illumination[source_x] = UINT8_MAX - darkness;
            illumination_signature =
                lighting_signature_byte(illumination_signature,
                                        structure_column_illumination[source_x]);
        }
    } else {
        /* Hash the exact projected illumination profile. The signature moves
         * with world geometry across a camera scroll, allowing an existing
         * lit surface to be reused at a new destination without accepting a
         * stale lighting result. */
        for (int source_y = 0; source_y < source_rect.h; source_y++) {
            int light_y = MAX(0, MIN(lighting_height - 1, y + source_y));
            for (int source_x = 0; source_x < source_rect.w; source_x++) {
                int light_x = MAX(0, MIN(lighting_width - 1, x + source_x));
                uint8_t illumination =
                    UINT8_MAX -
                    LIGHT_SAMPLE_ALPHA(
                        light_samples[(size_t)light_y * lighting_width + (size_t)light_x]);
                illumination_signature =
                    lighting_signature_byte(illumination_signature, illumination);
            }
        }
    }

    lighting_sprite_cache_key cache_key;
    memset(&cache_key, 0, sizeof(cache_key));
    cache_key.source = source;
    cache_key.source_x = source_rect.x;
    cache_key.source_y = source_rect.y;
    cache_key.source_w = source_rect.w;
    cache_key.source_h = source_rect.h;
    cache_key.illumination_signature = illumination_signature;
    cache_key.mode = mode;
    cache_key.surface_alpha = has_surface_alpha ? source->format->alpha : SDL_ALPHA_OPAQUE;
    lighting_sprite_cache_entry *cached;
    HASH_FIND(hh, lighting_context_current->sprite_cache, &cache_key, sizeof(cache_key), cached);
    if (cached != NULL) {
        lighting_sprite_cache_touch(lighting_context_current, cached);
        if (source_locked) {
            SDL_UnlockSurface(source);
        }
        surface_show(destination, x, y, NULL, cached->surface);
        return;
    }

    if (!source_locked) {
        if (SDL_LockSurface(source) != 0) {
            LOG(ERROR, "Could not lock smoothly lit sprite: %s", SDL_GetError());
            surface_show(destination, x, y, srcrect, source);
            return;
        }
    }
    if (SDL_LockSurface(lighting_lit_surface) != 0) {
        LOG(ERROR, "Could not lock smoothly lit sprite surface: %s", SDL_GetError());
        SDL_UnlockSurface(source);
        surface_show(destination, x, y, srcrect, source);
        return;
    }

    for (int source_y = 0; source_y < source_rect.h; source_y++) {
        Uint32 *destination_pixels = (Uint32 *)((Uint8 *)lighting_lit_surface->pixels +
                                                source_y * lighting_lit_surface->pitch);

        for (int source_x = 0; source_x < source_rect.w; source_x++) {
            Uint32 source_pixel =
                getpixel(source, source_rect.x + source_x, source_rect.y + source_y);
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            uint8_t source_alpha = SDL_ALPHA_OPAQUE;
            if (has_colorkey && source_pixel == source->format->colorkey) {
                source_alpha = SDL_ALPHA_TRANSPARENT;
            } else {
                SDL_GetRGBA(source_pixel, source->format, &red, &green, &blue, &source_alpha);
            }
            if (has_surface_alpha) {
                source_alpha = (uint8_t)((unsigned int)source_alpha * source->format->alpha /
                                         SDL_ALPHA_OPAQUE);
            }

            uint8_t illumination;
            if (mode == LIGHTING_SURFACE_STRUCTURE) {
                illumination = structure_column_illumination[source_x];
            } else {
                int light_x = MAX(0, MIN(lighting_width - 1, x + source_x));
                int light_y = MAX(0, MIN(lighting_height - 1, y + source_y));
                illumination =
                    UINT8_MAX -
                    LIGHT_SAMPLE_ALPHA(
                        light_samples[(size_t)light_y * lighting_width + (size_t)light_x]);
            }
            red = (uint8_t)((unsigned int)red * illumination / UINT8_MAX);
            green = (uint8_t)((unsigned int)green * illumination / UINT8_MAX);
            blue = (uint8_t)((unsigned int)blue * illumination / UINT8_MAX);
            destination_pixels[source_x] =
                SDL_MapRGBA(lighting_lit_surface->format, red, green, blue, source_alpha);
        }
    }

    SDL_UnlockSurface(lighting_lit_surface);
    SDL_UnlockSurface(source);

    SDL_Rect lit_rect = {.x = 0, .y = 0, .w = source_rect.w, .h = source_rect.h};
    size_t cache_bytes = (size_t)source_rect.w * (size_t)source_rect.h *
                         (size_t)lighting_lit_surface->format->BytesPerPixel;
    if (cache_bytes <= LIGHTING_SPRITE_CACHE_MAX_BYTES) {
        lighting_sprite_cache_reserve(lighting_context_current, cache_bytes);
        SDL_Surface *copy = lighting_lit_surface_copy(source_rect.w, source_rect.h);
        if (copy != NULL) {
            lighting_sprite_cache_entry *entry = xcalloc(1, sizeof(*entry));
            entry->key = cache_key;
            entry->surface = copy;
            entry->bytes = cache_bytes;
            HASH_ADD(hh, lighting_context_current->sprite_cache, key, sizeof(entry->key), entry);
            lighting_sprite_cache_append(lighting_context_current, entry);
            lighting_context_current->sprite_cache_bytes += cache_bytes;
            surface_show(destination, x, y, NULL, copy);
            return;
        }
    }

    surface_show(destination, x, y, &lit_rect, lighting_lit_surface);
}

#undef lightmap
#undef light_samples
#undef structure_samples
#undef structure_blur_row
#undef structure_rows_valid
#undef light_samples_num
#undef lighting_width
#undef lighting_height
#undef lighting_active
#undef lighting_cache_valid
#undef lighting_update_needed
#undef lighting_cache_key
#undef lighting_pending_cache_key

void lighting_deinit(void) {
    for (size_t i = 0; i < arraysize(lighting_contexts); i++) {
        lighting_context_free(&lighting_contexts[i]);
    }

    lighting_context_current = &lighting_contexts[MAP2_DEPTH_INDEX(0)];

    if (lighting_lit_surface != NULL) {
        SDL_FreeSurface(lighting_lit_surface);
        lighting_lit_surface = NULL;
    }

    free(structure_column_bottom);
    structure_column_bottom = NULL;
    free(structure_column_illumination);
    structure_column_illumination = NULL;
}
