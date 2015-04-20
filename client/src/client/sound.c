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
 * Sound related functions.
 *
 * @author Alex Tokar */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>

/**
 * Path to the background music file being played. */
static char *sound_background;
/**
 * If 1, will not allow music change based on map. */
static uint8_t sound_map_background_disabled = 0;
/**
 * Whether the sound system is active. */
static uint8_t enabled = 0;
/**
 * Doubly-linked list of all playing ambient sound effects. */
static sound_ambient_struct *sound_ambient_head = NULL;

#ifdef HAVE_SDL_MIXER

/**
 * When the background music started playing. */
static uint32_t sound_background_started;
/**
 * Duration of this background music. */
static uint32_t sound_background_duration;
/**
 * Whether to try to update the background music's duration in database. */
static uint8_t sound_background_update_duration;
/**
 * How many times to loop the currently playing background music, -1
 * to loopy infinitely. */
static int sound_background_loop;
/**
 * Volume the background music was started at. */
static int sound_background_volume;
/**
 * Loaded sounds. */
static sound_data_struct *sound_data;
/**
 * Hook function calle whenever ::sound_background changes its value. */
static void (*sound_background_hook)(void) ;

/**
 * Execute the ::sound_background_hook callback. */
static void sound_background_hook_execute(void)
{
    if (sound_background_hook) {
        sound_background_hook();
    }
}

/**
 * Add a sound entry to the ::sound_data array.
 * @param type Type of the sound, one of @ref SOUND_TYPE_xxx.
 * @param filename Sound's file name.
 * @param data Loaded sound data to store.
 * @return Pointer to the entry in ::sound_data. */
static sound_data_struct *sound_new(int type, const char *filename, void *data)
{
    sound_data_struct *tmp;

    tmp = emalloc(sizeof(sound_data_struct));
    tmp->type = type;
    tmp->filename = estrdup(filename);
    tmp->data = data;
    HASH_ADD_KEYPTR(hh, sound_data, tmp->filename, strlen(tmp->filename), tmp);

    return tmp;
}

/**
 * Free one sound data entry.
 * @param tmp What to free. */
static void sound_free(sound_data_struct *tmp)
{
    switch (tmp->type) {
    case SOUND_TYPE_CHUNK:
        Mix_FreeChunk(tmp->data);
        break;

    case SOUND_TYPE_MUSIC:
        Mix_FreeMusic(tmp->data);
        break;

    default:
        logger_print(LOG(BUG), "Trying to free sound with unknown type: %d.", tmp->type);
        break;
    }

    efree(tmp->filename);
    efree(tmp);
}

/**
 * Get duration of a music file.
 * @param filename The music file.
 * @return The duration. */
static uint32_t sound_music_file_get_duration(const char *filename)
{
    char path[HUGE_BUF], *contents, *cp;
    uint32_t duration;

    snprintf(path, sizeof(path), DIRECTORY_MEDIA "/durations/%s", filename);
    cp = file_path(path, "r");
    contents = path_file_contents(cp);
    efree(cp);

    if (!contents) {
        return 0;
    }

    duration = atoi(contents);
    efree(contents);

    return duration;
}

/**
 * Update duration of a music file.
 * @param filename The music file.
 * @param duration Duration to set. */
static void sound_music_file_set_duration(const char *filename, uint32_t duration)
{
    char path[HUGE_BUF];
    FILE *fp;

    snprintf(path, sizeof(path), DIRECTORY_MEDIA "/durations/%s", filename);
    fp = fopen_wrapper(path, "w");

    if (!fp) {
        logger_print(LOG(BUG), "Could not open file for writing: %s", path);
        return;
    }

    fprintf(fp, "%u", duration);
    fclose(fp);
}

/**
 * Hook called when music has finished playing. */
static void sound_music_finished(void)
{
    uint32_t duration;
    char *tmp;
    const char *bg_music;

    if (!sound_background) {
        return;
    }

    tmp = sound_background;
    bg_music = sound_get_bg_music_basename();
    duration = sound_music_get_offset();

    sound_background = NULL;
    sound_background_hook_execute();

    if (sound_background_update_duration && (!sound_background_duration || duration != sound_background_duration)) {
        sound_music_file_set_duration(bg_music, duration);
    }

    if (sound_background_loop) {
        if (sound_background_loop > 0) {
            sound_background_loop--;
        }

        sound_start_bg_music(bg_music, sound_background_volume, sound_background_loop);
    }

    efree(tmp);
}

#endif

/**
 * Register a new ::sound_background_hook callback.
 * @param ptr New callback to register. */
void sound_background_hook_register(void *ptr)
{
#ifdef HAVE_SDL_MIXER
    sound_background_hook = ptr;
#endif
}

/**
 * Initialize the sound system. */
void sound_init(void)
{
    sound_background = NULL;

#ifdef HAVE_SDL_MIXER
    sound_background_hook = NULL;
    sound_data = NULL;
    enabled = 1;

    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, AUDIO_S16, MIX_DEFAULT_CHANNELS, 1024) < 0) {
        draw_info_format(COLOR_RED, "Could not initialize audio device; sound will not be heard. Reason: %s", Mix_GetError());
        enabled = 0;
    }

    Mix_HookMusicFinished(sound_music_finished);
#else
    enabled = 0;
#endif
}

/**
 * Free the sound cache. */
static void sound_cache_free(void)
{
#ifdef HAVE_SDL_MIXER
    sound_data_struct *curr, *tmp;

    HASH_ITER(hh, sound_data, curr, tmp)
    {
        HASH_DEL(sound_data, curr);
        sound_free(curr);
    }
#endif
}

/**
 * Deinitialize the sound system. */
void sound_deinit(void)
{
    sound_cache_free();
#ifdef HAVE_SDL_MIXER
    Mix_CloseAudio();
#endif

    enabled = 0;

    if (sound_background != NULL) {
        efree(sound_background);
        sound_background = NULL;
    }
}

/**
 * Hook for clearing the sound API cache. */
void sound_clear_cache(void)
{
    sound_cache_free();
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

    if (!enabled) {
        return -1;
    }

    /* Try to find the sound first. */
    HASH_FIND_STR(sound_data, filename, tmp);

    if (!tmp) {
        Mix_Chunk *chunk = Mix_LoadWAV(filename);

        if (!chunk) {
            logger_print(LOG(BUG), "Could not load '%s'. Reason: %s.", filename, Mix_GetError());
            return -1;
        }

        /* We loaded it now, so add it to the array of loaded sounds. */
        tmp = sound_new(SOUND_TYPE_CHUNK, filename, chunk);
    }

    channel = Mix_PlayChannel(-1, (Mix_Chunk *) tmp->data, loop);

    if (channel == -1) {
        return -1;
    }

    Mix_Volume(channel, (int) ((setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_SOUND) / 100.0) * ((double) volume * (MIX_MAX_VOLUME / 100.0))));

    return channel;
#else
    return -1;
#endif
}

/**
 * Play a sound effect.
 * @param filename Sound file name to play.
 * @param volume Volume to play at. */
void sound_play_effect(const char *filename, int volume)
{
    char path[HUGE_BUF], *cp;

    snprintf(path, sizeof(path), DIRECTORY_SFX "/%s", filename);
    cp = file_path(path, "r");
    sound_add_effect(cp, volume, 0);
    efree(cp);
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
    char path[HUGE_BUF], *cp;
    int ret;

    snprintf(path, sizeof(path), DIRECTORY_SFX "/%s", filename);
    cp = file_path(path, "r");
    ret = sound_add_effect(cp, volume, loop);
    efree(cp);

    return ret;
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

    if (!enabled) {
        return;
    }

    if (!strcmp(filename, "no_music") || !strcmp(filename, "Disable music")) {
        sound_stop_bg_music();
        return;
    }

    snprintf(path, sizeof(path), DIRECTORY_MEDIA "/%s", filename);

    /* Same background music, nothing to do. */
    if (sound_background && !strcmp(sound_background, path)) {
        return;
    }

    /* Try to find the music. */
    HASH_FIND_STR(sound_data, path, tmp);

    if (!tmp) {
        char *cp;
        Mix_Music *music;

        cp = file_path(path, "r");
        music = Mix_LoadMUS(cp);
        efree(cp);

        if (music == NULL) {
            logger_print(LOG(BUG), "Could not load '%s'. Reason: %s.", path, Mix_GetError());
            return;
        }

        /* Add the loaded music to the array. */
        tmp = sound_new(SOUND_TYPE_MUSIC, path, music);
    }

    sound_stop_bg_music();

    sound_background = estrdup(path);
    sound_background_hook_execute();
    sound_background_loop = loop;
    sound_background_volume = volume;
    sound_background_duration = sound_music_file_get_duration(filename);
    sound_background_update_duration = 1;

    Mix_VolumeMusic(volume);
    Mix_PlayMusic(tmp->data, 0);

    sound_background_started = SDL_GetTicks();

    /* Due to a bug in SDL_mixer, some audio types (such as XM, among
     * others) will continue playing even when the volume has been set to
     * 0, which means we need to manually pause the music if volume is 0,
     * and unpause it in sound_update_volume(), if the volume changes. */
    if (volume == 0) {
        sound_pause_music();
    }
#endif
}

/**
 * Stop the background music, if there is any. */
void sound_stop_bg_music(void)
{
    if (!enabled) {
        return;
    }

    if (sound_background) {
        efree(sound_background);
        sound_background = NULL;
#ifdef HAVE_SDL_MIXER
        sound_background_hook_execute();
        Mix_HaltMusic();
#endif
    }
}

/**
 * Pause playing background music. */
void sound_pause_music(void)
{
#ifdef HAVE_SDL_MIXER
    Mix_PauseMusic();
    sound_background_update_duration = 0;
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
 * Update map's background music.
 * @param bg_music New background music. */
void update_map_bg_music(const char *bg_music)
{
    if (sound_map_background_disabled) {
        return;
    }

    if (!strcmp(bg_music, "no_music")) {
        sound_stop_bg_music();
    } else {
        int loop = -1, vol = 0;
        char filename[MAX_BUF];

        if (sscanf(bg_music, "%s %d %d", filename, &loop, &vol) < 1) {
            logger_print(LOG(BUG), "Bogus background music: '%s'", bg_music);
            return;
        }

        sound_start_bg_music(filename, setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + vol, loop);
    }
}

/**
 * Update volume of the background sound being played. */
void sound_update_volume(void)
{
    if (!enabled) {
        return;
    }

#ifdef HAVE_SDL_MIXER
    Mix_VolumeMusic(setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC));

    /* If there is any background music, due to a bug in SDL_mixer, we
     * may need to pause or unpause the music. */
    if (sound_background) {
        /* If the new volume is 0, pause the music. */
        if (setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) == 0) {
            if (!Mix_PausedMusic()) {
                sound_pause_music();
            }
        } else if (Mix_PausedMusic()) {
            /* Non-zero and already paused, so resume the music. */
            sound_resume_music();
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

    if (bg_music && (cp = strrchr(bg_music, '/'))) {
        bg_music = cp + 1;
    }

    return bg_music;
}

/**
 * Get or set ::sound_map_background_disabled.
 * @param val If -1, will return the current value of
 * ::sound_map_background_disabled;
 * any other value will set ::sound_map_background_disabled to that value.
 * @return Value of ::sound_map_background_disabled. */
uint8_t sound_map_background(int val)
{
    if (val == -1) {
        return sound_map_background_disabled;
    } else {
        sound_map_background_disabled = val;
        return val;
    }
}

/**
 * Get the offset of the background music that is being played.
 * @return The offset. */
uint32_t sound_music_get_offset(void)
{
    if (!sound_background) {
        return 0;
    }

#ifdef HAVE_SDL_MIXER
    return (SDL_GetTicks() - sound_background_started) / 1000;
#else
    return 0;
#endif
}

/**
 * Check whether the currently playing music can seek to a different
 * position.
 * @return 1 if the music can have playing position changed, 0 otherwise. */
int sound_music_can_seek(void)
{
    if (!sound_background) {
        return 0;
    }

#ifdef HAVE_SDL_MIXER
    switch (Mix_GetMusicType(NULL)) {
    case MUS_OGG:
    case MUS_MP3:
    case MUS_MP3_MAD:
        return 1;

    default:
        break;
    }
#endif

    return 0;
}

/**
 * Seek the currently playing background music to the specified offset
 * (in seconds).
 * @param offset Offset to seek to. */
void sound_music_seek(uint32_t offset)
{
    if (!sound_music_can_seek()) {
        return;
    }

#ifdef HAVE_SDL_MIXER
    Mix_RewindMusic();

    if (Mix_SetMusicPosition(offset) == -1) {
        logger_print(LOG(BUG), "Mix_SetMusicPosition: %s", Mix_GetError());
    }

    sound_background_started = SDL_GetTicks() - offset * 1000;
#endif
}

/**
 * Get duration of the currently playing background music.
 * @return The duration. */
uint32_t sound_music_get_duration()
{
#ifdef HAVE_SDL_MIXER
    return sound_background_duration;
#else
    return 0;
#endif
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_sound(uint8_t *data, size_t len, size_t pos)
{
    uint8_t type;
    int loop, volume;
    char filename[MAX_BUF];

    type = packet_to_uint8(data, len, &pos);
    packet_to_string(data, len, &pos, filename, sizeof(filename));
    loop = packet_to_int8(data, len, &pos);
    volume = packet_to_int8(data, len, &pos);

    if (type == CMD_SOUND_EFFECT) {
        int8_t x, y;
        int channel;

        x = packet_to_uint8(data, len, &pos);
        y = packet_to_uint8(data, len, &pos);

        channel = sound_play_effect_loop(filename, 100 + volume, loop);

        if (channel != -1) {
            int angle, distance;

            angle = 0;
            distance = (255 * isqrt(POW2(x) + POW2(y))) / MAX_SOUND_DISTANCE;

            if (setting_get_int(OPT_CAT_SOUND, OPT_3D_SOUNDS) && distance >= (255 / MAX_SOUND_DISTANCE) * 2) {
                angle = atan2(-y, x) * (180 / M_PI);
                angle = 90 - angle;
            }

#ifdef HAVE_SDL_MIXER
            Mix_SetPosition(channel, angle, distance);
#endif
        }
    } else if (type == CMD_SOUND_BACKGROUND) {
        if (!sound_map_background_disabled) {
            sound_start_bg_music(filename, setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + volume, loop);
        }
    } else if (type == CMD_SOUND_ABSOLUTE) {
        sound_add_effect(filename, (uint8_t) volume, loop);
    } else {
        logger_print(LOG(BUG), "Invalid sound type: %d", type);
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
    efree(tmp);
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
    if (setting_get_int(OPT_CAT_SOUND, OPT_3D_SOUNDS) && distance) {
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
        if (sound_ambient->x < 0 || sound_ambient->x >= setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) || sound_ambient->y < 0 || sound_ambient->y >= setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT)) {
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

/** @copydoc socket_command_struct::handle_func */
void socket_command_sound_ambient(uint8_t *data, size_t len, size_t pos)
{
    int tag, tag_old;
    uint8_t x, y;
    sound_ambient_struct *sound_ambient;

    /* Loop through the data, as there may be multiple sound effects. */
    while (pos < len) {
        x = packet_to_uint8(data, len, &pos);
        y = packet_to_uint8(data, len, &pos);
        tag_old = packet_to_uint32(data, len, &pos);
        tag = packet_to_uint32(data, len, &pos);

        /* If there is an old tag, the server is telling us to stop
         * playing a sound effect. */
        if (tag_old) {

            DL_FOREACH(sound_ambient_head, sound_ambient)
            {
                if (sound_ambient->tag == tag_old) {
                    sound_ambient_free(sound_ambient);
                    break;
                }
            }
        }

        /* Is there a new sound effect to start playing? */
        if (tag) {
            char filename[MAX_BUF];
            uint8_t volume, max_range;
            int channel;

            /* Get the sound effect filename, volume, etc. */
            packet_to_string(data, len, &pos, filename, sizeof(filename));
            volume = packet_to_uint8(data, len, &pos);
            max_range = packet_to_uint8(data, len, &pos);

            /* Try to start playing the sound effect. */
            channel = sound_play_effect_loop(filename, volume, -1);

            /* Successfully started playing the effect, add it to the
             * list of active sound effects. */
            if (channel != -1) {
                sound_ambient = ecalloc(1, sizeof(*sound_ambient));
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
