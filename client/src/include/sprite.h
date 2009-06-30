/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

#if !defined(__SPRITE_H)
#define __SPRITE_H

/* Our blt and sprite structure */

/* Status of this bitmap/sprite */
typedef enum _sprite_status {
	SPRITE_STATUS_UNLOADED,
	SPRITE_STATUS_LOADED
} _sprite_status;

/* Some other informations */
typedef enum _sprite_type {
	SPRITE_TYPE_NORMAL
} _sprite_type;

/* Use special values from BLTFX structures */
#define BLTFX_FLAG_NORMAL 	0
#define BLTFX_FLAG_DARK 	1
#define BLTFX_FLAG_SRCALPHA 2
#define BLTFX_FLAG_FOW 		4
#define BLTFX_FLAG_RED 		8
#define BLTFX_FLAG_GREY 	16

/** Here we can change default blt options or set special options */
typedef struct _BLTFX {
	/** Used from BLTFX_FLAG_xxxx */
    UINT32 flags;

	/** If != null, overrule default screen */
	SDL_Surface *surface;

	/** Use dark_level[i] surface */
    int dark_level;

	/** Alpha value */
    uint8 alpha;
}_BLTFX;

/** Sprite structure */
typedef struct _Sprite {
	/** Sprite status */
	_sprite_status status;

	/** Sprite type */
	_sprite_type type;

	/** Rows of blank pixels before first color information */
    int border_up;

	/** Border down */
    int border_down;

	/** Border left */
    int border_left;

	/** Border right */
    int border_right;

    /* We store our faces 7 times...
     * Perhaps we will run in memory problems when we boost the arch set.
     * ATM, we have around 15-25mb when we loaded ALL arches (what perhaps
     * never will happens in a single game
     * Later perhaps a smarter system, using the palettes and switch... */

	/** That's our native, unchanged bitmap */
    SDL_Surface *bitmap;

	/** Red (infravision) */
    SDL_Surface *red;

	/** Grey (xray) */
    SDL_Surface *grey;

	/** That's the fog of war palette */
    SDL_Surface *fog_of_war;

	/** Dark levels.
	 * Note: 0 = default sprite - it's only mapped */
    SDL_Surface *dark_level[DARK_LEVELS];
} _Sprite;

typedef struct _Font {
	/** Don't free this, we link here a Bitmaps[x] pointer */
	_Sprite *sprite;

	/** Space in pixel between 2 chars in a word */
	int char_offset;

	/** Character */
	SDL_Rect c[256];
}_Font;

#define ANIM_DAMAGE 	1
#define ANIM_KILL   	2

typedef struct _anim {
	/* Pointer to next anim in que */
    struct _anim *next;

	/* Pointer to anim before */
    struct _anim *before;

    int type;

	/* The time we started this anim */
    uint32 start_tick;

	/* This is the end-tick */
    uint32 last_tick;

	/* This is the number to display */
    int value;

	/* Where we are X */
    int x;

	/* Where we are Y */
    int y;

	/* Movement in X per tick */
    int xoff;

	/* Movement in y per tick */
    float yoff;

	/* Map position X */
    int mapx;

	/* Map position Y */
    int mapy;
}_anim;

/** ASCII code for UP character */
#define ASCII_UP 28
/** ASCII code for DOWN character */
#define ASCII_DOWN 29
/** ASCII code for LEFT character */
#define ASCII_LEFT 30
/** ASCII code for RIGHT character*/
#define ASCII_RIGHT 31

/* Anim queue of current active map */
extern struct _anim *start_anim;

extern struct _anim *add_anim(int type, int mapx, int mapy, int value);
extern void remove_anim(struct _anim *anim);
extern void play_anims();
extern void delete_anim_que();
extern void show_tooltip(int mx, int my, char* text);
extern int StringWidth(_Font *font, char *text);
extern int StringWidthOffset(_Font *font, char *text, int *line, int len);

extern _Sprite *sprite_load_file(char *fname, UINT32 flags);
extern _Sprite *sprite_tryload_file(char *fname, UINT32 flags,SDL_RWops *rwob);
extern void sprite_free_sprite(_Sprite *sprite);
extern int get_string_pixel_length(char *text, struct _Font *font);
extern void sprite_blt(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx);
extern void sprite_blt_map(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx);

extern Uint32 GetSurfacePixel(SDL_Surface *Surface, Sint32 X, Sint32 Y);
extern void CreateNewFont(_Sprite *sprite, _Font *font, int xlen, int ylen, int c32len);
extern void StringBlt(SDL_Surface *surf, _Font *font, char *text, int x, int y,int col, SDL_Rect *area, _BLTFX *bltfx);
extern int sprite_collision(int x1,int y1,int x2,int y2,_Sprite *sprite1, _Sprite *sprite2);

#endif
