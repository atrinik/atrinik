/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Dialog header file. */

#ifndef DIALOG_H
#define DIALOG_H

/** Option window max tabs */
#define OPTWIN_MAX_TAB 20

/** Option windows max options */
#define OPTWIN_MAX_OPT 26

/** Option window max keys */
#define OPTWIN_MAX_KEYS 100

/** Maximum length in pixels for a single server description line. */
#define MAX_MS_DESC_LINE 298

/** Maximum Y position the server description can go to. */
#define MAX_MS_DESC_Y 36

/** Option structure. */
typedef struct _option
{
	/** Option name. */
	char *name;

	/** Info text. */
	char *info;

	/** Text replacement for number values. */
	char *val_text;

	/** Select type. */
	int sel_type;

	/** Ranges. */
	int minRange, maxRange, deltaRange;

	/** Default value. */
	int default_val;

	/** Value. */
	void *value;

	/** Value type. */
	int value_type;
} _option;

extern _option opt[];

/** Value types for _option::value_type. */
extern enum
{
	/** True/false. */
	VAL_BOOL,
	/**
	 * Text.
	 * @todo Implement. */
	VAL_TEXT,
	/** Uint8. */
	VAL_CHAR,
	/** Integer. */
	VAL_INT,
	/** Uint32. */
	VAL_U32
} value_type;

#define TXT_START_NAME  136
#define TXT_Y_START      82

extern int active_button;
extern int dialog_yoff;

extern const char *opt_tab[];
extern int dialog_new_char_warn;

#endif
