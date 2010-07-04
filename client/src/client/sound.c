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

/** An array of all the sounds that are currently playing. */
static sound_data_struct *sound_data;
/** Mutex for protecting ::sound_data. */
static SDL_mutex *sound_data_mutex;
/** Pointer to the background sound that is playing. */
static sound_data_struct *sound_background;

/** The sound files. */
char *sound_files[SOUND_MAX] =
{
	"event01.ogg",
	"bow1.ogg",
	"learnspell.ogg",
	"missspell.ogg",
	"rod.ogg",
	"door.ogg",
	"push.ogg",

	"hit_impact.ogg",
	"hit_cleave.ogg",
	"hit_slash.ogg",
	"hit_pierce.ogg",
	"hit_block.ogg",
	"hit_hand.ogg",
	"miss_mob1.ogg",
	"miss_player1.ogg",

	"petdead.ogg",
	"playerdead.ogg",
	"explosion.ogg",
	"explosion.ogg",
	"kill.ogg",
	"pull.ogg",
	"fallhole.ogg",
	"poison.ogg",

	"drop.ogg",
	"lose_some.ogg",
	"throw.ogg",
	"gate_open.ogg",
	"gate_close.ogg",
	"open_container.ogg",
	"growl.ogg",
	"arrow_hit.ogg",
	"door_close.ogg",
	"teleport.ogg",
	"scroll.ogg",

	"magic_default.ogg",
	"magic_acid.ogg",
	"magic_animate.ogg",
	"magic_avatar.ogg",
	"magic_bomb.ogg",
	"magic_bullet1.ogg",
	"magic_bullet2.ogg",
	"magic_cancel.ogg",
	"magic_comet.ogg",
	"magic_confusion.ogg",
	"magic_create.ogg",
	"magic_dark.ogg",
	"magic_death.ogg",
	"magic_destruction.ogg",
	"magic_elec.ogg",
	"magic_fear.ogg",
	"magic_fire.ogg",
	"magic_fireball1.ogg",
	"magic_fireball2.ogg",
	"magic_hword.ogg",
	"magic_ice.ogg",
	"magic_invisible.ogg",
	"magic_invoke.ogg",
	"magic_invoke2.ogg",
	"magic_magic.ogg",
	"magic_manaball.ogg",
	"magic_missile.ogg",
	"magic_mmap.ogg",
	"magic_orb.ogg",
	"magic_paralyze.ogg",
	"magic_poison.ogg",
	"magic_protection.ogg",
	"magic_rstrike.ogg",
	"magic_rune.ogg",
	"magic_sball.ogg",
	"magic_slow.ogg",
	"magic_snowstorm.ogg",
	"magic_stat.ogg",
	"magic_steambolt.ogg",
	"magic_summon1.ogg",
	"magic_summon2.ogg",
	"magic_summon3.ogg",
	"magic_teleport.ogg",
	"magic_turn.ogg",
	"magic_wall.ogg",
	"magic_walls.ogg",
	"magic_wound.ogg",

	/* Here start the client side sounds */
	"step1.ogg",
	"step2.ogg",
	"pray.ogg",
	"console.ogg",
	"click_fail.ogg",
	"change1.ogg",
	"warning_food.ogg",
	"warning_drain.ogg",
	"warning_statup.ogg",
	"warning_statdown.ogg",
	"warning_hp.ogg",
	"warning_hp2.ogg",
	"weapon_attack.ogg",
	"weapon_hold.ogg",
	"get.ogg",
	"book.ogg",
	"page.ogg"
};

/**
 * Add sound to ::sound_data.
 * @param filename Name of the sound file.
 * @param volume Volume to use.
 * @param looping How many times to loop, -1 for infinite number.
 * @param type One of @ref SOUND_TYPE_xxx.
 * @return Pointer to the new sound. */
static sound_data_struct *sound_add_music(const char *filename, int volume, int looping, int type)
{
	Sound_Sample *sample;
	sound_data_struct *tmp;

	sample = Sound_NewSampleFromFile(filename, NULL, 16384);

	if (!sample)
	{
		LOG(llevError, "ERROR: Could not load music '%s'.\n", filename);
		return NULL;
	}

	tmp = calloc(1, sizeof(sound_data_struct));
	tmp->sample = sample;
	tmp->volume = volume;
	tmp->looping = looping;
	tmp->sample = sample;
	tmp->filename = strdup(filename);
	tmp->type = type;

	SDL_LockMutex(sound_data_mutex);

	if (!sound_data)
	{
		sound_data = tmp;
	}
	else
	{
		if (sound_data->prev)
		{
			tmp->prev = sound_data->prev;
			sound_data->prev->next = tmp;
		}

		sound_data->prev = tmp;
		tmp->next = sound_data;
		sound_data = tmp;
	}

	SDL_UnlockMutex(sound_data_mutex);
	return tmp;
}

/**
 * Remove sound from ::sound_data.
 * @param tmp What to remove. */
static void sound_remove_music(sound_data_struct *tmp)
{
	SDL_LockMutex(sound_data_mutex);

	if (sound_data == tmp)
	{
		sound_data = tmp->next;
	}

	if (tmp->prev)
	{
		tmp->prev->next = tmp->next;
	}

	if (tmp->next)
	{
		tmp->next->prev = tmp->prev;
	}

	Sound_FreeSample(tmp->sample);
	free(tmp->filename);
	free(tmp);
	SDL_UnlockMutex(sound_data_mutex);
}

/**
 * This updates sound_data_struct::decoded_bytes and sound_data_struct::decoded_ptr
 * with more audio data, taking into account potentional looping.
 * @param tmp Sound data.
 * @return Decoded bytes. */
static int read_more_data(sound_data_struct *tmp)
{
	if (tmp->done)
	{
		tmp->decoded_bytes = 0;
		return 0;
	}

	if (tmp->decoded_bytes > 0)
	{
		return tmp->decoded_bytes;
	}

	/* See if there's more to be read... */
	if (!(tmp->sample->flags & (SOUND_SAMPLEFLAG_ERROR | SOUND_SAMPLEFLAG_EOF)))
	{
		tmp->decoded_bytes = Sound_Decode(tmp->sample);

		if (tmp->sample->flags & SOUND_SAMPLEFLAG_ERROR)
		{
			LOG(llevError, "ERROR: read_more_data(): Error decoding sound file: %s\n", Sound_GetError());
		}

		tmp->decoded_ptr = tmp->sample->buffer;
		return read_more_data(tmp);
	}

	/* No more to be read from stream, but we may want to loop the sample. */
	if (!tmp->looping)
	{
		return 0;
	}

	if (tmp->looping != -1)
	{
		tmp->looping--;
	}

	Sound_Rewind(tmp->sample);
	return read_more_data(tmp);
}

/**
 * Audio callback, used by SDL audio to play the sound files.
 * @param userdata Unused.
 * @param stream Where to write the sound data.
 * @param len Max length. */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
	sound_data_struct *tmp, *next;

	(void) userdata;

	SDL_LockMutex(sound_data_mutex);

	for (tmp = sound_data; tmp; tmp = next)
	{
		int bw = 0;

		next = tmp->next;

		/* This sound has finished playing, remove it from the list. */
		if (tmp->done)
		{
			sound_remove_music(tmp);
			continue;
		}

		while (bw < len)
		{
			size_t cpysize;

			/* Read more data, if needed. */
			if (!read_more_data(tmp))
			{
				memset(stream + bw, 0, len - bw);
				tmp->done = 1;
				break;
			}

			/* decoded_bytes and decoder_ptr are updated as necessary. */
			cpysize = len - bw;

			if (cpysize > tmp->decoded_bytes)
			{
				cpysize = tmp->decoded_bytes;
			}

			if (cpysize > 0)
			{
				SDL_MixAudio(stream + bw, (Uint8 *) tmp->decoded_ptr, cpysize, tmp->volume);
				bw += cpysize;
				tmp->decoded_ptr += cpysize;
				tmp->decoded_bytes -= cpysize;
			}
		}
	}

	SDL_UnlockMutex(sound_data_mutex);
}

/**
 * Initialize the sound system. */
void sound_init()
{
	SDL_AudioSpec sdl_desired;

	if (!Sound_Init())
	{
		LOG(llevError, "ERROR: Sound_Init() failed! Reason: %s\n", Sound_GetError());
		SYSTEM_End();
		exit(0);
	}

	sound_data = NULL;
	sound_background = NULL;
	sound_data_mutex = SDL_CreateMutex();

	memset(&sdl_desired, 0, sizeof(SDL_AudioSpec));
	sdl_desired.freq = 44100;
	sdl_desired.format = 32784;
	sdl_desired.channels = 2;
	sdl_desired.samples = 4096;
	sdl_desired.callback = audio_callback;

	if (SDL_OpenAudio(&sdl_desired, NULL) < 0)
	{
		LOG(llevError, "ERROR: Couldn't open audio device: %s\n", SDL_GetError());
		SYSTEM_End();
		exit(0);
	}

	SDL_PauseAudio(0);
}

/**
 * Deinitialize the sound system. */
void sound_deinit()
{
	Sound_Quit();
	SDL_CloseAudio();
}

/**
 * Play a sound effect.
 * @param soundid Sound ID to play.
 * @param volume Volume to play at. */
void sound_play_effect(int soundid, int volume)
{
	char filename[HUGE_BUF];

	volume = (int) (((double) options.sound_volume / (double) 100) * ((double) volume * ((double) 100 / (double) 100)));
	snprintf(filename, sizeof(filename), "%s%s", GetSfxDirectory(), sound_files[soundid]);
	sound_add_music(filename, volume, 0, SOUND_TYPE_EFFECT);
}

/**
 * Start background music.
 * @param filename Filename of the music to start.
 * @param volume Volume to use.
 * @param loop How many times to loop, -1 for infinite number. */
void sound_start_bg_music(char *filename, int volume, int loop)
{
	char path[HUGE_BUF];

	snprintf(path, sizeof(path), "%s%s", GetMediaDirectory(), filename);

	/* Any background music already playing? */
	if (sound_background)
	{
		/* If it's the same music, there's nothing to do. */
		if (!strcmp(sound_background->filename, path))
		{
			return;
		}

		sound_remove_music(sound_background);
	}

	sound_background = sound_add_music(path, volume, loop, SOUND_TYPE_BACKGROUND);
}

/**
 * Stop the background music, if there is any. */
void sound_stop_bg_music()
{
	/* Anything playing? */
	if (sound_background)
	{
		/* We will mark this pointer as done, so when audio_callback()
		 * picks it up, it will be freed, but we'll also mark this pointer
		 * as NULL (audio_callback() doesn't use it, it will pick it up from
		 * ::sound_data). */
		sound_background->done = 1;
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

	/* Backwards compatibility for maps using the old syntax. */
	if (strstr(bg_music, "|"))
	{
		int junk = 0;

		if (sscanf(bg_music, "%256[^|]|%d|%d", filename, &junk, &loop) != 3)
		{
			LOG(llevMsg, "BUG: parse_map_bg_music(): Bogus old-style background music: '%s'\n", bg_music);
			return;
		}
	}
	else
	{
		if (sscanf(bg_music, "%s %d %d", filename, &loop, &vol) < 1)
		{
			LOG(llevMsg, "BUG: parse_map_bg_music(): Bogus background music: '%s'\n", bg_music);
			return;
		}
	}

	sound_start_bg_music(filename, options.music_volume + vol, loop);
}

/**
 * Update volume of the sounds being played.
 * @todo Actually use this when changing volume in the options.
 * @param old_volume Old music volume. */
void sound_update_volume(int old_volume)
{
	sound_data_struct *tmp;

	SDL_LockMutex(sound_data_mutex);

	for (tmp = sound_data; tmp; tmp = tmp->next)
	{
		if (tmp->type == SOUND_TYPE_BACKGROUND)
		{
			tmp->volume = tmp->volume - old_volume + options.music_volume;
		}
	}

	SDL_UnlockMutex(sound_data_mutex);
}

/**
 * Sound command, used to play a sound.
 * @param data Data to initialize the sound data from.
 * @param len Length of 'data'. */
void SoundCmd(uint8 *data, int len)
{
	size_t pos = 0;
	uint8 type;
	int sound_id, x = 0, y = 0, loop, volume;
	char filename[MAX_BUF];

	(void) len;
	filename[0] = '\0';
	type = data[pos++];

	if (type == CMD_SOUND_EFFECT)
	{
		x = data[pos++];
		y = data[pos++];
		sound_id = GetShort_String(data + pos);
		pos += 2;

		if (sound_id < 0 || sound_id >= SOUND_MAX)
		{
			LOG(llevError, "Got invalid sound ID: %d\n", sound_id);
			return;
		}
	}
	else if (type == CMD_SOUND_BACKGROUND)
	{
		char c;
		size_t i = 0;

		while ((c = (char) (data[pos++])))
		{
			filename[i++] = c;
		}

		filename[i] = '\0';
	}
	else
	{
		LOG(llevError, "ERROR: SoundCmd(): Invalid sound type: %d\n", type);
		return;
	}

	loop = data[pos++];
	volume = data[pos++];

	if (type == CMD_SOUND_EFFECT)
	{
		int dist_volume = isqrt(POW2(0 - x) + POW2(0 - y)) - 1;
		char path[HUGE_BUF];

		if (dist_volume < 0)
		{
			dist_volume = 0;
		}

		dist_volume = 100 - dist_volume * (100 / MAX_SOUND_DISTANCE);
		snprintf(path, sizeof(path), "%s%s", GetSfxDirectory(), sound_files[sound_id]);
		sound_add_music(path, dist_volume + volume, loop, SOUND_TYPE_EFFECT);
	}
	else if (type == CMD_SOUND_BACKGROUND)
	{
		sound_start_bg_music(filename, options.music_volume + volume, loop);
	}
}
