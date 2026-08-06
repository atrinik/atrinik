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
 * Sound related functions.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <wrapper.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <toolkit/path.h>

/**
 * Path to the background music file being played.
 */
static char *sound_background;
/**
 * If 1, will not allow music change based on map.
 */
static uint8_t sound_map_background_disabled = 0;
/**
 * Whether the sound system is active.
 */
static uint8_t enabled = 0;
/**
 * Doubly-linked list of all playing ambient sound effects.
 */
static sound_ambient_struct *sound_ambient_head = NULL;

/** Female player-hurt variants supplied by the sound submodule. */
static const char *const sound_female_hurt_effects[] = {
    "doh_female_1.ogg",
    "doh_female_2.ogg",
    "doh_female_3.ogg",
    "doh_female_4.ogg",
    "doh_female_5.ogg",
    "doh_female_6.ogg",
    "doh_female_7.ogg",
};

#ifdef HAVE_SDL_MIXER

/**
 * Duration of this background music.
 */
static uint32_t sound_background_duration;
/**
 * Whether to try to update the background music's duration in database.
 */
static uint8_t sound_background_update_duration;
/**
 * How many times to loop the currently playing background music, -1
 * to loopy infinitely.
 */
static int sound_background_loop;
/**
 * Volume the background music was started at.
 */
static int sound_background_volume;
/** Per-track adjustment supplied by the current map. */
static int sound_background_volume_adjustment;
/**
 * Loaded sounds.
 */
static sound_data_struct *sound_data;
static MIX_Mixer *sound_mixer;
static MIX_Track *sound_music_track;
#define SOUND_EFFECT_TRACKS 32
static MIX_Track *sound_effect_tracks[SOUND_EFFECT_TRACKS];
/**
 * Hook function calle whenever ::sound_background changes its value.
 */
static void (*sound_background_hook)(void);

static float sound_percent_to_gain(int64_t percent) {
    percent = MAX(INT64_C(0), MIN(INT64_C(100), percent));
    return (float)percent / 100.0f;
}

static void sound_apply_music_volume(int64_t percent) {
    sound_background_volume = (int)MAX(INT64_C(0), MIN(INT64_C(100), percent));
    MIX_SetTrackGain(sound_music_track, sound_percent_to_gain(sound_background_volume));

    if (sound_background == NULL) {
        return;
    }

    if (sound_background_volume == 0) {
        if (!MIX_TrackPaused(sound_music_track)) {
            sound_pause_music();
        }
    } else if (MIX_TrackPaused(sound_music_track)) {
        sound_resume_music();
    }
}

static void sound_start_bg_music_internal(const char *filename,
                                          int64_t volume,
                                          int loop,
                                          int volume_adjustment);

/**
 * Execute the ::sound_background_hook callback.
 */
static void sound_background_hook_execute(void) {
    if (sound_background_hook) {
        sound_background_hook();
    }
}

static uint32_t sound_music_track_get_offset(void) {
    Sint64 frames = MIX_GetTrackPlaybackPosition(sound_music_track);
    Sint64 milliseconds = frames >= 0 ? MIX_TrackFramesToMS(sound_music_track, frames) : -1;

    if (milliseconds <= 0) {
        return 0;
    }

    return (uint32_t)MIN((Uint64)milliseconds / 1000, UINT32_MAX);
}

/**
 * Add a sound entry to the ::sound_data array.
 * @param filename
 * Sound's file name.
 * @param data
 * Loaded sound data to store.
 * @return
 * Pointer to the entry in ::sound_data.
 */
static sound_data_struct *sound_new(const char *filename, MIX_Audio *data) {
    sound_data_struct *tmp;

    tmp = xmalloc(sizeof(sound_data_struct));
    tmp->filename = xstrdup(filename);
    tmp->data = data;
    HASH_ADD_KEYPTR(hh, sound_data, tmp->filename, strlen(tmp->filename), tmp);

    return tmp;
}

/**
 * Free one sound data entry.
 * @param tmp
 * What to free.
 */
static void sound_free(sound_data_struct *tmp) {
    MIX_DestroyAudio(tmp->data);
    free(tmp->filename);
    free(tmp);
}

/**
 * Get duration of a music file.
 * @param filename
 * The music file.
 * @return
 * The duration.
 */
static uint32_t sound_music_file_get_duration(const char *filename) {
    char path[HUGE_BUF], *contents, *cp;
    uint32_t duration;

    snprintf(path, sizeof(path), DIRECTORY_MEDIA "/durations/%s", filename);
    cp = file_path(path, "r");
    contents = path_file_contents(cp);
    free(cp);

    if (!contents) {
        return 0;
    }

    duration = atoi(contents);
    free(contents);

    return duration;
}

/**
 * Update duration of a music file.
 * @param filename
 * The music file.
 * @param duration
 * Duration to set.
 */
static void sound_music_file_set_duration(const char *filename, uint32_t duration) {
    char path[HUGE_BUF];
    FILE *fp;

    snprintf(path, sizeof(path), DIRECTORY_MEDIA "/durations/%s", filename);
    fp = path_fopen(path, "w");

    if (!fp) {
        LOG(BUG, "Could not open file for writing: %s", path);
        return;
    }

    fprintf(fp, "%u", duration);
    fclose(fp);
}

/**
 * SDL_mixer callback. This can run on the audio thread, so only enqueue an
 * event; the main thread owns all music state and cached resources.
 */
static void SDLCALL sound_music_finished(void *userdata, MIX_Track *track) {
    SDL_Event event;

    (void)userdata;
    (void)track;

    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_USER;
    event.user.code = EVENT_SOUND_MUSIC_FINISHED;
    SDL_PushEvent(&event);
}

/**
 * Handle completed music on the main thread.
 */
static void sound_music_finished_process(void) {
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

    if (sound_background_update_duration &&
        (!sound_background_duration || duration != sound_background_duration)) {
        sound_music_file_set_duration(bg_music, duration);
    }

    if (sound_background_loop) {
        if (sound_background_loop > 0) {
            sound_background_loop--;
        }

        sound_start_bg_music_internal(bg_music,
                                      sound_background_volume,
                                      sound_background_loop,
                                      sound_background_volume_adjustment);
    }

    free(tmp);
}

#endif

/**
 * Handle the music-finished event posted by SDL_mixer.
 */
void sound_music_finished_handle(void) {
#ifdef HAVE_SDL_MIXER
    /* SDL3_mixer also invokes the stopped callback for an explicit stop.
     * sound_stop_bg_music() clears this pointer before stopping the track, so
     * only a naturally exhausted background advances the playlist here. */
    if (enabled && sound_background != NULL && !MIX_TrackPlaying(sound_music_track) &&
        !MIX_TrackPaused(sound_music_track)) {
        sound_music_finished_process();
    }
#endif
}

/**
 * Register a new ::sound_background_hook callback.
 * @param ptr
 * New callback to register.
 */
void sound_background_hook_register(void *ptr) {
#ifdef HAVE_SDL_MIXER
    sound_background_hook = ptr;
#endif
}

/**
 * Initialize the sound system.
 */
void sound_init(void) {
    sound_background = NULL;

#ifdef HAVE_SDL_MIXER
    sound_background_hook = NULL;
    sound_data = NULL;
    sound_mixer = NULL;
    sound_music_track = NULL;
    memset(sound_effect_tracks, 0, sizeof(sound_effect_tracks));
    enabled = 0;

    if (!MIX_Init()) {
        draw_info_format(COLOR_RED,
                         "Could not initialize SDL3_mixer; sound will not be heard. Reason: %s",
                         SDL_GetError());
        return;
    }

    sound_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (sound_mixer == NULL) {
        draw_info_format(COLOR_RED,
                         "Could not initialize audio device; sound will not be heard. Reason: %s",
                         SDL_GetError());
        MIX_Quit();
        return;
    }

    sound_music_track = MIX_CreateTrack(sound_mixer);
    if (sound_music_track == NULL ||
        !MIX_SetTrackStoppedCallback(sound_music_track, sound_music_finished, NULL)) {
        draw_info_format(COLOR_RED,
                         "Could not create music track; sound will not be heard. Reason: %s",
                         SDL_GetError());
        MIX_DestroyMixer(sound_mixer);
        sound_mixer = NULL;
        sound_music_track = NULL;
        MIX_Quit();
        return;
    }

    enabled = 1;
#else
    enabled = 0;
#endif
}

/**
 * Free the sound cache.
 */
static void sound_cache_free(void) {
#ifdef HAVE_SDL_MIXER
    sound_data_struct *curr, *tmp;

    if (sound_music_track != NULL) {
        MIX_SetTrackAudio(sound_music_track, NULL);
    }
    for (size_t i = 0; i < arraysize(sound_effect_tracks); i++) {
        if (sound_effect_tracks[i] != NULL) {
            MIX_SetTrackAudio(sound_effect_tracks[i], NULL);
        }
    }

    HASH_ITER(hh, sound_data, curr, tmp) {
        HASH_DEL(sound_data, curr);
        sound_free(curr);
    }
#endif
}

/**
 * Deinitialize the sound system.
 */
void sound_deinit(void) {
    enabled = 0;
#ifdef HAVE_SDL_MIXER
    if (sound_mixer != NULL) {
        MIX_StopAllTracks(sound_mixer, 0);
    }
#endif

    sound_ambient_clear();
    free(sound_background);
    sound_background = NULL;

#ifdef HAVE_SDL_MIXER
    sound_cache_free();
    if (sound_mixer != NULL) {
        MIX_DestroyMixer(sound_mixer);
        sound_mixer = NULL;
        sound_music_track = NULL;
        memset(sound_effect_tracks, 0, sizeof(sound_effect_tracks));
    }
    MIX_Quit();
#endif
}

/**
 * Hook for clearing the sound API cache.
 */
void sound_clear_cache(void) {
#ifdef HAVE_SDL_MIXER
    if (enabled) {
        sound_stop_bg_music();
        sound_ambient_clear();
        MIX_StopAllTracks(sound_mixer, 0);
    }
#else
    sound_ambient_clear();
#endif

    sound_cache_free();
}

/**
 * Add sound effect to the playing queue.
 * @param filename
 * Sound file name to play. Will be loaded as needed.
 * @param volume
 * Volume to play at.
 * @param loop
 * How many times to loop, -1 for infinite number.
 * @return
 * Channel the sound effect is being played on, -1 on failure.
 */
static int sound_add_effect(const char *filename, int volume, int loop) {
#ifdef HAVE_SDL_MIXER
    sound_data_struct *tmp;

    if (!enabled) {
        return -1;
    }

    /* Try to find the sound first. */
    HASH_FIND_STR(sound_data, filename, tmp);

    if (!tmp) {
        MIX_Audio *audio = MIX_LoadAudio(sound_mixer, filename, true);

        if (audio == NULL) {
            LOG(BUG, "Could not load '%s'. Reason: %s.", filename, SDL_GetError());
            return -1;
        }

        /* We loaded it now, so add it to the array of loaded sounds. */
        tmp = sound_new(filename, audio);
    }

    int channel;
    for (channel = 0; channel < SOUND_EFFECT_TRACKS; channel++) {
        if (sound_effect_tracks[channel] == NULL) {
            sound_effect_tracks[channel] = MIX_CreateTrack(sound_mixer);
        }
        if (sound_effect_tracks[channel] != NULL &&
            !MIX_TrackPlaying(sound_effect_tracks[channel]) &&
            !MIX_TrackPaused(sound_effect_tracks[channel])) {
            break;
        }
    }
    if (channel == SOUND_EFFECT_TRACKS) {
        return -1;
    }

    int64_t effective_percent =
        setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_SOUND) * MAX(0, volume) / 100;
    MIX_Track *track = sound_effect_tracks[channel];
    MIX_StereoGains stereo = {.left = 1.0f, .right = 1.0f};
    SDL_PropertiesID options = 0;
    if (loop != 0) {
        options = SDL_CreateProperties();
        if (options == 0 || !SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, loop)) {
            LOG(BUG, "Could not configure loops for '%s'. Reason: %s.", filename, SDL_GetError());
            if (options != 0) {
                SDL_DestroyProperties(options);
            }
            return -1;
        }
    }

    if (!MIX_SetTrackAudio(track, tmp->data) ||
        !MIX_SetTrackGain(track, sound_percent_to_gain(effective_percent)) ||
        !MIX_SetTrackStereo(track, &stereo) || !MIX_PlayTrack(track, options)) {
        LOG(BUG, "Could not play '%s'. Reason: %s.", filename, SDL_GetError());
        MIX_StopTrack(track, 0);
        if (options != 0) {
            SDL_DestroyProperties(options);
        }
        return -1;
    }
    if (options != 0) {
        SDL_DestroyProperties(options);
    }

    return channel;
#else
    return -1;
#endif
}

/**
 * Play a sound effect.
 * @param filename
 * Sound file name to play.
 * @param volume
 * Volume to play at.
 */
void sound_play_effect(const char *filename, int volume) {
    char path[HUGE_BUF], *cp;

    snprintf(path, sizeof(path), DIRECTORY_SFX "/%s", filename);
    cp = file_path(path, "r");
    sound_add_effect(cp, volume, 0);
    free(cp);
}

/**
 * Same as sound_play_effect(), but allows specifying how many times to
 * loop the sound effect.
 * @param filename
 * Sound file name to play.
 * @param volume
 * Volume to play at.
 * @param loop
 * How many times to loop the sound effect, -1 to loop it
 * infinitely.
 * @return
 * Channel the sound effect will be playing on, -1 on failure.
 */
int sound_play_effect_loop(const char *filename, int volume, int loop) {
    char path[HUGE_BUF], *cp;
    int ret;

    snprintf(path, sizeof(path), DIRECTORY_SFX "/%s", filename);
    cp = file_path(path, "r");
    ret = sound_add_effect(cp, volume, loop);
    free(cp);

    return ret;
}

/**
 * Start background music.
 * @param filename
 * Filename of the music to start.
 * @param volume
 * Volume to use.
 * @param loop
 * How many times to loop, -1 for infinite number.
 */
static void sound_start_bg_music_internal(const char *filename,
                                          int64_t volume,
                                          int loop,
                                          int volume_adjustment) {
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
        sound_background_loop = loop;
        sound_background_volume_adjustment = volume_adjustment;
        sound_apply_music_volume(volume);
        return;
    }

    /* Try to find the music. */
    HASH_FIND_STR(sound_data, path, tmp);

    if (!tmp) {
        char *cp;
        MIX_Audio *music;

        cp = file_path(path, "r");
        music = MIX_LoadAudio(sound_mixer, cp, false);
        free(cp);

        if (music == NULL) {
            LOG(BUG, "Could not load '%s'. Reason: %s.", path, SDL_GetError());
            return;
        }

        /* Add the loaded music to the array. */
        tmp = sound_new(path, music);
    }

    sound_stop_bg_music();

    sound_background = xstrdup(path);
    sound_background_hook_execute();
    sound_background_loop = loop;
    sound_background_volume_adjustment = volume_adjustment;
    sound_background_duration = sound_music_file_get_duration(filename);
    sound_background_update_duration = 1;

    if (!MIX_SetTrackAudio(sound_music_track, tmp->data) || !MIX_PlayTrack(sound_music_track, 0)) {
        LOG(BUG, "Could not play '%s'. Reason: %s.", path, SDL_GetError());
        free(sound_background);
        sound_background = NULL;
        sound_background_hook_execute();
        return;
    }
    sound_apply_music_volume(volume);

#endif
}

void sound_start_bg_music(const char *filename, int volume, int loop) {
    sound_start_bg_music_internal(filename, volume, loop, 0);
}

/**
 * Stop the background music, if there is any.
 */
void sound_stop_bg_music(void) {
    if (!enabled) {
        return;
    }

    if (sound_background) {
        free(sound_background);
        sound_background = NULL;
#ifdef HAVE_SDL_MIXER
        sound_background_hook_execute();
        MIX_StopTrack(sound_music_track, 0);
#endif
    }
}

/**
 * Pause playing background music.
 */
void sound_pause_music(void) {
#ifdef HAVE_SDL_MIXER
    MIX_PauseTrack(sound_music_track);
    sound_background_update_duration = 0;
#endif
}

/**
 * Resume playing background music.
 */
void sound_resume_music(void) {
#ifdef HAVE_SDL_MIXER
    MIX_ResumeTrack(sound_music_track);
#endif
}

/**
 * Update map's background music.
 * @param bg_music
 * New background music.
 */
void update_map_bg_music(const char *bg_music) {
    if (sound_map_background_disabled) {
        return;
    }

    if (!strcmp(bg_music, "no_music")) {
        sound_stop_bg_music();
    } else {
        int loop = -1, vol = 0;
        char filename[MAX_BUF];

        if (sscanf(bg_music, "%255s %d %d", filename, &loop, &vol) < 1) {
            LOG(BUG, "Bogus background music: '%s'", bg_music);
            return;
        }

        sound_start_bg_music_internal(filename,
                                      setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + vol,
                                      loop,
                                      vol);
    }
}

/**
 * Update volume of the background sound being played.
 */
void sound_update_volume(void) {
    if (!enabled) {
        return;
    }

#ifdef HAVE_SDL_MIXER
    int64_t volume =
        setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + sound_background_volume_adjustment;
    sound_apply_music_volume(volume);
#endif
}

/**
 * Get the currently playing background music, if any.
 * @return
 * Background music file name, NULL if no music is playing.
 */
const char *sound_get_bg_music(void) {
    return sound_background;
}

/**
 * Get the background music base file name.
 * @return
 * The background music base file name, if any. NULL otherwise.
 */
const char *sound_get_bg_music_basename(void) {
    const char *bg_music = sound_background;
    const char *cp;

    if (bg_music && (cp = strrchr(bg_music, '/'))) {
        bg_music = cp + 1;
    }

    return bg_music;
}

/**
 * Get or set ::sound_map_background_disabled.
 * @param val
 * If -1, will return the current value of
 * ::sound_map_background_disabled;
 * any other value will set ::sound_map_background_disabled to that value.
 * @return
 * Value of ::sound_map_background_disabled.
 */
uint8_t sound_map_background(int val) {
    if (val == -1) {
        return sound_map_background_disabled;
    } else {
        sound_map_background_disabled = val;
        return val;
    }
}

/**
 * Get the offset of the background music that is being played.
 * @return
 * The offset.
 */
uint32_t sound_music_get_offset(void) {
    if (!sound_background) {
        return 0;
    }

#ifdef HAVE_SDL_MIXER
    return sound_music_track_get_offset();
#else
    return 0;
#endif
}

/**
 * Check whether the currently playing music can seek to a different
 * position.
 * @return
 * 1 if the music can have playing position changed, 0 otherwise.
 */
int sound_music_can_seek(void) {
    if (!sound_background) {
        return 0;
    }

#ifdef HAVE_SDL_MIXER
    return true;
#endif

    return 0;
}

/**
 * Seek the currently playing background music to the specified offset
 * (in seconds).
 * @param offset
 * Offset to seek to.
 */
void sound_music_seek(uint32_t offset) {
    if (!sound_music_can_seek()) {
        return;
    }

#ifdef HAVE_SDL_MIXER
    Sint64 frames = MIX_TrackMSToFrames(sound_music_track, (Sint64)offset * 1000);
    if (frames < 0 || !MIX_SetTrackPlaybackPosition(sound_music_track, frames)) {
        LOG(BUG, "Could not seek music: %s", SDL_GetError());
    }

#endif
}

/**
 * Get duration of the currently playing background music.
 * @return
 * The duration.
 */
uint32_t sound_music_get_duration() {
#ifdef HAVE_SDL_MIXER
    return sound_background_duration;
#else
    return 0;
#endif
}

void sound_stop_effect(int channel) {
#ifdef HAVE_SDL_MIXER
    if (channel >= 0 && channel < SOUND_EFFECT_TRACKS && sound_effect_tracks[channel] != NULL) {
        MIX_StopTrack(sound_effect_tracks[channel], 0);
    }
#else
    (void)channel;
#endif
}

static void sound_set_effect_position(int channel, int angle, int distance) {
#ifdef HAVE_SDL_MIXER
    if (channel < 0 || channel >= SOUND_EFFECT_TRACKS || sound_effect_tracks[channel] == NULL) {
        return;
    }

    float pan = sinf((float)angle * (float)M_PI / 180.0f);
    float attenuation = 1.0f - MIN(255, MAX(0, distance)) / 255.0f;
    MIX_StereoGains gains = {
        .left = attenuation * (pan > 0.0f ? 1.0f - pan : 1.0f),
        .right = attenuation * (pan < 0.0f ? 1.0f + pan : 1.0f),
    };
    MIX_SetTrackStereo(sound_effect_tracks[channel], &gains);
#else
    (void)channel;
    (void)angle;
    (void)distance;
#endif
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_sound(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint8_t type;
    int loop, volume;
    char filename[MAX_BUF];

    type = packet_reader_read_uint8(&reader);
    packet_reader_read_string(&reader, filename, sizeof(filename));
    loop = packet_reader_read_int8(&reader);
    volume = packet_reader_read_int8(&reader);

    if (type == CMD_SOUND_EFFECT) {
        int8_t x, y;
        int channel;

        x = packet_reader_read_uint8(&reader);
        y = packet_reader_read_uint8(&reader);

        const char *effect = filename;
        if (strcmp(filename, "player_hurt.ogg") == 0) {
            if (cpl.gender == GENDER_FEMALE) {
                effect =
                    sound_female_hurt_effects[rndm(0, arraysize(sound_female_hurt_effects) - 1)];
            } else {
                effect = "doh.ogg";
            }
        }

        channel = sound_play_effect_loop(effect, 100 + volume, loop);

        if (channel != -1) {
            int angle, distance;

            angle = 0;
            distance = (255 * isqrt(POW2(x) + POW2(y))) / MAX_SOUND_DISTANCE;

            if (setting_get_int(OPT_CAT_SOUND, OPT_3D_SOUNDS) &&
                distance >= (255 / MAX_SOUND_DISTANCE) * 2) {
                angle = atan2(-y, x) * (180 / M_PI);
                angle = 90 - angle;
            }

            sound_set_effect_position(channel, angle, distance);
        }
    } else if (type == CMD_SOUND_BACKGROUND) {
        if (!sound_map_background_disabled) {
            sound_start_bg_music_internal(filename,
                                          setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC) + volume,
                                          loop,
                                          volume);
        }
    } else if (type == CMD_SOUND_ABSOLUTE) {
        sound_add_effect(filename, (uint8_t)volume, loop);
    } else {
        LOG(BUG, "Invalid sound type: %d", type);
        return;
    }
}

/**
 * Free an ambient sound effect.
 *
 * The sound effect
 * @param tmp
 * Sound effect to free.
 */
static void sound_ambient_free(sound_ambient_struct *tmp) {
    DL_DELETE(sound_ambient_head, tmp);
    sound_stop_effect(tmp->channel);
    free(tmp);
}

/**
 * Set distance and angle of an ambient sound effect.
 * @param tmp
 * Sound effect.
 */
static void sound_ambient_set_position(sound_ambient_struct *tmp) {
#ifdef HAVE_SDL_MIXER
    int x, y, angle, distance, cx, cy;

    cx = MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH)) / 2;
    cy = MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT)) / 2;

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

    sound_set_effect_position(tmp->channel, angle, distance);
#else
    (void)tmp;
#endif
}

/**
 * Handle map scroll for ambient sound effects. We need to check whether
 * the sound effect is now off-screen and if so, remove it. We also need
 * to adjust the angle and distance effects of the channel the sound
 * effect is playing on.
 * @param xoff
 * X offset.
 * @param yoff
 * Y offset.
 */
void sound_ambient_mapcroll(int xoff, int yoff) {
    sound_ambient_struct *sound_ambient, *tmp;

    DL_FOREACH_SAFE(sound_ambient_head, sound_ambient, tmp) {
        /* Adjust the coordinates. */
        sound_ambient->x -= xoff;
        sound_ambient->y -= yoff;

        /* If the sound effect is now off-screen, remove it. */
        if (sound_ambient->x < 0 ||
            sound_ambient->x >=
                MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH)) ||
            sound_ambient->y < 0 ||
            sound_ambient->y >=
                MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))) {
            sound_ambient_free(sound_ambient);
            continue;
        }

        /* Adjust the distance and angle. */
        sound_ambient_set_position(sound_ambient);
    }
}

/**
 * Stop all ambient sound effects.
 */
void sound_ambient_clear(void) {
    sound_ambient_struct *sound_ambient, *tmp;

    DL_FOREACH_SAFE(sound_ambient_head, sound_ambient, tmp) {
        sound_ambient_free(sound_ambient);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_sound_ambient(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    int tag, tag_old;
    uint8_t x, y;
    sound_ambient_struct *sound_ambient;

    /* Loop through the data, as there may be multiple sound effects. */
    while (pos < len) {
        x = packet_reader_read_uint8(&reader);
        y = packet_reader_read_uint8(&reader);
        tag_old = packet_reader_read_uint32(&reader);
        tag = packet_reader_read_uint32(&reader);

        /* If there is an old tag, the server is telling us to stop
         * playing a sound effect. */
        if (tag_old != 0) {
            DL_FOREACH(sound_ambient_head, sound_ambient) {
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
            packet_reader_read_string(&reader, filename, sizeof(filename));
            volume = packet_reader_read_uint8(&reader);
            max_range = packet_reader_read_uint8(&reader);

            /* Try to start playing the sound effect. */
            channel = sound_play_effect_loop(filename, volume, -1);

            /* Successfully started playing the effect, add it to the
             * list of active sound effects. */
            if (channel != -1) {
                sound_ambient = xcalloc(1, sizeof(*sound_ambient));
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
 * @return
 * 1 if background music is being played, 0 otherwise.
 */
int sound_playing_music(void) {
#ifdef HAVE_SDL_MIXER
    return MIX_TrackPlaying(sound_music_track) || MIX_TrackPaused(sound_music_track);
#else
    return 0;
#endif
}
