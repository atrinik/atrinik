/*

SDL_gfxPrimitives - Graphics primitives for SDL surfaces

LGPL (c) A. Schiffler

 */

#include <global.h>

/* -===================- */

#define DEFAULT_ALPHA_PIXEL_ROUTINE
#undef EXPERIMENTAL_ALPHA_PIXEL_ROUTINE

/* ---- Structures */

/*!
\brief The structure passed to the internal Bresenham iterator.
 */
typedef struct {
    Sint16 x, y;
    int dx, dy, s1, s2, swapdir, error;
    Uint32 count;
} SDL_gfxBresenhamIterator;

/*!
\brief The structure passed to the internal Murphy iterator.
 */
typedef struct {
    Uint32 color;
    SDL_Surface *dst;
    int u, v;        /* delta x , delta y */
    int ku, kt, kv, kd;    /* loop constants */
    int oct2;
    int quad4;
    Sint16 last1x, last1y, last2x, last2y, first1x, first1y, first2x, first2y, tempx, tempy;
} SDL_gfxMurphyIterator;

/* ----- Defines for pixel clipping tests */

/*!
\brief Internal function to initialize the Bresenham line iterator.

Example of use:
SDL_gfxBresenhamIterator b;
_bresenhamInitialize (&b, x, y, x2, y2);
do {
plot(b.x, b.y);
} while (_bresenhamIterate(&b)==0);

\param b Pointer to struct for bresenham line drawing state.
\param x X coordinate of the first point of the line.
\param y Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.

\returns Returns 0 on success, -1 on failure.
 */
static int _bresenhamInitialize(SDL_gfxBresenhamIterator *b, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    int temp;

    if (b == NULL) {
        return (-1);
    }

    b->x = x;
    b->y = y;

    /* dx = abs(x2-x1), s1 = sign(x2-x1) */
    if ((b->dx = x2 - x) != 0) {
        if (b->dx < 0) {
            b->dx = -b->dx;
            b->s1 = -1;
        } else {
            b->s1 = 1;
        }
    } else {
        b->s1 = 0;
    }

    /* dy = abs(y2-y1), s2 = sign(y2-y1)    */
    if ((b->dy = y2 - y) != 0) {
        if (b->dy < 0) {
            b->dy = -b->dy;
            b->s2 = -1;
        } else {
            b->s2 = 1;
        }
    } else {
        b->s2 = 0;
    }

    if (b->dy > b->dx) {
        temp = b->dx;
        b->dx = b->dy;
        b->dy = temp;
        b->swapdir = 1;
    } else {
        b->swapdir = 0;
    }

    b->count = b->dx;
    b->dy <<= 1;
    b->error = b->dy - b->dx;
    b->dx <<= 1;

    return (0);
}

/*!
\brief Internal function to move Bresenham line iterator to the next position.

Maybe updates the x and y coordinates of the iterator struct.

\param b Pointer to struct for bresenham line drawing state.

\returns Returns 0 on success, 1 if last point was reached, 2 if moving past end-of-line, -1 on failure.
 */
static int _bresenhamIterate(SDL_gfxBresenhamIterator *b)
{
    if (b == NULL) {
        return (-1);
    }

    /* last point check */
    if (b->count <= 0) {
        return (2);
    }

    while (b->error >= 0) {
        if (b->swapdir) {
            b->x += b->s1;
        } else  {
            b->y += b->s2;
        }

        b->error -= b->dx;
    }

    if (b->swapdir) {
        b->y += b->s2;
    } else {
        b->x += b->s1;
    }

    b->error += b->dy;
    b->count--;

    /* count==0 indicates "end-of-line" */
    return ((b->count) ? 0 : 1);
}


/*!
\brief Internal function to to draw parallel lines with Murphy algorithm.

\param m Pointer to struct for murphy iterator.
\param x X coordinate of point.
\param y Y coordinate of point.
\param d1 Direction square/diagonal.
 */

#define clip_xmin(surface) surface->clip_rect.x
#define clip_xmax(surface) surface->clip_rect.x+surface->clip_rect.w-1
#define clip_ymin(surface) surface->clip_rect.y
#define clip_ymax(surface) surface->clip_rect.y+surface->clip_rect.h-1

/*!
\brief Internal pixel drawing - fast, no blending, no locking, clipping.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
 */
int fastPixelColorNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    int bpp;
    Uint8 *p;

    /*
     * Honor clipping setup at pixel level
     */
    if ((x >= clip_xmin(dst)) && (x <= clip_xmax(dst)) && (y >= clip_ymin(dst)) && (y <= clip_ymax(dst))) {

        /*
         * Get destination format
         */
        bpp = dst->format->BytesPerPixel;
        p = (Uint8 *) dst->pixels + y * dst->pitch + x * bpp;
        switch (bpp) {
        case 1:
            *p = color;
            break;
        case 2:
            *(Uint16 *) p = color;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = (color >> 16) & 0xff;
                p[1] = (color >> 8) & 0xff;
                p[2] = color & 0xff;
            } else {
                p[0] = color & 0xff;
                p[1] = (color >> 8) & 0xff;
                p[2] = (color >> 16) & 0xff;
            }
            break;
        case 4:
            *(Uint32 *) p = color;
            break;
        }            /* switch */


    }

    return (0);
}

/*!
\brief Internal pixel drawing - fast, no blending, no locking, no clipping.

Function is faster but dangerous since no clipping check is done.
Code needs to make sure we stay in surface bounds before calling.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
 */
int fastPixelColorNolockNoclip(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    int bpp;
    Uint8 *p;

    /*
     * Get destination format
     */
    bpp = dst->format->BytesPerPixel;
    p = (Uint8 *) dst->pixels + y * dst->pitch + x * bpp;
    switch (bpp) {
    case 1:
        *p = color;
        break;
    case 2:
        *(Uint16 *) p = color;
        break;
    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (color >> 16) & 0xff;
            p[1] = (color >> 8) & 0xff;
            p[2] = color & 0xff;
        } else {
            p[0] = color & 0xff;
            p[1] = (color >> 8) & 0xff;
            p[2] = (color >> 16) & 0xff;
        }
        break;
    case 4:
        *(Uint32 *) p = color;
        break;
    }                /* switch */

    return (0);
}

/*!
\brief Internal pixel drawing - fast, no blending, locking, clipping.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
 */
int fastPixelColor(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    int result;

    /*
     * Lock the surface
     */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    result = fastPixelColorNolock(dst, x, y, color);

    /*
     * Unlock surface
     */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Internal pixel drawing - fast, no blending, locking, RGB input.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param r The red value of the pixel to draw.
\param g The green value of the pixel to draw.
\param b The blue value of the pixel to draw.
\param a The alpha value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
 */
int fastPixelRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Uint32 color;

    /*
     * Setup color
     */
    color = SDL_MapRGBA(dst->format, r, g, b, a);

    /*
     * Draw
     */
    return (fastPixelColor(dst, x, y, color));
}

/*!
\brief Internal pixel drawing - fast, no blending, no locking RGB input.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param r The red value of the pixel to draw.
\param g The green value of the pixel to draw.
\param b The blue value of the pixel to draw.
\param a The alpha value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
 */
int fastPixelRGBANolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Uint32 color;

    /*
     * Setup color
     */
    color = SDL_MapRGBA(dst->format, r, g, b, a);

    /*
     * Draw
     */
    return (fastPixelColorNolock(dst, x, y, color));
}

/*!
\brief Internal pixel drawing function with alpha blending where input color in in destination format.

Contains two alternative 32 bit alpha blending routines which can be enabled at the source
level with the defines DEFAULT_ALPHA_PIXEL_ROUTINE or EXPERIMENTAL_ALPHA_PIXEL_ROUTINE.
Only the bits up to the surface depth are significant in the color value.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw.
\param alpha The blend factor to apply while drawing.

\returns Returns 0 on success, -1 on failure.
 */
int _putPixelAlpha(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color, Uint8 alpha)
{
    SDL_PixelFormat *format;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 Rshift, Gshift, Bshift, Ashift;
    Uint32 R, G, B, A;

    if (dst == NULL) {
        return (-1);
    }

    if (x >= clip_xmin(dst) && x <= clip_xmax(dst) &&
            y >= clip_ymin(dst) && y <= clip_ymax(dst)) {

        format = dst->format;

        switch (format->BytesPerPixel) {
        case 1:
        {        /* Assuming 8-bpp */
            if (alpha == 255) {
                *((Uint8 *) dst->pixels + y * dst->pitch + x) = color;
            } else {
                Uint8 *pixel = (Uint8 *) dst->pixels + y * dst->pitch + x;
                SDL_Palette *palette = format->palette;
                SDL_Color *colors = palette->colors;
                SDL_Color dColor = colors[*pixel];
                SDL_Color sColor = colors[color];
                Uint8 dR = dColor.r;
                Uint8 dG = dColor.g;
                Uint8 dB = dColor.b;
                Uint8 sR = sColor.r;
                Uint8 sG = sColor.g;
                Uint8 sB = sColor.b;

                dR = dR + ((sR - dR) * alpha >> 8);
                dG = dG + ((sG - dG) * alpha >> 8);
                dB = dB + ((sB - dB) * alpha >> 8);

                *pixel = SDL_MapRGB(format, dR, dG, dB);
            }
        }
            break;

        case 2:
        {        /* Probably 15-bpp or 16-bpp */
            if (alpha == 255) {
                *((Uint16 *) dst->pixels + y * dst->pitch / 2 + x) = color;
            } else {
                Uint16 *pixel = (Uint16 *) dst->pixels + y * dst->pitch / 2 + x;
                Uint32 dc = *pixel;

                Rmask = format->Rmask;
                Gmask = format->Gmask;
                Bmask = format->Bmask;
                Amask = format->Amask;
                R = ((dc & Rmask) + (((color & Rmask) - (dc & Rmask)) * alpha >> 8)) & Rmask;
                G = ((dc & Gmask) + (((color & Gmask) - (dc & Gmask)) * alpha >> 8)) & Gmask;
                B = ((dc & Bmask) + (((color & Bmask) - (dc & Bmask)) * alpha >> 8)) & Bmask;
                A = 255;
                if (Amask) {
                    A = ((dc & Amask) + (((color & Amask) - (dc & Amask)) * alpha >> 8)) & Amask;
                }
                *pixel = R | G | B | A;
            }
        }
            break;

        case 3:
        {        /* Slow 24-bpp mode, usually not used */
            Uint8 Rshift8, Gshift8, Bshift8, Ashift8;
            Uint8 *pixel = (Uint8 *) dst->pixels + y * dst->pitch + x * 3;

            Rshift = format->Rshift;
            Gshift = format->Gshift;
            Bshift = format->Bshift;
            Ashift = format->Ashift;

            Rshift8 = Rshift / 8;
            Gshift8 = Gshift / 8;
            Bshift8 = Bshift / 8;
            Ashift8 = Ashift / 8;

            if (alpha == 255) {
                *(pixel + Rshift8) = color >> Rshift;
                *(pixel + Gshift8) = color >> Gshift;
                *(pixel + Bshift8) = color >> Bshift;
                *(pixel + Ashift8) = color >> Ashift;
            } else {
                Uint8 dR, dG, dB, dA = 0;
                Uint8 sR, sG, sB, sA = 0;

                dR = *((pixel) + Rshift8);
                dG = *((pixel) + Gshift8);
                dB = *((pixel) + Bshift8);
                dA = *((pixel) + Ashift8);

                sR = (color >> Rshift) & 0xff;
                sG = (color >> Gshift) & 0xff;
                sB = (color >> Bshift) & 0xff;
                sA = (color >> Ashift) & 0xff;

                dR = dR + ((sR - dR) * alpha >> 8);
                dG = dG + ((sG - dG) * alpha >> 8);
                dB = dB + ((sB - dB) * alpha >> 8);
                dA = dA + ((sA - dA) * alpha >> 8);

                *((pixel) + Rshift8) = dR;
                *((pixel) + Gshift8) = dG;
                *((pixel) + Bshift8) = dB;
                *((pixel) + Ashift8) = dA;
            }
        }
            break;

#ifdef DEFAULT_ALPHA_PIXEL_ROUTINE

        case 4:
        {        /* Probably :-) 32-bpp */
            if (alpha == 255) {
                *((Uint32 *) dst->pixels + y * dst->pitch / 4 + x) = color;
            } else {
                Uint32 *pixel = (Uint32 *) dst->pixels + y * dst->pitch / 4 + x;
                Uint32 dc = *pixel;

                Rmask = format->Rmask;
                Gmask = format->Gmask;
                Bmask = format->Bmask;
                Amask = format->Amask;

                Rshift = format->Rshift;
                Gshift = format->Gshift;
                Bshift = format->Bshift;
                Ashift = format->Ashift;

                A = 0;
                R = ((dc & Rmask) + (((((color & Rmask) - (dc & Rmask)) >> Rshift) * alpha >> 8) << Rshift)) & Rmask;
                G = ((dc & Gmask) + (((((color & Gmask) - (dc & Gmask)) >> Gshift) * alpha >> 8) << Gshift)) & Gmask;
                B = ((dc & Bmask) + (((((color & Bmask) - (dc & Bmask)) >> Bshift) * alpha >> 8) << Bshift)) & Bmask;
                if (Amask) {
                    A = ((dc & Amask) + (((((color & Amask) - (dc & Amask)) >> Ashift) * alpha >> 8) << Ashift)) & Amask;
                }
                *pixel = R | G | B | A;
            }
        }
            break;
#endif

#ifdef EXPERIMENTAL_ALPHA_PIXEL_ROUTINE

        case 4:
        {        /* Probably :-) 32-bpp */
            if (alpha == 255) {
                *((Uint32 *) dst->pixels + y * dst->pitch / 4 + x) = color;
            } else {
                Uint32 *pixel = (Uint32 *) dst->pixels + y * dst->pitch / 4 + x;
                Uint32 dR, dG, dB, dA;
                Uint32 dc = *pixel;

                Uint32 surfaceAlpha, preMultR, preMultG, preMultB;
                Uint32 aTmp;

                Rmask = format->Rmask;
                Gmask = format->Gmask;
                Bmask = format->Bmask;
                Amask = format->Amask;

                dR = (color & Rmask);
                dG = (color & Gmask);
                dB = (color & Bmask);
                dA = (color & Amask);

                Rshift = format->Rshift;
                Gshift = format->Gshift;
                Bshift = format->Bshift;
                Ashift = format->Ashift;

                preMultR = (alpha * (dR >> Rshift));
                preMultG = (alpha * (dG >> Gshift));
                preMultB = (alpha * (dB >> Bshift));

                surfaceAlpha = ((dc & Amask) >> Ashift);
                aTmp = (255 - alpha);
                if (A = 255 - ((aTmp * (255 - surfaceAlpha)) >> 8 )) {
                    aTmp *= surfaceAlpha;
                    R = (preMultR + ((aTmp * ((dc & Rmask) >> Rshift)) >> 8)) / A << Rshift & Rmask;
                    G = (preMultG + ((aTmp * ((dc & Gmask) >> Gshift)) >> 8)) / A << Gshift & Gmask;
                    B = (preMultB + ((aTmp * ((dc & Bmask) >> Bshift)) >> 8)) / A << Bshift & Bmask;
                }
                *pixel = R | G | B | (A << Ashift & Amask);

            }
        }
            break;
#endif
        }
    }

    return (0);
}

/*!
\brief Pixel draw with blending enabled if a<255.

\param dst The surface to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int pixelColor(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    Uint8 alpha;
    Uint32 mcolor;
    int result = 0;

    /*
     * Lock the surface
     */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Setup color
     */
    alpha = color & 0x000000ff;
    mcolor =
            SDL_MapRGBA(dst->format, (color & 0xff000000) >> 24,
            (color & 0x00ff0000) >> 16, (color & 0x0000ff00) >> 8, alpha);

    /*
     * Draw
     */
    result = _putPixelAlpha(dst, x, y, mcolor, alpha);

    /*
     * Unlock the surface
     */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Pixel draw with blending enabled if a<255 - no surface locking.

\param dst The surface to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int pixelColorNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    Uint8 alpha;
    Uint32 mcolor;
    int result = 0;

    /*
     * Setup color
     */
    alpha = color & 0x000000ff;
    mcolor =
            SDL_MapRGBA(dst->format, (color & 0xff000000) >> 24,
            (color & 0x00ff0000) >> 16, (color & 0x0000ff00) >> 8, alpha);

    /*
     * Draw
     */
    result = _putPixelAlpha(dst, x, y, mcolor, alpha);

    return (result);
}

/*!
\brief Internal function to draw filled rectangle with alpha blending.

Assumes color is in destination format.

\param dst The surface to draw on.
\param xx X coordinate of the first corner (upper left) of the rectangle.
\param yy Y coordinate of the first corner (upper left) of the rectangle.
\param x2 X coordinate of the second corner (lower right) of the rectangle.
\param y2 Y coordinate of the second corner (lower right) of the rectangle.
\param color The color value of the rectangle to draw (0xRRGGBBAA).
\param alpha Alpha blending amount for pixels.

\returns Returns 0 on success, -1 on failure.
 */
int _filledRectAlpha(SDL_Surface * dst, Sint16 xx, Sint16 yy, Sint16 x2, Sint16 y2, Uint32 color, Uint8 alpha)
{
    SDL_PixelFormat *format;
    Uint32 Rmask, Bmask, Gmask, Amask;
    Uint32 Rshift, Bshift, Gshift, Ashift;
    Uint8 sR, sG, sB, sA;
    Uint32 R, G, B, A;
    Sint16 x, y;

    format = dst->format;
    switch (format->BytesPerPixel) {
    case 1:
    {            /* Assuming 8-bpp */
        Uint8 *row, *pixel;
        Uint8 dR, dG, dB;
        SDL_Palette *palette = format->palette;
        SDL_Color *colors = palette->colors;
        sR = colors[color].r;
        sG = colors[color].g;
        sB = colors[color].b;

        for (y = yy; y <= y2; y++) {
            row = (Uint8 *) dst->pixels + y * dst->pitch;
            for (x = xx; x <= x2; x++) {
                pixel = row + x;

                dR = colors[*pixel].r;
                dG = colors[*pixel].g;
                dB = colors[*pixel].b;

                dR = dR + ((sR - dR) * alpha >> 8);
                dG = dG + ((sG - dG) * alpha >> 8);
                dB = dB + ((sB - dB) * alpha >> 8);

                *pixel = SDL_MapRGB(format, dR, dG, dB);
            }
        }
    }
        break;

    case 2:
    {            /* Probably 15-bpp or 16-bpp */
        Uint16 *row, *pixel;
        Uint32 dR, dG, dB, dA;
        Rmask = format->Rmask;
        Gmask = format->Gmask;
        Bmask = format->Bmask;
        Amask = format->Amask;

        dR = (color & Rmask);
        dG = (color & Gmask);
        dB = (color & Bmask);
        dA = (color & Amask);

        A = 0;

        for (y = yy; y <= y2; y++) {
            row = (Uint16 *) dst->pixels + y * dst->pitch / 2;
            for (x = xx; x <= x2; x++) {
                pixel = row + x;

                R = ((*pixel & Rmask) + ((dR - (*pixel & Rmask)) * alpha >> 8)) & Rmask;
                G = ((*pixel & Gmask) + ((dG - (*pixel & Gmask)) * alpha >> 8)) & Gmask;
                B = ((*pixel & Bmask) + ((dB - (*pixel & Bmask)) * alpha >> 8)) & Bmask;
                if (Amask) {
                    A = ((*pixel & Amask) + ((dA - (*pixel & Amask)) * alpha >> 8)) & Amask;
                    *pixel = R | G | B | A;
                } else {
                    *pixel = R | G | B;
                }
            }
        }
    }
        break;

    case 3:
    {            /* Slow 24-bpp mode, usually not used */
        Uint8 *row, *pix;
        Uint8 dR, dG, dB, dA;
        Uint8 Rshift8, Gshift8, Bshift8, Ashift8;

        Rshift = format->Rshift;
        Gshift = format->Gshift;
        Bshift = format->Bshift;
        Ashift = format->Ashift;

        Rshift8 = Rshift / 8;
        Gshift8 = Gshift / 8;
        Bshift8 = Bshift / 8;
        Ashift8 = Ashift / 8;

        sR = (color >> Rshift) & 0xff;
        sG = (color >> Gshift) & 0xff;
        sB = (color >> Bshift) & 0xff;
        sA = (color >> Ashift) & 0xff;

        for (y = yy; y <= y2; y++) {
            row = (Uint8 *) dst->pixels + y * dst->pitch;
            for (x = xx; x <= x2; x++) {
                pix = row + x * 3;

                dR = *((pix) + Rshift8);
                dG = *((pix) + Gshift8);
                dB = *((pix) + Bshift8);
                dA = *((pix) + Ashift8);

                dR = dR + ((sR - dR) * alpha >> 8);
                dG = dG + ((sG - dG) * alpha >> 8);
                dB = dB + ((sB - dB) * alpha >> 8);
                dA = dA + ((sA - dA) * alpha >> 8);

                *((pix) + Rshift8) = dR;
                *((pix) + Gshift8) = dG;
                *((pix) + Bshift8) = dB;
                *((pix) + Ashift8) = dA;
            }
        }
    }
        break;

#ifdef DEFAULT_ALPHA_PIXEL_ROUTINE
    case 4:
    {            /* Probably :-) 32-bpp */
        Uint32 *row, *pixel;
        Uint32 dR, dG, dB, dA;

        Rmask = format->Rmask;
        Gmask = format->Gmask;
        Bmask = format->Bmask;
        Amask = format->Amask;

        Rshift = format->Rshift;
        Gshift = format->Gshift;
        Bshift = format->Bshift;
        Ashift = format->Ashift;

        dR = (color & Rmask);
        dG = (color & Gmask);
        dB = (color & Bmask);
        dA = (color & Amask);

        for (y = yy; y <= y2; y++) {
            row = (Uint32 *) dst->pixels + y * dst->pitch / 4;
            for (x = xx; x <= x2; x++) {
                pixel = row + x;

                R = ((*pixel & Rmask) + ((((dR - (*pixel & Rmask)) >> Rshift) * alpha >> 8) << Rshift)) & Rmask;
                G = ((*pixel & Gmask) + ((((dG - (*pixel & Gmask)) >> Gshift) * alpha >> 8) << Gshift)) & Gmask;
                B = ((*pixel & Bmask) + ((((dB - (*pixel & Bmask)) >> Bshift) * alpha >> 8) << Bshift)) & Bmask;
                if (Amask) {
                    A = ((*pixel & Amask) + ((((dA - (*pixel & Amask)) >> Ashift) * alpha >> 8) << Ashift)) & Amask;
                    *pixel = R | G | B | A;
                } else {
                    *pixel = R | G | B;
                }
            }
        }
    }
        break;
#endif

#ifdef EXPERIMENTAL_ALPHA_PIXEL_ROUTINE
    case 4:
    {            /* Probably :-) 32-bpp */
        Uint32 *row, *pixel;
        Uint32 dR, dG, dB, dA;
        Uint32 dc;
        Uint32 surfaceAlpha, preMultR, preMultG, preMultB;
        Uint32 aTmp;

        Rmask = format->Rmask;
        Gmask = format->Gmask;
        Bmask = format->Bmask;
        Amask = format->Amask;

        dR = (color & Rmask);
        dG = (color & Gmask);
        dB = (color & Bmask);
        dA = (color & Amask);

        Rshift = format->Rshift;
        Gshift = format->Gshift;
        Bshift = format->Bshift;
        Ashift = format->Ashift;

        preMultR = (alpha * (dR >> Rshift));
        preMultG = (alpha * (dG >> Gshift));
        preMultB = (alpha * (dB >> Bshift));

        for (y = y1; y <= y2; y++) {
            row = (Uint32 *) dst->pixels + y * dst->pitch / 4;
            for (x = x1; x <= x2; x++) {
                pixel = row + x;
                dc = *pixel;

                surfaceAlpha = ((dc & Amask) >> Ashift);
                aTmp = (255 - alpha);
                if (A = 255 - ((aTmp * (255 - surfaceAlpha)) >> 8 )) {
                    aTmp *= surfaceAlpha;
                    R = (preMultR + ((aTmp * ((dc & Rmask) >> Rshift)) >> 8)) / A << Rshift & Rmask;
                    G = (preMultG + ((aTmp * ((dc & Gmask) >> Gshift)) >> 8)) / A << Gshift & Gmask;
                    B = (preMultB + ((aTmp * ((dc & Bmask) >> Bshift)) >> 8)) / A << Bshift & Bmask;
                }
                *pixel = R | G | B | (A << Ashift & Amask);

            }
        }
    }
        break;
#endif

    }

    return (0);
}

/*!
\brief Draw filled rectangle of RGBA color with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first corner (upper left) of the rectangle.
\param y Y coordinate of the first corner (upper left) of the rectangle.
\param x2 X coordinate of the second corner (lower right) of the rectangle.
\param y2 Y coordinate of the second corner (lower right) of the rectangle.
\param color The color value of the rectangle to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int filledRectAlpha(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color)
{
    Uint8 alpha;
    Uint32 mcolor;
    int result = 0;

    /*
     * Lock the surface
     */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Setup color
     */
    alpha = color & 0x000000ff;
    mcolor =
            SDL_MapRGBA(dst->format, (color & 0xff000000) >> 24,
            (color & 0x00ff0000) >> 16, (color & 0x0000ff00) >> 8, alpha);

    /*
     * Draw
     */
    result = _filledRectAlpha(dst, x, y, x2, y2, mcolor, alpha);

    /*
     * Unlock the surface
     */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Internal function to draw horizontal line of RGBA color with alpha blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param color The color value of the line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int _HLineAlpha(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
    return (filledRectAlpha(dst, x1, y, x2, y, color));
}

/*!
\brief Internal function to draw vertical line of RGBA color with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the points of the line.
\param y Y coordinate of the first point (top) of the line.
\param y2 Y coordinate of the second point (bottom) of the line.
\param color The color value of the line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int _VLineAlpha(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 y2, Uint32 color)
{
    return (filledRectAlpha(dst, x, y, x, y2, color));
}

/*!
\brief Pixel draw with blending enabled and using alpha weight on color.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA).
\param weight The weight multiplied into the alpha value of the pixel.

\returns Returns 0 on success, -1 on failure.
 */
int pixelColorWeight(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color, Uint32 weight)
{
    Uint32 a;

    /*
     * Get alpha
     */
    a = (color & (Uint32) 0x000000ff);

    /*
     * Modify Alpha by weight
     */
    a = ((a * weight) >> 8);

    return (pixelColor(dst, x, y, (color & (Uint32) 0xffffff00) | (Uint32) a));
}

/*!
\brief Pixel draw with blending enabled and using alpha weight on color - no locking.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA).
\param weight The weight multiplied into the alpha value of the pixel.

\returns Returns 0 on success, -1 on failure.
 */
int pixelColorWeightNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color, Uint32 weight)
{
    Uint32 a;

    /*
     * Get alpha
     */
    a = (color & (Uint32) 0x000000ff);

    /*
     * Modify Alpha by weight
     */
    a = ((a * weight) >> 8);

    return (pixelColorNolock(dst, x, y, (color & (Uint32) 0xffffff00) | (Uint32) a));
}

/*!
\brief Pixel draw with blending enabled if a<255.

\param dst The surface to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param r The red color value of the pixel to draw.
\param g The green color value of the pixel to draw.
\param b The blue color value of the pixel to draw.
\param a The alpha value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
 */
int pixelRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Uint32 color;

    /*
     * Check Alpha
     */
    if (a == 255) {
        /*
         * No alpha blending required
         */
        /*
         * Setup color
         */
        color = SDL_MapRGBA(dst->format, r, g, b, a);
        /*
         * Draw
         */
        return (fastPixelColor(dst, x, y, color));
    } else {
        /*
         * Alpha blending required
         */
        /*
         * Draw
         */
        return (pixelColor(dst, x, y, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
    }
}

/*!
\brief Draw horizontal line without blending;

Just stores the color value (including the alpha component) without blending.
Only the same number of bits of the destination surface are transfered
from the input color value.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param color The color value of the line to draw.

\returns Returns 0 on success, -1 on failure.
 */
int hlineColorStore(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int dx;
    int pixx, pixy;
    Sint16 w;
    Sint16 xtmp;
    int result = -1;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Swap x1, x2 if required to ensure x1<=x2
     */
    if (x1 > x2) {
        xtmp = x1;
        x1 = x2;
        x2 = xtmp;
    }

    /*
     * Get clipping boundary and
     * check visibility of hline
     */
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (x1 > right) {
        return (0);
    }
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if ((y < top) || (y > bottom)) {
        return (0);
    }

    /*
     * Clip x
     */
    if (x1 < left) {
        x1 = left;
    }
    if (x2 > right) {
        x2 = right;
    }

    /*
     * Calculate width
     */
    w = x2 - x1;

    /*
     * Lock the surface
     */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * More variable setup
     */
    dx = w;
    pixx = dst->format->BytesPerPixel;
    pixy = dst->pitch;
    pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y;

    /*
     * Draw
     */
    switch (dst->format->BytesPerPixel) {
    case 1:
        memset(pixel, color, dx + 1);
        break;
    case 2:
        pixellast = pixel + dx + dx;
        for (; pixel <= pixellast; pixel += pixx) {
            *(Uint16 *) pixel = color;
        }
        break;
    case 3:
        pixellast = pixel + dx + dx + dx;
        for (; pixel <= pixellast; pixel += pixx) {
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                pixel[0] = (color >> 16) & 0xff;
                pixel[1] = (color >> 8) & 0xff;
                pixel[2] = color & 0xff;
            } else {
                pixel[0] = color & 0xff;
                pixel[1] = (color >> 8) & 0xff;
                pixel[2] = (color >> 16) & 0xff;
            }
        }
        break;
    default:        /* case 4 */
        dx = dx + dx;
        pixellast = pixel + dx + dx;
        for (; pixel <= pixellast; pixel += pixx) {
            *(Uint32 *) pixel = color;
        }
        break;
    }

    /*
     * Unlock surface
     */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    /*
     * Set result code
     */
    result = 0;

    return (result);
}

/*!
\brief Draw horizontal line without blending

Just stores the color value (including the alpha component) without blending.
Function should only be used for 32 bit target surfaces.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param r The red value of the line to draw.
\param g The green value of the line to draw.
\param b The blue value of the line to draw.
\param a The alpha value of the line to draw.

\returns Returns 0 on success, -1 on failure.
 */
int hlineRGBAStore(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (hlineColorStore(dst, x1, x2, y, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/*!
\brief Draw horizontal line with blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param color The color value of the line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int hlineColor(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int dx;
    int pixx, pixy;
    Sint16 xtmp;
    int result = -1;
    Uint8 *colorptr;
    Uint8 color3[3];

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Swap x1, x2 if required to ensure x1<=x2
     */
    if (x1 > x2) {
        xtmp = x1;
        x1 = x2;
        x2 = xtmp;
    }

    /*
     * Get clipping boundary and
     * check visibility of hline
     */
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (x1 > right) {
        return (0);
    }
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if ((y < top) || (y > bottom)) {
        return (0);
    }

    /*
     * Clip x
     */
    if (x1 < left) {
        x1 = left;
    }
    if (x2 > right) {
        x2 = right;
    }

    /*
     * Calculate width difference
     */
    dx = x2 - x1;

    /*
     * Alpha check
     */
    if ((color & 255) == 255) {

        /*
         * No alpha-blending required
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }

        /*
         * Lock the surface
         */
        if (SDL_MUSTLOCK(dst)) {
            if (SDL_LockSurface(dst) < 0) {
                return (-1);
            }
        }

        /*
         * More variable setup
         */
        pixx = dst->format->BytesPerPixel;
        pixy = dst->pitch;
        pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y;

        /*
         * Draw
         */
        switch (dst->format->BytesPerPixel) {
        case 1:
            memset(pixel, color, dx + 1);
            break;
        case 2:
            pixellast = pixel + dx + dx;
            for (; pixel <= pixellast; pixel += pixx) {
                *(Uint16 *) pixel = color;
            }
            break;
        case 3:
            pixellast = pixel + dx + dx + dx;
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                color3[0] = (color >> 16) & 0xff;
                color3[1] = (color >> 8) & 0xff;
                color3[2] = color & 0xff;
            } else {
                color3[0] = color & 0xff;
                color3[1] = (color >> 8) & 0xff;
                color3[2] = (color >> 16) & 0xff;
            }
            for (; pixel <= pixellast; pixel += pixx) {
                memcpy(pixel, color3, 3);
            }
            break;
        default:        /* case 4 */
            dx = dx + dx;
            pixellast = pixel + dx + dx;
            for (; pixel <= pixellast; pixel += pixx) {
                *(Uint32 *) pixel = color;
            }
            break;
        }

        /*
         * Unlock surface
         */
        if (SDL_MUSTLOCK(dst)) {
            SDL_UnlockSurface(dst);
        }

        /*
         * Set result code
         */
        result = 0;

    } else {

        /*
         * Alpha blending blit
         */
        result = _HLineAlpha(dst, x1, x1 + dx, y, color);
    }

    return (result);
}

/*!
\brief Draw horizontal line with blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param r The red value of the line to draw.
\param g The green value of the line to draw.
\param b The blue value of the line to draw.
\param a The alpha value of the line to draw.

\returns Returns 0 on success, -1 on failure.
 */
int hlineRGBA(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (hlineColor(dst, x1, x2, y, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/*!
\brief Draw vertical line with blending.

\param dst The surface to draw on.
\param x X coordinate of the points of the line.
\param y Y coordinate of the first point (i.e. top) of the line.
\param y2 Y coordinate of the second point (i.e. bottom) of the line.
\param color The color value of the line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int vlineColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 y2, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int dy;
    int pixx, pixy;
    Sint16 h;
    Sint16 ytmp;
    int result = -1;
    Uint8 *colorptr;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Swap y1, y2 if required to ensure y1<=y2
     */
    if (y > y2) {
        ytmp = y;
        y = y2;
        y2 = ytmp;
    }

    /*
     * Get clipping boundary and
     * check visibility of vline
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if ((x < left) || (x > right)) {
        return (0);
    }
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (y > bottom) {
        return (0);
    }

    /*
     * Clip x
     */
    if (y < top) {
        y = top;
    }
    if (y2 > bottom) {
        y2 = bottom;
    }

    /*
     * Calculate height
     */
    h = y2 - y;

    /*
     * Alpha check
     */
    if ((color & 255) == 255) {

        /*
         * No alpha-blending required
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }

        /*
         * Lock the surface
         */
        if (SDL_MUSTLOCK(dst)) {
            if (SDL_LockSurface(dst) < 0) {
                return (-1);
            }
        }

        /*
         * More variable setup
         */
        dy = h;
        pixx = dst->format->BytesPerPixel;
        pixy = dst->pitch;
        pixel = ((Uint8 *) dst->pixels) + pixx * (int) x + pixy * (int) y;
        pixellast = pixel + pixy * dy;

        /*
         * Draw
         */
        switch (dst->format->BytesPerPixel) {
        case 1:
            for (; pixel <= pixellast; pixel += pixy) {
                *(Uint8 *) pixel = color;
            }
            break;
        case 2:
            for (; pixel <= pixellast; pixel += pixy) {
                *(Uint16 *) pixel = color;
            }
            break;
        case 3:
            for (; pixel <= pixellast; pixel += pixy) {
                if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                    pixel[0] = (color >> 16) & 0xff;
                    pixel[1] = (color >> 8) & 0xff;
                    pixel[2] = color & 0xff;
                } else {
                    pixel[0] = color & 0xff;
                    pixel[1] = (color >> 8) & 0xff;
                    pixel[2] = (color >> 16) & 0xff;
                }
            }
            break;
        default:        /* case 4 */
            for (; pixel <= pixellast; pixel += pixy) {
                *(Uint32 *) pixel = color;
            }
            break;
        }

        /* Unlock surface */
        if (SDL_MUSTLOCK(dst)) {
            SDL_UnlockSurface(dst);
        }

        /*
         * Set result code
         */
        result = 0;

    } else {

        /*
         * Alpha blending blit
         */

        result = _VLineAlpha(dst, x, y, y + h, color);

    }

    return (result);
}

/*!
\brief Draw vertical line with blending.

\param dst The surface to draw on.
\param x X coordinate of the points of the line.
\param y Y coordinate of the first point (i.e. top) of the line.
\param y2 Y coordinate of the second point (i.e. bottom) of the line.
\param r The red value of the line to draw.
\param g The green value of the line to draw.
\param b The blue value of the line to draw.
\param a The alpha value of the line to draw.

\returns Returns 0 on success, -1 on failure.
 */
int vlineRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (vlineColor(dst, x, y, y2, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/*!
\brief Draw rectangle with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the rectangle.
\param y Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param color The color value of the rectangle to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int rectangleColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color)
{
    int result;
    Sint16 tmp;

    /* Check destination surface */
    if (dst == NULL) {
        return -1;
    }

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return 0;
    }

    /*
     * Test for special cases of straight lines or single point
     */
    if (x == x2) {
        if (y == y2) {
            return (pixelColor(dst, x, y, color));
        } else {
            return (vlineColor(dst, x, y, y2, color));
        }
    } else {
        if (y == y2) {
            return (hlineColor(dst, x, x2, y, color));
        }
    }

    /*
     * Swap x1, x2 if required
     */
    if (x > x2) {
        tmp = x;
        x = x2;
        x2 = tmp;
    }

    /*
     * Swap y1, y2 if required
     */
    if (y > y2) {
        tmp = y;
        y = y2;
        y2 = tmp;
    }

    /*
     * Draw rectangle
     */
    result = 0;
    result |= hlineColor(dst, x, x2, y, color);
    result |= hlineColor(dst, x, x2, y2, color);
    y += 1;
    y2 -= 1;
    if (y <= y2) {
        result |= vlineColor(dst, x, y, y2, color);
        result |= vlineColor(dst, x2, y, y2, color);
    }

    return (result);

}

/*!
\brief Draw rectangle with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the rectangle.
\param y Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param r The red value of the rectangle to draw.
\param g The green value of the rectangle to draw.
\param b The blue value of the rectangle to draw.
\param a The alpha value of the rectangle to draw.

\returns Returns 0 on success, -1 on failure.
 */
int rectangleRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (rectangleColor
            (dst, x, y, x2, y2, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/*!
\brief Draw rounded-corner rectangle with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the rectangle.
\param y Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param rad The radius of the corner arc.
\param color The color value of the rectangle to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int roundedRectangleColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint32 color)
{
    int result;
    Sint16 w, h, tmp;
    Sint16 xx1, xx2, yy1, yy2;

    /*
     * Check destination surface
     */
    if (dst == NULL) {
        return -1;
    }

    /*
     * Check radius vor valid range
     */
    if (rad < 0) {
        return -1;
    }

    /*
     * Special case - no rounding
     */
    if (rad == 0) {
        return rectangleColor(dst, x, y, x2, y2, color);
    }

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return 0;
    }

    /*
     * Test for special cases of straight lines or single point
     */
    if (x == x2) {
        if (y == y2) {
            return (pixelColor(dst, x, y, color));
        } else {
            return (vlineColor(dst, x, y, y2, color));
        }
    } else {
        if (y == y2) {
            return (hlineColor(dst, x, x2, y, color));
        }
    }

    /*
     * Swap x1, x2 if required
     */
    if (x > x2) {
        tmp = x;
        x = x2;
        x2 = tmp;
    }

    /*
     * Swap y1, y2 if required
     */
    if (y > y2) {
        tmp = y;
        y = y2;
        y2 = tmp;
    }

    /*
     * Calculate width&height
     */
    w = x2 - x;
    h = y2 - y;

    /*
     * Maybe adjust radius
     */
    if ((rad * 2) > w) {
        rad = w / 2;
    }
    if ((rad * 2) > h) {
        rad = h / 2;
    }

    /*
     * Draw corners
     */
    result = 0;
    xx1 = x + rad;
    xx2 = x2 - rad;
    yy1 = y + rad;
    yy2 = y2 - rad;
    result |= arcColor(dst, xx1, yy1, rad, 180, 270, color);
    result |= arcColor(dst, xx2, yy1, rad, 270, 360, color);
    result |= arcColor(dst, xx1, yy2, rad,  90, 180, color);
    result |= arcColor(dst, xx2, yy2, rad,   0,  90, color);

    /*
     * Draw lines
     */
    if (xx1 <= xx2) {
        result |= hlineColor(dst, xx1, xx2, y, color);
        result |= hlineColor(dst, xx1, xx2, y2, color);
    }
    if (yy1 <= yy2) {
        result |= vlineColor(dst, x, yy1, yy2, color);
        result |= vlineColor(dst, x2, yy1, yy2, color);
    }

    return result;
}

/*!
\brief Draw rounded-corner rectangle with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the rectangle.
\param y Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param rad The radius of the corner arc.
\param r The red value of the rectangle to draw.
\param g The green value of the rectangle to draw.
\param b The blue value of the rectangle to draw.
\param a The alpha value of the rectangle to draw.

\returns Returns 0 on success, -1 on failure.
 */
int roundedRectangleRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (roundedRectangleColor
            (dst, x, y, x2, y2, rad, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/*!
\brief Draw rounded-corner box (filled rectangle) with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the box.
\param y Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param rad The radius of the corner arcs of the box.
\param color The color value of the box to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int roundedBoxColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint32 color)
{
    int result;
    Sint16 w, h, tmp;
    Sint16 xx1, xx2, yy1, yy2;

    /*
     * Check destination surface
     */
    if (dst == NULL) {
        return -1;
    }

    /*
     * Check radius vor valid range
     */
    if (rad < 0) {
        return -1;
    }

    /*
     * Special case - no rounding
     */
    if (rad == 0) {
        return rectangleColor(dst, x, y, x2, y2, color);
    }

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return 0;
    }

    /*
     * Test for special cases of straight lines or single point
     */
    if (x == x2) {
        if (y == y2) {
            return (pixelColor(dst, x, y, color));
        } else {
            return (vlineColor(dst, x, y, y2, color));
        }
    } else {
        if (y == y2) {
            return (hlineColor(dst, x, x2, y, color));
        }
    }

    /*
     * Swap x1, x2 if required
     */
    if (x > x2) {
        tmp = x;
        x = x2;
        x2 = tmp;
    }

    /*
     * Swap y1, y2 if required
     */
    if (y > y2) {
        tmp = y;
        y = y2;
        y2 = tmp;
    }

    /*
     * Calculate width&height
     */
    w = x2 - x;
    h = y2 - y;

    /*
     * Maybe adjust radius
     */
    if ((rad * 2) > w) {
        rad = w / 2;
    }
    if ((rad * 2) > h) {
        rad = h / 2;
    }

    /*
     * Draw corners
     */
    result = 0;
    xx1 = x + rad;
    xx2 = x2 - rad;
    yy1 = y + rad;
    yy2 = y2 - rad;
    result |= filledPieColor(dst, xx1, yy1, rad, 180, 270, color);
    result |= filledPieColor(dst, xx2, yy1, rad, 270, 360, color);
    result |= filledPieColor(dst, xx1, yy2, rad,  90, 180, color);
    result |= filledPieColor(dst, xx2, yy2, rad,   0,  90, color);

    /*
     * Draw body
     */
    xx1++;
    xx2--;
    yy1++;
    yy2--;
    if (xx1 <= xx2) {
        result |= boxColor(dst, xx1, y, xx2, y2, color);
    }
    if (yy1 <= yy2) {
        result |= boxColor(dst, x, yy1, xx1 - 1, yy2, color);
        result |= boxColor(dst, xx2 + 1, yy1, x2, yy2, color);
    }

    return result;
}

/*!
\brief Draw rounded-corner box (filled rectangle) with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the box.
\param y Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param rad The radius of the corner arcs of the box.
\param r The red value of the box to draw.
\param g The green value of the box to draw.
\param b The blue value of the box to draw.
\param a The alpha value of the box to draw.

\returns Returns 0 on success, -1 on failure.
 */
int roundedBoxRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2,
        Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (roundedBoxColor
            (dst, x, y, x2, y2, rad, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* --------- Clipping routines for line */

/* Clipping based heavily on code from                       */
/* http://www.ncsa.uiuc.edu/Vis/Graphics/src/clipCohSuth.c   */

#define CLIP_LEFT_EDGE   0x1
#define CLIP_RIGHT_EDGE  0x2
#define CLIP_BOTTOM_EDGE 0x4
#define CLIP_TOP_EDGE    0x8
#define CLIP_INSIDE(a)   (!a)
#define CLIP_REJECT(a,b) (a&b)
#define CLIP_ACCEPT(a,b) (!(a|b))

/*!
\brief Internal clip-encoding routine.

Calculates a segement-based clipping encoding for a point against a rectangle.

\param x X coordinate of point.
\param y Y coordinate of point.
\param left X coordinate of left edge of the rectangle.
\param top Y coordinate of top edge of the rectangle.
\param right X coordinate of right edge of the rectangle.
\param bottom Y coordinate of bottom edge of the rectangle.
 */
static int _clipEncode(Sint16 x, Sint16 y, Sint16 left, Sint16 top, Sint16 right, Sint16 bottom)
{
    int code = 0;

    if (x < left) {
        code |= CLIP_LEFT_EDGE;
    } else if (x > right) {
        code |= CLIP_RIGHT_EDGE;
    }
    if (y < top) {
        code |= CLIP_TOP_EDGE;
    } else if (y > bottom) {
        code |= CLIP_BOTTOM_EDGE;
    }
    return code;
}

/*!
\brief Clip line to a the clipping rectangle of a surface.

\param dst Target surface to draw on.
\param x Pointer to X coordinate of first point of line.
\param y Pointer to Y coordinate of first point of line.
\param x2 Pointer to X coordinate of second point of line.
\param y2 Pointer to Y coordinate of second point of line.
 */
static int _clipLine(SDL_Surface * dst, Sint16 * x, Sint16 * y, Sint16 * x2, Sint16 * y2)
{
    Sint16 left, right, top, bottom;
    int code1, code2;
    int draw = 0;
    Sint16 swaptmp;
    float m;

    /*
     * Get clipping boundary
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    while (1) {
        code1 = _clipEncode(*x, *y, left, top, right, bottom);
        code2 = _clipEncode(*x2, *y2, left, top, right, bottom);
        if (CLIP_ACCEPT(code1, code2)) {
            draw = 1;
            break;
        } else if (CLIP_REJECT(code1, code2)) {
            break;
        } else {
            if (CLIP_INSIDE(code1)) {
                swaptmp = *x2;
                *x2 = *x;
                *x = swaptmp;
                swaptmp = *y2;
                *y2 = *y;
                *y = swaptmp;
                swaptmp = code2;
                code2 = code1;
                code1 = swaptmp;
            }
            if (*x2 != *x) {
                m = (*y2 - *y) / (float) (*x2 - *x);
            } else {
                m = 1.0f;
            }
            if (code1 & CLIP_LEFT_EDGE) {
                *y += (Sint16) ((left - *x) * m);
                *x = left;
            } else if (code1 & CLIP_RIGHT_EDGE) {
                *y += (Sint16) ((right - *x) * m);
                *x = right;
            } else if (code1 & CLIP_BOTTOM_EDGE) {
                if (*x2 != *x) {
                    *x += (Sint16) ((bottom - *y) / m);
                }
                *y = bottom;
            } else if (code1 & CLIP_TOP_EDGE) {
                if (*x2 != *x) {
                    *x += (Sint16) ((top - *y) / m);
                }
                *y = top;
            }
        }
    }

    return draw;
}

/*!
\brief Draw box (filled rectangle) with blending.

\param dst The surface to draw on.
\param xx X coordinate of the first point (i.e. top right) of the box.
\param yy Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param color The color value of the box to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int boxColor(SDL_Surface * dst, Sint16 xx, Sint16 yy, Sint16 x2, Sint16 y2, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int x, dx;
    int dy;
    int pixx, pixy;
    Sint16 w, h, tmp;
    int result;
    Uint8 *colorptr;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Order coordinates to ensure that
     * x1<=x2 and y1<=y2
     */
    if (xx > x2) {
        tmp = xx;
        xx = x2;
        x2 = tmp;
    }
    if (yy > y2) {
        tmp = yy;
        yy = y2;
        y2 = tmp;
    }

    /*
     * Get clipping boundary and
     * check visibility
     */
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /* Clip all points */
    if (xx < left) {
        xx = left;
    } else if (xx > right) {
        xx = right;
    }
    if (x2 < left) {
        x2 = left;
    } else if (x2 > right) {
        x2 = right;
    }
    if (yy < top) {
        yy = top;
    } else if (yy > bottom) {
        yy = bottom;
    }
    if (y2 < top) {
        y2 = top;
    } else if (y2 > bottom) {
        y2 = bottom;
    }

    /*
     * Test for special cases of straight line or single point
     */
    if (xx == x2) {
        if (yy == y2) {
            return (pixelColor(dst, xx, yy, color));
        } else {
            return (vlineColor(dst, xx, yy, y2, color));
        }
    }
    if (yy == y2) {
        return (hlineColor(dst, xx, x2, yy, color));
    }

    /*
     * Calculate width&height
     */
    w = x2 - xx;
    h = y2 - yy;

    /*
     * Alpha check
     */
    if ((color & 255) == 255) {

        /*
         * No alpha-blending required
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }

        /*
         * Lock the surface
         */
        if (SDL_MUSTLOCK(dst)) {
            if (SDL_LockSurface(dst) < 0) {
                return (-1);
            }
        }

        /*
         * More variable setup
         */
        dx = w;
        dy = h;
        pixx = dst->format->BytesPerPixel;
        pixy = dst->pitch;
        pixel = ((Uint8 *) dst->pixels) + pixx * (int) xx + pixy * (int) yy;
        pixellast = pixel + pixx * dx + pixy * dy;
        dx++;

        /*
         * Draw
         */
        switch (dst->format->BytesPerPixel) {
        case 1:
            for (; pixel <= pixellast; pixel += pixy) {
                memset(pixel, (Uint8) color, dx);
            }
            break;
        case 2:
            pixy -= (pixx * dx);
            for (; pixel <= pixellast; pixel += pixy) {
                for (x = 0; x < dx; x++) {
                    *(Uint16*) pixel = color;
                    pixel += pixx;
                }
            }
            break;
        case 3:
            pixy -= (pixx * dx);
            for (; pixel <= pixellast; pixel += pixy) {
                for (x = 0; x < dx; x++) {
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                        pixel[0] = (color >> 16) & 0xff;
                        pixel[1] = (color >> 8) & 0xff;
                        pixel[2] = color & 0xff;
                    } else {
                        pixel[0] = color & 0xff;
                        pixel[1] = (color >> 8) & 0xff;
                        pixel[2] = (color >> 16) & 0xff;
                    }
                    pixel += pixx;
                }
            }
            break;
        default:        /* case 4 */
            pixy -= (pixx * dx);
            for (; pixel <= pixellast; pixel += pixy) {
                for (x = 0; x < dx; x++) {
                    *(Uint32 *) pixel = color;
                    pixel += pixx;
                }
            }
            break;
        }

        /* Unlock surface */
        if (SDL_MUSTLOCK(dst)) {
            SDL_UnlockSurface(dst);
        }

        result = 0;

    } else {

        result = filledRectAlpha(dst, xx, yy, xx + w, yy + h, color);

    }

    return (result);
}

/*!
\brief Draw box (filled rectangle) with blending.

\param dst The surface to draw on.
\param x X coordinate of the first point (i.e. top right) of the box.
\param y Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param r The red value of the box to draw.
\param g The green value of the box to draw.
\param b The blue value of the box to draw.
\param a The alpha value of the box to draw.

\returns Returns 0 on success, -1 on failure.
 */
int boxRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (boxColor(dst, x, y, x2, y2, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- Line */

/* Non-alpha line drawing code adapted from routine          */
/* by Pete Shinners, pete@shinners.org                       */
/* Originally from pygame, http://pygame.seul.org            */

/*!
\brief Draw line with alpha blending.

\param dst The surface to draw on.
\param xx X coordinate of the first point of the line.
\param yy Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param color The color value of the line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int lineColor(SDL_Surface * dst, Sint16 xx, Sint16 yy, Sint16 x2, Sint16 y2, Uint32 color)
{
    int pixx, pixy;
    int x, y;
    int dx, dy;
    int ax, ay;
    int sx, sy;
    int swaptmp;
    Uint8 *pixel;
    Uint8 *colorptr;

    /*
     * Clip line and test if we have to draw
     */
    if (!(_clipLine(dst, &xx, &yy, &x2, &y2))) {
        return (0);
    }

    /*
     * Test for special cases of straight lines or single point
     */
    if (xx == x2) {
        if (yy < y2) {
            return (vlineColor(dst, xx, yy, y2, color));
        } else if (yy > y2) {
            return (vlineColor(dst, xx, y2, yy, color));
        } else {
            return (pixelColor(dst, xx, yy, color));
        }
    }
    if (yy == y2) {
        if (xx < x2) {
            return (hlineColor(dst, xx, x2, yy, color));
        } else if (xx > x2) {
            return (hlineColor(dst, x2, xx, yy, color));
        }
    }

    /*
     * Variable setup
     */
    dx = x2 - xx;
    dy = y2 - yy;
    sx = (dx >= 0) ? 1 : -1;
    sy = (dy >= 0) ? 1 : -1;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Check for alpha blending
     */
    if ((color & 255) == 255) {

        /*
         * No alpha blending - use fast pixel routines
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }

        /*
         * More variable setup
         */
        dx = sx * dx + 1;
        dy = sy * dy + 1;
        pixx = dst->format->BytesPerPixel;
        pixy = dst->pitch;
        pixel = ((Uint8 *) dst->pixels) + pixx * (int) xx + pixy * (int) yy;
        pixx *= sx;
        pixy *= sy;
        if (dx < dy) {
            swaptmp = dx;
            dx = dy;
            dy = swaptmp;
            swaptmp = pixx;
            pixx = pixy;
            pixy = swaptmp;
        }

        /*
         * Draw
         */
        x = 0;
        y = 0;
        switch (dst->format->BytesPerPixel) {
        case 1:
            for (; x < dx; x++, pixel += pixx) {
                *pixel = color;
                y += dy;
                if (y >= dx) {
                    y -= dx;
                    pixel += pixy;
                }
            }
            break;
        case 2:
            for (; x < dx; x++, pixel += pixx) {
                *(Uint16 *) pixel = color;
                y += dy;
                if (y >= dx) {
                    y -= dx;
                    pixel += pixy;
                }
            }
            break;
        case 3:
            for (; x < dx; x++, pixel += pixx) {
                if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                    pixel[0] = (color >> 16) & 0xff;
                    pixel[1] = (color >> 8) & 0xff;
                    pixel[2] = color & 0xff;
                } else {
                    pixel[0] = color & 0xff;
                    pixel[1] = (color >> 8) & 0xff;
                    pixel[2] = (color >> 16) & 0xff;
                }
                y += dy;
                if (y >= dx) {
                    y -= dx;
                    pixel += pixy;
                }
            }
            break;
        default:        /* case 4 */
            for (; x < dx; x++, pixel += pixx) {
                *(Uint32 *) pixel = color;
                y += dy;
                if (y >= dx) {
                    y -= dx;
                    pixel += pixy;
                }
            }
            break;
        }

    } else {

        /*
         * Alpha blending required - use single-pixel blits
         */

        ax = ABS(dx) << 1;
        ay = ABS(dy) << 1;
        x = xx;
        y = yy;
        if (ax > ay) {
            int d = ay - (ax >> 1);

            while (x != x2) {
                pixelColorNolock (dst, x, y, color);
                if (d > 0 || (d == 0 && sx == 1)) {
                    y += sy;
                    d -= ax;
                }
                x += sx;
                d += ay;
            }
        } else {
            int d = ax - (ay >> 1);

            while (y != y2) {
                pixelColorNolock (dst, x, y, color);
                if (d > 0 || ((d == 0) && (sy == 1))) {
                    x += sx;
                    d -= ay;
                }
                y += sy;
                d += ax;
            }
        }
        pixelColorNolock (dst, x, y, color);

    }

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (0);
}

/*!
\brief Draw line with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the line.
\param y Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param r The red value of the line to draw.
\param g The green value of the line to draw.
\param b The blue value of the line to draw.
\param a The alpha value of the line to draw.

\returns Returns 0 on success, -1 on failure.
 */
int lineRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (lineColor(dst, x, y, x2, y2, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* AA Line */

#define AAlevels 256
#define AAbits 8

/*!
\brief Internal function to draw anti-aliased line with alpha blending and endpoint control.

This implementation of the Wu antialiasing code is based on Mike Abrash's
DDJ article which was reprinted as Chapter 42 of his Graphics Programming
Black Book, but has been optimized to work with SDL and utilizes 32-bit
fixed-point arithmetic by A. Schiffler. The endpoint control allows the
supression to draw the last pixel useful for rendering continous aa-lines
with alpha<255.

\param dst The surface to draw on.
\param x X coordinate of the first point of the aa-line.
\param y Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param color The color value of the aa-line to draw (0xRRGGBBAA).
\param draw_endpoint Flag indicating if the endpoint should be drawn; draw if non-zero.

\returns Returns 0 on success, -1 on failure.
 */
int _aalineColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color, int draw_endpoint)
{
    Sint32 xx0, yy0, xx1, yy1;
    int result;
    Uint32 intshift, erracc, erradj;
    Uint32 erracctmp, wgt;
    int dx, dy, tmp, xdir, y0p1, x0pxdir;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Clip line and test if we have to draw
     */
    if (!(_clipLine(dst, &x, &y, &x2, &y2))) {
        return (0);
    }

    /*
     * Keep on working with 32bit numbers
     */
    xx0 = x;
    yy0 = y;
    xx1 = x2;
    yy1 = y2;

    /*
     * Reorder points if required
     */
    if (yy0 > yy1) {
        tmp = yy0;
        yy0 = yy1;
        yy1 = tmp;
        tmp = xx0;
        xx0 = xx1;
        xx1 = tmp;
    }

    /*
     * Calculate distance
     */
    dx = xx1 - xx0;
    dy = yy1 - yy0;

    /*
     * Check for special cases
     */
    if (dx == 0) {
        /*
         * Vertical line
         */
        if (draw_endpoint) {
            return (vlineColor(dst, x, y, y2, color));
        } else {
            if (dy > 0) {
                return (vlineColor(dst, x, yy0, yy0 + dy, color));
            } else {
                return (pixelColor(dst, x, y, color));
            }
        }
    } else if (dy == 0) {
        /*
         * Horizontal line
         */
        if (draw_endpoint) {
            return (hlineColor(dst, x, x2, y, color));
        } else {
            if (dx > 0) {
                return (hlineColor(dst, xx0, xx0 + dx, y, color));
            } else {
                return (pixelColor(dst, x, y, color));
            }
        }
    } else if ((dx == dy) && (draw_endpoint)) {
        /*
         * Diagonal line (with endpoint)
         */
        return (lineColor(dst, x, y, x2, y2, color));
    }

    /*
     * Adjust for negative dx and set xdir
     */
    if (dx >= 0) {
        xdir = 1;
    } else {
        xdir = -1;
        dx = (-dx);
    }

    /*
     * Line is not horizontal, vertical or diagonal (with endpoint)
     */
    result = 0;

    /*
     * Zero accumulator
     */
    erracc = 0;

    /*
     * # of bits by which to shift erracc to get intensity level
     */
    intshift = 32 - AAbits;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Draw the initial pixel in the foreground color
     */
    result |= pixelColorNolock(dst, x, y, color);

    /*
     * x-major or y-major?
     */
    if (dy > dx) {

        /*
         * y-major.  Calculate 16-bit fixed point fractional part of a pixel that
         * X advances every time Y advances 1 pixel, truncating the result so that
         * we won't overrun the endpoint along the X axis
         */
        /*
         * Not-so-portable version: erradj = ((Uint64)dx << 32) / (Uint64)dy;
         */
        erradj = ((dx << 16) / dy) << 16;

        /*
         * draw all pixels other than the first and last
         */
        x0pxdir = xx0 + xdir;
        while (--dy) {
            erracctmp = erracc;
            erracc += erradj;
            if (erracc <= erracctmp) {
                /*
                 * rollover in error accumulator, x coord advances
                 */
                xx0 = x0pxdir;
                x0pxdir += xdir;
            }
            yy0++;        /* y-major so always advance Y */

            /*
             * the AAbits most significant bits of erracc give us the intensity
             * weighting for this pixel, and the complement of the weighting for
             * the paired pixel.
             */
            wgt = (erracc >> intshift) & 255;
            result |= pixelColorWeightNolock (dst, xx0, yy0, color, 255 - wgt);
            result |= pixelColorWeightNolock (dst, x0pxdir, yy0, color, wgt);
        }

    } else {

        /*
         * x-major line.  Calculate 16-bit fixed-point fractional part of a pixel
         * that Y advances each time X advances 1 pixel, truncating the result so
         * that we won't overrun the endpoint along the X axis.
         */
        /*
         * Not-so-portable version: erradj = ((Uint64)dy << 32) / (Uint64)dx;
         */
        erradj = ((dy << 16) / dx) << 16;

        /*
         * draw all pixels other than the first and last
         */
        y0p1 = yy0 + 1;
        while (--dx) {

            erracctmp = erracc;
            erracc += erradj;
            if (erracc <= erracctmp) {
                /*
                 * Accumulator turned over, advance y
                 */
                yy0 = y0p1;
                y0p1++;
            }
            xx0 += xdir;    /* x-major so always advance X */
            /*
             * the AAbits most significant bits of erracc give us the intensity
             * weighting for this pixel, and the complement of the weighting for
             * the paired pixel.
             */
            wgt = (erracc >> intshift) & 255;
            result |= pixelColorWeightNolock (dst, xx0, yy0, color, 255 - wgt);
            result |= pixelColorWeightNolock (dst, xx0, y0p1, color, wgt);
        }
    }

    /*
     * Do we have to draw the endpoint
     */
    if (draw_endpoint) {
        /*
         * Draw final pixel, always exactly intersected by the line and doesn't
         * need to be weighted.
         */
        result |= pixelColorNolock (dst, x2, y2, color);
    }

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Ddraw anti-aliased line with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the aa-line.
\param y Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param color The color value of the aa-line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int aalineColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color)
{
    return (_aalineColor(dst, x, y, x2, y2, color, 1));
}

/*!
\brief Draw anti-aliased line with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the aa-line.
\param y Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param r The red value of the aa-line to draw.
\param g The green value of the aa-line to draw.
\param b The blue value of the aa-line to draw.
\param a The alpha value of the aa-line to draw.

\returns Returns 0 on success, -1 on failure.
 */
int aalineRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return (_aalineColor
            (dst, x, y, x2, y2, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a, 1));
}


/* ----- Circle */

/*!
\brief Draw circle with blending.

Note: Circle drawing routine is based on an algorithms from the sge library,
but modified by A. Schiffler for multiple pixel-draw removal and other
minor speedup changes.

\param dst The surface to draw on.
\param x X coordinate of the center of the circle.
\param y Y coordinate of the center of the circle.
\param rad Radius in pixels of the circle.
\param color The color value of the circle to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int circleColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 xx, yy, x2, y2;
    Sint16 cx = 0;
    Sint16 cy = rad;
    Sint16 df = 1 - rad;
    Sint16 d_e = 3;
    Sint16 d_se = -2 * rad + 5;
    Sint16 xpcx, xmcx, xpcy, xmcy;
    Sint16 ypcy, ymcy, ypcx, ymcx;
    Uint8 *colorptr;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radius
     */
    if (rad < 0) {
        return (-1);
    }

    /*
     * Special case for rad=0 - draw a point
     */
    if (rad == 0) {
        return (pixelColor(dst, x, y, color));
    }

    /*
     * Get circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rad;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx = x - rad;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    y2 = y + rad;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy = y - rad;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /*
     * Draw circle
     */
    result = 0;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Alpha Check
     */
    if ((color & 255) == 255) {

        /*
         * No Alpha - direct memory writes
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }

        /*
         * Draw
         */
        do {
            ypcy = y + cy;
            ymcy = y - cy;
            if (cx > 0) {
                xpcx = x + cx;
                xmcx = x - cx;
                result |= fastPixelColorNolock(dst, xmcx, ypcy, color);
                result |= fastPixelColorNolock(dst, xpcx, ypcy, color);
                result |= fastPixelColorNolock(dst, xmcx, ymcy, color);
                result |= fastPixelColorNolock(dst, xpcx, ymcy, color);
            } else {
                result |= fastPixelColorNolock(dst, x, ymcy, color);
                result |= fastPixelColorNolock(dst, x, ypcy, color);
            }
            xpcy = x + cy;
            xmcy = x - cy;
            if ((cx > 0) && (cx != cy)) {
                ypcx = y + cx;
                ymcx = y - cx;
                result |= fastPixelColorNolock(dst, xmcy, ypcx, color);
                result |= fastPixelColorNolock(dst, xpcy, ypcx, color);
                result |= fastPixelColorNolock(dst, xmcy, ymcx, color);
                result |= fastPixelColorNolock(dst, xpcy, ymcx, color);
            } else if (cx == 0) {
                result |= fastPixelColorNolock(dst, xmcy, y, color);
                result |= fastPixelColorNolock(dst, xpcy, y, color);
            }
            /*
             * Update
             */
            if (df < 0) {
                df += d_e;
                d_e += 2;
                d_se += 2;
            } else {
                df += d_se;
                d_e += 2;
                d_se += 4;
                cy--;
            }
            cx++;
        } while (cx <= cy);

        /*
         * Unlock surface
         */
        SDL_UnlockSurface(dst);

    } else {

        /*
         * Using Alpha - blended pixel blits
         */

        do {
            /*
             * Draw
             */
            ypcy = y + cy;
            ymcy = y - cy;
            if (cx > 0) {
                xpcx = x + cx;
                xmcx = x - cx;
                result |= pixelColorNolock (dst, xmcx, ypcy, color);
                result |= pixelColorNolock (dst, xpcx, ypcy, color);
                result |= pixelColorNolock (dst, xmcx, ymcy, color);
                result |= pixelColorNolock (dst, xpcx, ymcy, color);
            } else {
                result |= pixelColorNolock (dst, x, ymcy, color);
                result |= pixelColorNolock (dst, x, ypcy, color);
            }
            xpcy = x + cy;
            xmcy = x - cy;
            if ((cx > 0) && (cx != cy)) {
                ypcx = y + cx;
                ymcx = y - cx;
                result |= pixelColorNolock (dst, xmcy, ypcx, color);
                result |= pixelColorNolock (dst, xpcy, ypcx, color);
                result |= pixelColorNolock (dst, xmcy, ymcx, color);
                result |= pixelColorNolock (dst, xpcy, ymcx, color);
            } else if (cx == 0) {
                result |= pixelColorNolock (dst, xmcy, y, color);
                result |= pixelColorNolock (dst, xpcy, y, color);
            }
            /*
             * Update
             */
            if (df < 0) {
                df += d_e;
                d_e += 2;
                d_se += 2;
            } else {
                df += d_se;
                d_e += 2;
                d_se += 4;
                cy--;
            }
            cx++;
        } while (cx <= cy);

    }                /* Alpha check */

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Draw circle with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the circle.
\param y Y coordinate of the center of the circle.
\param rad Radius in pixels of the circle.
\param r The red value of the circle to draw.
\param g The green value of the circle to draw.
\param b The blue value of the circle to draw.
\param a The alpha value of the circle to draw.

\returns Returns 0 on success, -1 on failure.
 */
int circleRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (circleColor(dst, x, y, rad, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- Arc */

/*!
\brief Arc with blending.

Note Arc drawing is based on circle algorithm by A. Schiffler and
written by D. Raber. Calculates which octants arc goes through and
renders pixels accordingly.

\param dst The surface to draw on.
\param x X coordinate of the center of the arc.
\param y Y coordinate of the center of the arc.
\param rad Radius in pixels of the arc.
\param start Starting radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param end Ending radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param color The color value of the arc to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int arcColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 xx, yy, x2, y2;
    Sint16 cx = 0;
    Sint16 cy = rad;
    Sint16 df = 1 - rad;
    Sint16 d_e = 3;
    Sint16 d_se = -2 * rad + 5;
    Sint16 xpcx, xmcx, xpcy, xmcy;
    Sint16 ypcy, ymcy, ypcx, ymcx;
    Uint8 *colorptr;
    Uint8 drawoct;
    int startoct, endoct, oct, stopval_start = 0, stopval_end;
    double temp = 0;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radius
     */
    if (rad < 0) {
        return (-1);
    }

    /*
     * Special case for rad=0 - draw a point
     */
    if (rad == 0) {
        return (pixelColor(dst, x, y, color));
    }

    /*
     * Get arc's circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rad;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx = x - rad;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    y2 = y + rad;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy = y - rad;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /* Octant labelling
    //  \ 5 | 6 /
    //   \  |  /
    //  4 \ | / 7
    //     \|/
    //------+------ +x
    //     /|\
    //  3 / | \ 0
    //   /  |  \
    //  / 2 | 1 \
    //      +y              */

    /* Initially reset bitmask to 0x00000000
     * the set whether or not to keep drawing a given octant.
     * For example: 0x00111100 means we're drawing in octants 2-5 */
    drawoct = 0;

    /*
     * Fixup angles
     */
    start %= 360;
    end %= 360;
    /* 0 <= start & end < 360; note that sometimes start > end - if so, arc goes back through 0. */
    while (start < 0) {
        start += 360;
    }
    while (end < 0) {
        end += 360;
    }
    start %= 360;
    end %= 360;

    /* now, we find which octants we're drawing in. */
    startoct = start / 45;
    endoct = end / 45;
    oct = startoct - 1; /* we increment as first step in loop */

    /* stopval_start, stopval_end; */
    /* what values of cx to stop at. */
    do {
        oct = (oct + 1) % 8;

        if (oct == startoct) {
            /* need to compute stopval_start for this octant.  Look at picture above if this is unclear */
            switch (oct) {
            case 0:
            case 3:
                temp = sin(start * M_PI / 180);
                break;
            case 1:
            case 6:
                temp = cos(start * M_PI / 180);
                break;
            case 2:
            case 5:
                temp = -cos(start * M_PI / 180);
                break;
            case 4:
            case 7:
                temp = -sin(start * M_PI / 180);
                break;
            }
            temp *= rad;
            stopval_start = (int) temp; /* always round down. */
            /* This isn't arbitrary, but requires graph paper to explain well. */
            /* The basic idea is that we're always changing drawoct after we draw, so we */
            /* stop immediately after we render the last sensible pixel at x = ((int)temp). */

            /* and whether to draw in this octant initially */
            if (oct % 2) {
                drawoct |= (1 << oct);
            }/* this is basically like saying drawoct[oct] = true, if drawoct were a bool array */
            else         {
                drawoct &= 255 - (1 << oct);
            } /* this is basically like saying drawoct[oct] = false */
        }
        if (oct == endoct) {
            /* need to compute stopval_end for this octant */
            switch (oct) {
            case 0:
            case 3:
                temp = sin(end * M_PI / 180);
                break;
            case 1:
            case 6:
                temp = cos(end * M_PI / 180);
                break;
            case 2:
            case 5:
                temp = -cos(end * M_PI / 180);
                break;
            case 4:
            case 7:
                temp = -sin(end * M_PI / 180);
                break;
            }
            temp *= rad;
            stopval_end = (int) temp;

            /* and whether to draw in this octant initially */
            if (startoct == endoct)    {
                /* note:      we start drawing, stop, then start again in this case */
                /* otherwise: we only draw in this octant, so initialize it to false, it will get set back to true */
                if (start > end) {
                    /* unfortunately, if we're in the same octant and need to draw over the whole circle, */
                    /* we need to set the rest to true, because the while loop will end at the bottom. */
                    drawoct = 255;
                } else {
                    drawoct &= 255 - (1 << oct);
                }
            } else if (oct % 2) {
                drawoct &= 255 - (1 << oct);
            } else                      {
                drawoct |= (1 << oct);
            }
        } else if (oct != startoct) { /* already verified that it's != endoct */
            drawoct |= (1 << oct); /* draw this entire segment */
        }
    } while (oct != endoct);

    /* so now we have what octants to draw and when to draw them.  all that's left is the actual raster code. */

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Draw arc
     */
    result = 0;

    /*
     * Alpha Check
     */
    if ((color & 255) == 255) {

        /*
         * No Alpha - direct memory writes
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }

        /*
         * Draw
         */
        do {
            ypcy = y + cy;
            ymcy = y - cy;
            if (cx > 0) {
                xpcx = x + cx;
                xmcx = x - cx;
                if (drawoct & 4)  {
                    result |= fastPixelColorNolock(dst, xmcx, ypcy, color);
                }
                if (drawoct & 2)  {
                    result |= fastPixelColorNolock(dst, xpcx, ypcy, color);
                }
                if (drawoct & 32) {
                    result |= fastPixelColorNolock(dst, xmcx, ymcy, color);
                }
                if (drawoct & 64) {
                    result |= fastPixelColorNolock(dst, xpcx, ymcy, color);
                }
            } else {
                if (drawoct & 6)  {
                    result |= fastPixelColorNolock(dst, x, ypcy, color);
                }
                if (drawoct & 96) {
                    result |= fastPixelColorNolock(dst, x, ymcy, color);
                }
            }

            xpcy = x + cy;
            xmcy = x - cy;
            if (cx > 0 && cx != cy) {
                ypcx = y + cx;
                ymcx = y - cx;
                if (drawoct & 8)   {
                    result |= fastPixelColorNolock(dst, xmcy, ypcx, color);
                }
                if (drawoct & 1)   {
                    result |= fastPixelColorNolock(dst, xpcy, ypcx, color);
                }
                if (drawoct & 16)  {
                    result |= fastPixelColorNolock(dst, xmcy, ymcx, color);
                }
                if (drawoct & 128) {
                    result |= fastPixelColorNolock(dst, xpcy, ymcx, color);
                }
            } else if (cx == 0) {
                if (drawoct & 24)  {
                    result |= fastPixelColorNolock(dst, xmcy, y, color);
                }
                if (drawoct & 129) {
                    result |= fastPixelColorNolock(dst, xpcy, y, color);
                }
            }

            /*
             * Update whether we're drawing an octant
             */
            if (stopval_start == cx) {
                if (drawoct & (1 << startoct)) {
                    drawoct &= 255 - (1 << startoct);
                } else {
                    drawoct |= (1 << startoct);
                }
            }
            if (stopval_end == cx) {
                if (drawoct & (1 << endoct)) {
                    drawoct &= 255 - (1 << endoct);
                } else {
                    drawoct |= (1 << endoct);
                }
            }

            /*
             * Update pixels
             */
            if (df < 0) {
                df += d_e;
                d_e += 2;
                d_se += 2;
            } else {
                df += d_se;
                d_e += 2;
                d_se += 4;
                cy--;
            }
            cx++;
        } while (cx <= cy);

        /*
         * Unlock surface
         */
        SDL_UnlockSurface(dst);

    } else {

        /*
         * Using Alpha - blended pixel blits
         */

        do {
            ypcy = y + cy;
            ymcy = y - cy;
            if (cx > 0) {
                xpcx = x + cx;
                xmcx = x - cx;

                if (drawoct & 4)  {
                    result |= pixelColorNolock(dst, xmcx, ypcy, color);
                }
                if (drawoct & 2)  {
                    result |= pixelColorNolock(dst, xpcx, ypcy, color);
                }
                if (drawoct & 32) {
                    result |= pixelColorNolock(dst, xmcx, ymcy, color);
                }
                if (drawoct & 64) {
                    result |= pixelColorNolock(dst, xpcx, ymcy, color);
                }
            } else {
                if (drawoct & 96) {
                    result |= pixelColorNolock(dst, x, ymcy, color);
                }
                if (drawoct & 6)  {
                    result |= pixelColorNolock(dst, x, ypcy, color);
                }
            }

            xpcy = x + cy;
            xmcy = x - cy;
            if (cx > 0 && cx != cy) {
                ypcx = y + cx;
                ymcx = y - cx;
                if (drawoct & 8)   {
                    result |= pixelColorNolock(dst, xmcy, ypcx, color);
                }
                if (drawoct & 1)   {
                    result |= pixelColorNolock(dst, xpcy, ypcx, color);
                }
                if (drawoct & 16)  {
                    result |= pixelColorNolock(dst, xmcy, ymcx, color);
                }
                if (drawoct & 128) {
                    result |= pixelColorNolock(dst, xpcy, ymcx, color);
                }
            } else if (cx == 0) {
                if (drawoct & 24)  {
                    result |= pixelColorNolock(dst, xmcy, y, color);
                }
                if (drawoct & 129) {
                    result |= pixelColorNolock(dst, xpcy, y, color);
                }
            }

            /*
             * Update whether we're drawing an octant
             */
            if (stopval_start == cx) {
                if (drawoct & (1 << startoct)) {
                    drawoct &= 255 - (1 << startoct);
                } else                                               {
                    drawoct |= (1 << startoct);
                }
            }
            if (stopval_end == cx) {
                if (drawoct & (1 << endoct)) {
                    drawoct &= 255 - (1 << endoct);
                } else                                             {
                    drawoct |= (1 << endoct);
                }
            }

            /*
             * Update pixels
             */
            if (df < 0) {
                df += d_e;
                d_e += 2;
                d_se += 2;
            } else {
                df += d_se;
                d_e += 2;
                d_se += 4;
                cy--;
            }
            cx++;
        } while (cx <= cy);

    }                /* Alpha check */

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Arc with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the arc.
\param y Y coordinate of the center of the arc.
\param rad Radius in pixels of the arc.
\param start Starting radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param end Ending radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param r The red value of the arc to draw.
\param g The green value of the arc to draw.
\param b The blue value of the arc to draw.
\param a The alpha value of the arc to draw.

\returns Returns 0 on success, -1 on failure.
 */
int arcRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (arcColor(dst, x, y, rad, start, end, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- AA Circle */

/*!
\brief Draw anti-aliased circle with blending.

Note: The AA-circle routine is based on AA-ellipse with identical radii.

\param dst The surface to draw on.
\param x X coordinate of the center of the aa-circle.
\param y Y coordinate of the center of the aa-circle.
\param rad Radius in pixels of the aa-circle.
\param color The color value of the aa-circle to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int aacircleColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Uint32 color)
{
    return (aaellipseColor(dst, x, y, rad, rad, color));
}

/*!
\brief Draw anti-aliased circle with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the aa-circle.
\param y Y coordinate of the center of the aa-circle.
\param rad Radius in pixels of the aa-circle.
\param r The red value of the aa-circle to draw.
\param g The green value of the aa-circle to draw.
\param b The blue value of the aa-circle to draw.
\param a The alpha value of the aa-circle to draw.

\returns Returns 0 on success, -1 on failure.
 */
int aacircleRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (aaellipseColor
            (dst, x, y, rad, rad, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- Filled Circle */

/*!
\brief Draw filled circle with blending.

Note: Based on algorithms from sge library with modifications by A. Schiffler for
multiple-hline draw removal and other minor speedup changes.

\param dst The surface to draw on.
\param x X coordinate of the center of the filled circle.
\param y Y coordinate of the center of the filled circle.
\param rad Radius in pixels of the filled circle.
\param color The color value of the filled circle to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int filledCircleColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 xx, yy, x2, y2;
    Sint16 cx = 0;
    Sint16 cy = rad;
    Sint16 ocx = (Sint16) 0xffff;
    Sint16 ocy = (Sint16) 0xffff;
    Sint16 df = 1 - rad;
    Sint16 d_e = 3;
    Sint16 d_se = -2 * rad + 5;
    Sint16 xpcx, xmcx, xpcy, xmcy;
    Sint16 ypcy, ymcy, ypcx, ymcx;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radius
     */
    if (rad < 0) {
        return (-1);
    }

    /*
     * Special case for rad=0 - draw a point
     */
    if (rad == 0) {
        return (pixelColor(dst, x, y, color));
    }

    /*
     * Get circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rad;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx = x - rad;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    y2 = y + rad;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy = y - rad;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /*
     * Draw
     */
    result = 0;
    do {
        xpcx = x + cx;
        xmcx = x - cx;
        xpcy = x + cy;
        xmcy = x - cy;
        if (ocy != cy) {
            if (cy > 0) {
                ypcy = y + cy;
                ymcy = y - cy;
                result |= hlineColor(dst, xmcx, xpcx, ypcy, color);
                result |= hlineColor(dst, xmcx, xpcx, ymcy, color);
            } else {
                result |= hlineColor(dst, xmcx, xpcx, y, color);
            }
            ocy = cy;
        }
        if (ocx != cx) {
            if (cx != cy) {
                if (cx > 0) {
                    ypcx = y + cx;
                    ymcx = y - cx;
                    result |= hlineColor(dst, xmcy, xpcy, ymcx, color);
                    result |= hlineColor(dst, xmcy, xpcy, ypcx, color);
                } else {
                    result |= hlineColor(dst, xmcy, xpcy, y, color);
                }
            }
            ocx = cx;
        }
        /*
         * Update
         */
        if (df < 0) {
            df += d_e;
            d_e += 2;
            d_se += 2;
        } else {
            df += d_se;
            d_e += 2;
            d_se += 4;
            cy--;
        }
        cx++;
    } while (cx <= cy);

    return (result);
}

/*!
\brief Draw filled circle with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the filled circle.
\param y Y coordinate of the center of the filled circle.
\param rad Radius in pixels of the filled circle.
\param r The red value of the filled circle to draw.
\param g The green value of the filled circle to draw.
\param b The blue value of the filled circle to draw.
\param a The alpha value of the filled circle to draw.

\returns Returns 0 on success, -1 on failure.
 */
int filledCircleRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (filledCircleColor
            (dst, x, y, rad, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- Ellipse */

/*!
\brief Draw ellipse with blending.

Note: Based on algorithms from sge library with modifications by A. Schiffler for
multiple-pixel draw removal and other minor speedup changes.

\param dst The surface to draw on.
\param x X coordinate of the center of the ellipse.
\param y Y coordinate of the center of the ellipse.
\param rx Horizontal radius in pixels of the ellipse.
\param ry Vertical radius in pixels of the ellipse.
\param color The color value of the ellipse to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int ellipseColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 xx, yy, x2, y2;
    int ix, iy;
    int h, i, j, k;
    int oh, oi, oj, ok;
    int xmh, xph, ypk, ymk;
    int xmi, xpi, ymj, ypj;
    int xmj, xpj, ymi, ypi;
    int xmk, xpk, ymh, yph;
    Uint8 *colorptr;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radii
     */
    if ((rx < 0) || (ry < 0)) {
        return (-1);
    }

    /*
     * Special case for rx=0 - draw a vline
     */
    if (rx == 0) {
        return (vlineColor(dst, x, y - ry, y + ry, color));
    }
    /*
     * Special case for ry=0 - draw a hline
     */
    if (ry == 0) {
        return (hlineColor(dst, x - rx, x + rx, y, color));
    }

    /*
     * Get circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rx;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx = x - rx;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    y2 = y + ry;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy = y - ry;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /*
     * Init vars
     */
    oh = oi = oj = ok = 0xFFFF;

    /*
     * Draw
     */
    result = 0;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /*
     * Check alpha
     */
    if ((color & 255) == 255) {

        /*
         * No Alpha - direct memory writes
         */

        /*
         * Setup color
         */
        colorptr = (Uint8 *) & color;
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
        } else {
            color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
        }


        if (rx > ry) {
            ix = 0;
            iy = rx * 64;

            do {
                h = (ix + 32) >> 6;
                i = (iy + 32) >> 6;
                j = (h * ry) / rx;
                k = (i * ry) / rx;

                if (((ok != k) && (oj != k)) || ((oj != j) && (ok != j)) || (k != j)) {
                    xph = x + h;
                    xmh = x - h;
                    if (k > 0) {
                        ypk = y + k;
                        ymk = y - k;
                        result |= fastPixelColorNolock(dst, xmh, ypk, color);
                        result |= fastPixelColorNolock(dst, xph, ypk, color);
                        result |= fastPixelColorNolock(dst, xmh, ymk, color);
                        result |= fastPixelColorNolock(dst, xph, ymk, color);
                    } else {
                        result |= fastPixelColorNolock(dst, xmh, y, color);
                        result |= fastPixelColorNolock(dst, xph, y, color);
                    }
                    ok = k;
                    xpi = x + i;
                    xmi = x - i;
                    if (j > 0) {
                        ypj = y + j;
                        ymj = y - j;
                        result |= fastPixelColorNolock(dst, xmi, ypj, color);
                        result |= fastPixelColorNolock(dst, xpi, ypj, color);
                        result |= fastPixelColorNolock(dst, xmi, ymj, color);
                        result |= fastPixelColorNolock(dst, xpi, ymj, color);
                    } else {
                        result |= fastPixelColorNolock(dst, xmi, y, color);
                        result |= fastPixelColorNolock(dst, xpi, y, color);
                    }
                    oj = j;
                }

                ix = ix + iy / rx;
                iy = iy - ix / rx;

            } while (i > h);
        } else {
            ix = 0;
            iy = ry * 64;

            do {
                h = (ix + 32) >> 6;
                i = (iy + 32) >> 6;
                j = (h * rx) / ry;
                k = (i * rx) / ry;

                if (((oi != i) && (oh != i)) || ((oh != h) && (oi != h) && (i != h))) {
                    xmj = x - j;
                    xpj = x + j;
                    if (i > 0) {
                        ypi = y + i;
                        ymi = y - i;
                        result |= fastPixelColorNolock(dst, xmj, ypi, color);
                        result |= fastPixelColorNolock(dst, xpj, ypi, color);
                        result |= fastPixelColorNolock(dst, xmj, ymi, color);
                        result |= fastPixelColorNolock(dst, xpj, ymi, color);
                    } else {
                        result |= fastPixelColorNolock(dst, xmj, y, color);
                        result |= fastPixelColorNolock(dst, xpj, y, color);
                    }
                    oi = i;
                    xmk = x - k;
                    xpk = x + k;
                    if (h > 0) {
                        yph = y + h;
                        ymh = y - h;
                        result |= fastPixelColorNolock(dst, xmk, yph, color);
                        result |= fastPixelColorNolock(dst, xpk, yph, color);
                        result |= fastPixelColorNolock(dst, xmk, ymh, color);
                        result |= fastPixelColorNolock(dst, xpk, ymh, color);
                    } else {
                        result |= fastPixelColorNolock(dst, xmk, y, color);
                        result |= fastPixelColorNolock(dst, xpk, y, color);
                    }
                    oh = h;
                }

                ix = ix + iy / ry;
                iy = iy - ix / ry;

            } while (i > h);
        }

    } else {

        if (rx > ry) {
            ix = 0;
            iy = rx * 64;

            do {
                h = (ix + 32) >> 6;
                i = (iy + 32) >> 6;
                j = (h * ry) / rx;
                k = (i * ry) / rx;

                if (((ok != k) && (oj != k)) || ((oj != j) && (ok != j)) || (k != j)) {
                    xph = x + h;
                    xmh = x - h;
                    if (k > 0) {
                        ypk = y + k;
                        ymk = y - k;
                        result |= pixelColorNolock (dst, xmh, ypk, color);
                        result |= pixelColorNolock (dst, xph, ypk, color);
                        result |= pixelColorNolock (dst, xmh, ymk, color);
                        result |= pixelColorNolock (dst, xph, ymk, color);
                    } else {
                        result |= pixelColorNolock (dst, xmh, y, color);
                        result |= pixelColorNolock (dst, xph, y, color);
                    }
                    ok = k;
                    xpi = x + i;
                    xmi = x - i;
                    if (j > 0) {
                        ypj = y + j;
                        ymj = y - j;
                        result |= pixelColorNolock (dst, xmi, ypj, color);
                        result |= pixelColorNolock (dst, xpi, ypj, color);
                        result |= pixelColorNolock (dst, xmi, ymj, color);
                        result |= pixelColor(dst, xpi, ymj, color);
                    } else {
                        result |= pixelColorNolock (dst, xmi, y, color);
                        result |= pixelColorNolock (dst, xpi, y, color);
                    }
                    oj = j;
                }

                ix = ix + iy / rx;
                iy = iy - ix / rx;

            } while (i > h);
        } else {
            ix = 0;
            iy = ry * 64;

            do {
                h = (ix + 32) >> 6;
                i = (iy + 32) >> 6;
                j = (h * rx) / ry;
                k = (i * rx) / ry;

                if (((oi != i) && (oh != i)) || ((oh != h) && (oi != h) && (i != h))) {
                    xmj = x - j;
                    xpj = x + j;
                    if (i > 0) {
                        ypi = y + i;
                        ymi = y - i;
                        result |= pixelColorNolock (dst, xmj, ypi, color);
                        result |= pixelColorNolock (dst, xpj, ypi, color);
                        result |= pixelColorNolock (dst, xmj, ymi, color);
                        result |= pixelColorNolock (dst, xpj, ymi, color);
                    } else {
                        result |= pixelColorNolock (dst, xmj, y, color);
                        result |= pixelColorNolock (dst, xpj, y, color);
                    }
                    oi = i;
                    xmk = x - k;
                    xpk = x + k;
                    if (h > 0) {
                        yph = y + h;
                        ymh = y - h;
                        result |= pixelColorNolock (dst, xmk, yph, color);
                        result |= pixelColorNolock (dst, xpk, yph, color);
                        result |= pixelColorNolock (dst, xmk, ymh, color);
                        result |= pixelColorNolock (dst, xpk, ymh, color);
                    } else {
                        result |= pixelColorNolock (dst, xmk, y, color);
                        result |= pixelColorNolock (dst, xpk, y, color);
                    }
                    oh = h;
                }

                ix = ix + iy / ry;
                iy = iy - ix / ry;

            } while (i > h);
        }

    }                /* Alpha check */

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Draw ellipse with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the ellipse.
\param y Y coordinate of the center of the ellipse.
\param rx Horizontal radius in pixels of the ellipse.
\param ry Vertical radius in pixels of the ellipse.
\param r The red value of the ellipse to draw.
\param g The green value of the ellipse to draw.
\param b The blue value of the ellipse to draw.
\param a The alpha value of the ellipse to draw.

\returns Returns 0 on success, -1 on failure.
 */
int ellipseRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (ellipseColor(dst, x, y, rx, ry, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- AA Ellipse */

/*!
\brief Draw anti-aliased ellipse with blending.

Note: Based on code from Anders Lindstroem, which is based on code from sge library,
which is based on code from TwinLib.

\param dst The surface to draw on.
\param x X coordinate of the center of the aa-ellipse.
\param y Y coordinate of the center of the aa-ellipse.
\param rx Horizontal radius in pixels of the aa-ellipse.
\param ry Vertical radius in pixels of the aa-ellipse.
\param color The color value of the aa-ellipse to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int aaellipseColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Sint16 xx1, yy1, x2, y2;
    int i;
    int a2, b2, ds, dt, dxt, t, s, d;
    Sint16 xp, yp, xs, ys, dyt, od, xx, yy, xc2, yc2;
    float cp;
    double sab;
    Uint8 weight, iweight;
    int result;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radii
     */
    if ((rx < 0) || (ry < 0)) {
        return (-1);
    }

    /*
     * Special case for rx=0 - draw a vline
     */
    if (rx == 0) {
        return (vlineColor(dst, x, y - ry, y + ry, color));
    }
    /*
     * Special case for ry=0 - draw an hline
     */
    if (ry == 0) {
        return (hlineColor(dst, x - rx, x + rx, y, color));
    }

    /*
     * Get circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rx;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx1 = x - rx;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx1 > right) {
        return (0);
    }
    y2 = y + ry;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy1 = y - ry;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy1 > bottom) {
        return (0);
    }

    /* Variable setup */
    a2 = rx * rx;
    b2 = ry * ry;

    ds = 2 * a2;
    dt = 2 * b2;

    xc2 = 2 * x;
    yc2 = 2 * y;

    sab = sqrt(a2 + b2);
    od = (Sint16) lrint(sab * 0.01) + 1; /* introduce some overdraw */
    dxt = (Sint16) lrint((double) a2 / sab) + od;

    t = 0;
    s = -2 * a2 * ry;
    d = 0;

    xp = x;
    yp = y - ry;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return (-1);
        }
    }

    /* Draw */
    result = 0;

    /* "End points" */
    result |= pixelColorNolock(dst, xp, yp, color);
    result |= pixelColorNolock(dst, xc2 - xp, yp, color);
    result |= pixelColorNolock(dst, xp, yc2 - yp, color);
    result |= pixelColorNolock(dst, xc2 - xp, yc2 - yp, color);

    for (i = 1; i <= dxt; i++) {
        xp--;
        d += t - b2;

        if (d >= 0) {
            ys = yp - 1;
        } else if ((d - s - a2) > 0) {
            if ((2 * d - s - a2) >= 0) {
                ys = yp + 1;
            } else {
                ys = yp;
                yp++;
                d -= s + a2;
                s += ds;
            }
        } else {
            yp++;
            ys = yp + 1;
            d -= s + a2;
            s += ds;
        }

        t -= dt;

        /* Calculate alpha */
        if (s != 0.0) {
            cp = abs(d) / abs(s);
            if (cp > 1.0) {
                cp = 1.0;
            }
        } else {
            cp = 1.0;
        }

        /* Calculate weights */
        weight = (Uint8) (cp * 255);
        iweight = 255 - weight;

        /* Upper half */
        xx = xc2 - xp;
        result |= pixelColorWeightNolock(dst, xp, yp, color, iweight);
        result |= pixelColorWeightNolock(dst, xx, yp, color, iweight);

        result |= pixelColorWeightNolock(dst, xp, ys, color, weight);
        result |= pixelColorWeightNolock(dst, xx, ys, color, weight);

        /* Lower half */
        yy = yc2 - yp;
        result |= pixelColorWeightNolock(dst, xp, yy, color, iweight);
        result |= pixelColorWeightNolock(dst, xx, yy, color, iweight);

        yy = yc2 - ys;
        result |= pixelColorWeightNolock(dst, xp, yy, color, weight);
        result |= pixelColorWeightNolock(dst, xx, yy, color, weight);
    }

    /* Replaces original approximation code dyt = abs(yp - yc); */
    dyt = (Sint16) lrint((double) b2 / sab ) + od;

    for (i = 1; i <= dyt; i++) {
        yp++;
        d -= s + a2;

        if (d <= 0) {
            xs = xp + 1;
        } else if ((d + t - b2) < 0) {
            if ((2 * d + t - b2) <= 0) {
                xs = xp - 1;
            } else {
                xs = xp;
                xp--;
                d += t - b2;
                t -= dt;
            }
        } else {
            xp--;
            xs = xp - 1;
            d += t - b2;
            t -= dt;
        }

        s += ds;

        /* Calculate alpha */
        if (t != 0.0) {
            cp = abs(d) / abs(t);
            if (cp > 1.0) {
                cp = 1.0;
            }
        } else {
            cp = 1.0;
        }

        /* Calculate weight */
        weight = (Uint8) (cp * 255);
        iweight = 255 - weight;

        /* Left half */
        xx = xc2 - xp;
        yy = yc2 - yp;
        result |= pixelColorWeightNolock(dst, xp, yp, color, iweight);
        result |= pixelColorWeightNolock(dst, xx, yp, color, iweight);

        result |= pixelColorWeightNolock(dst, xp, yy, color, iweight);
        result |= pixelColorWeightNolock(dst, xx, yy, color, iweight);

        /* Right half */
        xx = xc2 - xs;
        result |= pixelColorWeightNolock(dst, xs, yp, color, weight);
        result |= pixelColorWeightNolock(dst, xx, yp, color, weight);

        result |= pixelColorWeightNolock(dst, xs, yy, color, weight);
        result |= pixelColorWeightNolock(dst, xx, yy, color, weight);

    }

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
        SDL_UnlockSurface(dst);
    }

    return (result);
}

/*!
\brief Draw anti-aliased ellipse with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the aa-ellipse.
\param y Y coordinate of the center of the aa-ellipse.
\param rx Horizontal radius in pixels of the aa-ellipse.
\param ry Vertical radius in pixels of the aa-ellipse.
\param r The red value of the aa-ellipse to draw.
\param g The green value of the aa-ellipse to draw.
\param b The blue value of the aa-ellipse to draw.
\param a The alpha value of the aa-ellipse to draw.

\returns Returns 0 on success, -1 on failure.
 */
int aaellipseRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (aaellipseColor
            (dst, x, y, rx, ry, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ---- Filled Ellipse */

/* Note: */
/* Based on algorithm from sge library with multiple-hline draw removal */
/* and other speedup changes. */

/*!
\brief Draw filled ellipse with blending.

Note: Based on algorithm from sge library with multiple-hline draw removal
and other speedup changes.

\param dst The surface to draw on.
\param x X coordinate of the center of the filled ellipse.
\param y Y coordinate of the center of the filled ellipse.
\param rx Horizontal radius in pixels of the filled ellipse.
\param ry Vertical radius in pixels of the filled ellipse.
\param color The color value of the filled ellipse to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int filledEllipseColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 xx, yy, x2, y2;
    int ix, iy;
    int h, i, j, k;
    int oh, oi, oj, ok;
    int xmh, xph;
    int xmi, xpi;
    int xmj, xpj;
    int xmk, xpk;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radii
     */
    if ((rx < 0) || (ry < 0)) {
        return (-1);
    }

    /*
     * Special case for rx=0 - draw a vline
     */
    if (rx == 0) {
        return (vlineColor(dst, x, y - ry, y + ry, color));
    }
    /*
     * Special case for ry=0 - draw a hline
     */
    if (ry == 0) {
        return (hlineColor(dst, x - rx, x + rx, y, color));
    }

    /*
     * Get circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rx;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx = x - rx;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    y2 = y + ry;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy = y - ry;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /*
     * Init vars
     */
    oh = oi = oj = ok = 0xFFFF;

    /*
     * Draw
     */
    result = 0;
    if (rx > ry) {
        ix = 0;
        iy = rx * 64;

        do {
            h = (ix + 32) >> 6;
            i = (iy + 32) >> 6;
            j = (h * ry) / rx;
            k = (i * ry) / rx;

            if ((ok != k) && (oj != k)) {
                xph = x + h;
                xmh = x - h;
                if (k > 0) {
                    result |= hlineColor(dst, xmh, xph, y + k, color);
                    result |= hlineColor(dst, xmh, xph, y - k, color);
                } else {
                    result |= hlineColor(dst, xmh, xph, y, color);
                }
                ok = k;
            }
            if ((oj != j) && (ok != j) && (k != j)) {
                xmi = x - i;
                xpi = x + i;
                if (j > 0) {
                    result |= hlineColor(dst, xmi, xpi, y + j, color);
                    result |= hlineColor(dst, xmi, xpi, y - j, color);
                } else {
                    result |= hlineColor(dst, xmi, xpi, y, color);
                }
                oj = j;
            }

            ix = ix + iy / rx;
            iy = iy - ix / rx;

        } while (i > h);
    } else {
        ix = 0;
        iy = ry * 64;

        do {
            h = (ix + 32) >> 6;
            i = (iy + 32) >> 6;
            j = (h * rx) / ry;
            k = (i * rx) / ry;

            if ((oi != i) && (oh != i)) {
                xmj = x - j;
                xpj = x + j;
                if (i > 0) {
                    result |= hlineColor(dst, xmj, xpj, y + i, color);
                    result |= hlineColor(dst, xmj, xpj, y - i, color);
                } else {
                    result |= hlineColor(dst, xmj, xpj, y, color);
                }
                oi = i;
            }
            if ((oh != h) && (oi != h) && (i != h)) {
                xmk = x - k;
                xpk = x + k;
                if (h > 0) {
                    result |= hlineColor(dst, xmk, xpk, y + h, color);
                    result |= hlineColor(dst, xmk, xpk, y - h, color);
                } else {
                    result |= hlineColor(dst, xmk, xpk, y, color);
                }
                oh = h;
            }

            ix = ix + iy / ry;
            iy = iy - ix / ry;

        } while (i > h);
    }

    return (result);
}

/*!
\brief Draw filled ellipse with blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the filled ellipse.
\param y Y coordinate of the center of the filled ellipse.
\param rx Horizontal radius in pixels of the filled ellipse.
\param ry Vertical radius in pixels of the filled ellipse.
\param r The red value of the filled ellipse to draw.
\param g The green value of the filled ellipse to draw.
\param b The blue value of the filled ellipse to draw.
\param a The alpha value of the filled ellipse to draw.

\returns Returns 0 on success, -1 on failure.
 */
int filledEllipseRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (filledEllipseColor
            (dst, x, y, rx, ry, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ----- pie */

/*!
\brief Internal float (low-speed) pie-calc implementation by drawing polygons.

Note: Determines vertex array and uses polygon or filledPolygon drawing routines to render.

\param dst The surface to draw on.
\param x X coordinate of the center of the pie.
\param y Y coordinate of the center of the pie.
\param rad Radius in pixels of the pie.
\param start Starting radius in degrees of the pie.
\param end Ending radius in degrees of the pie.
\param color The color value of the pie to draw (0xRRGGBBAA).
\param filled Flag indicating if the pie should be filled (=1) or not (=0).

\returns Returns 0 on success, -1 on failure.
 */
int _pieColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color, Uint8 filled)
{
    Sint16 left, right, top, bottom;
    Sint16 xx, yy, x2, y2;
    int result;
    double angle, start_angle, end_angle;
    double deltaAngle;
    double dr;
    int numpoints, i;
    Sint16 *vx, *vy;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check radii
     */
    if (rad < 0) {
        return (-1);
    }

    /*
     * Fixup angles
     */
    start = start % 360;
    end = end % 360;

    /*
     * Special case for rad=0 - draw a point
     */
    if (rad == 0) {
        return (pixelColor(dst, x, y, color));
    }

    /*
     * Clip against circle, not pie (not 100% optimal).
     * Get pie's circle and clipping boundary and
     * test if bounding box of circle is visible
     */
    x2 = x + rad;
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    xx = x - rad;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (xx > right) {
        return (0);
    }
    y2 = y + rad;
    top = dst->clip_rect.y;
    if (y2 < top) {
        return (0);
    }
    yy = y - rad;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if (yy > bottom) {
        return (0);
    }

    /*
     * Variable setup
     */
    dr = (double) rad;
    deltaAngle = 3.0 / dr;
    start_angle = (double) start * (2.0 * M_PI / 360.0);
    end_angle = (double) end * (2.0 * M_PI / 360.0);
    if (start > end) {
        end_angle += (2.0 * M_PI);
    }

    /* We will always have at least 2 points */
    numpoints = 2;

    /* Count points (rather than calculating it) */
    angle = start_angle;
    while (angle < end_angle) {
        angle += deltaAngle;
        numpoints++;
    }

    /* Allocate combined vertex array */
    vx = vy = malloc(2 * sizeof(Sint16) * numpoints);
    if (vx == NULL) {
        return (-1);
    }

    /* Update point to start of vy */
    vy += numpoints;

    /* Center */
    vx[0] = x;
    vy[0] = y;

    /* First vertex */
    angle = start_angle;
    vx[1] = x + (int) (dr * cos(angle));
    vy[1] = y + (int) (dr * sin(angle));

    if (numpoints < 3) {
        result = lineColor(dst, vx[0], vy[0], vx[1], vy[1], color);
    } else {
        /* Calculate other vertices */
        i = 2;
        angle = start_angle;
        while (angle < end_angle) {
            angle += deltaAngle;
            if (angle > end_angle) {
                angle = end_angle;
            }
            vx[i] = x + (int) (dr * cos(angle));
            vy[i] = y + (int) (dr * sin(angle));
            i++;
        }

        /* Draw */
        if (filled) {
            result = filledPolygonColor(dst, vx, vy, numpoints, color);
        } else {
            result = polygonColor(dst, vx, vy, numpoints, color);
        }
    }

    /* Free combined vertex array */
    free(vx);

    return (result);
}

/*!
\brief Draw pie (outline) with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the pie.
\param y Y coordinate of the center of the pie.
\param rad Radius in pixels of the pie.
\param start Starting radius in degrees of the pie.
\param end Ending radius in degrees of the pie.
\param color The color value of the pie to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int pieColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad,
        Sint16 start, Sint16 end, Uint32 color)
{
    return (_pieColor(dst, x, y, rad, start, end, color, 0));

}

/*!
\brief Draw pie (outline) with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the pie.
\param y Y coordinate of the center of the pie.
\param rad Radius in pixels of the pie.
\param start Starting radius in degrees of the pie.
\param end Ending radius in degrees of the pie.
\param r The red value of the pie to draw.
\param g The green value of the pie to draw.
\param b The blue value of the pie to draw.
\param a The alpha value of the pie to draw.

\returns Returns 0 on success, -1 on failure.
 */
int pieRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad,
        Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return (_pieColor(dst, x, y, rad, start, end,
            ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a, 0));

}

/*!
\brief Draw filled pie with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the filled pie.
\param y Y coordinate of the center of the filled pie.
\param rad Radius in pixels of the filled pie.
\param start Starting radius in degrees of the filled pie.
\param end Ending radius in degrees of the filled pie.
\param color The color value of the filled pie to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int filledPieColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color)
{
    return (_pieColor(dst, x, y, rad, start, end, color, 1));
}

/*!
\brief Draw filled pie with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the center of the filled pie.
\param y Y coordinate of the center of the filled pie.
\param rad Radius in pixels of the filled pie.
\param start Starting radius in degrees of the filled pie.
\param end Ending radius in degrees of the filled pie.
\param r The red value of the filled pie to draw.
\param g The green value of the filled pie to draw.
\param b The blue value of the filled pie to draw.
\param a The alpha value of the filled pie to draw.

\returns Returns 0 on success, -1 on failure.
 */
int filledPieRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad,
        Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return (_pieColor(dst, x, y, rad, start, end,
            ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a, 1));
}

/* ------ Trigon */

/*!
\brief Draw trigon (triangle outline) with alpha blending.

Note: Creates vertex array and uses polygon routine to render.

\param dst The surface to draw on.
\param x X coordinate of the first point of the trigon.
\param y Y coordinate of the first point of the trigon.
\param x2 X coordinate of the second point of the trigon.
\param y2 Y coordinate of the second point of the trigon.
\param x3 X coordinate of the third point of the trigon.
\param y3 Y coordinate of the third point of the trigon.
\param color The color value of the trigon to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int trigonColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
    Sint16 vx[3];
    Sint16 vy[3];

    vx[0] = x;
    vx[1] = x2;
    vx[2] = x3;
    vy[0] = y;
    vy[1] = y2;
    vy[2] = y3;

    return (polygonColor(dst, vx, vy, 3, color));
}

/*!
\brief Draw trigon (triangle outline) with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the trigon.
\param y Y coordinate of the first point of the trigon.
\param x2 X coordinate of the second point of the trigon.
\param y2 Y coordinate of the second point of the trigon.
\param x3 X coordinate of the third point of the trigon.
\param y3 Y coordinate of the third point of the trigon.
\param r The red value of the trigon to draw.
\param g The green value of the trigon to draw.
\param b The blue value of the trigon to draw.
\param a The alpha value of the trigon to draw.

\returns Returns 0 on success, -1 on failure.
 */
int trigonRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3,
        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Sint16 vx[3];
    Sint16 vy[3];

    vx[0] = x;
    vx[1] = x2;
    vx[2] = x3;
    vy[0] = y;
    vy[1] = y2;
    vy[2] = y3;

    return (polygonRGBA(dst, vx, vy, 3, r, g, b, a));
}

/* ------ AA-Trigon */

/*!
\brief Draw anti-aliased trigon (triangle outline) with alpha blending.

Note: Creates vertex array and uses aapolygon routine to render.

\param dst The surface to draw on.
\param x X coordinate of the first point of the aa-trigon.
\param y Y coordinate of the first point of the aa-trigon.
\param x2 X coordinate of the second point of the aa-trigon.
\param y2 Y coordinate of the second point of the aa-trigon.
\param x3 X coordinate of the third point of the aa-trigon.
\param y3 Y coordinate of the third point of the aa-trigon.
\param color The color value of the aa-trigon to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int aatrigonColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
    Sint16 vx[3];
    Sint16 vy[3];

    vx[0] = x;
    vx[1] = x2;
    vx[2] = x3;
    vy[0] = y;
    vy[1] = y2;
    vy[2] = y3;

    return (aapolygonColor(dst, vx, vy, 3, color));
}

/*!
\brief Draw anti-aliased trigon (triangle outline) with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the aa-trigon.
\param y Y coordinate of the first point of the aa-trigon.
\param x2 X coordinate of the second point of the aa-trigon.
\param y2 Y coordinate of the second point of the aa-trigon.
\param x3 X coordinate of the third point of the aa-trigon.
\param y3 Y coordinate of the third point of the aa-trigon.
\param r The red value of the aa-trigon to draw.
\param g The green value of the aa-trigon to draw.
\param b The blue value of the aa-trigon to draw.
\param a The alpha value of the aa-trigon to draw.

\returns Returns 0 on success, -1 on failure.
 */
int aatrigonRGBA(SDL_Surface * dst,  Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3,
        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Sint16 vx[3];
    Sint16 vy[3];

    vx[0] = x;
    vx[1] = x2;
    vx[2] = x3;
    vy[0] = y;
    vy[1] = y2;
    vy[2] = y3;

    return (aapolygonRGBA(dst, vx, vy, 3, r, g, b, a));
}

/* ------ Filled Trigon */

/*!
\brief Draw filled trigon (triangle) with alpha blending.

Note: Creates vertex array and uses aapolygon routine to render.

\param dst The surface to draw on.
\param x X coordinate of the first point of the filled trigon.
\param y Y coordinate of the first point of the filled trigon.
\param x2 X coordinate of the second point of the filled trigon.
\param y2 Y coordinate of the second point of the filled trigon.
\param x3 X coordinate of the third point of the filled trigon.
\param y3 Y coordinate of the third point of the filled trigon.
\param color The color value of the filled trigon to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int filledTrigonColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
    Sint16 vx[3];
    Sint16 vy[3];

    vx[0] = x;
    vx[1] = x2;
    vx[2] = x3;
    vy[0] = y;
    vy[1] = y2;
    vy[2] = y3;

    return (filledPolygonColor(dst, vx, vy, 3, color));
}

/*!
\brief Draw filled trigon (triangle) with alpha blending.

Note: Creates vertex array and uses aapolygon routine to render.

\param dst The surface to draw on.
\param x X coordinate of the first point of the filled trigon.
\param y Y coordinate of the first point of the filled trigon.
\param x2 X coordinate of the second point of the filled trigon.
\param y2 Y coordinate of the second point of the filled trigon.
\param x3 X coordinate of the third point of the filled trigon.
\param y3 Y coordinate of the third point of the filled trigon.
\param r The red value of the filled trigon to draw.
\param g The green value of the filled trigon to draw.
\param b The blue value of the filled trigon to draw.
\param a The alpha value of the filled trigon to draw.

\returns Returns 0 on success, -1 on failure.
 */
int filledTrigonRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3,
        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Sint16 vx[3];
    Sint16 vy[3];

    vx[0] = x;
    vx[1] = x2;
    vx[2] = x3;
    vy[0] = y;
    vy[1] = y2;
    vy[2] = y3;

    return (filledPolygonRGBA(dst, vx, vy, 3, r, g, b, a));
}

/* ---- Polygon */

/*!
\brief Draw polygon with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the polygon.
\param vy Vertex array containing Y coordinates of the points of the polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the polygon to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int polygonColor(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color)
{
    int result;
    int i;
    const Sint16 *x, *y, *x2, *y2;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Vertex array NULL check
     */
    if (vx == NULL) {
        return (-1);
    }
    if (vy == NULL) {
        return (-1);
    }

    /*
     * Sanity check
     */
    if (n < 3) {
        return (-1);
    }

    /*
     * Pointer setup
     */
    x = x2 = vx;
    y = y2 = vy;
    x2++;
    y2++;

    /*
     * Draw
     */
    result = 0;
    for (i = 1; i < n; i++) {
        result |= lineColor(dst, *x, *y, *x2, *y2, color);
        x = x2;
        y = y2;
        x2++;
        y2++;
    }
    result |= lineColor(dst, *x, *y, *vx, *vy, color);

    return (result);
}

/*!
\brief Draw polygon with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the polygon.
\param vy Vertex array containing Y coordinates of the points of the polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the polygon to draw.
\param g The green value of the polygon to draw.
\param b The blue value of the polygon to draw.
\param a The alpha value of the polygon to draw.

\returns Returns 0 on success, -1 on failure.
 */
int polygonRGBA(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (polygonColor(dst, vx, vy, n, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ---- AA-Polygon */

/*!
\brief Draw anti-aliased polygon with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the aa-polygon.
\param vy Vertex array containing Y coordinates of the points of the aa-polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the aa-polygon to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int aapolygonColor(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color)
{
    int result;
    int i;
    const Sint16 *x, *y, *x2, *y2;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Vertex array NULL check
     */
    if (vx == NULL) {
        return (-1);
    }
    if (vy == NULL) {
        return (-1);
    }

    /*
     * Sanity check
     */
    if (n < 3) {
        return (-1);
    }

    /*
     * Pointer setup
     */
    x = x2 = vx;
    y = y2 = vy;
    x2++;
    y2++;

    /*
     * Draw
     */
    result = 0;
    for (i = 1; i < n; i++) {
        result |= _aalineColor(dst, *x, *y, *x2, *y2, color, 0);
        x = x2;
        y = y2;
        x2++;
        y2++;
    }
    result |= _aalineColor(dst, *x, *y, *vx, *vy, color, 0);

    return (result);
}

/*!
\brief Draw anti-aliased polygon with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the aa-polygon.
\param vy Vertex array containing Y coordinates of the points of the aa-polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the aa-polygon to draw.
\param g The green value of the aa-polygon to draw.
\param b The blue value of the aa-polygon to draw.
\param a The alpha value of the aa-polygon to draw.

\returns Returns 0 on success, -1 on failure.
 */
int aapolygonRGBA(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (aapolygonColor(dst, vx, vy, n, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

/* ---- Filled Polygon */

/*!
\brief Internal helper qsort callback functions used in filled polygon drawing.

\param a The surface to draw on.
\param b Vertex array containing X coordinates of the points of the polygon.

\returns Returns 0 if a==b, a negative number if a<b or a positive number if a>b.
 */
int _gfxPrimitivesCompareInt(const void *a, const void *b)
{
    return (*(const int *) a) - (*(const int *) b);
}

/*!
\brief Global vertex array to use if optional parameters are not given in filledPolygonMT calls.

Note: Used for non-multithreaded (default) operation of filledPolygonMT.
 */
static int *gfxPrimitivesPolyIntsGlobal = NULL;

/*!
\brief Flag indicating if global vertex array was already allocated.

Note: Used for non-multithreaded (default) operation of filledPolygonMT.
 */
static int gfxPrimitivesPolyAllocatedGlobal = 0;

/*!
\brief Draw filled polygon with alpha blending (multi-threaded capable).

Note: The last two parameters are optional; but are required for multithreaded operation.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the filled polygon to draw (0xRRGGBBAA).
\param polyInts Preallocated, temporary vertex array used for sorting vertices. Required for multithreaded operation; set to NULL otherwise.
\param polyAllocated Flag indicating if temporary vertex array was allocated. Required for multithreaded operation; set to NULL otherwise.

\returns Returns 0 on success, -1 on failure.
 */
int filledPolygonColorMT(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color, int **polyInts, int *polyAllocated)
{
    int result;
    int i;
    int y, xa, xb;
    int miny, maxy;
    int xx, yy;
    int x2, y2;
    int ind1, ind2;
    int ints;
    int *gfxPrimitivesPolyInts = NULL;
    int gfxPrimitivesPolyAllocated = 0;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Vertex array NULL check
     */
    if (vx == NULL) {
        return (-1);
    }
    if (vy == NULL) {
        return (-1);
    }

    /*
     * Sanity check number of edges
     */
    if (n < 3) {
        return -1;
    }

    /*
     * Map polygon cache
     */
    if ((polyInts == NULL) || (polyAllocated == NULL)) {
        /* Use global cache */
        gfxPrimitivesPolyInts = gfxPrimitivesPolyIntsGlobal;
        gfxPrimitivesPolyAllocated = gfxPrimitivesPolyAllocatedGlobal;
    } else {
        /* Use local cache */
        gfxPrimitivesPolyInts = *polyInts;
        gfxPrimitivesPolyAllocated = *polyAllocated;
    }

    /*
     * Allocate temp array, only grow array
     */
    if (!gfxPrimitivesPolyAllocated) {
        gfxPrimitivesPolyInts = malloc(sizeof(int) * n);
        gfxPrimitivesPolyAllocated = n;
    } else {
        if (gfxPrimitivesPolyAllocated < n) {
            gfxPrimitivesPolyInts = realloc(gfxPrimitivesPolyInts, sizeof(int) * n);
            gfxPrimitivesPolyAllocated = n;
        }
    }

    /*
     * Check temp array
     */
    if (gfxPrimitivesPolyInts == NULL) {
        gfxPrimitivesPolyAllocated = 0;
    }

    /*
     * Update cache variables
     */
    if ((polyInts == NULL) || (polyAllocated == NULL)) {
        gfxPrimitivesPolyIntsGlobal =  gfxPrimitivesPolyInts;
        gfxPrimitivesPolyAllocatedGlobal = gfxPrimitivesPolyAllocated;
    } else {
        *polyInts = gfxPrimitivesPolyInts;
        *polyAllocated = gfxPrimitivesPolyAllocated;
    }

    /*
     * Check temp array again
     */
    if (gfxPrimitivesPolyInts == NULL) {
        return (-1);
    }

    /*
     * Determine Y maxima
     */
    miny = vy[0];
    maxy = vy[0];
    for (i = 1; (i < n); i++) {
        if (vy[i] < miny) {
            miny = vy[i];
        } else if (vy[i] > maxy) {
            maxy = vy[i];
        }
    }

    /*
     * Draw, scanning y
     */
    result = 0;
    for (y = miny; (y <= maxy); y++) {
        ints = 0;
        for (i = 0; (i < n); i++) {
            if (!i) {
                ind1 = n - 1;
                ind2 = 0;
            } else {
                ind1 = i - 1;
                ind2 = i;
            }
            yy = vy[ind1];
            y2 = vy[ind2];
            if (yy < y2) {
                xx = vx[ind1];
                x2 = vx[ind2];
            } else if (yy > y2) {
                y2 = vy[ind1];
                yy = vy[ind2];
                x2 = vx[ind1];
                xx = vx[ind2];
            } else {
                continue;
            }
            if ( ((y >= yy) && (y < y2)) || ((y == maxy) && (y > yy) && (y <= y2)) ) {
                gfxPrimitivesPolyInts[ints++] = ((65536 * (y - yy)) / (y2 - yy)) * (x2 - xx) + (65536 * xx);
            }
        }

        qsort(gfxPrimitivesPolyInts, ints, sizeof(int), _gfxPrimitivesCompareInt);

        for (i = 0; (i < ints); i += 2) {
            xa = gfxPrimitivesPolyInts[i] + 1;
            xa = (xa >> 16) + ((xa & 32768) >> 15);
            xb = gfxPrimitivesPolyInts[i + 1] - 1;
            xb = (xb >> 16) + ((xb & 32768) >> 15);
            result |= hlineColor(dst, xa, xb, y, color);
        }
    }

    return (result);
}

/*!
\brief Draw filled polygon with alpha blending (multi-threaded capable).

Note: The last two parameters are optional; but are required for multithreaded operation.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the filled polygon to draw.
\param g The green value of the filled polygon to draw.
\param b The blue value of the filed polygon to draw.
\param a The alpha value of the filled polygon to draw.
\param polyInts Preallocated, temporary vertex array used for sorting vertices. Required for multithreaded operation; set to NULL otherwise.
\param polyAllocated Flag indicating if temporary vertex array was allocated. Required for multithreaded operation; set to NULL otherwise.

\returns Returns 0 on success, -1 on failure.
 */
int filledPolygonRGBAMT(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int **polyInts, int *polyAllocated)
{
    /*
     * Draw
     */
    return (filledPolygonColorMT(dst, vx, vy, n, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a, polyInts, polyAllocated));
}

/*!
\brief Draw filled polygon with alpha blending.

Note: Standard filledPolygon function is calling multithreaded version with NULL parameters
to use the global vertex cache.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the filled polygon to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int filledPolygonColor(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color)
{
    /*
     * Draw
     */
    return (filledPolygonColorMT(dst, vx, vy, n, color, NULL, NULL));
}

/*!
\brief Draw filled polygon with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the filled polygon to draw.
\param g The green value of the filled polygon to draw.
\param b The blue value of the filed polygon to draw.
\param a The alpha value of the filled polygon to draw.

\returns Returns 0 on success, -1 on failure.
 */
int filledPolygonRGBA(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (filledPolygonColorMT(dst, vx, vy, n, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a, NULL, NULL));
}

/*!
\brief Internal function to draw a textured horizontal line.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param texture The texture surface to retrieve color information from.
\param texture_dx The X offset for the texture lookup.
\param texture_dy The Y offset for the textured lookup.

\returns Returns 0 on success, -1 on failure.
 */
int _HLineTextured(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, SDL_Surface *texture, int texture_dx, int texture_dy)
{
    Sint16 left, right, top, bottom;
    Sint16 w;
    Sint16 xtmp;
    int result = 0;
    int texture_x_walker;
    int texture_y_start;
    SDL_Rect source_rect, dst_rect;
    int pixels_written, write_width;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Swap x1, x2 if required to ensure x1<=x2
     */
    if (x1 > x2) {
        xtmp = x1;
        x1 = x2;
        x2 = xtmp;
    }

    /*
     * Get clipping boundary and
     * check visibility of hline
     */
    left = dst->clip_rect.x;
    if (x2 < left) {
        return (0);
    }
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    if (x1 > right) {
        return (0);
    }
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    if ((y < top) || (y > bottom)) {
        return (0);
    }

    /*
     * Clip x
     */
    if (x1 < left) {
        x1 = left;
    }
    if (x2 > right) {
        x2 = right;
    }

    /*
     * Calculate width to draw
     */
    w = x2 - x1 + 1;

    /*
     * Determine where in the texture we start drawing
     */
    texture_x_walker =   (x1 - texture_dx)  % texture->w;
    if (texture_x_walker < 0) {
        texture_x_walker = texture->w + texture_x_walker ;
    }

    texture_y_start = (y + texture_dy) % texture->h;
    if (texture_y_start < 0) {
        texture_y_start = texture->h + texture_y_start;
    }

    source_rect.y = texture_y_start;
    source_rect.x = texture_x_walker;
    source_rect.h = 1;

    dst_rect.y = y;

    if (w <= texture->w - texture_x_walker) {
        source_rect.w = w;
        source_rect.x = texture_x_walker;
        dst_rect.x = x1;
        result = (SDL_BlitSurface  (texture, &source_rect , dst, &dst_rect) == 0);
    } else {
        pixels_written = texture->w  - texture_x_walker;
        source_rect.w = pixels_written;
        source_rect.x = texture_x_walker;
        dst_rect.x = x1;
        result |= (SDL_BlitSurface (texture, &source_rect , dst, &dst_rect) == 0);
        write_width = texture->w;

        source_rect.x = 0;
        while (pixels_written < w) {
            if (write_width >= w - pixels_written) {
                write_width =  w - pixels_written;
            }
            source_rect.w = write_width;
            dst_rect.x = x1 + pixels_written;
            result  |= (SDL_BlitSurface  (texture, &source_rect , dst, &dst_rect) == 0);
            pixels_written += write_width;
        }
    }

    return result;
}

/*!
\brief Draws a polygon filled with the given texture (Multi-Threading Capable).

This operation use internally SDL_BlitSurface for lines of the source texture. It supports
alpha drawing.

To get the best performance of this operation you need to make sure the texture and the dst surface have the same format
(see  http://docs.mandragor.org/files/Common_libs_documentation/SDL/SDL_Documentation_project_en/sdlblitsurface.html).
The last two parameters are optional, but required for multithreaded operation. When set to NULL, uses global static temp array.

\param dst the destination surface,
\param vx array of x vector components
\param vy array of x vector components
\param n the amount of vectors in the vx and vy array
\param texture the sdl surface to use to fill the polygon
\param texture_dx the offset of the texture relative to the screeen. if you move the polygon 10 pixels
to the left and want the texture to apear the same you need to increase the texture_dx value
\param texture_dy see texture_dx
\param polyInts preallocated temp array storage for vertex sorting (used for multi-threaded operation)
\param polyAllocated flag indicating oif the temp array was allocated (used for multi-threaded operation)

\returns Returns 0 on success, -1 on failure.
 */
int texturedPolygonMT(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n,
        SDL_Surface * texture, int texture_dx, int texture_dy, int **polyInts, int *polyAllocated)
{
    int result;
    int i;
    int y, xa, xb;
    int minx, maxx, miny, maxy;
    int xx, yy;
    int x2, y2;
    int ind1, ind2;
    int ints;
    int *gfxPrimitivesPolyInts = NULL;
    int gfxPrimitivesPolyAllocated = 0;

    /*
     * Check visibility of clipping rectangle
     */
    if ((dst->clip_rect.w == 0) || (dst->clip_rect.h == 0)) {
        return (0);
    }

    /*
     * Sanity check number of edges
     */
    if (n < 3) {
        return -1;
    }

    /*
     * Map polygon cache
     */
    if ((polyInts == NULL) || (polyAllocated == NULL)) {
        /* Use global cache */
        gfxPrimitivesPolyInts = gfxPrimitivesPolyIntsGlobal;
        gfxPrimitivesPolyAllocated = gfxPrimitivesPolyAllocatedGlobal;
    } else {
        /* Use local cache */
        gfxPrimitivesPolyInts = *polyInts;
        gfxPrimitivesPolyAllocated = *polyAllocated;
    }

    /*
     * Allocate temp array, only grow array
     */
    if (!gfxPrimitivesPolyAllocated) {
        gfxPrimitivesPolyInts = malloc(sizeof(int) * n);
        gfxPrimitivesPolyAllocated = n;
    } else {
        if (gfxPrimitivesPolyAllocated < n) {
            gfxPrimitivesPolyInts = realloc(gfxPrimitivesPolyInts, sizeof(int) * n);
            gfxPrimitivesPolyAllocated = n;
        }
    }

    /*
     * Check temp array
     */
    if (gfxPrimitivesPolyInts == NULL) {
        gfxPrimitivesPolyAllocated = 0;
    }

    /*
     * Update cache variables
     */
    if ((polyInts == NULL) || (polyAllocated == NULL)) {
        gfxPrimitivesPolyIntsGlobal =  gfxPrimitivesPolyInts;
        gfxPrimitivesPolyAllocatedGlobal = gfxPrimitivesPolyAllocated;
    } else {
        *polyInts = gfxPrimitivesPolyInts;
        *polyAllocated = gfxPrimitivesPolyAllocated;
    }

    /*
     * Check temp array again
     */
    if (gfxPrimitivesPolyInts == NULL) {
        return (-1);
    }

    /*
     * Determine X,Y minima,maxima
     */
    miny = vy[0];
    maxy = vy[0];
    minx = vx[0];
    maxx = vx[0];
    for (i = 1; (i < n); i++) {
        if (vy[i] < miny) {
            miny = vy[i];
        } else if (vy[i] > maxy) {
            maxy = vy[i];
        }
        if (vx[i] < minx) {
            minx = vx[i];
        } else if (vx[i] > maxx) {
            maxx = vx[i];
        }
    }
    if (maxx < 0 || minx > dst->w) {
        return -1;
    }
    if (maxy < 0 || miny > dst->h) {
        return -1;
    }

    /*
     * Draw, scanning y
     */
    result = 0;
    for (y = miny; (y <= maxy); y++) {
        ints = 0;
        for (i = 0; (i < n); i++) {
            if (!i) {
                ind1 = n - 1;
                ind2 = 0;
            } else {
                ind1 = i - 1;
                ind2 = i;
            }
            yy = vy[ind1];
            y2 = vy[ind2];
            if (yy < y2) {
                xx = vx[ind1];
                x2 = vx[ind2];
            } else if (yy > y2) {
                y2 = vy[ind1];
                yy = vy[ind2];
                x2 = vx[ind1];
                xx = vx[ind2];
            } else {
                continue;
            }
            if ( ((y >= yy) && (y < y2)) || ((y == maxy) && (y > yy) && (y <= y2)) ) {
                gfxPrimitivesPolyInts[ints++] = ((65536 * (y - yy)) / (y2 - yy)) * (x2 - xx) + (65536 * xx);
            }
        }

        qsort(gfxPrimitivesPolyInts, ints, sizeof(int), _gfxPrimitivesCompareInt);

        for (i = 0; (i < ints); i += 2) {
            xa = gfxPrimitivesPolyInts[i] + 1;
            xa = (xa >> 16) + ((xa & 32768) >> 15);
            xb = gfxPrimitivesPolyInts[i + 1] - 1;
            xb = (xb >> 16) + ((xb & 32768) >> 15);
            result |= _HLineTextured(dst, xa, xb, y, texture, texture_dx, texture_dy);
        }
    }

    return (result);
}

/*!
\brief Draws a polygon filled with the given texture.

This standard version is calling multithreaded versions with NULL cache parameters.

\param dst the destination surface,
\param vx array of x vector components
\param vy array of x vector components
\param n the amount of vectors in the vx and vy array
\param texture the sdl surface to use to fill the polygon
\param texture_dx the offset of the texture relative to the screeen. if you move the polygon 10 pixels
to the left and want the texture to apear the same you need to increase the texture_dx value
\param texture_dy see texture_dx

\returns Returns 0 on success, -1 on failure.
 */
int texturedPolygon(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, SDL_Surface *texture, int texture_dx, int texture_dy)
{
    /*
     * Draw
     */
    return (texturedPolygonMT(dst, vx, vy, n, texture, texture_dx, texture_dy, NULL, NULL));
}

/*!
\brief Internal function to calculate bezier interpolator of data array with ndata values at position 't'.

\param data Array of values.
\param ndata Size of array.
\param t Position for which to calculate interpolated value. t should be between [0, ndata].

\returns Interpolated value at position t, value[0] when t<0, value[n-1] when t>n.
 */
double _evaluateBezier (double *data, int ndata, double t)
{
    double mu, result;
    int n, k, kn, nn, nkn;
    double blend, muk, munk;

    /* Sanity check bounds */
    if (t < 0.0) {
        return (data[0]);
    }
    if (t >= (double) ndata) {
        return (data[ndata - 1]);
    }

    /* Adjust t to the range 0.0 to 1.0 */
    mu = t / (double) ndata;

    /* Calculate interpolate */
    n = ndata - 1;
    result = 0.0;
    muk = 1;
    munk = pow(1 - mu, (double) n);
    for (k = 0; k <= n; k++) {
        nn = n;
        kn = k;
        nkn = n - k;
        blend = muk * munk;
        muk *= mu;
        munk /= (1 - mu);
        while (nn >= 1) {
            blend *= nn;
            nn--;
            if (kn > 1) {
                blend /= (double) kn;
                kn--;
            }
            if (nkn > 1) {
                blend /= (double) nkn;
                nkn--;
            }
        }
        result += data[k] * blend;
    }

    return (result);
}

/*!
\brief Draw a bezier curve with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the bezier curve.
\param vy Vertex array containing Y coordinates of the points of the bezier curve.
\param n Number of points in the vertex array. Minimum number is 3.
\param s Number of steps for the interpolation. Minimum number is 2.
\param color The color value of the bezier curve to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int bezierColor(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, int s, Uint32 color)
{
    int result;
    int i;
    double *x, *y, t, stepsize;
    Sint16 xx, yy, x2, y2;

    /*
     * Sanity check
     */
    if (n < 3) {
        return (-1);
    }
    if (s < 2) {
        return (-1);
    }

    /*
     * Variable setup
     */
    stepsize = (double) 1.0 / (double) s;

    /* Transfer vertices into float arrays */
    if ((x = malloc(sizeof(double)*(n + 1))) == NULL) {
        return (-1);
    }
    if ((y = malloc(sizeof(double)*(n + 1))) == NULL) {
        free(x);
        return (-1);
    }
    for (i = 0; i < n; i++) {
        x[i] = vx[i];
        y[i] = vy[i];
    }
    x[n] = vx[0];
    y[n] = vy[0];

    /*
     * Draw
     */
    result = 0;
    t = 0.0;
    xx = (Sint16) lrint(_evaluateBezier(x, n + 1, t));
    yy = (Sint16) lrint(_evaluateBezier(y, n + 1, t));
    for (i = 0; i <= (n * s); i++) {
        t += stepsize;
        x2 = _evaluateBezier(x, n, t);
        y2 = _evaluateBezier(y, n, t);
        result |= lineColor(dst, xx, yy, x2, y2, color);
        xx = x2;
        yy = y2;
    }

    /* Clean up temporary array */
    free(x);
    free(y);

    return (result);
}

/*!
\brief Draw a bezier curve with alpha blending.

\param dst The surface to draw on.
\param vx Vertex array containing X coordinates of the points of the bezier curve.
\param vy Vertex array containing Y coordinates of the points of the bezier curve.
\param n Number of points in the vertex array. Minimum number is 3.
\param s Number of steps for the interpolation. Minimum number is 2.
\param r The red value of the bezier curve to draw.
\param g The green value of the bezier curve to draw.
\param b The blue value of the bezier curve to draw.
\param a The alpha value of the bezier curve to draw.

\returns Returns 0 on success, -1 on failure.
 */
int bezierRGBA(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, int s, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /*
     * Draw
     */
    return (bezierColor(dst, vx, vy, n, s, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}

static void _murphyParaline(SDL_gfxMurphyIterator *m, Sint16 x, Sint16 y, int d1)
{
    int p;
    d1 = -d1;

    /*
     * Lock the surface
     */
    if (SDL_MUSTLOCK(m->dst)) {
        SDL_LockSurface(m->dst);
    }

    for (p = 0; p <= m->u; p++) {

        pixelColorNolock(m->dst, x, y, m->color);

        if (d1 <= m->kt) {
            if (m->oct2 == 0) {
                x++;
            } else {
                if (m->quad4 == 0) {
                    y++;
                } else {
                    y--;
                }
            }
            d1 += m->kv;
        } else {
            x++;
            if (m->quad4 == 0) {
                y++;
            } else {
                y--;
            }
            d1 += m->kd;
        }
    }

    /* Unlock surface */
    if (SDL_MUSTLOCK(m->dst)) {
        SDL_UnlockSurface(m->dst);
    }

    m->tempx = x;
    m->tempy = y;
}

/*!
\brief Internal function to to draw one iteration of the Murphy algorithm.

\param m Pointer to struct for murphy iterator.
\param miter Iteration count.
\param ml1bx X coordinate of a point.
\param ml1by Y coordinate of a point.
\param ml2bx X coordinate of a point.
\param ml2by Y coordinate of a point.
\param ml1x X coordinate of a point.
\param ml1y Y coordinate of a point.
\param ml2x X coordinate of a point.
\param ml2y Y coordinate of a point.

 */
static void _murphyIteration(SDL_gfxMurphyIterator *m, Uint8 miter,
        Uint16 ml1bx, Uint16 ml1by, Uint16 ml2bx, Uint16 ml2by,
        Uint16 ml1x, Uint16 ml1y, Uint16 ml2x, Uint16 ml2y)
{
    int atemp1, atemp2;
    int ftmp1, ftmp2;
    Uint16 m1x, m1y, m2x, m2y;
    Uint16 fix, fiy, lax, lay, curx, cury;
    Uint16 px[4], py[4];
    SDL_gfxBresenhamIterator b;

    if (miter > 1) {
        if (m->first1x != -32768) {
            fix = (m->first1x + m->first2x) / 2;
            fiy = (m->first1y + m->first2y) / 2;
            lax = (m->last1x + m->last2x) / 2;
            lay = (m->last1y + m->last2y) / 2;
            curx = (ml1x + ml2x) / 2;
            cury = (ml1y + ml2y) / 2;

            atemp1 = (fix - curx);
            atemp2 = (fiy - cury);
            ftmp1 = atemp1 * atemp1 + atemp2 * atemp2;
            atemp1 = (lax - curx);
            atemp2 = (lay - cury);
            ftmp2 = atemp1 * atemp1 + atemp2 * atemp2;

            if (ftmp1 <= ftmp2) {
                m1x = m->first1x;
                m1y = m->first1y;
                m2x = m->first2x;
                m2y = m->first2y;
            } else {
                m1x = m->last1x;
                m1y = m->last1y;
                m2x = m->last2x;
                m2y = m->last2y;
            }

            atemp1 = (m2x - ml2x);
            atemp2 = (m2y - ml2y);
            ftmp1 = atemp1 * atemp1 + atemp2 * atemp2;
            atemp1 = (m2x - ml2bx);
            atemp2 = (m2y - ml2by);
            ftmp2 = atemp1 * atemp1 + atemp2 * atemp2;

            if (ftmp2 >= ftmp1) {
                ftmp1 = ml2bx;
                ftmp2 = ml2by;
                ml2bx = ml2x;
                ml2by = ml2y;
                ml2x = ftmp1;
                ml2y = ftmp2;
                ftmp1 = ml1bx;
                ftmp2 = ml1by;
                ml1bx = ml1x;
                ml1by = ml1y;
                ml1x = ftmp1;
                ml1y = ftmp2;
            }

            /*
             * Lock the surface
             */
            if (SDL_MUSTLOCK(m->dst)) {
                SDL_LockSurface(m->dst);
            }

            _bresenhamInitialize(&b, m2x, m2y, m1x, m1y);
            do {
                pixelColorNolock(m->dst, b.x, b.y, m->color);
            } while (_bresenhamIterate(&b) == 0);

            _bresenhamInitialize(&b, m1x, m1y, ml1bx, ml1by);
            do {
                pixelColorNolock(m->dst, b.x, b.y, m->color);
            } while (_bresenhamIterate(&b) == 0);

            _bresenhamInitialize(&b, ml1bx, ml1by, ml2bx, ml2by);
            do {
                pixelColorNolock(m->dst, b.x, b.y, m->color);
            } while (_bresenhamIterate(&b) == 0);

            _bresenhamInitialize(&b, ml2bx, ml2by, m2x, m2y);
            do {
                pixelColorNolock(m->dst, b.x, b.y, m->color);
            } while (_bresenhamIterate(&b) == 0);

            /* Unlock surface */
            if (SDL_MUSTLOCK(m->dst)) {
                SDL_UnlockSurface(m->dst);
            }

            px[0] = m1x;
            px[1] = m2x;
            px[2] = ml1bx;
            px[3] = ml2bx;
            py[0] = m1y;
            py[1] = m2y;
            py[2] = ml1by;
            py[3] = ml2by;
            polygonColor(m->dst, (const Sint16 *) px, (const Sint16 *) py, 4, m->color);
        }
    }

    m->last1x = ml1x;
    m->last1y = ml1y;
    m->last2x = ml2x;
    m->last2y = ml2y;
    m->first1x = ml1bx;
    m->first1y = ml1by;
    m->first2x = ml2bx;
    m->first2y = ml2by;
}


#define HYPOT(x,y) sqrt((double)(x)*(double)(x)+(double)(y)*(double)(y))

/*!
\brief Internal function to to draw wide lines with Murphy algorithm.

Draws lines parallel to ideal line.

\param m Pointer to struct for murphy iterator.
\param x X coordinate of first point.
\param y Y coordinate of first point.
\param x2 X coordinate of second point.
\param y2 Y coordinate of second point.
\param width Width of line.
\param miter Iteration count.

 */
static void _murphyWideline(SDL_gfxMurphyIterator *m, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 width, Uint8 miter)
{
    float offset = width / 2.f;

    Sint16 temp;
    Sint16 ptx, pty, ml1x, ml1y, ml2x, ml2y, ml1bx, ml1by, ml2bx, ml2by;

    int d0, d1;        /* difference terms d0=perpendicular to line, d1=along line */

    int q;            /* pel counter,q=perpendicular to line */
    int tmp;

    int dd;            /* distance along line */
    int tk;            /* thickness threshold */
    double ang;        /* angle for initial point calculation */
    double sang, cang;

    ml2by = ml2bx = ml1by = ml1bx = ml2y = ml2x = ml1y = ml1x = 0;

    /* Initialisation */
    m->u = x2 - x;    /* delta x */
    m->v = y2 - y;    /* delta y */

    if (m->u < 0) {    /* swap to make sure we are in quadrants 1 or 4 */
        temp = x;
        x = x2;
        x2 = temp;
        temp = y;
        y = y2;
        y = temp;
        m->u *= -1;
        m->v *= -1;
    }

    if (m->v < 0) {    /* swap to 1st quadrant and flag */
        m->v *= -1;
        m->quad4 = 1;
    } else {
        m->quad4 = 0;
    }

    if (m->v > m->u) {    /* swap things if in 2 octant */
        tmp = m->u;
        m->u = m->v;
        m->v = tmp;
        m->oct2 = 1;
    } else {
        m->oct2 = 0;
    }

    m->ku = m->u + m->u;    /* change in l for square shift */
    m->kv = m->v + m->v;    /* change in d for square shift */
    m->kd = m->kv - m->ku;    /* change in d for diagonal shift */
    m->kt = m->u - m->kv;    /* diag/square decision threshold */

    d0 = 0;
    d1 = 0;
    dd = 0;

    ang = atan((double) m->v / (double) m->u);    /* calc new initial point - offset both sides of ideal */
    sang = sin(ang);
    cang = cos(ang);

    if (m->oct2 == 0) {
        ptx = x + (Sint16) lrint(offset * sang);
        if (m->quad4 == 0) {
            pty = y - (Sint16) lrint(offset * cang);
        } else {
            pty = y + (Sint16) lrint(offset * cang);
        }
    } else {
        ptx = x - (Sint16) lrint(offset * cang);
        if (m->quad4 == 0) {
            pty = y + (Sint16) lrint(offset * sang);
        } else {
            pty = y - (Sint16) lrint(offset * sang);
        }
    }

    /* used here for constant thickness line */
    tk = (int) (4. * HYPOT(ptx - x, pty - y) * HYPOT(m->u, m->v));

    if (miter == 0) {
        m->first1x = -32768;
        m->first1y = -32768;
        m->first2x = -32768;
        m->first2y = -32768;
        m->last1x = -32768;
        m->last1y = -32768;
        m->last2x = -32768;
        m->last2y = -32768;
    }

    for (q = 0; dd <= tk; q++) {    /* outer loop, stepping perpendicular to line */

        _murphyParaline(m, ptx, pty, d1);    /* call to inner loop - right edge */
        if (q == 0) {
            ml1x = ptx;
            ml1y = pty;
            ml1bx = m->tempx;
            ml1by = m->tempy;
        } else {
            ml2x = ptx;
            ml2y = pty;
            ml2bx = m->tempx;
            ml2by = m->tempy;
        }
        if (d0 < m->kt) {    /* square move */
            if (m->oct2 == 0) {
                if (m->quad4 == 0) {
                    pty++;
                } else {
                    pty--;
                }
            } else {
                ptx++;
            }
        } else {    /* diagonal move */
            dd += m->kv;
            d0 -= m->ku;
            if (d1 < m->kt) {    /* normal diagonal */
                if (m->oct2 == 0) {
                    ptx--;
                    if (m->quad4 == 0) {
                        pty++;
                    } else {
                        pty--;
                    }
                } else {
                    ptx++;
                    if (m->quad4 == 0) {
                        pty--;
                    } else {
                        pty++;
                    }
                }
                d1 += m->kv;
            } else {    /* double square move, extra parallel line */
                if (m->oct2 == 0) {
                    ptx--;
                } else {
                    if (m->quad4 == 0) {
                        pty--;
                    } else {
                        pty++;
                    }
                }
                d1 += m->kd;
                if (dd > tk) {
                    _murphyIteration(m, miter, ml1bx, ml1by, ml2bx, ml2by, ml1x, ml1y, ml2x, ml2y);
                    return;    /* breakout on the extra line */
                }
                _murphyParaline(m, ptx, pty, d1);
                if (m->oct2 == 0) {
                    if (m->quad4 == 0) {
                        pty++;
                    } else {

                        pty--;
                    }
                } else {
                    ptx++;
                }
            }
        }
        dd += m->ku;
        d0 += m->kv;
    }

    _murphyIteration(m, miter, ml1bx, ml1by, ml2bx, ml2by, ml1x, ml1y, ml2x, ml2y);
}

/*!
\brief Draw a thick line with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the line.
\param y Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param width Width of the line in pixels. Must be >0.
\param color The color value of the line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
 */
int thickLineColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 width, Uint32 color)
{
    SDL_gfxMurphyIterator m;

    if (dst == NULL) {
        return -1;
    }
    if (width < 1) {
        return -1;
    }

    m.dst = dst;
    m.color = color;

    _murphyWideline(&m, x, y, x2, y2, width, 0);
    _murphyWideline(&m, x, y, x2, y2, width, 1);

    return (0);
}

/*!
\brief Draw a thick line with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the first point of the line.
\param y Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param width Width of the line in pixels. Must be >0.
\param r The red value of the character to draw.
\param g The green value of the character to draw.
\param b The blue value of the character to draw.
\param a The alpha value of the character to draw.

\returns Returns 0 on success, -1 on failure.
 */
int thickLineRGBA(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 width, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return (thickLineColor(dst, x, y, x2, y2, width,
            ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a));
}
