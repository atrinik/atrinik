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
 * Client main related functions. */

#include <global.h>
#include <gitversion.h>
#include <region_map.h>
#include <packet.h>
#include <toolkit_string.h>

/** The main screen surface. */
SDL_Surface *ScreenSurface;
/** Server's attributes */
struct sockaddr_in insock;
/** Client socket. */
ClientSocket csocket;

/** Our selected server that we want to connect to. */
server_struct *selected_server = NULL;

/** System time counter in ms since program start. */
uint32_t LastTick;

texture_struct *cursor_texture;
int cursor_x = -1;
int cursor_y = -1;

/* update map area */
int map_redraw_flag;
int minimap_redraw_flag;

/** The stored "anim commands" we created out of anims.tmp. */
_anim_table *anim_table = NULL;
Animations *animations = NULL;
/** Number of animations. */
size_t animations_num = 0;

/** Size of the screen. */
struct screensize *Screensize;

/** Face data */
_face_struct FaceList[MAX_FACE_TILES];

/** The message animation structure. */
struct msg_anim_struct msg_anim;
/** Last time we sent keepalive command. */
static uint32_t last_keepalive;

/**
 * Command line option settings. */
clioption_settings_struct clioption_settings;

/**
 * Used to keep track of keepalive commands.
 */
typedef struct keepalive_data_struct {
    struct keepalive_data_struct *next; ///< Next keepalive data.

    uint32_t ticks; ///< When the keepalive command was sent.
    uint32_t id; ///< ID of the keepalive command.
} keepalive_data_struct;

static keepalive_data_struct *keepalive_data; ///< Keepalive data.
static int keepalive_id; ///< UID for sending keepalives.
static int keepalive_ping; ///< Last keepalive ping time.
static int keepalive_ping_avg; ///< Average keepalive ping time.
static int keepalive_ping_num; ///< Number of keepalive pings.

/**
 * Reset keepalive data.
 */
static void keepalive_reset(void)
{
    keepalive_data_struct *keepalive, *tmp;

    last_keepalive = SDL_GetTicks();
    keepalive_id = 0;
    keepalive_ping = 0;
    keepalive_ping_avg = 0;
    keepalive_ping_num = 0;

    LL_FOREACH_SAFE(keepalive_data, keepalive, tmp)
    {
        LL_DELETE(keepalive_data, keepalive);
        efree(keepalive);
    }
}

/**
 * Send a keepalive packet.
 */
static void keepalive_send(void)
{
    keepalive_data_struct *keepalive;
    packet_struct *packet;

    keepalive = emalloc(sizeof(*keepalive));
    keepalive->ticks = SDL_GetTicks();
    keepalive->id = ++keepalive_id;
    LL_PREPEND(keepalive_data, keepalive);

    packet = packet_new(SERVER_CMD_KEEPALIVE, 0, 0);
    packet_append_uint32(packet, keepalive->id);
    packet_enable_ndelay(packet);
    socket_send_packet(packet);
    last_keepalive = keepalive->ticks;
}

/**
 * Display ping statistics.
 */
void keepalive_ping_stats(void)
{
    draw_info(COLOR_WHITE, "\nPing statistics this session:");
    draw_info_format(COLOR_WHITE, "Keepalive TX: %d, RX: %d, missed: %d",
            keepalive_id, keepalive_ping_num,
            keepalive_id - keepalive_ping_num);
    draw_info_format(COLOR_WHITE, "Average ping: %d", keepalive_ping_avg);
    draw_info_format(COLOR_WHITE, "Last ping: %d", keepalive_ping);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_keepalive(uint8_t *data, size_t len, size_t pos)
{
    uint32_t id, ticks;
    keepalive_data_struct *keepalive, *tmp;

    id = packet_to_uint32(data, len, &pos);
    ticks = SDL_GetTicks();

    LL_FOREACH_SAFE(keepalive_data, keepalive, tmp)
    {
        if (id == keepalive->id) {
            LL_DELETE(keepalive_data, keepalive);

            keepalive_ping = ticks - keepalive->ticks;
            keepalive_ping_num++;
            keepalive_ping_avg = keepalive_ping_avg + ((keepalive_ping -
                    keepalive_ping_avg) / keepalive_ping_num);
            efree(keepalive);

            return;
        }
    }

    LOG(BUG, "Received unknown keepalive ID: %d", id);
}

/**
 * Initialize game data. */
static void init_game_data(void)
{
    init_map_data(0, 0, 0, 0);
    memset(FaceList, 0, sizeof(struct _face_struct) * MAX_FACE_TILES);

    init_keys();
    memset(&cpl, 0, sizeof(cpl));
    object_init();
    clear_player();

    memset(&MapData, 0, sizeof(MapData));

    msg_anim.message[0] = '\0';

    cpl.state = ST_INIT;
    map_redraw_flag = minimap_redraw_flag = 1;
    csocket.fd = -1;

    metaserver_init();
    spells_init();
    skills_init();
}

/**
 * Game status chain.
 * @return 1. */
static int game_status_chain(void)
{
    if (cpl.state == ST_INIT) {
        clear_map(true);
        effect_stop();
        sound_ambient_clear();
        cpl.state = ST_META;
    } else if (cpl.state == ST_META) {
        size_t i, pos;
        char host[MAX_BUF], port[MAX_BUF];
        uint16_t port_num;

        metaserver_clear_data();

        metaserver_add("127.0.0.1", 13327, "Localhost", "localhost", -1, "local", "Localhost. Start server before you try to connect.");

        for (i = 0; i < clioption_settings.servers_num; i++) {
            pos = 0;
            string_get_word(clioption_settings.servers[i], &pos, ':', host, sizeof(host), 0);
            string_get_word(clioption_settings.servers[i], &pos, ':', port, sizeof(port), 0);
            port_num = atoi(port);
            metaserver_add(host, port_num ? port_num : 13327, host, host, -1, "user server", "Server from command line --server option.");
        }

        metaserver_get_servers();
        cpl.state = ST_START;
    } else if (cpl.state == ST_START) {
        if (csocket.fd != -1) {
            socket_close(&csocket);
        }

        clear_map(true);
        map_redraw_flag = minimap_redraw_flag = 1;
        cpl.state = ST_WAITLOOP;
    } else if (cpl.state == ST_STARTCONNECT) {
        draw_info_format(COLOR_GREEN, "Trying server %s (%d)...", selected_server->name, selected_server->port);
        keepalive_reset();
        cpl.state = ST_CONNECT;
    } else if (cpl.state == ST_CONNECT) {
        packet_struct *packet;

        if (!socket_open(&csocket, selected_server->ip, selected_server->port)) {
            draw_info(COLOR_RED, "Connection failed!");
            cpl.state = ST_START;
            return 1;
        }

        socket_thread_start();
        clear_player();

        packet = packet_new(SERVER_CMD_VERSION, 16, 0);
        packet_append_uint32(packet, SOCKET_VERSION);
        socket_send_packet(packet);

        keepalive_send();

        cpl.state = ST_WAITVERSION;
    } else if (cpl.state == ST_VERSION) {
        packet_struct *packet;

        packet = packet_new(SERVER_CMD_SETUP, 256, 256);
        packet_append_uint8(packet, CMD_SETUP_SOUND);
        packet_append_uint8(packet, 1);
        packet_append_uint8(packet, CMD_SETUP_MAPSIZE);
        packet_append_uint8(packet, setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH));
        packet_append_uint8(packet, setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT));
        packet_append_uint8(packet, CMD_SETUP_DATA_URL);
        packet_append_string_terminated(packet, "");
        socket_send_packet(packet);

        cpl.state = ST_WAITSETUP;
    } else if (cpl.state == ST_REQUEST_FILES_LISTING) {
        /* Retrieve the server files listing. */
        server_files_listing_retrieve();
        /* Load up the existing server files. */
        server_files_load(0);
        cpl.state = ST_WAITREQUEST_FILES_LISTING;
    } else if (cpl.state == ST_WAITREQUEST_FILES_LISTING) {
        if (server_files_listing_processed()) {
            cpl.state = ST_REQUEST_FILES;
        }
    } else if (cpl.state == ST_REQUEST_FILES) {
        if (server_files_processed()) {
            server_files_load(1);
            cpl.state = ST_LOGIN;
        }
    } else if (cpl.state == ST_WAITFORPLAY) {
        clear_map(true);
    }

    return 1;
}

/**
 * Play various action sounds. */
static void play_action_sounds(void)
{
    if (cpl.warn_statdown) {
        sound_play_effect("warning_statdown.ogg", 100);
        cpl.warn_statdown = 0;
    }

    if (cpl.warn_statup) {
        sound_play_effect("warning_statup.ogg", 100);
        cpl.warn_statup = 0;
    }

    if (cpl.warn_hp) {
        if (cpl.warn_hp == 2) {
            sound_play_effect("warning_hp2.ogg", 100);
        } else {
            sound_play_effect("warning_hp.ogg", 100);
        }

        cpl.warn_hp = 0;
    }
}

/**
 * List video modes available. */
void list_vid_modes(void)
{
    SDL_Rect **modes;

    /* Get available fullscreen/hardware modes */
    modes = SDL_ListModes(NULL, SDL_HWACCEL);

    /* Check if there are any modes available */
    if (modes == (SDL_Rect **) 0) {
        LOG(ERROR, "No video modes available!");
        exit(1);
    }
}

/**
 * Hook for detecting background music changes. */
static void sound_background_hook(void)
{
    WIDGET_REDRAW_ALL(MPLAYER_ID);
}

void clioption_settings_deinit(void)
{
    size_t i;

    for (i = 0; i < clioption_settings.servers_num; i++) {
        efree(clioption_settings.servers[i]);
    }

    if (clioption_settings.servers) {
        efree(clioption_settings.servers);
    }

    for (i = 0; i < clioption_settings.metaservers_num; i++) {
        efree(clioption_settings.metaservers[i]);
    }

    if (clioption_settings.metaservers) {
        efree(clioption_settings.metaservers);
    }

    for (i = 0; i < arraysize(clioption_settings.connect); i++) {
        if (clioption_settings.connect[i]) {
            efree(clioption_settings.connect[i]);
        }
    }

    if (clioption_settings.game_news_url) {
        efree(clioption_settings.game_news_url);
    }
}

static void clioptions_option_server(const char *arg)
{
    clioption_settings.servers = erealloc(clioption_settings.servers, sizeof(*clioption_settings.servers) * (clioption_settings.servers_num + 1));
    clioption_settings.servers[clioption_settings.servers_num] = estrdup(arg);
    clioption_settings.servers_num++;
}

static void clioptions_option_metaserver(const char *arg)
{
    clioption_settings.metaservers = erealloc(clioption_settings.metaservers, sizeof(*clioption_settings.metaservers) * (clioption_settings.metaservers_num + 1));
    clioption_settings.metaservers[clioption_settings.metaservers_num] = estrdup(arg);
    clioption_settings.metaservers_num++;
}

static void clioptions_option_connect(const char *arg)
{
    size_t pos, idx;
    char word[MAX_BUF];

    pos = idx = 0;

    while (string_get_word(arg, &pos, ':', word, sizeof(word), 0)) {
        clioption_settings.connect[idx] = estrdup(word);
        string_whitespace_trim(clioption_settings.connect[idx]);
        idx++;
    }
}

static void clioptions_option_nometa(const char *arg)
{
    metaserver_disable();
}

static void clioptions_option_text_debug(const char *arg)
{
    text_enable_debug();
}

static void clioptions_option_widget_render_debug(const char *arg)
{
    widget_render_enable_debug();
}

static void clioptions_option_game_news_url(const char *arg)
{
    clioption_settings.game_news_url = estrdup(arg);
}

static void clioptions_option_reconnect(const char *arg)
{
    clioption_settings.reconnect = 1;
}

static void clioptions_option_logger_filter_stdout(const char *arg)
{
    logger_set_filter_stdout(arg);
}

static void clioptions_option_logger_filter_logfile(const char *arg)
{
    logger_set_filter_logfile(arg);
}

/**
 * The main function.
 * @param argc Number of arguments.
 * @param argv[] Arguments.
 * @return 0 */
int main(int argc, char *argv[])
{
    char *path;
    int done = 0, update, frames;
    uint32_t anim_tick, frame_start_time, elapsed_time, fps_limit,
            last_frame_ticks, last_memory_check;
    int fps_limits[] = {30, 60, 120, 0};
    char version[MAX_BUF], buf[HUGE_BUF];

    toolkit_import(signals);

    toolkit_import(binreloc);
    toolkit_import(clioptions);
    toolkit_import(colorspace);
    toolkit_import(datetime);
    toolkit_import(logger);
    toolkit_import(math);
    toolkit_import(memory);
    toolkit_import(packet);
    toolkit_import(porting);
    toolkit_import(sha1);
    toolkit_import(string);
    toolkit_import(stringbuffer);
    toolkit_import(x11);

    clioptions_add(
            "server",
            NULL,
            clioptions_option_server,
            1,
            "",
            ""
            );

    clioptions_add(
            "metaserver",
            NULL,
            clioptions_option_metaserver,
            1,
            "",
            ""
            );

    clioptions_add(
            "connect",
            NULL,
            clioptions_option_connect,
            1,
            "",
            ""
            );

    clioptions_add(
            "nometa",
            NULL,
            clioptions_option_nometa,
            0,
            "",
            ""
            );

    clioptions_add(
            "text_debug",
            NULL,
            clioptions_option_text_debug,
            0,
            "",
            ""
            );

    clioptions_add(
            "widget_render_debug",
            NULL,
            clioptions_option_widget_render_debug,
            0,
            "",
            ""
            );

    clioptions_add(
            "tiles_debug",
            NULL,
            clioptions_option_tiles_debug,
            0,
            "",
            ""
            );

    clioptions_add(
            "game_news_url",
            NULL,
            clioptions_option_game_news_url,
            0,
            "",
            ""
            );

    clioptions_add(
            "reconnect",
            NULL,
            clioptions_option_reconnect,
            0,
            "",
            ""
            );

    clioptions_add(
            "logger_filter_stdout",
            NULL,
            clioptions_option_logger_filter_stdout,
            1,
            "",
            ""
            );

    clioptions_add(
            "logger_filter_logfile",
            NULL,
            clioptions_option_logger_filter_logfile,
            1,
            "",
            ""
            );

    memset(&clioption_settings, 0, sizeof(clioption_settings));
    path = file_path("client.cfg", "r");
    clioptions_load_config(path, "[General]");
    efree(path);
    path = file_path("client-custom.cfg", "r");

    if (path_exists(path)) {
        clioptions_load_config(path, "[General]");
    }

    efree(path);

    clioptions_parse(argc, argv);

    logger_open_log(LOG_FILE);

    upgrader_init();
    settings_init();
    init_game_data();
    curl_init();

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        LOG(ERROR, "Couldn't initialize SDL: %s", SDL_GetError());
        exit(1);
    }

    /* Start the system after starting SDL */
    system_start();
    video_init();
    sprite_init_system();
    SDL_EnableUNICODE(1);
    text_init();
    texture_init();
    sound_init();
    cmd_aliases_init();
    keybind_load();
    load_mapdef_dat();
    read_bmaps_p0();
    server_files_init();
    toolkit_widget_init();

    snprintf(VS(buf), "Welcome to Atrinik version %s",
            package_get_version_full(version, sizeof(version)));
#ifdef GITVERSION
    snprintfcat(VS(buf), "%s", " (" STRINGIFY(GITBRANCH) "/"
            STRINGIFY(GITVERSION) " by " STRINGIFY(GITAUTHOR) ")");
#endif
    draw_info(COLOR_HGOLD, buf);

    if (!socket_initialize()) {
        exit(1);
    }

    if (!x11_clipboard_register_events()) {
        draw_info(COLOR_RED, "Failed to initialize clipboard support, clipboard actions will not be possible.");
    }

    settings_apply();
    scrollbar_init();
    button_init();

    atexit(system_end);

    SDL_ShowCursor(0);
    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_default");

    sound_background_hook_register(sound_background_hook);

    LastTick = anim_tick = last_frame_ticks = last_memory_check =
            SDL_GetTicks();
    frames = 0;

    while (!done) {
        frame_start_time = SDL_GetTicks();
        done = Event_PollInputDevice();

        /* Have we been shutdown? */
        if (handle_socket_shutdown()) {
            if (cpl.state != ST_STARTCONNECT) {
                cpl.state = ST_INIT;
                /* Make sure no popup is visible. */
                popup_destroy_all();
            }

            continue;
        }

        /* Check the memory every 10 seconds. */
        if (SDL_GetTicks() - last_memory_check > 10 * 1000) {
            memory_check_all();
            last_memory_check = SDL_GetTicks();
        }

        if (cpl.state > ST_CONNECT) {
            /* Send keepalive command every 2 minutes. */
            if (SDL_GetTicks() - last_keepalive > (2 * 60) * 1000) {
                keepalive_send();
            }

            DoClient();
        }

        /* If not connected, walk through connection chain and/or wait for
         * action */
        if (cpl.state != ST_PLAY) {
            if (!game_status_chain()) {
                LOG(BUG, "Error connecting: cpl.state: %d  SocketError: %d", cpl.state, socket_get_error());
            }
        } else if (SDL_GetAppState() & SDL_APPACTIVE) {
            if (LastTick - anim_tick > 125) {
                anim_tick = LastTick;
                animate_objects();
                map_animate();
            }

            play_action_sounds();
        }

        update = 0;

        if (!(SDL_GetAppState() & SDL_APPACTIVE)) {
        } else if (cpl.state == ST_PLAY) {
            static int old_cursor_x = -1, old_cursor_y = -1;

            if (widgets_need_redraw()) {
                update = 1;
            } else if (cursor_x != old_cursor_x || cursor_y != old_cursor_y) {
                update = 1;
                old_cursor_x = cursor_x;
                old_cursor_y = cursor_y;
            } else if (event_dragging_need_redraw()) {
                update = 1;
            } else if (popup_need_redraw()) {
                update = 1;
            } else if (tooltip_need_redraw()) {
                update = 1;
            } else if (map_redraw_flag || minimap_redraw_flag) {
                update = 1;
            } else if (map_anims_need_redraw()) {
                update = 1;
            }
        } else {
            update = 1;
        }

        if (update) {
            SDL_FillRect(ScreenSurface, NULL, 0);
        }

        if (cpl.state <= ST_WAITFORPLAY) {
            intro_show();
        } else if (cpl.state == ST_PLAY) {
            process_widgets(update);
        }

        popup_render_all();
        tooltip_show();

        /* Show the currently dragged item. */
        if (event_dragging_check()) {
            int mx, my;

            SDL_GetMouseState(&mx, &my);
            object_show_centered(ScreenSurface, object_find(cpl.dragging_tag),
                    mx, my, INVENTORY_ICON_SIZE, INVENTORY_ICON_SIZE);
        }

        if (cursor_x != -1 && cursor_y != -1 &&
                SDL_GetAppState() & SDL_APPMOUSEFOCUS) {
            surface_show(ScreenSurface,
                    cursor_x - (texture_surface(cursor_texture)->w / 2),
                    cursor_y - (texture_surface(cursor_texture)->h / 2),
                    NULL, texture_surface(cursor_texture));
        }

        texture_gc();
        font_gc();

        if (update) {
            SDL_Flip(ScreenSurface);
        }

        LastTick = SDL_GetTicks();

        if (SDL_GetAppState() & SDL_APPACTIVE) {
            frames++;

            if (LastTick - last_frame_ticks >= 1000) {
                last_frame_ticks = LastTick;
                effect_frames(frames);
                frames = 0;
            }
        }

        elapsed_time = SDL_GetTicks() - frame_start_time;
        fps_limit = fps_limits[setting_get_int(OPT_CAT_CLIENT, OPT_FPS_LIMIT)];

        if (fps_limit != 0) {
            while (1) {
                if (elapsed_time < 1000 / fps_limit) {
                    SDL_Delay(MAX(1, 1000 / fps_limit - elapsed_time));

                    if (!(SDL_GetAppState() & SDL_APPACTIVE) && SDL_GetTicks() - frame_start_time < 1000) {
                        SDL_PumpEvents();
                        continue;
                    }
                }

                break;
            }
        }
    }

    return 0;
}
