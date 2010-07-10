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
 * Sound related functions. */

#include <include.h>

/** Path to the background music file being played. */
static char *sound_background;
/** Loaded sounds. */
static sound_data_struct *sound_data;
/** Number of ::sound_data. */
static size_t sound_data_num;

/**
 * Compare two sound data structure filenames.
 * @param a First sound data.
 * @param b Second sound data.
 * @return Return value of strcmp(). */
static int sound_compare(const void *a, const void *b)
{
	return strcmp(((sound_data_struct *) a)->filename, ((sound_data_struct *) b)->filename);
}

/**
 * (Re-)Sort the ::sound_data array using Quicksort. */
static void sound_sort()
{
	qsort((void *) sound_data, sound_data_num, sizeof(sound_data_struct), (void *) (int (*)()) sound_compare);
}

/**
 * Try to find the specified sound file name in ::sound_data.
 * @param filename What to look for.
 * @return The ::sound_data entry if found, NULL otherwise. */
static sound_data_struct *sound_find(const char *filename)
{
	sound_data_struct key;

	key.filename = (char *) filename;
	return (sound_data_struct *) bsearch((void *) &key, (void *) sound_data, sound_data_num, sizeof(sound_data_struct), sound_compare);
}

/**
 * Add a sound entry to the ::sound_data array.
 * @param type Type of the sound, one of @ref SOUND_TYPE_xxx.
 * @param filename Sound's file name.
 * @param data Loaded sound data to store.
 * @return Pointer to the entry in ::sound_data. */
static sound_data_struct *sound_new(int type, const char *filename, void *data)
{
	sound_data_num++;
	sound_data = realloc(sound_data, sizeof(sound_data_struct) * sound_data_num);
	sound_data[sound_data_num - 1].type = type;
	sound_data[sound_data_num - 1].filename = strdup(filename);
	sound_data[sound_data_num - 1].data = data;

	return &sound_data[sound_data_num - 1];
}

/**
 * Free one sound data entry.
 * @param tmp What to free. */
static void sound_free(sound_data_struct *tmp)
{
	switch (tmp->type)
	{
		case SOUND_TYPE_CHUNK:
			Mix_FreeChunk((Mix_Chunk *) tmp->data);
			break;

		case SOUND_TYPE_MUSIC:
			Mix_FreeMusic((Mix_Music *) tmp->data);
			break;

		default:
			LOG(llevMsg, "BUG: sound_free(): Trying to free sound with unknown type: %d.\n", tmp->type);
			return;
	}

	free(tmp->filename);
}

/**
 * Initialize the sound system. */
void sound_init()
{
	sound_background = NULL;
	sound_data = NULL;
	sound_data_num = 0;

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, AUDIO_S16, MIX_DEFAULT_CHANNELS, 1024) < 0)
	{
		LOG(llevError, "ERROR: sound_init(): Couldn't set sound device. Reason: %s\n", SDL_GetError());
		SYSTEM_End();
		exit(0);
	}
}

/**
 * Deinitialize the sound system. */
void sound_deinit()
{
	size_t i;

	for (i = 0; i < sound_data_num; i++)
	{
		sound_free(&sound_data[i]);
	}

	free(sound_data);
	sound_data = NULL;
	sound_data_num = 0;
	Mix_CloseAudio();
}

/**
 * Add sound effect to the playing queue.
 * @param filename Sound file name to play. Will be loaded as needed.
 * @param volume Volume to play at.
 * @param loop How many times to loop, -1 for infinite number. */
static void sound_add_effect(const char *filename, int volume, int loop)
{
	int channel;
	sound_data_struct *tmp;
	Mix_Chunk *chunk = NULL;

	/* Try to find the sound first. */
	tmp = sound_find(filename);

	if (!tmp)
	{
		chunk = Mix_LoadWAV(filename);

		if (!chunk)
		{
			LOG(llevError, "ERROR: sound_add_effect(): Could not load '%s'. Reason: %s.\n", filename, Mix_GetError());
			return;
		}

		/* We loaded it now, so add it to the array of loaded sounds. */
		tmp = sound_new(SOUND_TYPE_CHUNK, filename, chunk);
	}

	channel = Mix_PlayChannel(-1, (Mix_Chunk *) tmp->data, loop);

	if (channel == -1)
	{
		return;
	}

	Mix_Volume(channel, (int) (((double) options.sound_volume / (double) 100) * ((double) volume * ((double) MIX_MAX_VOLUME / (double) 100))));

	/* Re-sort the array as needed. */
	if (chunk)
	{
		sound_sort();
	}
}

/**
 * Play a sound effect.
 * @param filename Sound file name to play.
 * @param volume Volume to play at. */
void sound_play_effect(const char *filename, int volume)
{
	char path[HUGE_BUF];

	snprintf(path, sizeof(path), "%s%s", GetSfxDirectory(), filename);
	sound_add_effect(path, volume, 0);
}

/**
 * Start background music.
 * @param filename Filename of the music to start.
 * @param volume Volume to use.
 * @param loop How many times to loop, -1 for infinite number. */
void sound_start_bg_music(const char *filename, int volume, int loop)
{
	char path[HUGE_BUF];
	sound_data_struct *tmp;
	Mix_Music *music = NULL;

	if (!strcmp(filename, "no_music"))
	{
		sound_stop_bg_music();
		return;
	}

	snprintf(path, sizeof(path), "%s%s", GetMediaDirectory(), filename);

	/* Same background music, nothing to do. */
	if (sound_background && !strcmp(sound_background, path))
	{
		return;
	}

	/* Try to find the music. */
	tmp = sound_find(path);

	if (!tmp)
	{
		music = Mix_LoadMUS(path);

		if (!music)
		{
			LOG(llevError, "ERROR: sound_add_effect(): Could not load '%s'. Reason: %s.\n", path, Mix_GetError());
			return;
		}

		/* Add the loaded music to the array. */
		tmp = sound_new(SOUND_TYPE_MUSIC, path, music);
	}

	sound_stop_bg_music();

	sound_background = strdup(path);
	Mix_VolumeMusic(volume);
	Mix_PlayMusic((Mix_Music *) tmp->data, loop);

	/* Re-sort the array as needed. */
	if (music)
	{
		sound_sort();
	}
}

/**
 * Stop the background music, if there is any. */
void sound_stop_bg_music()
{
	if (sound_background)
	{
		Mix_HaltMusic();
		free(sound_background);
		sound_background = NULL;
	}
}

/**
 * Parse map's background music information.
 * @param bg_music What to parse. */
void parse_map_bg_music(const char *bg_music)
{
	int loop = -1, vol = 0;
	char filename[MAX_BUF];

	if (sscanf(bg_music, "%s %d %d", filename, &loop, &vol) < 1)
	{
		LOG(llevMsg, "BUG: parse_map_bg_music(): Bogus background music: '%s'\n", bg_music);
		return;
	}

	sound_start_bg_music(filename, options.music_volume + vol, loop);
}

/**
 * Update volume of the background sound being played. */
void sound_update_volume()
{
	Mix_VolumeMusic(options.music_volume);
}

/**
 * Sound command, used to play a sound.
 * @param data Data to initialize the sound data from.
 * @param len Length of 'data'. */
void SoundCmd(uint8 *data, int len)
{
	size_t pos = 0, i = 0;
	uint8 type;
	int loop, volume;
	char filename[MAX_BUF], c;

	(void) len;
	filename[0] = '\0';
	type = data[pos++];

	while ((c = (char) (data[pos++])))
	{
		filename[i++] = c;
	}

	filename[i] = '\0';
	loop = data[pos++];
	volume = data[pos++];

	if (type == CMD_SOUND_EFFECT)
	{
		int dist_volume, x, y;
		char path[HUGE_BUF];

		x = data[pos++];
		y = data[pos++];
		dist_volume = isqrt(POW2(0 - x) + POW2(0 - y)) - 1;

		if (dist_volume < 0)
		{
			dist_volume = 0;
		}

		dist_volume = 100 - dist_volume * (100 / MAX_SOUND_DISTANCE);
		snprintf(path, sizeof(path), "%s%s", GetSfxDirectory(), filename);
		sound_add_effect(path, dist_volume + volume, loop);
	}
	else if (type == CMD_SOUND_BACKGROUND)
	{
		sound_start_bg_music(filename, options.music_volume + volume, loop);
	}
	else if (type == CMD_SOUND_ABSOLUTE)
	{
		sound_add_effect(filename, volume, loop);
	}
	else
	{
		LOG(llevError, "ERROR: SoundCmd(): Invalid sound type: %d\n", type);
		return;
	}
}
