/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Sound related functions.
 *
 * @author Alex Tokar */

#include <global.h>

/** Path to the background music file being played. */
static char *sound_background;
/** If 1, will not allow music change based on map. */
static uint8 sound_map_background_disabled = 0;
/** Whether the sound system is active. */
static uint8 enabled = 0;
/** Doubly-linked list of all playing ambient sound effects. */
static sound_ambient_struct *sound_ambient_head = NULL;

#ifdef HAVE_SDL_MIXER

/** Loaded sounds. */
static sound_data_struct *sound_data;

/**
 * Add a sound entry to the ::sound_data array.
 * @param type Type of the sound, one of @ref SOUND_TYPE_xxx.
 * @param filename Sound's file name.
 * @param data Loaded sound data to store.
 * @return Pointer to the entry in ::sound_data. */
static sound_data_struct *sound_new(int type, const char *filename, void *data)
{
	sound_data_struct *tmp;

	tmp = malloc(sizeof(sound_data_struct));
	tmp->type = type;
	tmp->filename = strdup(filename);
	tmp->data = data;
	HASH_ADD_KEYPTR(hh, sound_data, tmp->filename, strlen(tmp->filename), tmp);

	return tmp;
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
			LOG(llevBug, "sound_free(): Trying to free sound with unknown type: %d.\n", tmp->type);
			break;
	}

	free(tmp->filename);
	free(tmp);
}

#endif

/**
 * Initialize the sound system. */
void sound_init(void)
{
	sound_background = NULL;

#ifdef HAVE_SDL_MIXER
	sound_data = NULL;
	enabled = 1;

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, AUDIO_S16, MIX_DEFAULT_CHANNELS, 1024) < 0)
	{
		draw_info_format(COLOR_RED, "Could not initialize audio device; sound will not be heard. Reason: %s", Mix_GetError());
		enabled = 0;
	}
#else
	enabled = 0;
#endif
}

/**
 * Deinitialize the sound system. */
void sound_deinit(void)
{
#ifdef HAVE_SDL_MIXER
	sound_data_struct *curr, *tmp;

	HASH_ITER(hh, sound_data, curr, tmp)
	{
		HASH_DEL(sound_data, curr);
		sound_free(curr);
	}

	Mix_CloseAudio();
#endif

	enabled = 0;
}

/**
 * Add sound effect to the playing queue.
 * @param filename Sound file name to play. Will be loaded as needed.
 * @param volume Volume to play at.
 * @param loop How many times to loop, -1 for infinite number.
 * @return Channel the sound effect is being played on, -1 on failure. */
static int sound_add_effect(const char *filename, int volume, int loop)
{
#ifdef HAVE_SDL_MIXER
	int channel;
	sound_data_struct *tmp;

	if (!enabled)
	{
		return -1;
	}

	/* Try to find the sound first. */
	HASH_FIND_STR(sound_data, filename, tmp);

	if (!tmp)
	{
		Mix_Chunk *chunk = Mix_LoadWAV(filename);

		if (!chunk)
		{
			LOG(llevBug, "sound_add_effect(): Could not load '%s'. Reason: %s.\n", filename, Mix_GetError());
			return -1;
		}

		/* We loaded it now, so add it to the array of loaded sounds. */
		tmp = sound_new(SOUND_TYPE_CHUNK, filename, chunk);
	}

	channel = Mix_PlayChannel(-1, (Mix_Chunk *) tmp->data, loop);

	if (channel == -1)
	{
		return -1;
	}

	Mix_Volume(channel, (int) ((setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_SOUND) / 100.0) * ((double) volume * (MIX_MAX_VOLUME / 100.0))));

	return channel;
#else
	(void) filename;
	(void) volume;
	(void) loop;

	return -1;
#endif
}

/**
 * Play a sound effect.
 * @param filename Sound file name to play.
 * @param volume Volume to play at. */
void sound_play_effect(const char *filename, int volume)
{
	char path[HUGE_BUF];

	snprintf(path, sizeof(path), DIRECTORY_SFX"/%s", filename);
	sound_add_effect(file_path(path, "r"), volume, 0);
}

/**
 * Same as sound_play_effect(), but allows specifying how many times to
 * loop the sound effect.
 * @param filename Sound file name to play.
 * @param volume Volume to play at.
 * @param loop How many times to loop the sound effect, -1 to loop it
 * infinitely.
 * @return Channel the sound effect will be playing on, -1 on failure. */
int sound_play_effect_loop(const char *filename, int volume, int loop)
{
	char path[HUGE_BUF];

	snprintf(path, sizeof(path), DIRECTORY_SFX"/%s", filename);
	return sound_add_effect(file_path(path, "r"), volume, loop);
}

/**
 * Start background music.
 * @param filename Filename of the music to start.
 * @param volume Volume to use.
 * @param loop How many times to loop, -1 for infinite number. */
void sound_start_bg_music(const char *filename, int volume, int loop)
{
#ifdef HAVE_SDL_MIXER
	char path[HUGE_BUF];
	sound_data_struct *tmp;

	if (!enabled)
	{
		return;
	}

	if (!strcmp(filename, "no_music") || !strcmp(filename, "Disable music"))
	{
		sound_stop_bg_music();
		return;
	}

	snprintf(path, sizeof(path), DIRECTORY_MEDIA"/%s", filename);

	/* Same background music, nothing to do. */
	if (sound_background && !strcmp(sound_background, path))
	{
		return;
	}

	/* Try to find the music. */
	HASH_FIND_STR(sound_data, path, tmp);

	if (!tmp)
	{
		Mix_Music *music = Mix_LoadMUS(file_path(path, "r"));

		if (!music)
		{
			LOG(llevBug, "sound_start_bg_music(): Could not load '%s'. Reason: %s.\n", path, Mix_GetError());
			return;
		}

		/* Add the loaded music to the array. */
		tmp = sound_new(SOUND_TYPE_MUSIC, path, music);
	}

	sound_stop_bg_music();

	sound_background = strdup(path);
	Mix_VolumeMusic(volume);
	Mix_PlayMusic((Mix_Music *) tmp->data, loop);

	/* Due to a bug in SDL_mixer, some audio types (such as XM, among
	 * others) will continue playing even when the volume has been set to
	 * 0, which means we need to manually pause the music if volume is 0,
	 * and unpause it in sound_update_volume(), if the volume changes. */
	if (volume == 0)
	{
		Mix_PauseMusic();
	}
#else
	(void) filename;
	(void) volume;
	(void) loop;
#endif
}

/**
 * Stop the background music, if there is any. */
void sound_stop_bg_music(void)
{
	if (!enabled)
	{
		return;
	}

	if (sound_background)
	{
#ifdef HAVE_SDL_MIXER
		Mix_HaltMusic();
#endif
		free(sound_background);
		sound_background = NULL;
	}
}

/**
 * Update map's background music.
 * @param bg_music New background music. */
void update_map_bg_music(const char *bg_music)
{
	if (sound_map_background_disabled)
	{
		return;
	}

	if (!strcmp(bg_music, "no_music"))
	{
		sound_stop_bg_music();
	}
	else
	{
		int loop = -1, vol = 0;
		char filename[MAX_BUF];

		if (sscanf(bg_music, "%s %d %d", filename, &loop, &vol) < 1)
		{
			LOG(llevBug, "parse_map_bg_music(): Bogus background music: '%s'\n", bg_music);
			return;
		}

		sound_start_bg_music(filename, setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + vol, loop);
	}
}

/**
 * Update volume of the background sound being played. */
void sound_update_volume(void)
{
	if (!enabled)
	{
		return;
	}

#ifdef HAVE_SDL_MIXER
	Mix_VolumeMusic(setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC));

	/* If there is any background music, due to a bug in SDL_mixer, we
	 * may need to pause or unpause the music. */
	if (sound_background)
	{
		/* If the new volume is 0, pause the music. */
		if (setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) == 0)
		{
			if (!Mix_PausedMusic())
			{
				Mix_PauseMusic();
			}
		}
		/* Non-zero and already paused, so resume the music. */
		else if (Mix_PausedMusic())
		{
			Mix_ResumeMusic();
		}
	}
#endif
}

/**
 * Get the currently playing background music, if any.
 * @return Background music file name, NULL if no music is playing. */
const char *sound_get_bg_music(void)
{
	return sound_background;
}

/**
 * Get the background music base file name.
 * @return The background music base file name, if any. NULL otherwise. */
const char *sound_get_bg_music_basename(void)
{
	const char *bg_music = sound_background;
	char *cp;

	if (bg_music && (cp = strrchr(bg_music, '/')))
	{
		bg_music = cp + 1;
	}

	return bg_music;
}

/**
 * Get or set ::sound_map_background_disabled.
 * @param new If -1, will return the current value of ::sound_map_background_disabled;
 * any other value will set ::sound_map_background_disabled to that value.
 * @return Value of ::sound_map_background_disabled. */
uint8 sound_map_background(int new)
{
	if (new == -1)
	{
		return sound_map_background_disabled;
	}
	else
	{
		sound_map_background_disabled = new;
		return new;
	}
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
		sint8 x, y;
		int channel;

		x = data[pos++];
		y = data[pos++];

		channel = sound_play_effect_loop(filename, 100 + volume, loop);

		if (channel != -1)
		{
			int angle, distance;

			angle = 0;
			distance = (255 * isqrt(POW2(x) + POW2(y))) / MAX_SOUND_DISTANCE;

			if (setting_get_int(OPT_CAT_SOUND, OPT_3D_SOUNDS) && distance >= (255 / MAX_SOUND_DISTANCE) * 2)
			{
				angle = atan2(-y, x) * (180 / M_PI);
				angle = 90 - angle;
			}

#ifdef HAVE_SDL_MIXER
			Mix_SetPosition(channel, angle, distance);
#endif
		}
	}
	else if (type == CMD_SOUND_BACKGROUND)
	{
		if (!sound_map_background_disabled)
		{
			sound_start_bg_music(filename, setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + volume, loop);
		}
	}
	else if (type == CMD_SOUND_ABSOLUTE)
	{
		sound_add_effect(filename, volume, loop);
	}
	else if (type == CMD_SOUND_MIDI_NOTE)
	{
		static uint16 midi_tmp_counter = 0;
		char path[HUGE_BUF], *cp;
		FILE *fp;
		int midi_program, midi_channel, midi_pitch, midi_duration, midi_volume;

		snprintf(path, sizeof(path), "%s/.atrinik/midi-note-%d.mid.tmp", get_config_dir(), midi_tmp_counter++);

		fp = fopen(path, "wb");

		if (!fp)
		{
			return;
		}

		/* Initialize defaults. */
		midi_program = 0;
		midi_channel = 0;
		midi_pitch = 60;
		midi_duration = 1;
		midi_volume = (int) ((setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_SOUND) / 100.0) * 127);

		cp = strtok(filename, ",");

		while (cp)
		{
			while (isspace(*cp))
			{
				cp++;
			}

			if (!strncmp(cp, "program:", 8))
			{
				midi_program = atoi(cp + 8);
			}
			else if (!strncmp(cp, "channel:", 8))
			{
				midi_channel = atoi(cp + 8);
			}
			else if (!strncmp(cp, "pitch:", 6))
			{
				midi_pitch = atoi(cp + 6);
			}
			else if (!strncmp(cp, "duration:", 9))
			{
				midi_duration = atoi(cp + 9);
			}
			else if (!strncmp(cp, "volume:", 7))
			{
				midi_volume += atoi(cp + 7);
			}

			cp = strtok(NULL, ",");
		}

		if (!midi_volume)
		{
			return;
		}

		fwrite("\x4d\x54\x68\x64\x00\x00\x00\x06\x00\x01\x00\x01\x00\x80\x4d\x54\x72\x6b\x00\x00\x00\x17\x00", 1, 23, fp);
		fputc(192 + midi_channel, fp);
		fputc(midi_program, fp);
		fputc(0, fp);
		fputc(144 + midi_channel, fp);
		fputc(midi_pitch, fp);
		fputc(midi_volume, fp);
		fwrite("\x00\xff\x51\x03\x07\xa1\x20", 1, 7, fp);
		fputc(128 + midi_duration, fp);
		fputc(0, fp);
		fputc(128 + midi_channel, fp);
		fputc(midi_pitch, fp);
		fputc(midi_volume, fp);
		fwrite("\x00\xff\x2f\x00", 1, 4, fp);
		fclose(fp);

		sound_midi_play(path);
	}
	else
	{
		LOG(llevBug, "SoundCmd(): Invalid sound type: %d\n", type);
		return;
	}
}

/**
 * Free an ambient sound effect.
 *
 * The sound effect
 * @param tmp Sound effect to free. */
static void sound_ambient_free(sound_ambient_struct *tmp)
{
	DL_DELETE(sound_ambient_head, tmp);
#ifdef HAVE_SDL_MIXER
	Mix_HaltChannel(tmp->channel);
#endif
	free(tmp);
}

/**
 * Set distance and angle of an ambient sound effect.
 * @param tmp Sound effect. */
static void sound_ambient_set_position(sound_ambient_struct *tmp)
{
#ifdef HAVE_SDL_MIXER
	int x, y, angle, distance, cx, cy;

	cx = setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2;
	cy = setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2;

	/* The x/y positions stored in the sound effect structure are the
	 * positions on the map, so we have to convert it to coordinates
	 * relative to the player. */
	x = tmp->x - cx;
	y = tmp->y - cy;

	angle = 0;
	/* Calculate the distance. */
	distance = MIN(255, (255 * isqrt(POW2(x) + POW2(y))) / (tmp->max_range + (tmp->max_range / 2)));

	/* Calculate the angle. */
	if (setting_get_int(OPT_CAT_SOUND, OPT_3D_SOUNDS) && distance)
	{
		angle = atan2(-y, x) * (180 / M_PI);
		angle = 90 - angle;
	}

	Mix_SetPosition(tmp->channel, angle, distance);
#else
	(void) tmp;
#endif
}

/**
 * Handle map scroll for ambient sound effects. We need to check whether
 * the sound effect is now off-screen and if so, remove it. We also need
 * to adjust the angle and distance effects of the channel the sound
 * effect is playing on.
 * @param xoff X offset.
 * @param yoff Y offset. */
void sound_ambient_mapcroll(int xoff, int yoff)
{
	sound_ambient_struct *sound_ambient, *tmp;

	DL_FOREACH_SAFE(sound_ambient_head, sound_ambient, tmp)
	{
		/* Adjust the coordinates. */
		sound_ambient->x -= xoff;
		sound_ambient->y -= yoff;

		/* If the sound effect is now off-screen, remove it. */
		if (sound_ambient->x < 0 || sound_ambient->x >= setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) || sound_ambient->y < 0 || sound_ambient->y >= setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))
		{
			sound_ambient_free(sound_ambient);
			continue;
		}

		/* Adjust the distance and angle. */
		sound_ambient_set_position(sound_ambient);
	}
}

/**
 * Stop all ambient sound effects. */
void sound_ambient_clear(void)
{
	sound_ambient_struct *sound_ambient, *tmp;

	DL_FOREACH_SAFE(sound_ambient_head, sound_ambient, tmp)
	{
		sound_ambient_free(sound_ambient);
	}
}

/**
 * The server informs us about the ambient sound effects to play/stop.
 * @param data Data.
 * @param len Length of 'data'. */
void cmd_sound_ambient(uint8 *data, int len)
{
	int pos = 0, tag, tag_old;
	uint8 x, y;
	sound_ambient_struct *sound_ambient;

	/* Loop through the data, as there may be multiple sound effects. */
	while (pos < len)
	{
		x = data[pos++];
		y = data[pos++];
		tag_old = GetInt_String(data + pos);
		pos += 4;
		tag = GetInt_String(data + pos);
		pos += 4;

		/* If there is an old tag, the server is telling us to stop
		 * playing a sound effect. */
		if (tag_old)
		{
			DL_FOREACH(sound_ambient_head, sound_ambient)
			{
				if (sound_ambient->tag == tag_old)
				{
					sound_ambient_free(sound_ambient);
					break;
				}
			}
		}

		/* Is there a new sound effect to start playing? */
		if (tag)
		{
			char filename[MAX_BUF];
			uint8 volume, max_range;
			int channel;

			/* Get the sound effect filename and volume. */
			GetString_String(data, &pos, filename, sizeof(filename));
			volume = data[pos++];
			max_range = data[pos++];

			/* Try to start playing the sound effect. */
			channel = sound_play_effect_loop(filename, volume, -1);

			/* Successfully started playing the effect, add it to the
			 * list of active sound effects. */
			if (channel != -1)
			{
				sound_ambient = calloc(1, sizeof(*sound_ambient));
				sound_ambient->channel = channel;
				sound_ambient->tag = tag;
				sound_ambient->x = x;
				sound_ambient->y = y;
				sound_ambient->max_range = max_range;
				sound_ambient_set_position(sound_ambient);
				DL_APPEND(sound_ambient_head, sound_ambient);
			}
		}
	}
}

/**
 * Pause playing background music. */
void sound_pause_music(void)
{
#ifdef HAVE_SDL_MIXER
	Mix_PauseMusic();
#endif
}

/**
 * Resume playing background music. */
void sound_resume_music(void)
{
#ifdef HAVE_SDL_MIXER
	Mix_ResumeMusic();
#endif
}

/**
 * Check whether background music is being played.
 * @return 1 if background music is being played, 0 otherwise. */
int sound_playing_music(void)
{
#ifdef HAVE_SDL_MIXER
	return Mix_PlayingMusic();
#else
	return 0;
#endif
}

/**
 * The MIDI playing thread.
 * @param data What to play.
 * @return 0. */
static int sound_midi_thread(void *data)
{
	char *path, buf[HUGE_BUF];

	path = (char *) data;
	snprintf(buf, sizeof(buf), "timidity \"%s\"", path);
	system(buf);

	if (strlen(path) >= 4 && strcmp(path + (strlen(path) - 4), ".tmp") == 0)
	{
		unlink(path);
	}

	free(path);

	return 0;
}

/**
 * Play a MIDI file.
 * @param path What to play. */
void sound_midi_play(const char *path)
{
	SDL_Thread *thread;
	char *cp;

	cp = strdup(path);
	thread = SDL_CreateThread(sound_midi_thread, cp);

	if (!thread)
	{
		LOG(llevError, "sound_midi_play(): Thread creation failed.\n");
		free(cp);
	}
}
