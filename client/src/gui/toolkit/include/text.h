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
 * Header file for text drawing API. */

#ifndef TEXT_H
#define TEXT_H

/** One font. */
typedef struct font_struct {
    /** Key of the font (name@size) */
    char *key;

    /** Name of the font. */
    char *name;

    /** Size of the font. */
    uint8_t size;

    /** The actual font used by SDL_ttf. */
    TTF_Font *font;

    /** Maximum line height. */
    int height;

    /** Number of references. */
    unsigned int ref;

    /** When the font was last used. */
    time_t last_used;

    /** UT hash handle. */
    UT_hash_handle hh;
} font_struct;

/**
 * Shortcut macro for getting a weak reference to the specified font.
 */
#define FONT(font_name, font_size) font_get_weak(font_name, font_size)
/**
 * Increase reference count of the specified font.
 */
#define FONT_INCREF(font) (font)->ref++;
/**
 * Decrease reference count of the specified font.
 * @warning Only use this if you know *exactly* what you're doing; almost always
 * you should use font_free() instead.
 */
#define FONT_DECREF(font) (font)->ref--;

/**
 * Maximum amount of time the font_gc() function can spend attempting to free
 * fonts.
 */
#define FONT_GC_MAX_TIME 100000
/**
 * The font_gc() routine will execute once in X number of times.
 */
#define FONT_GC_CHANCE 500
/**
 * Number of seconds that must pass after the last registered usage of a font
 * before it's garbage-collected.
 */
#define FONT_GC_FREE_TIME 60 * 30

#define FONT_SANS7 FONT("sans", 7)
#define FONT_SANS8 FONT("sans", 8)
#define FONT_SANS9 FONT("sans", 9)
#define FONT_SANS10 FONT("sans", 10)
#define FONT_SANS11 FONT("sans", 11)
#define FONT_SANS12 FONT("sans", 12)
#define FONT_SANS13 FONT("sans", 13)
#define FONT_SANS14 FONT("sans", 14)
#define FONT_SANS15 FONT("sans", 15)
#define FONT_SANS16 FONT("sans", 16)
#define FONT_SANS18 FONT("sans", 18)
#define FONT_SANS20 FONT("sans", 20)

#define FONT_SERIF8 FONT("serif", 8)
#define FONT_SERIF10 FONT("serif", 10)
#define FONT_SERIF12 FONT("serif", 12)
#define FONT_SERIF14 FONT("serif", 14)
#define FONT_SERIF16 FONT("serif", 16)
#define FONT_SERIF18 FONT("serif", 18)
#define FONT_SERIF20 FONT("serif", 20)
#define FONT_SERIF22 FONT("serif", 22)
#define FONT_SERIF24 FONT("serif", 24)
#define FONT_SERIF26 FONT("serif", 26)
#define FONT_SERIF28 FONT("serif", 28)
#define FONT_SERIF30 FONT("serif", 30)
#define FONT_SERIF32 FONT("serif", 32)
#define FONT_SERIF34 FONT("serif", 34)
#define FONT_SERIF36 FONT("serif", 36)
#define FONT_SERIF38 FONT("serif", 38)
#define FONT_SERIF40 FONT("serif", 40)

#define FONT_MONO8 FONT("mono", 8)
#define FONT_MONO9 FONT("mono", 9)
#define FONT_MONO10 FONT("mono", 10)
#define FONT_MONO12 FONT("mono", 12)
#define FONT_MONO14 FONT("mono", 14)
#define FONT_MONO16 FONT("mono", 16)
#define FONT_MONO18 FONT("mono", 18)
#define FONT_MONO20 FONT("mono", 20)

#define FONT_ARIAL8 FONT("arial", 8)
#define FONT_ARIAL10 FONT("arial", 10)
#define FONT_ARIAL11 FONT("arial", 11)
#define FONT_ARIAL12 FONT("arial", 12)
#define FONT_ARIAL13 FONT("arial", 13)
#define FONT_ARIAL14 FONT("arial", 14)
#define FONT_ARIAL15 FONT("arial", 15)
#define FONT_ARIAL16 FONT("arial", 16)
#define FONT_ARIAL18 FONT("arial", 18)
#define FONT_ARIAL20 FONT("arial", 20)

/**
 * Structure that holds information about markup and other things when
 * drawing. */
typedef struct text_info_struct {
    /** Anchor tag position. */
    const char *anchor_tag;

    /** Action anchor tag should execute. */
    char anchor_action[HUGE_BUF];

    /** Outline tag color. */
    SDL_Color outline_color;

    /** Whether to show an outline. */
    uint8_t outline_show;

    /** Whether we are in old-style book title. */
    uint8_t in_book_title;

    /** Alpha. */
    uint8_t used_alpha;

    /** Whether we are in bold tag. */
    uint8_t in_bold;

    /** Whether we are in italic tag. */
    uint8_t in_italic;

    /** Whether we are in underline tag. */
    uint8_t in_underline;

    /** Whether we are in strikethrough tag. */
    uint8_t in_strikethrough;

    /**
     *  If 1, the character is not being drawn due to line skip (due to
     * scroll value for example). */
    uint8_t obscured;

    /** Whether bold width is being calculated. */
    uint8_t calc_bold;

    /**
     * Whether font width (font changed using a tag) is being
     * calculated. */
    font_struct *calc_font;

    /**
     * Used for calculations by the 'hcenter' tag. */
    int hcenter_y;

    int height;

    int start_x;

    int start_y;

    uint8_t highlight;

    uint8_t flip;

    SDL_Rect highlight_rect;

    SDL_Color highlight_color;

    char tooltip_text[MAX_BUF * 2];
} text_info_struct;

/**
 * @defgroup TEXT_xxx Text flags
 * Various text flags for controlling behavior of text_show().
 *@{*/
/** Parse markup, otherwise it will be rendered as normal text. */
#define TEXT_MARKUP 1
/** Wrap words, otherwise only newlines. */
#define TEXT_WORD_WRAP 2
/** Render the text in solid mode (faster, but worse looking). */
#define TEXT_SOLID 4
/** Align the text to center of box's width. */
#define TEXT_ALIGN_CENTER 8
/**
 * Instead of quitting drawing when maximum height passed was reached,
 * continue going on, but without doing any more drawing, and store the
 * final height in box->h (where the initial height limit came from).
 *
 * If this flag is passed, you can also specify skipping # of rows in
 * the box's 'y'. Even if you don't want to skip any rows, you must still
 * initialize the y member of the box structure to 0, in order to avoid
 * uninitialized reads. */
#define TEXT_HEIGHT 16
/**
 * Vertically center the text to that of the passed box's height. Note
 * that this will NOT take font tag changing into account. */
#define TEXT_VALIGN_CENTER 32
/**
 * Do not allow color changing using markup. */
#define TEXT_NO_COLOR_CHANGE 64
/**
 * Show a black outline around the text (can be changed to different
 * color using \<o\> markup. */
#define TEXT_OUTLINE 128
/**
 * Store number of lines in box->h. */
#define TEXT_LINES_CALC 256
/**
 * Skip first box->y lines. */
#define TEXT_LINES_SKIP 512
/**
 * Do not allow font changing using markup. */
#define TEXT_NO_FONT_CHANGE 1024
/**
 * Like @ref TEXT_WORD_WRAP, but will stop drawing when the characters
 * width would be more than box->w. */
#define TEXT_WIDTH 2048
/**
 * Calculate maximum width of the text, taking multi-line text into
 * consideration. */
#define TEXT_MAX_WIDTH 4096
/*@}*/

#define TEXT_FLIP_HORIZONTAL 1
#define TEXT_FLIP_VERTICAL 2
#define TEXT_FLIP_BOTH (TEXT_FLIP_HORIZONTAL | TEXT_FLIP_VERTICAL)

/**
 * @defgroup COLOR_xxx Color HTML notations
 * HTML notations of various common collors.
 *@{*/
/** White. */
#define COLOR_WHITE "ffffff"
/** Orange. */
#define COLOR_ORANGE "ff9900"
/** Navy (most used for NPC messages). */
#define COLOR_NAVY "00ffff"
/** Red. */
#define COLOR_RED "ff3030"
/** Green. */
#define COLOR_GREEN "00ff00"
/** Blue. */
#define COLOR_BLUE "0080ff"
/** Gray. */
#define COLOR_GRAY "999999"
/** Brown. */
#define COLOR_BROWN "c07f40"
/** Purple. */
#define COLOR_PURPLE "cc66ff"
/** Pink. */
#define COLOR_PINK "ff9999"
/** Yellow. */
#define COLOR_YELLOW "ffff33"
/** Dark navy. */
#define COLOR_DK_NAVY "00c4c2"
/** Dark green. */
#define COLOR_DK_GREEN "006600"
/** Dark orange. */
#define COLOR_DK_ORANGE "ff6600"
/** Bright purple. */
#define COLOR_BRIGHT_PURPLE "ff66ff"
/** Gold. */
#define COLOR_HGOLD "d4d553"
/** Dark gold. */
#define COLOR_DGOLD "999900"
/** Black. */
#define COLOR_BLACK "000000"
/*@}*/

/** Get font's maximum height. */
#define FONT_HEIGHT(font) ((font)->height)

#define FONT_TRY_INFO(_font, _info, _surface) ((_info).calc_font != NULL && !(_surface) && !(_info).obscured ? (_info).calc_font : (_font))

/**
 * Anchor handler function to try and execute before the defaults.
 * @param anchor_action The action to execute, which can be empty (but
 * not NULL) - for example, 'help'.
 * @param buf Text to pass to the action decided by the anchor action; in
 * case of links, the URL to open, for example. Will not contain any
 * markup, and should not be freed.
 * @param len Length of 'buf'.
 * @param custom_data User-supplied data. Can be NULL.
 * @return 1 if handled the action and should not handle it using default
 * actions, 0 otherwise. */
typedef int (*text_anchor_handle_func)(const char *anchor_action, const char *buf, size_t len, void *custom_data);

#endif
