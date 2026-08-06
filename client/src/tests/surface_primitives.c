/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2026 Atrinik Development Team                         *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

#include <global.h>

SDL_Surface *ScreenSurface = NULL;

#define TEST_CHECK(condition) \
    do {                      \
        if (!(condition)) {   \
            abort();          \
        }                     \
    } while (0)

static void test_packed_indexed_conversion(void) {
    SDL_Surface *surface = SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_INDEX4MSB);
    TEST_CHECK(surface != NULL);

    SDL_Color colors[16] = {{0}};
    colors[0] = (SDL_Color){255, 0, 255, SDL_ALPHA_TRANSPARENT};
    colors[1] = (SDL_Color){240, 10, 20, SDL_ALPHA_OPAQUE};
    SDL_Palette *palette = SDL_CreatePalette(arraysize(colors));
    TEST_CHECK(palette != NULL);
    TEST_CHECK(SDL_SetPaletteColors(palette, colors, 0, arraysize(colors)));
    TEST_CHECK(SDL_SetSurfacePalette(surface, palette));
    SDL_DestroyPalette(palette);
    ((Uint8 *)surface->pixels)[0] = 0x10;

    TEST_CHECK(surface_ensure_blittable(&surface));
    const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(surface->format);
    TEST_CHECK(details != NULL);
    TEST_CHECK(details->bits_per_pixel == sizeof(Uint32) * 8);

    Uint8 red, green, blue, alpha;
    TEST_CHECK(SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &alpha));
    TEST_CHECK(red == 240 && green == 10 && blue == 20 && alpha == SDL_ALPHA_OPAQUE);
    TEST_CHECK(SDL_ReadSurfacePixel(surface, 1, 0, &red, &green, &blue, &alpha));
    TEST_CHECK(alpha == SDL_ALPHA_TRANSPARENT);

    SDL_Surface *destination = SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_XRGB8888);
    TEST_CHECK(destination != NULL);
    TEST_CHECK(SDL_FillSurfaceRect(destination, NULL, surface_map_rgb(destination, 1, 2, 3)));
    TEST_CHECK(SDL_BlitSurface(surface, NULL, destination, NULL));
    TEST_CHECK(SDL_ReadSurfacePixel(destination, 0, 0, &red, &green, &blue, &alpha));
    TEST_CHECK(red == 240 && green == 10 && blue == 20);
    TEST_CHECK(SDL_ReadSurfacePixel(destination, 1, 0, &red, &green, &blue, &alpha));
    TEST_CHECK(red == 1 && green == 2 && blue == 3);

    SDL_DestroySurface(destination);
    SDL_DestroySurface(surface);
}

static void test_darken_preserves_alpha(void) {
    SDL_Surface *surface = SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_RGBA32);
    TEST_CHECK(surface != NULL);
    TEST_CHECK(SDL_WriteSurfacePixel(surface, 0, 0, 100, 150, 200, SDL_ALPHA_TRANSPARENT));
    TEST_CHECK(SDL_WriteSurfacePixel(surface, 1, 0, 100, 150, 200, 200));

    TEST_CHECK(surface_darken_preserve_alpha(surface, 128));

    Uint8 red, green, blue, alpha;
    TEST_CHECK(SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &alpha));
    TEST_CHECK(red == 50 && green == 75 && blue == 100 && alpha == SDL_ALPHA_TRANSPARENT);
    TEST_CHECK(SDL_ReadSurfacePixel(surface, 1, 0, &red, &green, &blue, &alpha));
    TEST_CHECK(red == 50 && green == 75 && blue == 100 && alpha == 200);

    SDL_DestroySurface(surface);
}

int main(void) {
    test_packed_indexed_conversion();
    test_darken_preserves_alpha();
    return 0;
}
