/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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

/** The main screen surface. */
SDL_Surface *ScreenSurface;
/** Server's attributes */
struct sockaddr_in insock;
/** Client socket. */
ClientSocket csocket;

/** Our selected server that we want to connect to. */
server_struct *selected_server = NULL;

/** Used with -server command line option. */
static char argServerName[2048];
/** Used with -server command line option. */
static int argServerPort;

/** System time counter in ms since program start. */
uint32 LastTick;

int f_custom_cursor = 0;
int x_custom_cursor = 0;
int y_custom_cursor = 0;

/* update map area */
int map_udate_flag, map_redraw_flag;

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
static uint32 last_keepalive;

static void init_game_data();

/**
 * Initialize game data. */
static void init_game_data(void)
{
	init_map_data(0, 0, 0, 0);
	memset(FaceList, 0, sizeof(struct _face_struct) * MAX_FACE_TILES);

	init_keys();
	memset(&cpl, 0, sizeof(cpl));
	clear_player();

	msg_anim.message[0] = '\0';

	start_anim = NULL;

	argServerName[0] = '\0';
	argServerPort = 13327;

	cpl.state = ST_INIT;
	map_udate_flag = 2;
	map_redraw_flag = 1;
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
	if (cpl.state == ST_INIT)
	{
		cpl.mark_count = -1;

		map_udate_flag = 2;
		sound_start_bg_music("orchestral.ogg", setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), -1);
		clear_map();
		effect_stop();
		cpl.state = ST_META;
	}
	else if (cpl.state == ST_META)
	{
		metaserver_clear_data();
		map_udate_flag = 2;

		metaserver_add("127.0.0.1", 13327, "Localhost", -1, "local", "Localhost. Start server before you try to connect.");

		if (argServerName[0] != '\0')
		{
			metaserver_add(argServerName, argServerPort, argServerName, -1, "user server", "Server from command line -server option.");
		}

		metaserver_get_servers();
		cpl.state = ST_START;
	}
	else if (cpl.state == ST_START)
	{
		map_udate_flag = 2;

		if (csocket.fd != -1)
		{
			socket_close(&csocket);
		}

		clear_map();
		map_redraw_flag = 1;
		clear_player();
		cpl.state = ST_WAITLOOP;
	}
	else if (cpl.state == ST_STARTCONNECT)
	{
		map_udate_flag = 2;
		draw_info_format(COLOR_GREEN, "Trying server %s (%d)...", selected_server->name, selected_server->port);
		last_keepalive = SDL_GetTicks();
		cpl.state = ST_CONNECT;
	}
	else if (cpl.state == ST_CONNECT)
	{
		packet_struct *packet;

		if (!socket_open(&csocket, selected_server->ip, selected_server->port))
		{
			draw_info(COLOR_RED, "Connection failed!");
			cpl.state = ST_START;
			return 1;
		}

		socket_thread_start();
		region_map_clear();

		packet = packet_new(SERVER_CMD_VERSION, 16, 0);
		packet_append_uint32(packet, SOCKET_VERSION);
		socket_send_packet(packet);

		packet = packet_new(SERVER_CMD_SETUP, 256, 256);
		packet_append_uint8(packet, CMD_SETUP_SOUND);
		packet_append_uint8(packet, 1);
		packet_append_uint8(packet, CMD_SETUP_MAPSIZE);
		packet_append_uint8(packet, setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH));
		packet_append_uint8(packet, setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT));
		server_files_setup_add(packet);
		socket_send_packet(packet);

		cpl.state = ST_WAITSETUP;
	}
	else if (cpl.state == ST_REQUEST_FILES)
	{
		if (!server_files_updating())
		{
			server_files_load(1);
			cpl.state = ST_LOGIN;
		}
	}
	else if (cpl.state == ST_WAITFORPLAY)
	{
		clear_map();
		map_udate_flag = 2;
	}

	return 1;
}

/**
 * Play various action sounds. */
static void play_action_sounds(void)
{
	if (cpl.warn_statdown)
	{
		sound_play_effect("warning_statdown.ogg", 100);
		cpl.warn_statdown = 0;
	}

	if (cpl.warn_statup)
	{
		sound_play_effect("warning_statup.ogg", 100);
		cpl.warn_statup = 0;
	}

	if (cpl.warn_hp)
	{
		if (cpl.warn_hp == 2)
		{
			sound_play_effect("warning_hp2.ogg", 100);
		}
		else
		{
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
	if (modes == (SDL_Rect **) 0)
	{
		logger_print(LOG(ERROR), "No video modes available!");
		exit(1);
	}
}

/**
 * Map, animations and other effects. */
static void display_layer1(void)
{
	SDL_FillRect(ScreenSurface, NULL, 0);

	if (cpl.state == ST_PLAY)
	{
		widget_map_render(cur_widget[MAP_ID]);
	}
}

/**
 * Process the widgets if we're playing. */
static void display_layer3(void)
{
	/* Process the widgets */
	if (cpl.state == ST_PLAY)
	{
		process_widgets();
	}
}

/**
 * Show a custom cursor. */
static void DisplayCustomCursor(void)
{
	if (f_custom_cursor == MSCURSOR_MOVE)
	{
		SDL_Surface *cursor;

		cursor = TEXTURE_CLIENT("mouse_cursor_move");
		surface_show(ScreenSurface, x_custom_cursor - (cursor->w / 2), y_custom_cursor - (cursor->h / 2), NULL, cursor);
	}
}

/**
 * The main function.
 * @param argc Number of arguments.
 * @param argv[] Arguments.
 * @return 0 */
int main(int argc, char *argv[])
{
	int done = 0;
	uint32 anim_tick, frame_start_time;
	char version[MAX_BUF];

	toolkit_import(binreloc);
	toolkit_import(logger);
	toolkit_import(math);
	toolkit_import(memory);
	toolkit_import(packet);
	toolkit_import(porting);
	toolkit_import(sha1);
	toolkit_import(string);
	toolkit_import(stringbuffer);

	logger_open_log(LOG_FILE);

	upgrader_init();
	settings_init();
	init_game_data();
	curl_init();

	while (argc > 1)
	{
		argc--;

		if (strcmp(argv[argc - 1], "-port") == 0)
		{
			argServerPort = atoi(argv[argc]);
			argc--;
		}
		else if (strcmp(argv[argc - 1], "-server") == 0)
		{
			strcpy(argServerName, argv[argc]);

			if (strchr(argv[argc], ':') != '\0')
			{
				argServerPort = atoi(strrchr(argv[argc], ':') + 1);
				*strchr(argServerName, ':') = '\0';
			}

			argc--;
		}
		else if (!strcmp(argv[argc], "-nometa"))
		{
			metaserver_disable();
		}
		else if (!strcmp(argv[argc], "-text-debug"))
		{
			text_enable_debug();
		}
		else
		{
			logger_print(LOG(INFO), "Usage: %s [-server <name>] [-port <num>]", argv[0]);
			exit(1);
		}
	}

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
	{
		logger_print(LOG(ERROR), "Couldn't initialize SDL: %s", SDL_GetError());
		exit(1);
	}

	/* Start the system after starting SDL */
	system_start();
	list_vid_modes();

	if (!video_set_size())
	{
		logger_print(LOG(ERROR), "Couldn't set video size: %s", SDL_GetError());
		exit(1);
	}

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
	init_widgets_fromCurrent();

	sound_start_bg_music("orchestral.ogg", setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), -1);

	draw_info_format(COLOR_HGOLD, "Welcome to Atrinik version %s.", package_get_version_full(version, sizeof(version)));
	draw_info(COLOR_GREEN, "Init network...");

	if (!socket_initialize())
	{
		exit(1);
	}

	if (!clipboard_init())
	{
		draw_info(COLOR_RED, "Failed to initialize clipboard support, clipboard actions will not be possible.");
	}

	textwin_init();
	fps_init();
	settings_apply();
	scrollbar_init();
	button_init();

	LastTick = anim_tick = SDL_GetTicks();

	while (!done)
	{
		frame_start_time = SDL_GetTicks();
		done = Event_PollInputDevice();

		/* Have we been shutdown? */
		if (handle_socket_shutdown())
		{
			cpl.state = ST_INIT;
			/* Make sure no popup is visible. */
			popup_destroy_all();
			continue;
		}

		if (cpl.state > ST_CONNECT)
		{
			/* Send keepalive command every 2 minutes. */
			if (SDL_GetTicks() - last_keepalive > (2 * 60) * 1000)
			{
				packet_struct *packet;

				packet = packet_new(SERVER_CMD_KEEPALIVE, 0, 0);
				socket_send_packet(packet);
				last_keepalive = SDL_GetTicks();
			}

			DoClient();
		}

		/* If not connected, walk through connection chain and/or wait for action */
		if (cpl.state != ST_PLAY)
		{
			if (!game_status_chain())
			{
				logger_print(LOG(ERROR), "Error connecting: cpl.state: %d  SocketError: %d", cpl.state, socket_get_error());
			}
		}

		if (SDL_GetAppState() & SDL_APPACTIVE)
		{
			if (cpl.state == ST_PLAY)
			{
				if (LastTick - anim_tick > 110)
				{
					anim_tick = LastTick;
					animate_objects();
					map_udate_flag = 2;
				}

				play_action_sounds();
			}

			map_udate_flag = 2;

			if (map_udate_flag > 0)
			{
				player_doll_update_items();
				display_layer1();
				display_layer3();

				if (cpl.state != ST_PLAY)
				{
					SDL_FillRect(ScreenSurface, NULL, 0);
				}

				map_udate_flag = 0;
			}

			/* Show the currently dragged item. */
			if (event_dragging_check())
			{
				int mx, my;

				SDL_GetMouseState(&mx, &my);
				object_show_centered(object_find(cpl.dragging_tag), mx, my);
			}

			if (cpl.state <= ST_WAITFORPLAY)
			{
				main_screen_render();
			}

			if (f_custom_cursor)
			{
				DisplayCustomCursor();
			}

			popup_render_all();

			tooltip_show();
		}

		SDL_Flip(ScreenSurface);

		LastTick = SDL_GetTicks();

		if (!setting_get_int(OPT_CAT_CLIENT, OPT_SLEEP_TIME))
		{
			uint32 elapsed_time = SDL_GetTicks() - frame_start_time, wanted_fps;

			wanted_fps = FRAMES_PER_SECOND;

			if (!(SDL_GetAppState() & SDL_APPACTIVE))
			{
				wanted_fps = 2;
			}

			if (elapsed_time < 1000 / wanted_fps)
			{
				SDL_Delay(1000 / wanted_fps - elapsed_time);
			}
		}
		else
		{
			SDL_Delay(setting_get_int(OPT_CAT_CLIENT, OPT_SLEEP_TIME));
		}

		fps_do();
	}

	system_end();

	return 0;
}
