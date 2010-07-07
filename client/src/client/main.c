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
 * Client main related functions. */

#include <include.h>

/* list of possible chars/race with setup when we want create a char */
_server_char *first_server_char = NULL;
/* if we login as new char, thats the values of it we set */
_server_char new_character;

/** The main screen surface. */
SDL_Surface *ScreenSurface;
/** Map surface. */
SDL_Surface *ScreenSurfaceMap;
/** Zoomed map surface. */
SDL_Surface *zoomed;
_Font MediumFont;
/* our main font */
_Font SystemFont;
/* our main font - black outlined */
_Font SystemFontOut;
/* bigger special font */
_Font BigFont;
/* our main font with shadow */
_Font Font6x3Out;
/** Server's attributes */
struct sockaddr_in insock;
/** Client socket. */
ClientSocket csocket;

Uint32 sdl_dgreen, sdl_gray1, sdl_gray2, sdl_gray3, sdl_gray4, sdl_blue1;

int music_global_fade = 0;
/** Whether the mouse button was clicked. */
int mb_clicked = 0;

/** Bitmaps table size. */
int bmaptype_table_size;
/** The srv/client files. */
_srv_client_files srv_client_files[SRV_CLIENT_FILES];
/** Settings. */
struct _options options;
Uint32 videoflags_full, videoflags_win;

struct _fire_mode fire_mode_tab[FIRE_MODE_INIT];
int RangeFireMode;

int current_intro = 0;

/* cache status... set this in hardware depend */
int CacheStatus;
/* SoundStatus 0=no 1= yes */
int SoundStatus;
/* map x,y len */
int MapStatusX;
int MapStatusY;

/** Our selected server that we want to connect to. */
server_struct *selected_server = NULL;

/** Used with -server command line option. */
static char argServerName[2048];
/** Used with -server command line option. */
static int argServerPort;

/** System time counter in ms since program start. */
uint32 LastTick;
/** Ticks since this second frame in ms. */
static uint32 GameTicksSec;
/** Used from several functions, just to store real ticks. */
uint32 tmpGameTick;
/** Number of frames drawn. */
uint32 FrameCount = 0;

/** Is the esc menu open? */
int esc_menu_flag;
/** ID of the selected option in esc menu. */
int esc_menu_index;

int f_custom_cursor = 0;
int x_custom_cursor = 0;
int y_custom_cursor = 0;

/** The bitmap table. */
_bmaptype *bmap_table[BMAPTABLE];

/* update map area */
int map_udate_flag, map_transfer_flag, map_redraw_flag;
int request_file_chain;

int ToggleScreenFlag;
char InputString[MAX_INPUT_STRING];
char InputHistory[MAX_HISTORY_LINES][MAX_INPUT_STRING];
int HistoryPos;
int CurrentCursorPos;

int InputCount, InputMax;
/** If 1, we have an open console. */
int InputStringFlag;
/** If 1, we submitted some text using the console. */
int InputStringEndFlag;
/** If 1, ESC was pressed while entering some text to console. */
int InputStringEscFlag;
/** The book GUI. */
struct gui_book_struct *gui_interface_book;
/** The party GUI. */
struct gui_party_struct *gui_interface_party;
/** All the loaded help files. */
help_files_struct *help_files;
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

/** The list of the servers. */
server_struct *start_server;
int metaserver_sel, metaserver_count = 0;

/** The message animation structure. */
struct msg_anim_struct msg_anim;

/** All the bitmaps. */
static _bitmap_name bitmap_name[BITMAP_INIT] =
{
	{"palette.png", PIC_TYPE_PALETTE},
	{"font7x4.png", PIC_TYPE_PALETTE},
	{"font6x3out.png", PIC_TYPE_PALETTE},
	{"font_big.png", PIC_TYPE_PALETTE},
	{"font7x4out.png", PIC_TYPE_PALETTE},
	{"font11x15.png", PIC_TYPE_PALETTE},
	{"intro.png", PIC_TYPE_DEFAULT},

	{"progress.png", PIC_TYPE_DEFAULT},
	{"progress_back.png", PIC_TYPE_DEFAULT},

	{"player_doll_bg.png", PIC_TYPE_TRANS},
	{"black_tile.png", PIC_TYPE_DEFAULT},
	{"textwin.png", PIC_TYPE_DEFAULT},
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

	{"range.png", PIC_TYPE_TRANS},
	{"range_marker.png", PIC_TYPE_TRANS},
	{"range_ctrl.png", PIC_TYPE_TRANS},
	{"range_ctrl_no.png", PIC_TYPE_TRANS},
	{"range_skill.png", PIC_TYPE_TRANS},
	{"range_skill_no.png", PIC_TYPE_TRANS},
	{"range_throw.png", PIC_TYPE_TRANS},
	{"range_throw_no.png", PIC_TYPE_TRANS},
	{"range_tool.png", PIC_TYPE_TRANS},
	{"range_tool_no.png", PIC_TYPE_TRANS},
	{"range_wizard.png", PIC_TYPE_TRANS},
	{"range_wizard_no.png", PIC_TYPE_TRANS},
	{"range_priest.png", PIC_TYPE_TRANS},
	{"range_priest_no.png", PIC_TYPE_TRANS},

	{"cmark_start.png", PIC_TYPE_TRANS},
	{"cmark_end.png", PIC_TYPE_TRANS},
	{"cmark_middle.png", PIC_TYPE_TRANS},

	{"textwin_scroll.png", PIC_TYPE_DEFAULT},
	{"inv_scroll.png", PIC_TYPE_DEFAULT},
	{"below_scroll.png", PIC_TYPE_DEFAULT},

	{"number.png", PIC_TYPE_DEFAULT},
	{"invslot_u.png", PIC_TYPE_TRANS},

	{"death.png", PIC_TYPE_TRANS},
	{"sleep.png", PIC_TYPE_TRANS},
	{"confused.png", PIC_TYPE_TRANS},
	{"paralyzed.png", PIC_TYPE_TRANS},
	{"scared.png", PIC_TYPE_TRANS},
	{"blind.png", PIC_TYPE_TRANS},

	{"enemy1.png", PIC_TYPE_TRANS},
	{"enemy2.png", PIC_TYPE_TRANS},
	{"probe.png", PIC_TYPE_TRANS},

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
	{"frame_line.png", PIC_TYPE_DEFAULT},

	{"target_attack.png", PIC_TYPE_TRANS},
	{"target_talk.png", PIC_TYPE_TRANS},
	{"target_normal.png", PIC_TYPE_TRANS},

	{"loading.png", PIC_TYPE_TRANS},
	{"warn_hp.png", PIC_TYPE_DEFAULT},
	{"warn_food.png", PIC_TYPE_DEFAULT},
	{"logo270.png", PIC_TYPE_DEFAULT},

	{"dialog_bg.png", PIC_TYPE_DEFAULT},
	{"dialog_title_options.png", PIC_TYPE_DEFAULT},
	{"dialog_title_keybind.png", PIC_TYPE_DEFAULT},
	{"dialog_title_skill.png", PIC_TYPE_DEFAULT},
	{"dialog_title_spell.png", PIC_TYPE_DEFAULT},
	{"dialog_title_creation.png", PIC_TYPE_DEFAULT},
	{"dialog_title_login.png", PIC_TYPE_DEFAULT},
	{"dialog_title_server.png", PIC_TYPE_DEFAULT},
	{"dialog_title_party.png", PIC_TYPE_DEFAULT},
	{"dialog_button_up.png", PIC_TYPE_DEFAULT},
	{"dialog_button_down.png", PIC_TYPE_DEFAULT},
	{"dialog_tab_start.png", PIC_TYPE_DEFAULT},
	{"dialog_tab.png", PIC_TYPE_DEFAULT},
	{"dialog_tab_stop.png", PIC_TYPE_DEFAULT},
	{"dialog_tab_sel.png", PIC_TYPE_DEFAULT},
	{"dialog_checker.png", PIC_TYPE_DEFAULT},
	{"dialog_range_off.png", PIC_TYPE_DEFAULT},
	{"dialog_range_l.png", PIC_TYPE_DEFAULT},
	{"dialog_range_r.png", PIC_TYPE_DEFAULT},

	{"target_hp.png", PIC_TYPE_DEFAULT},
	{"target_hp_b.png", PIC_TYPE_DEFAULT},

	{"textwin_mask.png", PIC_TYPE_DEFAULT},
	{"slider_up.png", PIC_TYPE_TRANS},
	{"slider_down.png", PIC_TYPE_TRANS},
	{"slider.png", PIC_TYPE_TRANS},

	{"exp_skill_border.png", PIC_TYPE_DEFAULT},
	{"exp_skill_line.png", PIC_TYPE_DEFAULT},
	{"exp_skill_bubble.png", PIC_TYPE_TRANS},

	{"options_head.png", PIC_TYPE_TRANS},
	{"options_keys.png", PIC_TYPE_TRANS},
	{"options_settings.png", PIC_TYPE_TRANS},
	{"options_logout.png", PIC_TYPE_TRANS},
	{"options_back.png", PIC_TYPE_TRANS},
	{"options_mark_left.png", PIC_TYPE_TRANS},
	{"options_mark_right.png", PIC_TYPE_TRANS},
	{"options_alpha.png", PIC_TYPE_DEFAULT},

	{"pentagram.png", PIC_TYPE_DEFAULT},
	{"quad_button_up.png", PIC_TYPE_DEFAULT},
	{"quad_button_down.png", PIC_TYPE_DEFAULT},
	{"nchar_marker.png", PIC_TYPE_TRANS},

	{"trapped.png", PIC_TYPE_TRANS},
	{"pray.png", PIC_TYPE_TRANS},
	{"wand.png", PIC_TYPE_TRANS},
	{"journal.png", PIC_TYPE_TRANS},
	{"slider_long.png", PIC_TYPE_DEFAULT},
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
	{"shop.png", PIC_TYPE_DEFAULT},
	{"shop_input.png", PIC_TYPE_DEFAULT}
};

/** Number of bitmaps. */
#define BITMAP_MAX (sizeof(bitmap_name) / sizeof(_bitmap_name))
/** The actual bitmaps. */
_Sprite *Bitmaps[BITMAP_MAX];

static void init_game_data();
static void flip_screen();
static void show_intro(char *text);
static void delete_player_lists();
static void reset_input_mode();
static int load_bitmap(int index);
static void free_faces();
static void free_bitmaps();
static void load_options_dat();

/**
 * Clear player lists like skill list, spell list, etc. */
static void delete_player_lists()
{
	int i, ii;

	for (i = 0; i < FIRE_MODE_INIT; i++)
	{
		fire_mode_tab[i].amun = FIRE_ITEM_NO;
		fire_mode_tab[i].item = FIRE_ITEM_NO;
		fire_mode_tab[i].skill = NULL;
		fire_mode_tab[i].spell = NULL;
		fire_mode_tab[i].name[0] = '\0';
	}

	for (i = 0; i < SKILL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			if (skill_list[i].entry[ii].flag == LIST_ENTRY_KNOWN)
			{
				skill_list[i].entry[ii].flag = LIST_ENTRY_USED;
			}
		}
	}

	for (i = 0;i < SPELL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			if (spell_list[i].entry[0][ii].flag == LIST_ENTRY_KNOWN)
			{
				spell_list[i].entry[0][ii].flag = LIST_ENTRY_USED;
			}

			if (spell_list[i].entry[1][ii].flag == LIST_ENTRY_KNOWN)
			{
				spell_list[i].entry[1][ii].flag = LIST_ENTRY_USED;
			}
		}
	}
}

/**
 * Initialize game data. */
static void init_game_data()
{
	int i;

	textwin_init();
	textwin_flags = 0;
	first_server_char = NULL;

	esc_menu_flag = 0;
	srand(time(NULL));

	memset(bmaptype_table, 0, sizeof(bmaptype_table));

	ToggleScreenFlag = 0;
	KeyScanFlag = 0;

	memset(&fire_mode_tab, 0, sizeof(fire_mode_tab));

	memset(&options, 0, sizeof(struct _options));
	init_map_data(0, 0, 0, 0);

	for (i = 0; i < (int) BITMAP_MAX; i++)
	{
		Bitmaps[i] = NULL;
	}

	memset(FaceList, 0, sizeof(struct _face_struct) * MAX_FACE_TILES);
	memset(&cpl, 0, sizeof(cpl));
	cpl.ob = player_item();

	init_keys();
	init_player_data();
	metaserver_clear_data();
	reset_input_mode();

	msg_anim.message[0] = '\0';

	start_anim = NULL;

	map_transfer_flag = 0;
	start_server = NULL;
	argServerName[0] = '\0';
	argServerPort = 13327;

	SoundSystem = SOUND_SYSTEM_OFF;
	GameStatus = GAME_STATUS_INIT;
	CacheStatus = CF_FACE_CACHE;
	SoundStatus = 1;
	MapStatusX = MAP_MAX_SIZE;
	MapStatusY = MAP_MAX_SIZE;
	map_udate_flag = 2;
	map_redraw_flag = 1;
	InputStringFlag = 0;
	InputStringEndFlag = 0;
	InputStringEscFlag = 0;
	csocket.fd = SOCKET_NO;
	RangeFireMode = 0;
	gui_interface_book = NULL;
	gui_interface_party = NULL;
	help_files = NULL;
	options.resolution_x = 800;
	options.resolution_y = 600;
	options.playerdoll = 0;
#ifdef WIDGET_SNAP
	options.widget_snap = 0;
#endif

	txtwin[TW_MIX].size = 50;
	txtwin[TW_MSG].size = 16;
	txtwin[TW_CHAT].size = 16;
	options.zoom = 100;
	options.mapstart_x = 0;
	options.mapstart_y = 10;

	load_options_dat();

	Screensize = (_screensize *) malloc(sizeof(_screensize));
	Screensize->x = options.resolution_x;
	Screensize->y = options.resolution_y;

	change_textwin_font(options.chat_font_size);

	init_widgets_fromCurrent();

	textwin_clearhistory();
	delete_player_lists();
}

/**
 * Save the options file. */
void save_options_dat()
{
	char txtBuffer[20];
	int i = -1, j = -1;
	FILE *stream;

	if (!(stream = fopen_wrapper(OPTION_FILE, "w")))
	{
		return;
	}

	fputs("##########################################\n", stream);
	fputs("# This is the Atrinik client option file #\n", stream);
	fputs("##########################################\n", stream);

	snprintf(txtBuffer, sizeof(txtBuffer), "%%21 %d\n", txtwin[TW_MSG].size);
	fputs(txtBuffer, stream);

	snprintf(txtBuffer, sizeof(txtBuffer), "%%22 %d\n", txtwin[TW_CHAT].size);
	fputs(txtBuffer, stream);

	snprintf(txtBuffer, sizeof(txtBuffer), "%%3x %d\n", options.resolution_x);
	fputs(txtBuffer, stream);

	snprintf(txtBuffer, sizeof(txtBuffer), "%%3y %d\n", options.resolution_y);
	fputs(txtBuffer, stream);

	while (opt_tab[++i])
	{
		fputs("\n# ", stream);
		fputs(opt_tab[i], stream);
		fputs("\n", stream);

		while (opt[++j].name && opt[j].name[0] != '#')
		{
			fputs(opt[j].name, stream);

			switch (opt[j].value_type)
			{
				case VAL_BOOL:
				case VAL_INT:
					snprintf(txtBuffer, sizeof(txtBuffer), " %d",  *((int *) opt[j].value));
					break;

				case VAL_U32:
					snprintf(txtBuffer, sizeof(txtBuffer), " %d",  *((uint32 *) opt[j].value));
					break;

				case VAL_CHAR:
					snprintf(txtBuffer, sizeof(txtBuffer), " %d",  *((uint8 *)opt[j].value));
					break;
			}

			fputs(txtBuffer, stream);
			fputs("\n", stream);
		}
	}

	fclose(stream);
}

/**
 * Load the options file. */
static void load_options_dat()
{
	int i = -1, pos;
	FILE *stream;
	char line[256], keyword[256], parameter[256];

	/* Fill all options with default values */
	while (opt[++i].name)
	{
		if (opt[i].name[0] == '#')
		{
			continue;
		}

		switch (opt[i].value_type)
		{
			case VAL_BOOL:
			case VAL_INT:
				*((int *) opt[i].value) = opt[i].default_val;
				break;

			case VAL_U32:
				*((uint32 *) opt[i].value) = opt[i].default_val;
				break;

			case VAL_CHAR:
				*((uint8 *) opt[i].value)= (uint8) opt[i].default_val;
				break;
		}
	}

	txtwin_start_size = txtwin[TW_MIX].size;

	/* Read the options from file */
	if (!(stream = fopen_wrapper(OPTION_FILE, "r")))
	{
		LOG(llevMsg, "Can't find file %s. Using defaults.\n", OPTION_FILE);
		return;
	}

	while (fgets(line, 255, stream))
	{
		if (line[0] == '#' || line[0] == '\n')
			continue;

		/* This are special settings which won't show in the options window, this has to be reworked in a general way */
		if (line[0] == '%')
		{
			switch (line[1])
			{
				case '2':
					switch (line[2])
					{
						case '1':
							txtwin[TW_MSG].size = atoi(line + 4);
							break;

						case '2':
							txtwin[TW_CHAT].size = atoi(line + 4);
							break;
					}

					break;

				case '3':
					switch (line[2])
					{
						case 'x':
							options.resolution_x = atoi(line + 4);
							break;

						case 'y':
							options.resolution_y = atoi(line + 4);
							break;
					}

					break;
			}

			continue;
		}

		i = 0;

		while (line[i] && line[i] != ':')
		{
			i++;
		}

		line[++i] = '\0';
		strncpy(keyword, line, sizeof(keyword));
		strncpy(parameter, line + i + 1, sizeof(parameter));
		i = atoi(parameter);

		pos = -1;

		while (opt[++pos].name)
		{
			if (!strcmp(keyword, opt[pos].name))
			{
				switch (opt[pos].value_type)
				{
					case VAL_BOOL:
					case VAL_INT:
						*((int *) opt[pos].value) = i;
						break;

					case VAL_U32:
						*((uint32 *) opt[pos].value) = i;
						break;

					case VAL_CHAR:
						*((uint8 *) opt[pos].value)= (uint8) i;
						break;
				}
			}
		}
	}

	fclose(stream);
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

	LOG(llevMsg, "\nSIGSEGV received.\n");
	fatal_signal(1);
}

static void rec_sighup(int i)
{
	(void) i;

	LOG(llevMsg, "\nSIGHUP received\n");
	exit(0);
}

static void rec_sigquit(int i)
{
	(void) i;

	LOG(llevMsg, "\nSIGQUIT received\n");
	fatal_signal(1);
}

static void rec_sigterm(int i)
{
	(void) i;

	LOG(llevMsg, "\nSIGTERM received\n");
	fatal_signal(0);
}
#endif

/** @endcond */

/**
 * Initialize the signal handlers. */
static void init_signals()
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
static int game_status_chain()
{
	char buf[1024];

	if (GameStatus == GAME_STATUS_INIT)
	{
		cpl.mark_count = -1;
		LOG(llevMsg, "GAMES_STATUS_INIT_1\n");

		map_udate_flag = 2;
		delete_player_lists();
		LOG(llevMsg, "GAMES_STATUS_INIT_2\n");

#ifdef INSTALL_SOUND
		if (!music.flag || strcmp(music.name, "orchestral.ogg"))
		{
			sound_play_music("orchestral.ogg", options.music_volume, 0, -1, MUSIC_MODE_DIRECT);
		}
#endif

		clear_map();
		LOG(llevMsg, "GAMES_STATUS_INIT_3\n");
		metaserver_clear_data();
		LOG(llevMsg, "GAMES_STATUS_INIT_4\n");
		GameStatus = GAME_STATUS_META;
	}
	else if (GameStatus == GAME_STATUS_META)
	{
		map_udate_flag = 2;

		metaserver_add("127.0.0.1", 13327, "Localhost", -1, "local", "Localhost. Start server before you try to connect.");

		if (argServerName[0] != '\0')
		{
			metaserver_add(argServerName, argServerPort, argServerName, -1, "user server", "Server from command line -server option.");
		}

		/* Skip if -nometa in command line */
		if (options.no_meta)
		{
			draw_info("Metaserver ignored.", COLOR_GREEN);
			metaserver_connecting = 0;
		}
		else
		{
			metaserver_get_servers();
		}

		GameStatus = GAME_STATUS_START;
	}
	else if (GameStatus == GAME_STATUS_START)
	{
		map_udate_flag = 2;

		if ((int) csocket.fd != SOCKET_NO)
		{
			socket_close(&csocket);
		}

		clear_map();
		map_redraw_flag = 1;
		clear_player();
		reset_keys();
		free_faces();
		GameStatus = GAME_STATUS_WAITLOOP;
	}
	else if (GameStatus == GAME_STATUS_STARTCONNECT)
	{
		char sbuf[256];

		snprintf(sbuf, sizeof(sbuf), "%s%s", GetBitmapDirectory(), bitmap_name[BITMAP_LOADING].name);
		FaceList[MAX_FACE_TILES - 1].sprite = sprite_tryload_file(sbuf, 0, NULL);

		map_udate_flag = 2;
		snprintf(buf, sizeof(buf), "Trying server %s (%d)...", selected_server->name, selected_server->port);
		draw_info(buf, COLOR_GREEN);
		GameStatus = GAME_STATUS_CONNECT;
	}
	else if (GameStatus == GAME_STATUS_CONNECT)
	{
		if (!socket_open(&csocket, selected_server->ip, selected_server->port))
		{
			draw_info("Connection failed!", COLOR_RED);
			GameStatus = GAME_STATUS_START;
			return 1;
		}

		socket_thread_start();
		GameStatus = GAME_STATUS_VERSION;
		draw_info("Connected. Exchange version.", COLOR_GREEN);
	}
	else if (GameStatus == GAME_STATUS_VERSION)
	{
		SendVersion();
		GameStatus = GAME_STATUS_SETUP;
	}
	else if (GameStatus == GAME_STATUS_SETUP)
	{
		map_transfer_flag = 0;

		srv_client_files[SRV_CLIENT_SETTINGS].status = SRV_CLIENT_STATUS_OK;
		srv_client_files[SRV_CLIENT_BMAPS].status = SRV_CLIENT_STATUS_OK;
		srv_client_files[SRV_CLIENT_ANIMS].status = SRV_CLIENT_STATUS_OK;
		srv_client_files[SRV_CLIENT_HFILES].status = SRV_CLIENT_STATUS_OK;
		srv_client_files[SRV_CLIENT_SKILLS].status = SRV_CLIENT_STATUS_OK;
		srv_client_files[SRV_CLIENT_SPELLS].status = SRV_CLIENT_STATUS_OK;

		snprintf(buf, sizeof(buf), "setup sound %d map2cmd 1 mapsize %dx%d darkness 1 facecache 1 skf %d|%x spf %d|%x bpf %d|%x stf %d|%x amf %d|%x hpf %d|%x", SoundStatus, MapStatusX, MapStatusY, srv_client_files[SRV_CLIENT_SKILLS].len, srv_client_files[SRV_CLIENT_SKILLS].crc, srv_client_files[SRV_CLIENT_SPELLS].len, srv_client_files[SRV_CLIENT_SPELLS].crc, srv_client_files[SRV_CLIENT_BMAPS].len, srv_client_files[SRV_CLIENT_BMAPS].crc, srv_client_files[SRV_CLIENT_SETTINGS].len, srv_client_files[SRV_CLIENT_SETTINGS].crc, srv_client_files[SRV_CLIENT_ANIMS].len, srv_client_files[SRV_CLIENT_ANIMS].crc, srv_client_files[SRV_CLIENT_HFILES].len, srv_client_files[SRV_CLIENT_HFILES].crc);

		cs_write_string(buf, strlen(buf));
		request_file_chain = 0;

		GameStatus = GAME_STATUS_WAITSETUP;
	}
	else if (GameStatus == GAME_STATUS_REQUEST_FILES)
	{
		if (request_file_chain == 0)
		{
			if (srv_client_files[SRV_CLIENT_SETTINGS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 1;
				RequestFile(SRV_CLIENT_SETTINGS);
			}
			else
			{
				request_file_chain = 2;
			}
		}
		else if (request_file_chain == 2)
		{
			if (srv_client_files[SRV_CLIENT_SPELLS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 3;
				RequestFile(SRV_CLIENT_SPELLS);
			}
			else
			{
				request_file_chain = 4;
			}
		}
		else if (request_file_chain == 4)
		{
			if (srv_client_files[SRV_CLIENT_SKILLS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 5;
				RequestFile(SRV_CLIENT_SKILLS);
			}
			else
			{
				request_file_chain = 6;
			}
		}
		else if (request_file_chain == 6)
		{
			if (srv_client_files[SRV_CLIENT_BMAPS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 7;
				RequestFile(SRV_CLIENT_BMAPS);
			}
			else
			{
				request_file_chain = 8;
			}
		}
		else if (request_file_chain == 8)
		{
			if (srv_client_files[SRV_CLIENT_ANIMS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 9;
				RequestFile(SRV_CLIENT_ANIMS);
			}
			else
			{
				request_file_chain = 10;
			}
		}
		else if (request_file_chain == 10)
		{
			if (srv_client_files[SRV_CLIENT_HFILES].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 11;
				RequestFile(SRV_CLIENT_HFILES);
			}
			else
			{
				request_file_chain = 12;
			}
		}
		else if (request_file_chain == 12)
		{
			read_skills();
			read_spells();
			read_settings();
			read_bmaps();
			read_bmap_tmp();
			read_anims();
			read_anim_tmp();
			read_help_files();
			load_settings();
			GameStatus = GAME_STATUS_ADDME;
		}
	}
	else if (GameStatus == GAME_STATUS_ADDME)
	{
		cpl.mark_count = -1;
		map_transfer_flag = 0;
		SendAddMe();
		cpl.name[0] = '\0';
		cpl.password[0] = '\0';
		GameStatus = GAME_STATUS_LOGIN;
		/* Now wait for login request of the server */
	}
	else if (GameStatus == GAME_STATUS_LOGIN)
	{
		map_transfer_flag = 0;

		if (InputStringEscFlag)
		{
			draw_info("Break login.", COLOR_RED);
			GameStatus = GAME_STATUS_START;
		}

		reset_input_mode();
	}
	else if (GameStatus == GAME_STATUS_NAME)
	{
		map_transfer_flag = 0;

		/* We have a finished console input */
		if (InputStringEscFlag)
		{
			GameStatus = GAME_STATUS_LOGIN;
		}
		else if (InputStringFlag == 0 && InputStringEndFlag)
		{
			strcpy(cpl.name, InputString);
			LOG(llevMsg, "Login: send name %s\n", InputString);
			send_reply(InputString);
			GameStatus = GAME_STATUS_LOGIN;
		}
	}
	else if (GameStatus == GAME_STATUS_PSWD)
	{
		map_transfer_flag = 0;

		textwin_clearhistory();

		/* We have a finished console input */
		if (InputStringEscFlag)
		{
			GameStatus = GAME_STATUS_LOGIN;
		}
		else if (InputStringFlag == 0 && InputStringEndFlag)
		{
			strncpy(cpl.password, InputString, 39);
			cpl.password[39] = '\0';

			LOG(llevMsg, "Login: send password <*****>\n");
			send_reply(cpl.password);
			GameStatus = GAME_STATUS_LOGIN;
		}
	}
	else if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		map_transfer_flag = 0;

		/* We have a finished console input */
		if (InputStringEscFlag)
		{
			GameStatus = GAME_STATUS_LOGIN;
		}
		else if (InputStringFlag == 0 && InputStringEndFlag)
		{
			LOG(llevMsg, "Login: send verify password <*****>\n");
			send_reply(InputString);
			GameStatus = GAME_STATUS_LOGIN;
		}
	}
	else if (GameStatus == GAME_STATUS_WAITFORPLAY)
	{
		clear_map();
		map_draw_map_clear();
		map_udate_flag = 2;
		map_transfer_flag = 1;
	}
	else if (GameStatus == GAME_STATUS_NEW_CHAR)
	{
		map_transfer_flag = 0;
	}
	else if (GameStatus == GAME_STATUS_QUIT)
	{
		map_transfer_flag = 0;
	}

	return 1;
}

/**
 * Load the necessary bitmaps. */
static void load_bitmaps()
{
	int i;

	/* add later better error handling here */
	for (i = 0; i <= BITMAP_PROGRESS_BACK; i++)
	{
		load_bitmap(i);
	}

	CreateNewFont(Bitmaps[BITMAP_FONT1], &SystemFont, 16, 16, 1);
	CreateNewFont(Bitmaps[BITMAP_FONTMEDIUM], &MediumFont, 16, 16, 1);
	CreateNewFont(Bitmaps[BITMAP_FONT1OUT], &SystemFontOut, 16, 16, 1);
	CreateNewFont(Bitmaps[BITMAP_FONT6x3OUT], &Font6x3Out, 16, 16, -1);
	CreateNewFont(Bitmaps[BITMAP_BIGFONT], &BigFont, 11, 16, 3);
}

/**
 * Load a single bitmap.
 * @param index ID of the bitmap to load.
 * @return 1 on success, 0 on failure. */
static int load_bitmap(int index)
{
	char buf[2048];
	uint32 flags = 0;

	snprintf(buf, sizeof(buf), "%s%s", GetBitmapDirectory(), bitmap_name[index].name);

	if (bitmap_name[index].type == PIC_TYPE_PALETTE)
	{
		flags |= SURFACE_FLAG_PALETTE;
	}

	if (bitmap_name[index].type == PIC_TYPE_TRANS)
	{
		flags |= SURFACE_FLAG_COLKEY_16M;
	}

	if (index >= BITMAP_INTRO && index != BITMAP_TEXTWIN_MASK)
	{
		flags |= SURFACE_FLAG_DISPLAYFORMAT;
	}

	Bitmaps[index] = sprite_load_file(buf, flags);

	if (!Bitmaps[index] || !Bitmaps[index]->bitmap)
	{
		LOG(llevMsg, "load_bitmap(): Can't load bitmap %s\n", buf);
		return 0;
	}

	return 1;
}

/**
 * Free the bitmaps. */
static void free_bitmaps()
{
	int i, ii;

	for (i = 0; i < (int) BITMAP_MAX; i++)
	{
		sprite_free_sprite(Bitmaps[i]);
	}

	for (i = 0; i < SPELL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			if ((spell_list[i].entry[0][ii].flag != LIST_ENTRY_UNUSED) && spell_list[i].entry[0][ii].icon)
			{
				sprite_free_sprite(spell_list[i].entry[0][ii].icon);
			}

			if ((spell_list[i].entry[1][ii].flag != LIST_ENTRY_UNUSED) && spell_list[i].entry[1][ii].icon)
			{
				sprite_free_sprite(spell_list[i].entry[1][ii].icon);
			}
		}
	}

	for (i = 0; i < SKILL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			if ((skill_list[i].entry[ii].flag != LIST_ENTRY_UNUSED) && skill_list[i].entry[ii].icon)
			{
				sprite_free_sprite(skill_list[i].entry[ii].icon);
			}
		}
	}
}

/**
 * Free all loaded faces. */
static void free_faces()
{
	int i;

	for (i = 0; i < MAX_FACE_TILES; i++)
	{
		if (FaceList[i].sprite)
		{
			sprite_free_sprite(FaceList[i].sprite);
			FaceList[i].sprite = NULL;
		}

		if (FaceList[i].name)
		{
			void *tmp_free = &FaceList[i].name;
			FreeMemory(tmp_free);
		}

		FaceList[i].flags =0;
	}
}

/**
 * Reset input mode. */
static void reset_input_mode()
{
	InputString[0] = '\0';
	InputCount = 0;
	HistoryPos = 0;
	InputHistory[0][0] = '\0';
	CurrentCursorPos = 0;
	InputStringFlag = 0;
	InputStringEndFlag = 0;
	InputStringEscFlag = 0;
}

/**
 * Open input mode.
 * @param maxchar Maximum number of allowed characters. */
void open_input_mode(int maxchar)
{
	int interval = (options.key_repeat > 0) ? 70 / options.key_repeat : 0;
	int delay = (options.key_repeat > 0) ? interval + 280 / options.key_repeat : 0;

	reset_input_mode();
	InputMax = maxchar;
	SDL_EnableKeyRepeat(delay, interval);

	if (cpl.input_mode != INPUT_MODE_NUMBER)
	{
		cpl.inventory_win = IWIN_BELOW;
	}

	InputStringFlag = 1;
}

/**
 * Play various action sounds. */
static void play_action_sounds()
{
	if (cpl.warn_statdown)
	{
		sound_play_one_repeat(SOUND_WARN_STATDOWN, SPECIAL_SOUND_STATDOWN);
		cpl.warn_statdown = 0;
	}

	if (cpl.warn_statup)
	{
		sound_play_one_repeat(SOUND_WARN_STATUP, SPECIAL_SOUND_STATUP);
		cpl.warn_statup = 0;
	}

	if (cpl.warn_drain)
	{
		sound_play_one_repeat(SOUND_WARN_DRAIN, SPECIAL_SOUND_DRAIN);
		cpl.warn_drain = 0;
	}

	if (cpl.warn_hp)
	{
		if (cpl.warn_hp == 2)
		{
			sound_play_effect(SOUND_WARN_HP2, 0, 100);
		}
		else
		{
			sound_play_effect(SOUND_WARN_HP, 0, 100);
		}

		cpl.warn_hp = 0;
	}
}

/**
 * List video modes available. */
void list_vid_modes()
{
	const SDL_VideoInfo* vinfo = NULL;
	SDL_Rect **modes;
	int i;

	LOG(llevMsg, "List Video Modes\n");

	/* Get available fullscreen/hardware modes */
	modes = SDL_ListModes(NULL, SDL_HWACCEL);

	/* Check if there are any modes available */
	if (modes == (SDL_Rect **) 0)
	{
		LOG(llevMsg, "No modes available!\n");
		exit(-1);
	}

	/* Check if resolution is restricted */
	if (modes == (SDL_Rect **) -1)
	{
		LOG(llevMsg, "All resolutions available.\n");
	}
	else
	{
		/* Print valid modes */
		LOG(llevMsg, "Available Modes\n");

		for (i = 0; modes[i]; ++i)
		{
			LOG(llevMsg, "  %d x %d\n", modes[i]->w, modes[i]->h);
		}
	}

	vinfo = SDL_GetVideoInfo();
	LOG(llevMsg, "VideoInfo: hardware surfaces? %s\n", vinfo->hw_available ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: windows manager? %s\n", vinfo->wm_available ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: hw to hw blit? %s\n", vinfo->blit_hw ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: hw to hw ckey blit? %s\n", vinfo->blit_hw_CC ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: hw to hw alpha blit? %s\n", vinfo->blit_hw_A ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: sw to hw blit? %s\n", vinfo->blit_sw ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: sw to hw ckey blit? %s\n", vinfo->blit_sw_CC ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: sw to hw alpha blit? %s\n", vinfo->blit_sw_A ? "yes":  "no");
	LOG(llevMsg, "VideoInfo: color fill? %s\n", vinfo->blit_fill ? "yes" : "no");
	LOG(llevMsg, "VideoInfo: video memory: %dKB\n", vinfo->video_mem);
}

/**
 * Show the options window (the 'ESC' menu).
 * @param x X position.
 * @param y Y position. */
static void show_option(int x, int y)
{
	int index = 0, x1, y1 = 0, x2, y2 = 0;
	_BLTFX bltfx;

	bltfx.dark_level = 0;
	bltfx.surface = NULL;
	bltfx.alpha = 118;
	bltfx.flags = BLTFX_FLAG_SRCALPHA;

	sprite_blt(Bitmaps[BITMAP_OPTIONS_ALPHA], x - Bitmaps[BITMAP_OPTIONS_ALPHA]->bitmap->w / 2, y, NULL, &bltfx);
	sprite_blt(Bitmaps[BITMAP_OPTIONS_HEAD], x - Bitmaps[BITMAP_OPTIONS_HEAD]->bitmap->w / 2, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_OPTIONS_KEYS], x - Bitmaps[BITMAP_OPTIONS_KEYS]->bitmap->w / 2, y + 100, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_OPTIONS_SETTINGS], x - Bitmaps[BITMAP_OPTIONS_SETTINGS]->bitmap->w / 2, y + 165, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_OPTIONS_LOGOUT], x - Bitmaps[BITMAP_OPTIONS_LOGOUT]->bitmap->w / 2, y + 235, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_OPTIONS_BACK], x - Bitmaps[BITMAP_OPTIONS_BACK]->bitmap->w / 2, y + 305, NULL, NULL);

	if (esc_menu_index == ESC_MENU_KEYS)
	{
		index = BITMAP_OPTIONS_KEYS;
		y1 = y2 = y + 105;
	}
	else if (esc_menu_index == ESC_MENU_SETTINGS)
	{
		index = BITMAP_OPTIONS_SETTINGS;
		y1 = y2 = y + 170;
	}
	else if (esc_menu_index == ESC_MENU_LOGOUT)
	{
		index = BITMAP_OPTIONS_LOGOUT;
		y1 = y2 = y + 244;
	}
	else if (esc_menu_index == ESC_MENU_BACK)
	{
		index = BITMAP_OPTIONS_BACK;
		y1 = y2 = y + 310;
	}

	x1 = x - Bitmaps[index]->bitmap->w / 2 - 6;
	x2 = x + Bitmaps[index]->bitmap->w / 2 + 6;

	sprite_blt(Bitmaps[BITMAP_OPTIONS_MARK_LEFT], x1 - Bitmaps[BITMAP_OPTIONS_MARK_LEFT]->bitmap->w, y1, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_OPTIONS_MARK_RIGHT], x2, y2, NULL, NULL);
}

/**
 * Map, animations and other effects. */
static void display_layer1()
{
	static int gfx_toggle = 0;
	SDL_Rect rect;

	SDL_FillRect(ScreenSurface, NULL, 0);

	/* We recreate the map only when there is a change */
	if (map_redraw_flag)
	{
		SDL_FillRect(ScreenSurfaceMap, NULL, 0);
		map_draw_map();
		map_redraw_flag = 0;

		if (options.zoom != 100)
		{
			SDL_FreeSurface(zoomed);
			zoomed = zoomSurface(ScreenSurfaceMap, options.zoom / 100.0, options.zoom / 100.0, options.zoom_smooth);
		}
	}

	rect.x = options.mapstart_x;
	rect.y = options.mapstart_y;

	if (options.zoom == 100)
	{
		SDL_BlitSurface(ScreenSurfaceMap, NULL, ScreenSurface, &rect);
	}
	else
	{
		SDL_BlitSurface(zoomed, NULL, ScreenSurface, &rect);
	}

	/* The damage numbers */
	play_anims();

	/* Draw warning icons above player */
	if ((gfx_toggle++ & 63) < 25)
	{
		if (options.warning_hp && ((float) cpl.stats.hp / (float) cpl.stats.maxhp) * 100 <= options.warning_hp)
		{
			sprite_blt(Bitmaps[BITMAP_WARN_HP], options.mapstart_x + 393, options.mapstart_y + 298, NULL, NULL);
		}
	}
	else
	{
		/* Low food */
		if (options.warning_food && ((float) cpl.stats.food / 1000.0f) * 100 <= options.warning_food)
		{
			sprite_blt(Bitmaps[BITMAP_WARN_FOOD], options.mapstart_x + 390, options.mapstart_y + 294, NULL, NULL);
		}
	}
}

/**
 * Inventory. */
static void display_layer2()
{
	cpl.container = NULL;

	if (GameStatus == GAME_STATUS_PLAY)
	{
		cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);

		cpl.real_weight = cpl.window_weight;

		cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
	}
}

/**
 * Process the widgets if we're playing. */
static void display_layer3()
{
	/* Process the widgets */
	if (GameStatus == GAME_STATUS_PLAY)
	{
		process_widgets();
	}
}

/* dialogs, highest-priority layer */
/**
 * Dialogs, highest priority layer. */
static void display_layer4()
{
	if (GameStatus == GAME_STATUS_PLAY)
	{
		/* We have to make sure that these two get hidden right */
		cur_widget[IN_CONSOLE_ID].show = 0;
		cur_widget[IN_NUMBER_ID].show = 0;

		if (cpl.input_mode == INPUT_MODE_CONSOLE)
		{
			do_console();
		}
		else if (cpl.input_mode == INPUT_MODE_NUMBER)
		{
			do_number();
		}
		else if (cpl.input_mode == INPUT_MODE_GETKEY)
		{
			do_keybind_input();
		}
	}

	if (esc_menu_flag)
	{
		show_option(Screensize->x / 2, (Screensize->y / 2) - (Bitmaps[BITMAP_OPTIONS_ALPHA]->bitmap->h / 2));
	}
}

/**
 * Show a custom cursor. */
static void DisplayCustomCursor()
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
	int x, y, drag;
	uint32 anim_tick;
	Uint32 videoflags;
	int i, done = 0;

	init_signals();
	init_game_data();
	curl_global_init(CURL_GLOBAL_ALL);

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
		else if (strcmp(argv[argc], "-nometa") == 0)
		{
			options.no_meta = 1;
		}
		else if (strcmp(argv[argc], "-key") == 0)
		{
			KeyScanFlag = 1;
		}
		else
		{
			char tmp[1024];

			snprintf(tmp, sizeof(tmp), "Usage: %s [-server <name>] [-port <num>]\n", argv[0]);
			LOG(llevMsg, tmp);
			fprintf(stderr, "%s", tmp);
			exit(1);
		}
	}

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
	{
		LOG(llevError, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	/* Start the system after starting SDL */
	SYSTEM_Start();
	list_vid_modes();

	videoflags = get_video_flags();
	options.used_video_bpp = 16;

	if (!options.fullscreen_flag)
	{
		if (options.auto_bpp_flag)
		{
			const SDL_VideoInfo* info = NULL;
			info = SDL_GetVideoInfo();
			options.used_video_bpp = info->vfmt->BitsPerPixel;
		}
	}

	if ((ScreenSurface = SDL_SetVideoMode(Screensize->x, Screensize->y, options.used_video_bpp, videoflags)) == NULL)
	{
		/* We have a problem, not supportet screensize */
		/* If we have higher resolution we try the default 800x600 */
		if (Screensize->x > 800 && Screensize->y > 600)
		{
			LOG(llevError, "Try to set to default 800x600...\n");

			if ((ScreenSurface = SDL_SetVideoMode(Screensize->x, Screensize->y, options.used_video_bpp, videoflags)) == NULL)
			{
				/* Now we have a really really big problem */
				LOG(llevError, "Couldn't set %dx%dx%d video mode: %s\n", Screensize->x, Screensize->y, options.used_video_bpp, SDL_GetError());
				exit(2);
			}
			else
			{
				const SDL_VideoInfo *info = SDL_GetVideoInfo();

				options.real_video_bpp = info->vfmt->BitsPerPixel;
			}
		}
		else
		{
			exit(2);
		}
	}
	else
	{
		const SDL_VideoInfo *info = SDL_GetVideoInfo();

		options.used_video_bpp = info->vfmt->BitsPerPixel;
	}

	sprite_init_system();
	ScreenSurfaceMap = SDL_CreateRGBSurface(videoflags, 850, 600, options.used_video_bpp, 0,0,0,0);

	/* 60, 70*/
	sdl_dgreen = SDL_MapRGB(ScreenSurface->format, 0x00, 0x80, 0x00);
	sdl_gray1 = SDL_MapRGB(ScreenSurface->format, 0x45, 0x45, 0x45);
	sdl_gray2 = SDL_MapRGB(ScreenSurface->format, 0x55, 0x55, 0x55);

	sdl_gray3 = SDL_MapRGB(ScreenSurface->format, 0x55, 0x55, 0x55);
	sdl_gray4 = SDL_MapRGB(ScreenSurface->format, 0x60, 0x60, 0x60);

	sdl_blue1 = SDL_MapRGB(ScreenSurface->format, 0x00, 0x00, 0xef);

	SDL_EnableUNICODE(1);

	load_bitmaps();
	show_intro("Load bitmaps");

	/* TODO: add later better error handling here */
	for (i = BITMAP_DOLL; i < (int) BITMAP_MAX; i++)
		load_bitmap(i);

	sound_init();
	show_intro("Start sound system");

	sound_loadall();
	show_intro("Load sounds");

	read_keybind_file(KEYBIND_FILE);
	show_intro("Load keys");

	load_mapdef_dat();
	show_intro("Load mapdefs");

	read_bmaps_p0();
	show_intro("Load picture data");

	read_settings();
	show_intro("Load settings");

	read_spells();
	show_intro("Load spells");

	read_skills();
	show_intro("Load skills");

	read_anims();
	show_intro("Load anims");

	read_bmaps();
	show_intro("Load bmaps");

	read_help_files();
	show_intro("Load help files");

	sound_play_music("orchestral.ogg", options.music_volume, 0, -1, MUSIC_MODE_DIRECT);
	show_intro(NULL);

	while (1)
	{
		SDL_Event event;

		SDL_PollEvent(&event);

		if (event.type == SDL_QUIT)
		{
			sound_freeall();
			sound_deinit();
			free_bitmaps();
			SYSTEM_End();
			return 0;
		}

		if (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN)
		{
			reset_keys();
			break;
		}

		/* force the thread to sleep */
		SDL_Delay(15);

		/* wait for keypress */
	}

	script_autoload();

	draw_info_format(COLOR_HGOLD, "Welcome to Atrinik version %s.", PACKAGE_VERSION);
	draw_info("Init network...", COLOR_GREEN);

	if (!socket_initialize())
	{
		exit(1);
	}

	LastTick = tmpGameTick = anim_tick = SDL_GetTicks();
	GameTicksSec = 0;

	while (!done)
	{
		done = Event_PollInputDevice();

		/* Have we been shutdown? */
		if (handle_socket_shutdown())
		{
			GameStatus = GAME_STATUS_INIT;
			continue;
		}

#ifdef INSTALL_SOUND
		if (music_global_fade)
		{
			sound_fadeout_music(music_new.flag);
		}
#endif

		GameTicksSec = LastTick - tmpGameTick;

		if (GameStatus > GAME_STATUS_CONNECT)
		{
			DoClient();
			/* Flush face request buffer. */
			request_face(0, 1);
		}

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
			display_layer2();
			display_layer3();
			display_layer4();

			if (GameStatus != GAME_STATUS_PLAY)
			{
				SDL_FillRect(ScreenSurface, NULL, 0);
			}

			if (esc_menu_flag)
			{
				map_udate_flag = 1;
			}
			else if (!options.force_redraw)
			{
				if (options.doublebuf_flag)
				{
					map_udate_flag--;
				}
				else
				{
					map_udate_flag = 0;
				}
			}
		}

		/* Get our ticks */
		if ((LastTick - tmpGameTick) > 1000)
		{
			tmpGameTick = LastTick;
			FrameCount = 0;
			GameTicksSec = 0;
		}

		if (GameStatus != GAME_STATUS_PLAY)
		{
			textwin_show(539, 485);
		}

		if (GameStatus == GAME_STATUS_PLAY)
		{
			SDL_Rect tmp_rect;
			tmp_rect.w = 275;

			if (cpl.input_mode == INPUT_MODE_CONSOLE)
				do_console();
			else if (cpl.input_mode == INPUT_MODE_NUMBER)
				do_number();
			else if (cpl.input_mode == INPUT_MODE_GETKEY)
				do_keybind_input();
		}
		else if (GameStatus == GAME_STATUS_WAITFORPLAY)
		{
			StringBlt(ScreenSurface, &SystemFont, "Transfer Character to Map...", 300, 300, COLOR_DEFAULT, NULL, NULL);
		}

		/* If not connected, walk through connection chain and/or wait for action */
		if (GameStatus != GAME_STATUS_PLAY)
		{
			if (!game_status_chain())
			{
				LOG(llevError, "Error connecting: GStatus: %d  SocketError: %d\n", GameStatus, socket_get_error());
				exit(1);
			}
		}

		/* Show main option menu */
		if (esc_menu_flag)
		{
			show_option(Screensize->x / 2, (Screensize->y / 2) - (Bitmaps[BITMAP_OPTIONS_ALPHA]->bitmap->h / 2));
		}

		if (map_transfer_flag)
		{
			StringBlt(ScreenSurface, &SystemFont, "Transfer Character to Map...", 300, 300, COLOR_DEFAULT, NULL, NULL);
		}

		/* Show the current dragged item */
		if ((drag = draggingInvItem(DRAG_GET_STATUS)))
		{
			item *Item = NULL;

			if (drag == DRAG_IWIN_INV)
			{
				Item = locate_item(cpl.win_inv_tag);
			}
			else if (drag == DRAG_IWIN_BELOW)
			{
				Item = locate_item(cpl.win_below_tag);
			}
			else if (drag == DRAG_QUICKSLOT)
			{
				Item = locate_item(cpl.win_quick_tag);
			}
			else if (drag == DRAG_PDOLL)
			{
				Item = locate_item(cpl.win_pdoll_tag);
			}

			SDL_GetMouseState(&x, &y);

			if (drag == DRAG_QUICKSLOT_SPELL)
			{
				sprite_blt(spell_list[quick_slots[cpl.win_quick_tag].groupNr].entry[quick_slots[cpl.win_quick_tag].classNr][quick_slots[cpl.win_quick_tag].spellNr].icon, x,y, NULL, NULL);
			}
			else
			{
				blt_inv_item_centered(Item, x, y);
			}
		}

		if (GameStatus < GAME_STATUS_REQUEST_FILES)
		{
			show_meta_server(start_server, metaserver_sel);
		}
		else if (GameStatus >= GAME_STATUS_REQUEST_FILES && GameStatus < GAME_STATUS_NEW_CHAR)
		{
			show_login_server();
		}
		else if (GameStatus == GAME_STATUS_NEW_CHAR)
		{
			cpl.menustatus = MENU_CREATE;
		}

		/* Show all kind of the big dialog windows */
		show_menu();

		/* We have a non-standard mouse-pointer (win-size changer, etc.) */
		if (cursor_type)
		{
			SDL_Rect rec;

			SDL_GetMouseState(&x, &y);
			rec.w = 14;
			rec.h = 1;
			rec.x = x - 7;
			rec.y = y - 2;
			SDL_FillRect(ScreenSurface, &rec, -1);
			rec.y = y - 5;
			SDL_FillRect(ScreenSurface, &rec, -1);
		}

		if (f_custom_cursor)
		{
			DisplayCustomCursor();
		}

		FrameCount++;
		LastTick = SDL_GetTicks();

		script_process();

		/* Process message animations */
		if ((GameStatus == GAME_STATUS_PLAY) && msg_anim.message[0] != '\0')
		{
			map_udate_flag = 2;

			if ((LastTick - msg_anim.tick) < 3000)
			{
				_BLTFX bmbltfx;
				int bmoff = (int) ((50.0f / 3.0f) * ((float) (LastTick - msg_anim.tick) / 1000.0f) * ((float) (LastTick - msg_anim.tick) / 1000.0f) + ((int) (150.0f * ((float) (LastTick - msg_anim.tick) / 3000.0f))));

				bmbltfx.alpha = 255;
				bmbltfx.flags = BLTFX_FLAG_SRCALPHA;

				if (LastTick - msg_anim.tick > 2000)
				{
					bmbltfx.alpha -= (int) (255.0f * ((float) (LastTick - msg_anim.tick - 2000) / 1000.0f));
				}

				StringBlt(ScreenSurface, &BigFont, msg_anim.message, Screensize->x / 2 - (StringWidth(&BigFont, msg_anim.message) / 2), 300 - bmoff, COLOR_BLACK, NULL, &bmbltfx);
				StringBlt(ScreenSurface, &BigFont, msg_anim.message, Screensize->x / 2 - (StringWidth(&BigFont, msg_anim.message) / 2) - 2 , 300 - 2 - bmoff, msg_anim.flags & 0xff, NULL, &bmbltfx);
			}
			else
			{
				msg_anim.message[0] = '\0';
			}
		}

		flip_screen();

		/* Force the thread to sleep */
		if (options.max_speed)
		{
			SDL_Delay(options.sleep);
		}
	}

	script_killall();
	save_interface_file();
	kill_widgets();
	save_options_dat();
	curl_global_cleanup();
	socket_deinitialize();
	sound_freeall();
	sound_deinit();
	free_bitmaps();
	SYSTEM_End();

	return 0;
}

/**
 * Draws the intro bitmap and updates progress bar.
 * @param text Text to show. */
static void show_intro(char *text)
{
	char buf[256];
	int x, y, progress, progress_x, progress_y;
	SDL_Rect box;

	current_intro++;

	x = Screensize->x / 2 - Bitmaps[BITMAP_INTRO]->bitmap->w / 2;
	y = Screensize->y / 2 - Bitmaps[BITMAP_INTRO]->bitmap->h / 2;

	progress_x = Screensize->x / 2 - Bitmaps[BITMAP_PROGRESS]->bitmap->w / 2;
	progress_y = Bitmaps[BITMAP_INTRO]->bitmap->h + y - Bitmaps[BITMAP_PROGRESS]->bitmap->h;

	sprite_blt(Bitmaps[BITMAP_INTRO], x, y, NULL, NULL);

	/* Update progress bar */
	sprite_blt(Bitmaps[BITMAP_PROGRESS_BACK], progress_x, progress_y, NULL, NULL);

	progress = MIN(100, current_intro * 8);
	box.x = 0;
	box.y = 0;
	box.h = Bitmaps[BITMAP_PROGRESS]->bitmap->h;
	box.w = (int) ((float) Bitmaps[BITMAP_PROGRESS]->bitmap->w / 100 * progress);
	sprite_blt(Bitmaps[BITMAP_PROGRESS], progress_x, progress_y, &box, NULL);

	if (text)
	{
		StringBlt(ScreenSurface, &SystemFont, text, progress_x + Bitmaps[BITMAP_PROGRESS]->bitmap->w / 3, progress_y + 5, COLOR_DEFAULT, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "** Press Key **", progress_x + Bitmaps[BITMAP_PROGRESS]->bitmap->w / 3, progress_y + 5, COLOR_DEFAULT, NULL, NULL);
	}

	snprintf(buf, sizeof(buf), "v. %s", PACKAGE_VERSION);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 10, progress_y + 5, COLOR_DEFAULT, NULL, NULL);
	flip_screen();
}

/**
 * Flip the screen. */
static void flip_screen()
{
	if (options.use_rect)
	{
		SDL_UpdateRect(ScreenSurface, 0, 0, Screensize->x, Screensize->y);
	}
	else
	{
		SDL_Flip(ScreenSurface);
	}
}
