/* Coverity Scan model
 *
 * This is a modeling file for Coverity Scan. Modeling helps to avoid false
 * positives.
 *
 * - A model file can't import any header files.
 * - Therefore only some built-in primitives like int, char and void are
 *   available but not wchar_t, NULL etc.
 * - Modeling doesn't need full structs and typedefs. Rudimentary structs
 *   and similar types are sufficient.
 * - An uninitialized local pointer is not an error. It signifies that the
 *   variable could be either NULL or have some data.
 *
 * Coverity Scan doesn't pick up modifications automatically. The model file
 * must be uploaded by an admin in the analysis settings of
 * https://scan.coverity.com/projects/2179
 */

#define NULL (void *) 0

typedef struct {} SDL_Surface;
typedef struct {} SDL_PixelFormat;
typedef unsigned int Uint32;

void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free (void *);

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height,
        int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{

    SDL_Surface *ptr;

    __coverity_negative_sink__(width);
    __coverity_negative_sink__(height);
    __coverity_negative_sink__(depth);

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_FreeSurface");
    return ptr;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height,
        int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask,
        Uint32 Amask)
{
    SDL_Surface *ptr;

    __coverity_negative_sink__(width);
    __coverity_negative_sink__(height);
    __coverity_negative_sink__(depth);
    __coverity_negative_sink__(pitch);

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_FreeSurface");
    return ptr;
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt,
        Uint32 flags)
{
    SDL_Surface *ptr;

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_FreeSurface");
    return ptr;
}

SDL_Surface *SDL_DisplayFormat(SDL_Surface *surface)
{
    SDL_Surface *ptr;

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_FreeSurface");
    return ptr;
}

SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *surface)
{
    SDL_Surface *ptr;

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_FreeSurface");
    return ptr;
}

void SDL_FreeSurface(SDL_Surface *surface)
{
    __coverity_free__(surface);
    __coverity_mark_as_afm_freed__(surface, "SDL_FreeSurface");
}
