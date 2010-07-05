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

/** Pointer to the background sound that is playing. */
static Mix_Music *sound_background;
/** Sound effects loaded from ::sound_files. */
static Mix_Chunk *sound_effects[SOUND_MAX];

/** The sound files. */
static const char *const sound_files[SOUND_MAX] =
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
 * Initialize the sound system. */
void sound_init()
{
	size_t i;
	char filename[HUGE_BUF];

	sound_background = NULL;

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, AUDIO_S16, MIX_DEFAULT_CHANNELS, 1024) < 0)
	{
		LOG(llevError, "ERROR: sound_init(): Couldn't set sound device. Reason: %s\n", SDL_GetError());
		SYSTEM_End();
		exit(0);
	}

	for (i = 0; i < SOUND_MAX; i++)
	{
		snprintf(filename, sizeof(filename), "%s%s", GetSfxDirectory(), sound_files[i]);
		sound_effects[i] = Mix_LoadWAV(filename);

		if (!sound_effects[i])
		{
			LOG(llevError, "ERROR: sound_init(): Could not load '%s'. Reason: %s.\n", filename, Mix_GetError());
			SYSTEM_End();
			exit(0);
		}
	}
}

/**
 * Deinitialize the sound system. */
void sound_deinit()
{
	size_t i;

	for (i = 0; i < SOUND_MAX; i++)
	{
		Mix_FreeChunk(sound_effects[i]);
	}

	Mix_CloseAudio();
}

/**
 * Add sound effect to the playing queue from ::sound_effects.
 * @param sound_id Sound ID to play.
 * @param volume Volume to play at.
 * @param loop How many times to loop, -1 for infinite number. */
static void sound_add_effect(int sound_id, int volume, int loop)
{
	int tmp = Mix_PlayChannel(-1, sound_effects[sound_id], loop);

	if (tmp == -1)
	{
		LOG(llevMsg, "BUG: sound_add_effect(): %s\n", Mix_GetError());
		return;
	}

	Mix_Volume(tmp, (int) (((double) options.sound_volume / (double) 100) * ((double) volume * ((double) MIX_MAX_VOLUME / (double) 100))));
}

/**
 * Play a sound effect.
 * @param sound_id Sound ID to play.
 * @param volume Volume to play at. */
void sound_play_effect(int sound_id, int volume)
{
	sound_add_effect(sound_id, volume, 0);
}

/**
 * Start background music.
 * @param filename Filename of the music to start.
 * @param volume Volume to use.
 * @param loop How many times to loop, -1 for infinite number. */
void sound_start_bg_music(const char *filename, int volume, int loop)
{
	char path[HUGE_BUF];

	snprintf(path, sizeof(path), "%s%s", GetMediaDirectory(), filename);
	sound_stop_bg_music();
	sound_background = Mix_LoadMUS(path);

	if (!sound_background)
	{
		LOG(llevError, "ERROR: sound_add_music(): Could not load '%s'.\n", path);
		return;
	}

	Mix_VolumeMusic(volume);
	Mix_PlayMusic(sound_background, loop);
}

/**
 * Stop the background music, if there is any. */
void sound_stop_bg_music()
{
	if (sound_background)
	{
		Mix_HaltMusic();
		Mix_FreeMusic(sound_background);
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
 * Update volume of the background sound being played.
 * @todo Actually use this when changing volume in the options.
 * @param old_volume Old music volume. */
void sound_update_volume(int old_volume)
{
	Mix_VolumeMusic(Mix_VolumeMusic(-1) - old_volume + options.music_volume);
}

/**
 * Sound command, used to play a sound.
 * @param data Data to initialize the sound data from.
 * @param len Length of 'data'. */
void SoundCmd(uint8 *data, int len)
{
	size_t pos = 0;
	uint8 type;
	int sound_id = 0, x = 0, y = 0, loop, volume;
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

		if (dist_volume < 0)
		{
			dist_volume = 0;
		}

		dist_volume = 100 - dist_volume * (100 / MAX_SOUND_DISTANCE);
		sound_add_effect(sound_id, dist_volume + volume, loop);
	}
	else if (type == CMD_SOUND_BACKGROUND)
	{
		sound_start_bg_music(filename, options.music_volume + volume, loop);
	}
}
