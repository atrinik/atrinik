/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Sound related header file.
 */

#ifndef SOUND_H
#define SOUND_H

/**
 * One 'cached' sound.
 */
typedef struct sound_data_struct {
    /** The sound's data. */
#ifdef HAVE_SDL_MIXER
    MIX_Audio *data;
#else
    void *data;
#endif

    /** Filename that was used to load sound_data_struct::data from. */
    char *filename;

    /** Hash handle. */
    UT_hash_handle hh;
} sound_data_struct;

#define POW2(x) ((x) * (x))

/** This value is defined in server too - change only both at once */
#define MAX_SOUND_DISTANCE 12

/**
 * One ambient sound effect.
 */
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

/** Public API implemented in src/client/sound.c. */

extern void sound_background_hook_register(void *ptr);

extern void sound_init(void);

extern void sound_deinit(void);

extern void sound_music_finished_handle(void);

extern void sound_clear_cache(void);

extern void sound_play_effect(const char *filename, int volume);

extern int sound_play_effect_loop(const char *filename, int volume, int loop);

extern void sound_stop_effect(int channel);

extern void sound_start_bg_music(const char *filename, int volume, int loop);

extern void sound_stop_bg_music(void);

extern void sound_pause_music(void);

extern void sound_resume_music(void);

extern void update_map_bg_music(const char *bg_music);

extern void sound_update_volume(void);

extern const char *sound_get_bg_music(void);

extern const char *sound_get_bg_music_basename(void);

extern uint8_t sound_map_background(int val);

extern uint32_t sound_music_get_offset(void);

extern int sound_music_can_seek(void);

extern void sound_music_seek(uint32_t offset);

extern uint32_t sound_music_get_duration(void);

extern void socket_command_sound(uint8_t *data, size_t len, size_t pos);

extern void sound_ambient_mapcroll(int xoff, int yoff);

extern void sound_ambient_clear(void);

extern void socket_command_sound_ambient(uint8_t *data, size_t len, size_t pos);

extern int sound_playing_music(void);

#endif
