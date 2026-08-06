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

/**
 * @file
 * Small SDL3-native set of software-surface primitives used by Atrinik.
 */

#include <global.h>

SDL_Surface *surface_create_rgb(Uint32 flags,
                                int width,
                                int height,
                                int depth,
                                Uint32 red_mask,
                                Uint32 green_mask,
                                Uint32 blue_mask,
                                Uint32 alpha_mask) {
    (void)flags;

    SDL_PixelFormat format;
    if (red_mask != 0 || green_mask != 0 || blue_mask != 0 || alpha_mask != 0) {
        format = SDL_GetPixelFormatForMasks(depth, red_mask, green_mask, blue_mask, alpha_mask);
    } else if (depth == 8) {
        format = SDL_PIXELFORMAT_INDEX8;
    } else {
        format = ScreenSurface != NULL ? ScreenSurface->format : SDL_PIXELFORMAT_RGBA32;
    }

    return SDL_CreateSurface(width, height, format);
}

Uint32 pixel_format_map_rgb(SDL_PixelFormat format, Uint8 red, Uint8 green, Uint8 blue) {
    return SDL_MapRGB(SDL_GetPixelFormatDetails(format), NULL, red, green, blue);
}

Uint32
pixel_format_map_rgba(SDL_PixelFormat format, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    return SDL_MapRGBA(SDL_GetPixelFormatDetails(format), NULL, red, green, blue, alpha);
}

void pixel_format_get_rgb(Uint32 pixel,
                          SDL_PixelFormat format,
                          Uint8 *red,
                          Uint8 *green,
                          Uint8 *blue) {
    SDL_GetRGB(pixel, SDL_GetPixelFormatDetails(format), NULL, red, green, blue);
}

void pixel_format_get_rgba(Uint32 pixel,
                           SDL_PixelFormat format,
                           Uint8 *red,
                           Uint8 *green,
                           Uint8 *blue,
                           Uint8 *alpha) {
    SDL_GetRGBA(pixel, SDL_GetPixelFormatDetails(format), NULL, red, green, blue, alpha);
}

SDL_Surface *surface_to_display(SDL_Surface *surface) {
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(ScreenSurface != NULL);

    return SDL_ConvertSurface(surface, ScreenSurface->format);
}

Uint32 surface_map_rgb(SDL_Surface *surface, Uint8 red, Uint8 green, Uint8 blue) {
    return SDL_MapRGB(SDL_GetPixelFormatDetails(surface->format),
                      SDL_GetSurfacePalette(surface),
                      red,
                      green,
                      blue);
}

Uint32 surface_map_rgba(SDL_Surface *surface, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    return SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format),
                       SDL_GetSurfacePalette(surface),
                       red,
                       green,
                       blue,
                       alpha);
}

void surface_get_rgba(SDL_Surface *surface,
                      Uint32 pixel,
                      Uint8 *red,
                      Uint8 *green,
                      Uint8 *blue,
                      Uint8 *alpha) {
    SDL_GetRGBA(pixel,
                SDL_GetPixelFormatDetails(surface->format),
                SDL_GetSurfacePalette(surface),
                red,
                green,
                blue,
                alpha);
}

static int surface_pixel_blend(SDL_Surface *surface,
                               int x,
                               int y,
                               Uint8 red,
                               Uint8 green,
                               Uint8 blue,
                               Uint8 alpha) {
    SDL_Rect clip;
    Uint8 dst_red, dst_green, dst_blue, dst_alpha;

    if (surface == NULL) {
        return -1;
    }

    SDL_GetSurfaceClipRect(surface, &clip);
    if (x < clip.x || y < clip.y || x >= clip.x + clip.w || y >= clip.y + clip.h) {
        return 0;
    }

    if (alpha == SDL_ALPHA_TRANSPARENT) {
        return 0;
    }

    if (alpha == SDL_ALPHA_OPAQUE) {
        return SDL_WriteSurfacePixel(surface, x, y, red, green, blue, alpha) ? 0 : -1;
    }

    if (!SDL_ReadSurfacePixel(surface, x, y, &dst_red, &dst_green, &dst_blue, &dst_alpha)) {
        return -1;
    }

    const unsigned int inverse = SDL_ALPHA_OPAQUE - alpha;
    red = (Uint8)((red * alpha + dst_red * inverse + 127) / SDL_ALPHA_OPAQUE);
    green = (Uint8)((green * alpha + dst_green * inverse + 127) / SDL_ALPHA_OPAQUE);
    blue = (Uint8)((blue * alpha + dst_blue * inverse + 127) / SDL_ALPHA_OPAQUE);
    dst_alpha = (Uint8)(alpha + (dst_alpha * inverse + 127) / SDL_ALPHA_OPAQUE);

    return SDL_WriteSurfacePixel(surface, x, y, red, green, blue, dst_alpha) ? 0 : -1;
}

int lineRGBA(SDL_Surface *surface,
             Sint16 x1,
             Sint16 y1,
             Sint16 x2,
             Sint16 y2,
             Uint8 red,
             Uint8 green,
             Uint8 blue,
             Uint8 alpha) {
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        if (surface_pixel_blend(surface, x1, y1, red, green, blue, alpha) < 0) {
            return -1;
        }

        if (x1 == x2 && y1 == y2) {
            return 0;
        }

        int error2 = error * 2;
        if (error2 >= dy) {
            error += dy;
            x1 += sx;
        }
        if (error2 <= dx) {
            error += dx;
            y1 += sy;
        }
    }
}

int boxRGBA(SDL_Surface *surface,
            Sint16 x1,
            Sint16 y1,
            Sint16 x2,
            Sint16 y2,
            Uint8 red,
            Uint8 green,
            Uint8 blue,
            Uint8 alpha) {
    if (surface == NULL) {
        return -1;
    }

    if (x1 > x2) {
        Sint16 tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    if (y1 > y2) {
        Sint16 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }

    SDL_Rect destination = {
        .x = x1,
        .y = y1,
        .w = x2 - x1 + 1,
        .h = y2 - y1 + 1,
    };
    if (alpha == SDL_ALPHA_TRANSPARENT) {
        return 0;
    }
    if (alpha == SDL_ALPHA_OPAQUE) {
        return SDL_FillSurfaceRect(surface,
                                   &destination,
                                   surface_map_rgba(surface, red, green, blue, alpha))
                   ? 0
                   : -1;
    }

    SDL_Surface *source = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
    if (source == NULL || !SDL_WriteSurfacePixel(source, 0, 0, red, green, blue, alpha) ||
        !SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_BLEND)) {
        SDL_DestroySurface(source);
        return -1;
    }

    bool success =
        SDL_BlitSurfaceScaled(source, NULL, surface, &destination, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(source);
    return success ? 0 : -1;
}

int filledRectAlpha(SDL_Surface *surface,
                    Sint16 x1,
                    Sint16 y1,
                    Sint16 x2,
                    Sint16 y2,
                    Uint32 color) {
    return boxRGBA(surface,
                   x1,
                   y1,
                   x2,
                   y2,
                   (Uint8)(color >> 24),
                   (Uint8)(color >> 16),
                   (Uint8)(color >> 8),
                   (Uint8)color);
}

void zoomSurfaceSize(int width,
                     int height,
                     double zoom_x,
                     double zoom_y,
                     int *destination_width,
                     int *destination_height) {
    *destination_width = MAX(1, (int)lround(width * fabs(zoom_x)));
    *destination_height = MAX(1, (int)lround(height * fabs(zoom_y)));
}

void rotozoomSurfaceSizeXY(int width,
                           int height,
                           double angle,
                           double zoom_x,
                           double zoom_y,
                           int *destination_width,
                           int *destination_height) {
    double radians = angle * M_PI / 180.0;
    double scaled_width = width * fabs(zoom_x);
    double scaled_height = height * fabs(zoom_y);

    *destination_width =
        MAX(1, (int)ceil(fabs(scaled_width * cos(radians)) + fabs(scaled_height * sin(radians))));
    *destination_height =
        MAX(1, (int)ceil(fabs(scaled_width * sin(radians)) + fabs(scaled_height * cos(radians))));
}

SDL_Surface *zoomSurface(SDL_Surface *surface, double zoom_x, double zoom_y, int smooth) {
    if (surface == NULL || zoom_x == 0.0 || zoom_y == 0.0) {
        SDL_SetError("Invalid surface or zoom factor");
        return NULL;
    }

    int width, height;
    zoomSurfaceSize(surface->w, surface->h, zoom_x, zoom_y, &width, &height);
    SDL_Surface *scaled = SDL_ScaleSurface(surface,
                                           width,
                                           height,
                                           smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    if (scaled == NULL || (zoom_x > 0.0 && zoom_y > 0.0)) {
        return scaled;
    }

    SDL_FlipMode flip = SDL_FLIP_NONE;
    if (zoom_x < 0.0) {
        flip |= SDL_FLIP_HORIZONTAL;
    }
    if (zoom_y < 0.0) {
        flip |= SDL_FLIP_VERTICAL;
    }

    if (!SDL_FlipSurface(scaled, flip)) {
        SDL_DestroySurface(scaled);
        return NULL;
    }
    return scaled;
}

SDL_Surface *rotozoomSurface(SDL_Surface *surface, double angle, double zoom, int smooth) {
    return rotozoomSurfaceXY(surface, angle, zoom, zoom, smooth);
}

SDL_Surface *
rotozoomSurfaceXY(SDL_Surface *surface, double angle, double zoom_x, double zoom_y, int smooth) {
    SDL_Surface *scaled = zoomSurface(surface, zoom_x, zoom_y, smooth);
    if (scaled == NULL) {
        return NULL;
    }

    if (angle == 0.0) {
        return scaled;
    }

    SDL_Surface *rotated = SDL_RotateSurface(scaled, (float)angle);
    SDL_DestroySurface(scaled);
    return rotated;
}
