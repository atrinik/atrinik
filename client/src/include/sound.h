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
 * Sound related header file. */

#ifndef SOUND_H
#define SOUND_H

/**
 * @defgroup SOUND_TYPE_xxx Sound mixer type
 * Sound mixer types.
 *@{*/
/** Sound chunk, OGG/WAV, no MIDI. */
#define SOUND_TYPE_CHUNK 1
/** Music, OGG/MIDI/etc. */
#define SOUND_TYPE_MUSIC 2
/*@}*/

/**
 * One 'cached' sound. */
typedef struct sound_data_struct {
    /** The sound's data. */
    void *data;

    /** Sound's type, one of @ref SOUND_TYPE_xxx. */
    int type;

    /** Filename that was used to load sound_data_struct::data from. */
    char *filename;

    /** Hash handle. */
    UT_hash_handle hh;
} sound_data_struct;

#define POW2(x) ((x) * (x))

/** This value is defined in server too - change only both at once */
#define MAX_SOUND_DISTANCE 12

/**
 * One ambient sound effect. */
typedef struct sound_ambient_struct {
    /** Next ambient sound effect in a doubly-linked list. */
    struct sound_ambient_struct *next;

    /** Previous ambient sound effect in a doubly-linked list. */
    struct sound_ambient_struct *prev;

    /** ID of the object the sound is coming from. */
    int tag;

    /** Channel ID we are playing the sound effect on. */
    int channel;

    /** X position of the sound effect object on the client map. */
    int x;

    /** Y position of the sound effect object on the client map. */
    int y;

    /** Maximum range. */
    uint8_t max_range;
} sound_ambient_struct;

#endif
