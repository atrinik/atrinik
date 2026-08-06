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

#define NULL (void *)0

typedef struct {
} SDL_Surface;
typedef unsigned int Uint32;
typedef Uint32 SDL_PixelFormat;
typedef int SDL_ScaleMode;

void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);

SDL_Surface *SDL_CreateSurface(int width, int height, SDL_PixelFormat format) {

    SDL_Surface *ptr;

    __coverity_negative_sink__(width);
    __coverity_negative_sink__(height);
    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_DestroySurface");
    return ptr;
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat format) {
    SDL_Surface *ptr;

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_DestroySurface");
    return ptr;
}

SDL_Surface *SDL_ScaleSurface(SDL_Surface *src, int width, int height, SDL_ScaleMode scaleMode) {
    SDL_Surface *ptr;

    __coverity_negative_sink__(width);
    __coverity_negative_sink__(height);

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_DestroySurface");
    return ptr;
}

SDL_Surface *SDL_RotateSurface(SDL_Surface *src, float angle) {
    SDL_Surface *ptr;

    ptr = __coverity_alloc__(sizeof(SDL_Surface));
    __coverity_writeall__(ptr);
    __coverity_mark_as_afm_allocated__(ptr, "SDL_DestroySurface");
    return ptr;
}

void SDL_DestroySurface(SDL_Surface *surface) {
    __coverity_free__(surface);
    __coverity_mark_as_afm_freed__(surface, "SDL_DestroySurface");
}
