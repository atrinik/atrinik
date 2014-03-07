/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
typedef struct font_struct
{
    /** The font's path. */
    const char *path;

    /** Size of the font. */
    size_t size;

    /** The actual font used by SDL_ttf. */
    TTF_Font *font;

    /** Maximum line height. */
    int height;
} font_struct;

/**
 * @anchor FONT_xxx
 * The font IDs. */
enum
{
    /** Sans, 7px. */
    FONT_SANS7,
    /** Sans, 8px. */
    FONT_SANS8,
    /** Sans, 9px. */
    FONT_SANS9,
    /** Sans, 10px. */
    FONT_SANS10,
    /** Sans, 11px. */
    FONT_SANS11,
    /** Sans, 12px. */
    FONT_SANS12,
    /** Sans, 13px. */
    FONT_SANS13,
    /** Sans, 14px. */
    FONT_SANS14,
    /** Sans, 15px. */
    FONT_SANS15,
    /** Sans, 16px. */
    FONT_SANS16,
    /** Sans, 18px. */
    FONT_SANS18,
    /** Sans, 20px. */
    FONT_SANS20,
    /** Serif, 8px. */
    FONT_SERIF8,
    /** Serif, 10px. */
    FONT_SERIF10,
    /** Serif, 12px. */
    FONT_SERIF12,
    /** Serif, 14px. */
    FONT_SERIF14,
    /** Serif, 16px. */
    FONT_SERIF16,
    /** Serif, 18px. */
    FONT_SERIF18,
    /** Serif, 20px. */
    FONT_SERIF20,
    /** Serif, 22px. */
    FONT_SERIF22,
    /** Serif, 24px. */
    FONT_SERIF24,
    /** Serif, 26px. */
    FONT_SERIF26,
    /** Serif, 28px. */
    FONT_SERIF28,
    /** Serif, 30px. */
    FONT_SERIF30,
    /** Serif, 32px. */
    FONT_SERIF32,
    /** Serif, 34px. */
    FONT_SERIF34,
    /** Serif, 36px. */
    FONT_SERIF36,
    /** Serif, 38px. */
    FONT_SERIF38,
    /** Serif, 40px. */
    FONT_SERIF40,
    /** Mono, 8px. */
    FONT_MONO8,
    /** Mono, 9px. */
    FONT_MONO9,
    /** Mono, 10px. */
    FONT_MONO10,
    /** Mono, 12px. */
    FONT_MONO12,
    /** Mono, 14px. */
    FONT_MONO14,
    /** Mono, 16px. */
    FONT_MONO16,
    /** Mono, 18px. */
    FONT_MONO18,
    /** Mono, 20px. */
    FONT_MONO20,
    /** Arial, 8px. */
    FONT_ARIAL8,
    /** Arial, 10px, good for general drawing (looks the same across systems).
     * */
    FONT_ARIAL10,
    /** Arial, 11px. */
    FONT_ARIAL11,
    /** Arial, 12px. */
    FONT_ARIAL12,
    /** Arial, 13px. */
    FONT_ARIAL13,
    /** Arial, 14px. */
    FONT_ARIAL14,
    /** Arial, 15px. */
    FONT_ARIAL15,
    /** Arial, 16px. */
    FONT_ARIAL16,
    /** Arial, 18px. */
    FONT_ARIAL18,
    /** Arial, 20px. */
    FONT_ARIAL20,
    /** Logisoso, 8px. */
    FONT_LOGISOSO8,
    /** Logisoso, 10px. */
    FONT_LOGISOSO10,
    /** Logisoso, 12px. */
    FONT_LOGISOSO12,
    /** Logisoso, 14px. */
    FONT_LOGISOSO14,
    /** Logisoso, 16px. */
    FONT_LOGISOSO16,
    /** Logisoso, 18px. */
    FONT_LOGISOSO18,
    /** Logisoso, 20px. */
    FONT_LOGISOSO20,
    /** Fanwood, 8px. */
    FONT_FANWOOD8,
    /** Fanwood, 10px. */
    FONT_FANWOOD10,
    /** Fanwood, 12px. */
    FONT_FANWOOD12,
    /** Fanwood, 14px. */
    FONT_FANWOOD14,
    /** Fanwood, 16px. */
    FONT_FANWOOD16,
    /** Fanwood, 18px. */
    FONT_FANWOOD18,
    /** Fanwood, 20px. */
    FONT_FANWOOD20,
    /** Courier, 8px. */
    FONT_COURIER8,
    /** Courier, 10px. */
    FONT_COURIER10,
    /** Courier, 12px. */
    FONT_COURIER12,
    /** Courier, 14px. */
    FONT_COURIER14,
    /** Courier, 16px. */
    FONT_COURIER16,
    /** Courier, 18px. */
    FONT_COURIER18,
    /** Courier, 20px. */
    FONT_COURIER20,
    /** Pecita, 8px. */
    FONT_PECITA8,
    /** Pecita, 10px. */
    FONT_PECITA10,
    /** Pecita, 12px. */
    FONT_PECITA12,
    /** Pecita, 14px. */
    FONT_PECITA14,
    /** Pecita, 16px. */
    FONT_PECITA16,
    /** Pecita, 18px. */
    FONT_PECITA18,
    /** Pecita, 20px. */
    FONT_PECITA20,

    /** Number of the fonts. */
    FONTS_MAX
};

/**
 * Structure that holds information about markup and other things when
 * drawing. */
typedef struct text_info_struct
{
    /** Anchor tag position. */
    const char *anchor_tag;

    /** Action anchor tag should execute. */
    char anchor_action[HUGE_BUF];

    /** Outline tag color. */
    SDL_Color outline_color;

    /** Whether to show an outline. */
    uint8 outline_show;

    /** Whether we are in old-style book title. */
    uint8 in_book_title;

    /** Alpha. */
    uint8 used_alpha;

    /** Whether we are in bold tag. */
    uint8 in_bold;

    /** Whether we are in italic tag. */
    uint8 in_italic;

    /** Whether we are in underline tag. */
    uint8 in_underline;

    /** Whether we are in strikethrough tag. */
    uint8 in_strikethrough;

    /**
     *  If 1, the character is not being drawn due to line skip (due to
     * scroll value for example). */
    uint8 obscured;

    /** Whether bold width is being calculated. */
    uint8 calc_bold;

    /**
     * Whether font width (font changed using a tag) is being
     * calculated. */
    int calc_font;

    /**
     * Used for calculations by the 'hcenter' tag. */
    int hcenter_y;

    int height;

    int start_x;

    int start_y;

    uint8 highlight;

    uint8 flip;

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
#define FONT_HEIGHT(font) (fonts[(font)].height)

#define FONT_TRY_INFO(_font, _info, _surface) ((_info).calc_font != -1 && !(_surface) && !(_info).obscured ? (_info).calc_font : (_font))

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
