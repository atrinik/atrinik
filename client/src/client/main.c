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
 * Client main related functions. */

#include <global.h>

/** The main screen surface. */
SDL_Surface *ScreenSurface;
/** Server's attributes */
struct sockaddr_in insock;
/** Client socket. */
ClientSocket csocket;

struct _fire_mode fire_mode_tab[FIRE_MODE_INIT];
int RangeFireMode;

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

/** Global status identifier. */
_game_status GameStatus;
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

/** All the bitmaps. */
static _bitmap_name bitmap_name[BITMAP_INIT] =
{
	{"intro.png", PIC_TYPE_DEFAULT},

	{"player_doll_bg.png", PIC_TYPE_TRANS},
	{"login_inp.png", PIC_TYPE_DEFAULT},
	{"invslot.png", PIC_TYPE_TRANS},

	{"hp.png", PIC_TYPE_TRANS},
	{"sp.png", PIC_TYPE_TRANS},
	{"grace.png", PIC_TYPE_TRANS},
	{"food.png", PIC_TYPE_TRANS},
	{"hp_back.png", PIC_TYPE_DEFAULT},
	{"sp_back.png", PIC_TYPE_DEFAULT},
	{"grace_back.png", PIC_TYPE_DEFAULT},
	{"food_back.png", PIC_TYPE_DEFAULT},

	{"apply.png", PIC_TYPE_DEFAULT},
	{"unpaid.png", PIC_TYPE_DEFAULT},
	{"cursed.png", PIC_TYPE_DEFAULT},
	{"damned.png", PIC_TYPE_DEFAULT},
	{"lock.png", PIC_TYPE_DEFAULT},
	{"magic.png", PIC_TYPE_DEFAULT},
	{"fire_ready.png", PIC_TYPE_DEFAULT},

	{"range.png", PIC_TYPE_TRANS},
	{"range_marker.png", PIC_TYPE_TRANS},
	{"range_skill.png", PIC_TYPE_TRANS},
	{"range_skill_no.png", PIC_TYPE_TRANS},
	{"range_throw.png", PIC_TYPE_TRANS},
	{"range_throw_no.png", PIC_TYPE_TRANS},
	{"range_tool.png", PIC_TYPE_TRANS},
	{"range_tool_no.png", PIC_TYPE_TRANS},
	{"range_wizard.png", PIC_TYPE_TRANS},
	{"range_wizard_no.png", PIC_TYPE_TRANS},

	{"cmark_start.png", PIC_TYPE_TRANS},
	{"cmark_end.png", PIC_TYPE_TRANS},
	{"cmark_middle.png", PIC_TYPE_TRANS},

	{"number.png", PIC_TYPE_DEFAULT},
	{"invslot_u.png", PIC_TYPE_TRANS},

	{"death.png", PIC_TYPE_TRANS},
	{"sleep.png", PIC_TYPE_TRANS},
	{"confused.png", PIC_TYPE_TRANS},
	{"paralyzed.png", PIC_TYPE_TRANS},
	{"scared.png", PIC_TYPE_TRANS},
	{"blind.png", PIC_TYPE_TRANS},

	{"quickslots.png", PIC_TYPE_DEFAULT},
	{"quickslotsv.png", PIC_TYPE_DEFAULT},
	{"inventory.png", PIC_TYPE_DEFAULT},
	{"inventory_bg.png", PIC_TYPE_DEFAULT},

	{"exp_border.png", PIC_TYPE_DEFAULT},
	{"exp_line.png", PIC_TYPE_DEFAULT},
	{"exp_bubble.png", PIC_TYPE_TRANS},
	{"exp_bubble2.png", PIC_TYPE_TRANS},

	{"main_stats.png", PIC_TYPE_DEFAULT},
	{"below.png", PIC_TYPE_DEFAULT},

	{"target_attack.png", PIC_TYPE_TRANS},
	{"target_talk.png", PIC_TYPE_TRANS},
	{"target_normal.png", PIC_TYPE_TRANS},

	{"warn_hp.png", PIC_TYPE_DEFAULT},
	{"warn_food.png", PIC_TYPE_DEFAULT},

	{"range_buttons_off.png", PIC_TYPE_DEFAULT},
	{"range_buttons_left.png", PIC_TYPE_DEFAULT},
	{"range_buttons_right.png", PIC_TYPE_DEFAULT},

	{"target_hp.png", PIC_TYPE_DEFAULT},
	{"target_hp_b.png", PIC_TYPE_DEFAULT},

	{"textwin_mask.png", PIC_TYPE_DEFAULT},

	{"exp_skill_border.png", PIC_TYPE_DEFAULT},
	{"exp_skill_line.png", PIC_TYPE_DEFAULT},
	{"exp_skill_bubble.png", PIC_TYPE_TRANS},

	{"trapped.png", PIC_TYPE_TRANS},
	{"pray.png", PIC_TYPE_TRANS},
	{"book.png", PIC_TYPE_ALPHA},
	{"book_border.png", PIC_TYPE_ALPHA},
	{"region_map.png", PIC_TYPE_ALPHA},
	{"invslot_marked.png", PIC_TYPE_TRANS},
	{"mouse_cursor_move.png", PIC_TYPE_TRANS},
	{"resist_bg.png", PIC_TYPE_DEFAULT},
	{"main_level_bg.png",PIC_TYPE_DEFAULT},
	{"skill_exp_bg.png",PIC_TYPE_DEFAULT},
	{"regen_bg.png",PIC_TYPE_DEFAULT},
	{"skill_lvl_bg.png",PIC_TYPE_DEFAULT},
	{"menu_buttons.png",PIC_TYPE_DEFAULT},
	{"player_info_bg.png",PIC_TYPE_DEFAULT},
	{"target_bg.png", PIC_TYPE_DEFAULT},
	{"textinput.png", PIC_TYPE_DEFAULT},

	{"square_highlight.png", PIC_TYPE_DEFAULT},
	{"servers_bg.png", PIC_TYPE_DEFAULT},
	{"servers_bg_over.png", PIC_TYPE_TRANS},
	{"news_bg.png", PIC_TYPE_DEFAULT},
	{"eyes.png", PIC_TYPE_DEFAULT},
	{"popup.png", PIC_TYPE_ALPHA},
	{"button_round.png", PIC_TYPE_ALPHA},
	{"button_round_down.png", PIC_TYPE_ALPHA},
	{"button_round_hover.png", PIC_TYPE_ALPHA},
	{"button_rect.png", PIC_TYPE_DEFAULT},
	{"button_rect_hover.png", PIC_TYPE_DEFAULT},
	{"button_rect_down.png", PIC_TYPE_DEFAULT},
	{"map_marker.png", PIC_TYPE_ALPHA},
	{"loading_off.png", PIC_TYPE_DEFAULT},
	{"loading_on.png", PIC_TYPE_DEFAULT},
	{"button.png", PIC_TYPE_ALPHA},
	{"button_down.png", PIC_TYPE_ALPHA},
	{"button_hover.png", PIC_TYPE_ALPHA},
	{"checkbox.png", PIC_TYPE_TRANS},
	{"checkbox_on.png", PIC_TYPE_TRANS},
	{"content.png", PIC_TYPE_DEFAULT},
	{"icon_music.png", PIC_TYPE_ALPHA},
	{"icon_magic.png", PIC_TYPE_ALPHA},
	{"icon_skill.png", PIC_TYPE_ALPHA},
	{"icon_party.png", PIC_TYPE_ALPHA},
	{"icon_map.png", PIC_TYPE_ALPHA},
	{"icon_cogs.png", PIC_TYPE_ALPHA},
	{"icon_quest.png", PIC_TYPE_ALPHA},
	{"fps.png", PIC_TYPE_DEFAULT},
	{"interface.png", PIC_TYPE_ALPHA},
	{"interface_border.png", PIC_TYPE_ALPHA},
	{"button_large.png", PIC_TYPE_ALPHA},
	{"button_large_down.png", PIC_TYPE_ALPHA},
	{"button_large_hover.png", PIC_TYPE_ALPHA},
	{"button_round_large.png", PIC_TYPE_ALPHA},
	{"button_round_large_down.png", PIC_TYPE_ALPHA},
	{"button_round_large_hover.png", PIC_TYPE_ALPHA}
};

/** The actual bitmaps. */
_Sprite *Bitmaps[BITMAP_INIT];

static void init_game_data();
static void delete_player_lists();
static int load_bitmap(int index);

/**
 * Clear player lists like skill list, spell list, etc. */
static void delete_player_lists(void)
{
	size_t i;

	for (i = 0; i < FIRE_MODE_INIT; i++)
	{
		fire_mode_tab[i].amun = FIRE_ITEM_NO;
		fire_mode_tab[i].item = FIRE_ITEM_NO;
		fire_mode_tab[i].skill = NULL;
		fire_mode_tab[i].spell = NULL;
		fire_mode_tab[i].name[0] = '\0';
	}
}

/**
 * Initialize game data. */
static void init_game_data(void)
{
	size_t i;

	memset(&fire_mode_tab, 0, sizeof(fire_mode_tab));

	init_map_data(0, 0, 0, 0);

	for (i = 0; i < BITMAP_INIT; i++)
	{
		Bitmaps[i] = NULL;
	}

	memset(FaceList, 0, sizeof(struct _face_struct) * MAX_FACE_TILES);

	init_keys();
	clear_player();
	text_input_clear();

	msg_anim.message[0] = '\0';

	start_anim = NULL;

	argServerName[0] = '\0';
	argServerPort = 13327;

	GameStatus = GAME_STATUS_INIT;
	map_udate_flag = 2;
	map_redraw_flag = 1;
	text_input_string_flag = 0;
	text_input_string_end_flag = 0;
	text_input_string_esc_flag = 0;
	csocket.fd = -1;
	RangeFireMode = 0;

	delete_player_lists();
	metaserver_init();

	SRANDOM(time(NULL));
}

/** @cond */
#ifndef WIN32

static void fatal_signal(int make_core)
{
	if (make_core)
	{
		abort();
	}

	exit(0);
}

static void rec_sigsegv(int i)
{
	(void) i;

	LOG(llevInfo, "\nSIGSEGV received.\n");
	fatal_signal(1);
}

static void rec_sighup(int i)
{
	(void) i;

	LOG(llevInfo, "\nSIGHUP received\n");
	exit(0);
}

static void rec_sigquit(int i)
{
	(void) i;

	LOG(llevInfo, "\nSIGQUIT received\n");
	fatal_signal(1);
}

static void rec_sigterm(int i)
{
	(void) i;

	LOG(llevInfo, "\nSIGTERM received\n");
	fatal_signal(0);
}
#endif

/** @endcond */

/**
 * Initialize the signal handlers. */
static void init_signals(void)
{
#ifndef WIN32
	signal(SIGHUP, rec_sighup);
	signal(SIGQUIT, rec_sigquit);
	signal(SIGSEGV, rec_sigsegv);
	signal(SIGTERM, rec_sigterm);
#endif
}

/**
 * Game status chain.
 * @return 1. */
static int game_status_chain(void)
{
	if (GameStatus == GAME_STATUS_INIT)
	{
		cpl.mark_count = -1;
		LOG(llevInfo, "GAMES_STATUS_INIT_1\n");

		map_udate_flag = 2;
		delete_player_lists();
		LOG(llevInfo, "GAMES_STATUS_INIT_2\n");
		sound_start_bg_music("orchestral.ogg", setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), -1);
		clear_map();
		effect_stop();
		LOG(llevInfo, "GAMES_STATUS_INIT_3\n");
		LOG(llevInfo, "GAMES_STATUS_INIT_4\n");
		GameStatus = GAME_STATUS_META;
	}
	else if (GameStatus == GAME_STATUS_META)
	{
		metaserver_clear_data();
		map_udate_flag = 2;

		metaserver_add("127.0.0.1", 13327, "Localhost", -1, "local", "Localhost. Start server before you try to connect.");

		if (argServerName[0] != '\0')
		{
			metaserver_add(argServerName, argServerPort, argServerName, -1, "user server", "Server from command line -server option.");
		}

		metaserver_get_servers();
		GameStatus = GAME_STATUS_START;
	}
	else if (GameStatus == GAME_STATUS_START)
	{
		map_udate_flag = 2;

		if (csocket.fd != -1)
		{
			socket_close(&csocket);
		}

		clear_map();
		map_redraw_flag = 1;
		clear_player();
		GameStatus = GAME_STATUS_WAITLOOP;
	}
	else if (GameStatus == GAME_STATUS_STARTCONNECT)
	{
		map_udate_flag = 2;
		draw_info_format(COLOR_GREEN, "Trying server %s (%d)...", selected_server->name, selected_server->port);
		last_keepalive = SDL_GetTicks();
		GameStatus = GAME_STATUS_CONNECT;
	}
	else if (GameStatus == GAME_STATUS_CONNECT)
	{
		if (!socket_open(&csocket, selected_server->ip, selected_server->port))
		{
			draw_info(COLOR_RED, "Connection failed!");
			GameStatus = GAME_STATUS_START;
			return 1;
		}

		socket_thread_start();
		GameStatus = GAME_STATUS_VERSION;
		draw_info(COLOR_GREEN, "Connected. Exchange version.");
		cpl.name[0] = '\0';
		cpl.password[0] = '\0';
		region_map_clear();
	}
	else if (GameStatus == GAME_STATUS_VERSION)
	{
		packet_struct *packet;

		packet = packet_new(SERVER_CMD_VERSION, 16, 0);
		packet_append_uint32(packet, SOCKET_VERSION);
		socket_send_packet(packet);

		GameStatus = GAME_STATUS_SETUP;
	}
	else if (GameStatus == GAME_STATUS_SETUP)
	{
		packet_struct *packet;

		packet = packet_new(SERVER_CMD_SETUP, 256, 256);
		packet_append_uint8(packet, CMD_SETUP_SOUND);
		packet_append_uint8(packet, 1);
		packet_append_uint8(packet, CMD_SETUP_MAPSIZE);
		packet_append_uint8(packet, setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH));
		packet_append_uint8(packet, setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT));
		server_files_setup_add(packet);
		socket_send_packet(packet);

		GameStatus = GAME_STATUS_WAITSETUP;
	}
	else if (GameStatus == GAME_STATUS_REQUEST_FILES)
	{
		if (!server_files_updating())
		{
			server_files_load(1);
			GameStatus = GAME_STATUS_ADDME;
		}
	}
	else if (GameStatus == GAME_STATUS_ADDME)
	{
		packet_struct *packet;

		packet = packet_new(SERVER_CMD_ADDME, 1, 0);
		socket_send_packet(packet);

		cpl.mark_count = -1;
		GameStatus = GAME_STATUS_LOGIN;
		/* Now wait for login request of the server */
	}
	else if (GameStatus == GAME_STATUS_LOGIN)
	{
		if (text_input_string_esc_flag)
		{
			draw_info(COLOR_RED, "Break login.");
			GameStatus = GAME_STATUS_START;
		}

		text_input_clear();
	}
	else if (GameStatus == GAME_STATUS_NAME)
	{
		/* We have a finished console input */
		if (text_input_string_esc_flag)
		{
			GameStatus = GAME_STATUS_LOGIN;
		}
		else if (text_input_string_flag == 0 && text_input_string_end_flag)
		{
			strcpy(cpl.name, text_input_string);
			LOG(llevInfo, "Login: send name %s\n", text_input_string);
			send_reply(text_input_string);
			GameStatus = GAME_STATUS_LOGIN;
		}
	}
	else if (GameStatus == GAME_STATUS_PSWD)
	{
		/* We have a finished console input */
		if (text_input_string_esc_flag)
		{
			GameStatus = GAME_STATUS_LOGIN;
		}
		else if (text_input_string_flag == 0 && text_input_string_end_flag)
		{
			strncpy(cpl.password, text_input_string, 39);
			cpl.password[39] = '\0';

			LOG(llevInfo, "Login: send password <*****>\n");
			send_reply(cpl.password);
			GameStatus = GAME_STATUS_LOGIN;
		}
	}
	else if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		/* We have a finished console input */
		if (text_input_string_esc_flag)
		{
			GameStatus = GAME_STATUS_LOGIN;
		}
		else if (text_input_string_flag == 0 && text_input_string_end_flag)
		{
			LOG(llevInfo, "Login: send verify password <*****>\n");
			send_reply(text_input_string);
			GameStatus = GAME_STATUS_LOGIN;
		}
	}
	else if (GameStatus == GAME_STATUS_WAITFORPLAY)
	{
		clear_map();
		map_udate_flag = 2;
	}

	return 1;
}

/**
 * Load the necessary bitmaps. */
static void load_bitmaps(void)
{
	int i;

	/* add later better error handling here */
	for (i = 0; i <= BITMAP_INTRO; i++)
	{
		load_bitmap(i);
	}
}

/**
 * Load a single bitmap.
 * @param index ID of the bitmap to load.
 * @return 1 on success, 0 on failure. */
static int load_bitmap(int idx)
{
	char buf[2048];
	uint32 flags = 0;

	snprintf(buf, sizeof(buf), DIRECTORY_BITMAPS"/%s", bitmap_name[idx].name);

	if (bitmap_name[idx].type == PIC_TYPE_PALETTE)
	{
		flags |= SURFACE_FLAG_PALETTE;
	}

	if (bitmap_name[idx].type == PIC_TYPE_TRANS)
	{
		flags |= SURFACE_FLAG_COLKEY_16M;
	}

	if (idx >= BITMAP_INTRO && idx != BITMAP_TEXTWIN_MASK)
	{
		flags |= bitmap_name[idx].type == PIC_TYPE_ALPHA ? SURFACE_FLAG_DISPLAYFORMATALPHA : SURFACE_FLAG_DISPLAYFORMAT;
	}

	Bitmaps[idx] = sprite_load_file(buf, flags);

	if (!Bitmaps[idx] || !Bitmaps[idx]->bitmap)
	{
		LOG(llevBug, "load_bitmap(): Can't load bitmap %s\n", buf);
		return 0;
	}

	return 1;
}

/**
 * Free the bitmaps. */
void free_bitmaps(void)
{
	size_t i;

	for (i = 0; i < BITMAP_INIT; i++)
	{
		sprite_free_sprite(Bitmaps[i]);
	}
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
	const SDL_VideoInfo* vinfo = NULL;
	SDL_Rect **modes;
	int i;

	LOG(llevInfo, "List Video Modes\n");

	/* Get available fullscreen/hardware modes */
	modes = SDL_ListModes(NULL, SDL_HWACCEL);

	/* Check if there are any modes available */
	if (modes == (SDL_Rect **) 0)
	{
		LOG(llevError, "No modes available!\n");
	}

	/* Check if resolution is restricted */
	if (modes == (SDL_Rect **) -1)
	{
		LOG(llevInfo, "All resolutions available.\n");
	}
	else
	{
		/* Print valid modes */
		LOG(llevInfo, "Available Modes\n");

		for (i = 0; modes[i]; ++i)
		{
			LOG(llevInfo, "  %d x %d\n", modes[i]->w, modes[i]->h);
		}
	}

	vinfo = SDL_GetVideoInfo();
	LOG(llevInfo, "VideoInfo: hardware surfaces? %s\n", vinfo->hw_available ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: windows manager? %s\n", vinfo->wm_available ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: hw to hw blit? %s\n", vinfo->blit_hw ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: hw to hw ckey blit? %s\n", vinfo->blit_hw_CC ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: hw to hw alpha blit? %s\n", vinfo->blit_hw_A ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: sw to hw blit? %s\n", vinfo->blit_sw ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: sw to hw ckey blit? %s\n", vinfo->blit_sw_CC ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: sw to hw alpha blit? %s\n", vinfo->blit_sw_A ? "yes":  "no");
	LOG(llevInfo, "VideoInfo: color fill? %s\n", vinfo->blit_fill ? "yes" : "no");
	LOG(llevInfo, "VideoInfo: video memory: %dKB\n", vinfo->video_mem);
}

/**
 * Map, animations and other effects. */
static void display_layer1(void)
{
	SDL_FillRect(ScreenSurface, NULL, 0);

	if (GameStatus == GAME_STATUS_PLAY)
	{
		widget_map_render(cur_widget[MAP_ID]);
	}
}

/**
 * Process the widgets if we're playing. */
static void display_layer3(void)
{
	/* Process the widgets */
	if (GameStatus == GAME_STATUS_PLAY)
	{
		process_widgets();
	}
}

/**
 * Dialogs, highest priority layer. */
static void display_layer4(void)
{
	if (GameStatus == GAME_STATUS_PLAY)
	{
		/* We have to make sure that these two get hidden right */
		/* sanity checks in case they don't exist */
		if (cur_widget[IN_CONSOLE_ID])
		{
			cur_widget[IN_CONSOLE_ID]->show = 0;
		}
		else
		{
			create_widget_object(IN_CONSOLE_ID);
		}

		if (cur_widget[IN_NUMBER_ID])
		{
			cur_widget[IN_NUMBER_ID]->show = 0;
		}
		else
		{
			create_widget_object(IN_NUMBER_ID);
		}

		if (cpl.input_mode == INPUT_MODE_CONSOLE)
		{
			widget_input_do(cur_widget[IN_CONSOLE_ID]);
		}
		else if (cpl.input_mode == INPUT_MODE_NUMBER)
		{
			widget_input_do(cur_widget[IN_NUMBER_ID]);
		}
	}
}

/**
 * Show a custom cursor. */
static void DisplayCustomCursor(void)
{
	if (f_custom_cursor == MSCURSOR_MOVE)
	{
		sprite_blt(Bitmaps[BITMAP_MSCURSOR_MOVE], x_custom_cursor - (Bitmaps[BITMAP_MSCURSOR_MOVE]->bitmap->w / 2), y_custom_cursor - (Bitmaps[BITMAP_MSCURSOR_MOVE]->bitmap->h / 2), NULL, NULL);
	}
}

/**
 * The main function.
 * @param argc Number of arguments.
 * @param argv[] Arguments.
 * @return 0 */
int main(int argc, char *argv[])
{
	int x, y, drag, done = 0;
	uint32 anim_tick, frame_start_time;
	size_t i;
	char version[MAX_BUF];

	toolkit_import(binreloc);
	toolkit_import(math);
	toolkit_import(packet);
	toolkit_import(porting);
	toolkit_import(sha1);
	toolkit_import(string);
	toolkit_import(stringbuffer);

	init_signals();
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
			LOG(llevInfo, "Usage: %s [-server <name>] [-port <num>]\n", argv[0]);
			exit(1);
		}
	}

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
	{
		LOG(llevBug, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	/* Start the system after starting SDL */
	system_start();
	list_vid_modes();

	if (!video_set_size())
	{
		LOG(llevError, "Couldn't set video size: %s\n", SDL_GetError());
	}

	sprite_init_system();

	SDL_EnableUNICODE(1);

	text_init();

	load_bitmaps();

	/* TODO: add later better error handling here */
	for (i = BITMAP_DOLL; i < BITMAP_INIT; i++)
	{
		load_bitmap(i);
	}

	sound_init();
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
			GameStatus = GAME_STATUS_INIT;
			/* Make sure no popup is visible. */
			popup_destroy_all();
			continue;
		}

		if (GameStatus > GAME_STATUS_CONNECT)
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
		if (GameStatus != GAME_STATUS_PLAY)
		{
			if (!game_status_chain())
			{
				LOG(llevError, "Error connecting: GameStatus: %d  SocketError: %d\n", GameStatus, socket_get_error());
			}
		}

		if (SDL_GetAppState() & SDL_APPACTIVE)
		{
			if (GameStatus == GAME_STATUS_PLAY)
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
				display_layer1();
				display_layer3();
				display_layer4();

				if (GameStatus != GAME_STATUS_PLAY)
				{
					SDL_FillRect(ScreenSurface, NULL, 0);
				}

				map_udate_flag = 0;
			}

			/* Show the current dragged item */
			if ((drag = draggingInvItem(DRAG_GET_STATUS)))
			{
				object *Item = NULL;

				if (drag == DRAG_QUICKSLOT)
				{
					Item = object_find(cpl.dragging.tag);
				}

				SDL_GetMouseState(&x, &y);

				if (drag == DRAG_QUICKSLOT_SPELL)
				{
					blit_face(cpl.dragging.spell->icon, x, y);
				}
				else
				{
					object_blit_centered(Item, x, y);
				}
			}

			if (GameStatus <= GAME_STATUS_WAITFORPLAY)
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
