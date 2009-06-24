/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

#include <include.h>

/* list of possible chars/race with setup when we want create a char */
_server_char *first_server_char = NULL;
/* if we login as new char, thats the values of it we set */
_server_char new_character;

/* THE main surface (backbuffer)*/
SDL_Surface *ScreenSurface;
SDL_Surface *ScreenSurfaceMap;
_Font MediumFont;
/* our main font*/
_Font SystemFont;
/* our main font - black outlined*/
_Font SystemFontOut;
/* bigger special font*/
_Font BigFont;
/* our main font with shadow*/
_Font Font6x3Out;
/* Server's attributes */
struct sockaddr_in insock;
ClientSocket csocket;
/* if an socket error, this is it */
int SocketStatusErrorNr;

Uint32 sdl_dgreen, sdl_gray1, sdl_gray2, sdl_gray3, sdl_gray4, sdl_blue1;

int music_global_fade = FALSE;
int mb_clicked = 0;

int debug_layer[MAXFACES];
int bmaptype_table_size;
_srv_client_files srv_client_files[SRV_CLIENT_FILES];

struct _options options;
Uint32 videoflags_full, videoflags_win;

struct _fire_mode fire_mode_tab[FIRE_MODE_INIT];
int RangeFireMode;

/* cache status... set this in hardware depend */
int CacheStatus;
/* SoundStatus 0=no 1= yes */
int SoundStatus;
/* map x,y len */
int MapStatusX;
int MapStatusY;

/* name of the server we want connect */
char ServerName[2048];
/* port addr */
int ServerPort;

/* name of the server we want connect */
char argServerName[2048];
/* port addr */
int argServerPort;

/* system time counter in ms since prg start */
uint32 LastTick;
/* ticks since this second frame in ms */
uint32 GameTicksSec;
/* used from several functions, just to store real ticks */
uint32 tmpGameTick;

int esc_menu_flag;
int esc_menu_index;

int f_custom_cursor = 0;
int x_custom_cursor = 0;
int y_custom_cursor = 0;

_bmaptype *bmap_table[BMAPTABLE];

/* update map area */
int map_udate_flag, map_transfer_flag, map_redraw_flag;
int GameStatusVersionFlag;
int GameStatusVersionOKFlag;
int request_file_chain, request_file_flags;

int ToggleScreenFlag;
char InputString[MAX_INPUT_STRING];
char InputHistory[MAX_HISTORY_LINES][MAX_INPUT_STRING];
int HistoryPos;
int CurrentCursorPos;

int InputCount, InputMax;
/* if true keyboard and game is in input str mode */
Boolean InputStringFlag;
/* if true, we had entered some in text mode and its ready */
Boolean InputStringEndFlag;
Boolean InputStringEscFlag;
struct gui_book_struct *gui_interface_book;
struct gui_party_struct *gui_interface_party;

/* the global status identifier */
_game_status GameStatus;

/* the stored "anim commands" we created out of anims.tmp */
_anim_table anim_table[MAXANIM];
/* get this from commands.c to this place */
Animations animations[MAXANIM];

_screensize Screensize;

_screensize Screendefs[16] =
{
    {800, 600, 0, 0},
    {960,  600, 160, 0},
    {1024, 768, 224, 168},
    {1100, 700, 210, 100},
    {1280, 720, 480, 120},
    {1280, 800, 480, 200},
    {1280, 960, 480, 360},
    {1280, 1024,480, 424},
    {1440, 900, 640, 300},
    {1400, 1050, 600, 450},
    {1600, 1200, 800, 600},
    {1680, 1050, 880, 450},
    {1920, 1080, 1120, 480},
    {1920, 1200, 1120, 600},
    {2048, 1536, 1248, 936},
    {2560, 1600, 1760, 1000},
};

/* face data */
_face_struct FaceList[MAX_FACE_TILES];

void init_game_data(void);
Boolean game_status_chain(void);
Boolean load_bitmap(int index);

#define NCOMMANDS (sizeof(commands) / sizeof(struct CmdMapping))

_server *start_server, *end_server;
int metaserver_start, metaserver_sel, metaserver_count;

typedef enum _pic_type
{
	PIC_TYPE_DEFAULT, PIC_TYPE_PALETTE, PIC_TYPE_TRANS
} _pic_type;

typedef struct _bitmap_name
{
	char *name;
	_pic_type type;
} _bitmap_name ;

/* for loading, use BITMAP_xx in the other modules*/
static _bitmap_name  bitmap_name[BITMAP_INIT] =
{
    {"palette.png", PIC_TYPE_PALETTE},
    {"font7x4.png", PIC_TYPE_PALETTE},
    {"font6x3out.png", PIC_TYPE_PALETTE},
    {"font_big.png", PIC_TYPE_PALETTE},
    {"font7x4out.png", PIC_TYPE_PALETTE},
	{"font11x15.png", PIC_TYPE_PALETTE},
    {"intro.png", PIC_TYPE_DEFAULT},
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

    {"traped.png", PIC_TYPE_TRANS},
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
};

#define BITMAP_MAX (sizeof(bitmap_name) / sizeof(struct _bitmap_name))
_Sprite *Bitmaps[BITMAP_MAX];

static void count_meta_server(void);
static void flip_screen(void);
static void show_intro(char *text);
static void delete_player_lists(void);
void reset_input_mode(void);

static void delete_player_lists(void)
{
	int i, ii;

    for (i = 0; i < FIRE_MODE_INIT; i++)
    {
        fire_mode_tab[i].amun = FIRE_ITEM_NO;
        fire_mode_tab[i].item = FIRE_ITEM_NO;
        fire_mode_tab[i].skill = NULL;
		fire_mode_tab[i].spell = NULL;
        fire_mode_tab[i].name[0] = 0;
    }

    for (i = 0; i < SKILL_LIST_MAX; i++)
    {
        for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
        {
			if (skill_list[i].entry[ii].flag == LIST_ENTRY_KNOWN)
	            skill_list[i].entry[ii].flag = LIST_ENTRY_USED;
        }
    }

    for (i = 0;i < SPELL_LIST_MAX; i++)
    {
        for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
        {
			if (spell_list[i].entry[0][ii].flag == LIST_ENTRY_KNOWN)
				spell_list[i].entry[0][ii].flag = LIST_ENTRY_USED;

			if (spell_list[i].entry[1][ii].flag == LIST_ENTRY_KNOWN)
	            spell_list[i].entry[1][ii].flag = LIST_ENTRY_USED;
        }
    }
}

/* pre init, overrule in hardware module if needed */
void init_game_data(void)
{
	int i;
	textwin_init();
	textwin_flags = 0;
	first_server_char = NULL;

	esc_menu_flag = 0;
	srand(time(NULL));

	memset(anim_table, 0, sizeof(anim_table));
	memset(animations, 0, sizeof(animations));
	memset(bmaptype_table, 0, sizeof(bmaptype_table));

	ToggleScreenFlag = 0;
	KeyScanFlag = 0;

	memset(&fire_mode_tab, 0, sizeof(fire_mode_tab));

	for (i = 0; i < MAXFACES; i++)
		debug_layer[i] = 1;

	memset(&options, 0, sizeof(struct _options));
	InitMapData("", 0, 0, 0, 0, "no_music");

	for (i = 0; i < (int) BITMAP_MAX; i++)
		Bitmaps[i] = NULL;

	memset(FaceList, 0, sizeof(struct _face_struct) * MAX_FACE_TILES);
	memset(&cpl, 0, sizeof(cpl));
	cpl.ob = player_item();

	init_keys();
	init_player_data();
	clear_metaserver_data();
	reset_input_mode();

	/* anim queue of current active map */
	start_anim = NULL;

	map_transfer_flag = 0;
	start_server = NULL;
	ServerName[0] = 0;
	ServerPort = 13327;
	argServerName[0] = 0;
	argServerPort = 13327;

	SoundSystem = SOUND_SYSTEM_OFF;
	GameStatus = GAME_STATUS_INIT;
	CacheStatus = CF_FACE_CACHE;
	SoundStatus = 1;
	MapStatusX = MAP_MAX_SIZE;
	MapStatusY = MAP_MAX_SIZE;
	map_udate_flag = 2;
	map_redraw_flag = 1;
	/* if true keyboard and game is in input str mode */
	InputStringFlag = 0;
	InputStringEndFlag = 0;
	InputStringEscFlag = 0;
	csocket.fd = SOCKET_NO;
	RangeFireMode = 0;
	gui_interface_book = NULL;
	gui_interface_party = NULL;
	options.resolution = 2;
	options.playerdoll = 0;
#ifdef WIDGET_SNAP
    options.widget_snap = 0;
#endif

	txtwin[TW_MIX].size = 50;
    txtwin[TW_MSG].size = 22;
    txtwin[TW_CHAT].size = 22;
 	options.mapstart_x = -10;
    options.mapstart_y = 100;

	memset(media_file, 0, sizeof(_media_file ) * MEDIA_MAX);
	/* buffered media files */
	media_count = 0;
	/* show this media file */
	media_show = MEDIA_SHOW_NO;

	/* now load options, allowing the user to override the presetings */
	load_options_dat();

	Screensize = Screendefs[options.resolution];
	init_widgets_fromCurrent();

	textwin_clearhistory();
	delete_player_lists();
}

/* Save the option file. */
void save_options_dat(void)
{
   	char txtBuffer[20];
   	int i = -1, j = -1;
   	FILE *stream;

   	if (!(stream = fopen(OPTION_FILE, "w")))
		return;

   	fputs("##########################################\n", stream);
   	fputs("# This is the Atrinik client option file #\n", stream);
   	fputs("##########################################\n", stream);

	snprintf(txtBuffer, sizeof(txtBuffer), "%%21 %d\n", txtwin[TW_MSG].size);
    fputs(txtBuffer, stream);

    snprintf(txtBuffer, sizeof(txtBuffer), "%%22 %d\n", txtwin[TW_CHAT].size);
    fputs(txtBuffer, stream);

   	while (opt_tab[++i])
   	{
      	fputs("\n# ", stream);
      	fputs(opt_tab[i], stream);
      	fputs("\n", stream);

      	while (opt[++j].name && opt[j].name[0] != '#')
      	{
         	fputs(opt[j].name,stream);

         	switch (opt[j].value_type)
         	{
            	case VAL_BOOL:
		    	case VAL_INT:
			  		snprintf(txtBuffer, sizeof(txtBuffer), " %d",  *((int *)opt[j].value));
			  		break;

				case VAL_U32:
					snprintf(txtBuffer, sizeof(txtBuffer), " %d",  *((uint32 *)opt[j].value));
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

/* Load the option file. */
void load_options_dat(void)
{
	int i = -1, pos;
	FILE *stream;
	char line[256], keyword[256], parameter[256];

	/* Fill all options with default values */
	while (opt[++i].name)
	{
		if (opt[i].name[0] == '#')
			continue;

		switch (opt[i].value_type)
		{
			case VAL_BOOL:
			case VAL_INT:
				*((int *)opt[i].value) = opt[i].default_val;
				break;

			case VAL_U32:
				*((uint32 *)opt[i].value) = opt[i].default_val;
				break;

			case VAL_CHAR:
				*((uint8 *)opt[i].value)= (uint8) opt[i].default_val;
				break;
		}
	}

	txtwin_start_size = txtwin[TW_MIX].size;

	/* Read the options from file */
	if (!(stream = fopen(OPTION_FILE, "r")))
	{
		LOG(LOG_MSG, "Can't find file %s. Using defaults.\n", OPTION_FILE);
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
            }
            continue;
        }

		i = 0;

		while (line[i] && line[i] != ':')
			i++;

		line[++i] = 0;
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
						*((int *)opt[pos].value) = i;
						break;

					case VAL_U32:
						*((uint32 *)opt[pos].value) = i;
						break;

					case VAL_CHAR:
						*((uint8 *)opt[pos].value)= (uint8) i;
						break;
				}
			}
		}
	}

   	fclose(stream);
}


/* asynchron connection chain */
Boolean game_status_chain(void)
{
	char buf[1024];

	/* autoinit or reset prg data */
	if (GameStatus == GAME_STATUS_INIT)
	{
		LOG(LOG_MSG, "GAMES_STATUS_INIT_1\n");

		map_udate_flag = 2;
		delete_player_lists();
		LOG(LOG_MSG, "GAMES_STATUS_INIT_2\n");

#ifdef INSTALL_SOUND
		if (!music.flag || strcmp(music.name, "orchestral.ogg"))
			sound_play_music("orchestral.ogg", options.music_volume, -1, 0, MUSIC_MODE_DIRECT);
#endif

		clear_map();
		LOG(LOG_MSG, "GAMES_STATUS_INIT_3\n");
		clear_metaserver_data();
		LOG(LOG_MSG, "GAMES_STATUS_INIT_4\n");
		GameStatus = GAME_STATUS_META;
	}
	/* connect to meta and get server data */
	else if (GameStatus == GAME_STATUS_META)
	{
		map_udate_flag = 2;

		if (argServerName[0] != 0)
			add_metaserver_data(argServerName, argServerPort, -1, "user server", "Server from -server '...' command line.");

		/* skip of -nometa in command line or no metaserver set in options */
		if (options.no_meta)
		{
			draw_info("Option '-nometa'. Metaserver ignored.", COLOR_GREEN);
		}
		else
		{
			draw_info("Query metaserver...", COLOR_GREEN);
			metaserver_connect();
		}

		add_metaserver_data("127.0.0.1", 13327, -1, "local", "localhost. Start server before you try to connect.");
		count_meta_server();
		draw_info("Select a server.", COLOR_GREEN);
		GameStatus = GAME_STATUS_START;
	}
	else if (GameStatus == GAME_STATUS_START)
	{
		map_udate_flag = 2;

		if (csocket.fd != SOCKET_NO)
			SOCKET_CloseSocket(csocket.fd);

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
		snprintf(buf, sizeof(buf), "Trying server %s:%d...", ServerName, ServerPort);
		draw_info(buf, COLOR_GREEN);
		GameStatus = GAME_STATUS_CONNECT;
	}
	else if (GameStatus == GAME_STATUS_CONNECT)
	{
		GameStatusVersionFlag = 0;

		if (!SOCKET_OpenSocket(&csocket.fd, &csocket, ServerName, ServerPort))
		{
			draw_info("Connection failed!", COLOR_RED);
			GameStatus = GAME_STATUS_START;
		}

		GameStatus = GAME_STATUS_VERSION;
		draw_info("Connected. Exchange version.", COLOR_GREEN);
	}
	else if (GameStatus == GAME_STATUS_VERSION)
	{
		SendVersion(csocket);
		GameStatus = GAME_STATUS_WAITVERSION;
	}
	else if (GameStatus == GAME_STATUS_WAITVERSION)
	{
		/* Perhaps here should be a timer?
		 * The version exchange server<->client is asynchron
		 * so perhaps the server sends its version faster
		 * than the client sends it to the server */

		/* Wait for version answer when needed */
		if (GameStatusVersionFlag)
		{
			/* False version! */
			if (!GameStatusVersionOKFlag)
			{
				GameStatus = GAME_STATUS_START;
			}
			else
			{
				draw_info("Version confirmed.\nStarting login procedure...", COLOR_GREEN);
				GameStatus = GAME_STATUS_SETUP;
			}
		}
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

		cs_write_string(csocket.fd, buf, strlen(buf));
		request_file_chain = 0;
		request_file_flags = 0;

		GameStatus = GAME_STATUS_WAITSETUP;
	}
	else if (GameStatus == GAME_STATUS_REQUEST_FILES)
	{
		/* check setting list */
		if (request_file_chain == 0)
		{
			if (srv_client_files[SRV_CLIENT_SETTINGS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 1;
				RequestFile(csocket, SRV_CLIENT_SETTINGS);
			}
			else
				request_file_chain = 2;
		}
		/* check spell list */
		else if (request_file_chain == 2)
		{
			if (srv_client_files[SRV_CLIENT_SPELLS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 3;
				RequestFile(csocket, SRV_CLIENT_SPELLS);
			}
			else
				request_file_chain = 4;
		}
		/* check skill list */
		else if (request_file_chain == 4)
		{
			if (srv_client_files[SRV_CLIENT_SKILLS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 5;
				RequestFile(csocket, SRV_CLIENT_SKILLS);
			}
			else
				request_file_chain = 6;
		}
		else if (request_file_chain == 6)
		{
			if (srv_client_files[SRV_CLIENT_BMAPS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 7;
				RequestFile(csocket, SRV_CLIENT_BMAPS);
			}
			else
				request_file_chain = 8;
		}
		else if (request_file_chain == 8)
		{
			if (srv_client_files[SRV_CLIENT_ANIMS].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 9;
				RequestFile(csocket, SRV_CLIENT_ANIMS);
			}
			else
				request_file_chain = 10;
		}
		else if (request_file_chain == 10)
		{
			if (srv_client_files[SRV_CLIENT_HFILES].status == SRV_CLIENT_STATUS_UPDATE)
			{
				request_file_chain = 11;
				RequestFile(csocket, SRV_CLIENT_HFILES);
			}
			else
				request_file_chain = 12;
		}
		/* We have all files - start check */
		else if (request_file_chain == 12)
		{
			/* This ensures one loop tick and updating the messages */
			request_file_chain++;
		}
		else if (request_file_chain == 13)
		{
			/* OK, now we check for bmap and anims processing... */
			read_bmap_tmp();
			read_anim_tmp();
			load_settings();
			read_help_files();
			request_file_chain++;
		}
		else if (request_file_chain == 14)
		{
			/* This ensures one loop tick and updating the messages */
			request_file_chain++;
		}
		else if (request_file_chain == 15)
			GameStatus = GAME_STATUS_ADDME;
	}
	else if (GameStatus == GAME_STATUS_ADDME)
	{
		map_transfer_flag = 0;
		SendAddMe(csocket);
		cpl.name[0] = 0;
		cpl.password[0] = 0;
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
			GameStatus = GAME_STATUS_LOGIN;
		else if (InputStringFlag == 0 && InputStringEndFlag)
		{
			strcpy(cpl.name, InputString);
			LOG(LOG_MSG, "Login: send name %s\n", InputString);
			send_reply(InputString);
			GameStatus = GAME_STATUS_LOGIN;
			/* Now wait again for next server question*/
		}
	}
	else if (GameStatus == GAME_STATUS_PSWD)
	{
		map_transfer_flag = 0;

		textwin_clearhistory();

		/* We have a finished console input */
		if (InputStringEscFlag)
			GameStatus = GAME_STATUS_LOGIN;
		else if (InputStringFlag == 0 && InputStringEndFlag)
		{
			strncpy(cpl.password, InputString, 39);
			cpl.password[39] = 0;

			LOG(LOG_MSG, "Login: send password <*****>\n");
			send_reply(cpl.password);
			GameStatus = GAME_STATUS_LOGIN;
			/* Now wait again for next server question*/
		}
	}
	else if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		map_transfer_flag = 0;

		/* We have a finished console input */
		if (InputStringEscFlag)
			GameStatus = GAME_STATUS_LOGIN;
		else if (InputStringFlag == 0 && InputStringEndFlag)
		{
			LOG(LOG_MSG, "Login: send verify password <*****>\n");
			send_reply(InputString);
			GameStatus = GAME_STATUS_LOGIN;
			/* Now wait again for next server question*/
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


/* Load the skin and standard gfx */
void load_bitmaps(void)
{
	int i;

	/* add later better error handling here */
	for (i = 0; i <= BITMAP_INTRO; i++)
		load_bitmap(i);

	CreateNewFont(Bitmaps[BITMAP_FONT1], &SystemFont, 16, 16, 1);
	CreateNewFont(Bitmaps[BITMAP_FONTMEDIUM], &MediumFont, 16, 16, 1);
	CreateNewFont(Bitmaps[BITMAP_FONT1OUT], &SystemFontOut, 16, 16, 1);
	CreateNewFont(Bitmaps[BITMAP_FONT6x3OUT], &Font6x3Out, 16, 16, -1);
	CreateNewFont(Bitmaps[BITMAP_BIGFONT], &BigFont, 11, 16, 3);
}

Boolean load_bitmap(int index)
{
	char buf[2048];
	uint32 flags = 0;

	snprintf(buf, sizeof(buf), "%s%s", GetBitmapDirectory(), bitmap_name[index].name);

	if (bitmap_name[index].type == PIC_TYPE_PALETTE)
		flags |= SURFACE_FLAG_PALETTE;

	if (bitmap_name[index].type == PIC_TYPE_TRANS)
		flags |= SURFACE_FLAG_COLKEY_16M;

	Bitmaps[index] = sprite_load_file(buf, flags);

	if (!Bitmaps[index] || !Bitmaps[index]->bitmap)
	{
		LOG(LOG_MSG, "load_bitmap(): Can't load bitmap %s\n", buf);
		return 0;
	}

	return 1;
}

/* Free the skin and standard gfx */
void free_bitmaps(void)
{
	int i;

	for (i = 0; i < (int) BITMAP_MAX; i++)
		sprite_free_sprite(Bitmaps[i]);
}

void free_faces(void)
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


void clear_metaserver_data(void)
{
	_server *node, *tmp;
	void *tmp_free;

	node = start_server;

	for (; node ;)
	{
		tmp_free = &node->nameip;
		FreeMemory(tmp_free);

		tmp_free = &node->version;
		FreeMemory(tmp_free);

		tmp_free = &node->desc;
		FreeMemory(tmp_free);

		tmp = node->next;
		tmp_free = &node;
		FreeMemory(tmp_free);
		node = tmp;
	}

	start_server = NULL;
	end_server = NULL;
	metaserver_start = 0;
	metaserver_sel = 0;
	metaserver_count = 0;
}

void add_metaserver_data(char *server, int port, int player, char *ver, char *desc)
{
	_server *node;

	node = (_server*) _malloc(sizeof(_server), "add_metaserver_data(): add server struct");
	memset(node, 0, sizeof(_server));

	if (!start_server)
		start_server = node;

	if (!end_server)
		end_server = node;
	else
		end_server->next = node;

	end_server = node;

	node->player = player;
	node->port = port;
	node->nameip = _malloc(strlen(server) + 1, "add_metaserver_data(): nameip string");
	strcpy(node->nameip, server);
	node->version = _malloc(strlen(ver) + 1, "add_metaserver_data(): version string");
	strcpy(node->version, ver);
	node->desc = _malloc(strlen(desc) + 1, "add_metaserver_data(): desc string");
	strcpy(node->desc, desc);
}

static void count_meta_server(void)
{
	_server *node = start_server;

	for (metaserver_count = 0; node; metaserver_count++)
		node = node->next;
}

void get_meta_server_data(int num, char *server, int *port)
{
	_server *node = start_server;
	int i;

	for (i = 0; node; i++)
	{
		if (i == num)
		{
			strcpy(server, node->nameip);
			*port = node->port;
			return;
		}

		node = node->next;
	}
}

void reset_input_mode(void)
{
	InputString[0] = 0;
	InputCount = 0;
	HistoryPos = 0;
	InputHistory[0][0] = 0;
	CurrentCursorPos = 0;
	InputStringFlag = 0;
	InputStringEndFlag = 0;
	InputStringEscFlag = 0;
}

void open_input_mode(int maxchar)
{
	reset_input_mode();
	InputMax = maxchar;
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	if (cpl.input_mode != INPUT_MODE_NUMBER)
		cpl.inventory_win = IWIN_BELOW;

	/* Ff true keyboard and game is in input string mode */
	InputStringFlag = 1;
}


static void play_action_sounds(void)
{
    if (!cpl.stats.food)
    {
        sound_play_one_repeat(SOUND_WARN_FOOD, SPECIAL_SOUND_FOOD);
    }

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
		/* more than 10% damage */
        if (cpl.warn_hp == 2)
            sound_play_effect(SOUND_WARN_HP2, 0, 100);
        else
            sound_play_effect(SOUND_WARN_HP, 0, 100);

        cpl.warn_hp = 0;
    }
}

void list_vid_modes(void)
{
    const SDL_VideoInfo* vinfo = NULL;
    SDL_Rect **modes;
    int i;

	LOG(LOG_MSG, "List Video Modes\n");

	/* Get available fullscreen/hardware modes */
	modes = SDL_ListModes(NULL, SDL_HWACCEL);

	/* Check if there are any modes available */
	if (modes == (SDL_Rect **)0)
	{
		LOG(LOG_MSG, "No modes available!\n");
		exit(-1);
	}

	/* Check if or resolution is restricted */
	if (modes == (SDL_Rect **)-1)
	{
		LOG(LOG_MSG, "All resolutions available.\n");
	}
	else
	{
		/* Print valid modes */
		LOG(LOG_MSG, "Available Modes\n");
		for (i = 0; modes[i]; ++i)
			LOG(LOG_MSG, "  %d x %d\n", modes[i]->w, modes[i]->h);
	}

	vinfo = SDL_GetVideoInfo();
	LOG(LOG_MSG, "VideoInfo: hardware surfaces? %s\n", vinfo->hw_available ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: windows manager? %s\n", vinfo->wm_available ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: hw to hw blit? %s\n", vinfo->blit_hw ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: hw to hw ckey blit? %s\n", vinfo->blit_hw_CC ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: hw to hw alpha blit? %s\n", vinfo->blit_hw_A ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: sw to hw blit? %s\n", vinfo->blit_sw ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: sw to hw ckey blit? %s\n", vinfo->blit_sw_CC ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: sw to hw alpha blit? %s\n", vinfo->blit_sw_A ? "yes":  "no");
	LOG(LOG_MSG, "VideoInfo: color fill? %s\n", vinfo->blit_fill ? "yes" : "no");
	LOG(LOG_MSG, "VideoInfo: video memory: %dKB\n", vinfo->video_mem);
}

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

/* map & player & anims */
static void display_layer1(void)
{
	static int gfx_toggle = 0;
    SDL_Rect    rect;

    /* we clear the screen and start drawing
     * this is done every frame, this should and hopefully can be optimized. */
    SDL_FillRect(ScreenSurface, NULL, 0);

    /* we recreate the ma only when there is a change (which happens maybe 1-3 times a second) */
    if (map_redraw_flag)
    {
		SDL_FillRect(ScreenSurfaceMap, NULL, 0);
        map_draw_map();
#if 0
        SDL_FreeSurface(zoomed);
        if (options.zoom==100)
            zoomed=SDL_DisplayFormat(ScreenSurfaceMap);
        else
            zoomed=zoomSurface(ScreenSurfaceMap, options.zoom/100.0, options.zoom/100.0, options.smooth);
#endif
        map_redraw_flag = 0;
    }
    rect.x = options.mapstart_x;
    rect.y = options.mapstart_y;
    SDL_BlitSurface(ScreenSurfaceMap, NULL, ScreenSurface, &rect);

    /* the damage numbers */
    play_anims();

    /* draw warning icons above player */
    if ((gfx_toggle++ & 63) < 25)
    {
		if (options.warning_hp && ((float)cpl.stats.hp / (float)cpl.stats.maxhp)*100 <= options.warning_hp)
			sprite_blt(Bitmaps[BITMAP_WARN_HP],options.mapstart_x + 393, options.mapstart_y + 298, NULL, NULL);
    }
    else
    {
		/* Low food */
		if (options.warning_food &&  ((float)cpl.stats.food/1000.0f)*100 <= options.warning_food)
			sprite_blt(Bitmaps[BITMAP_WARN_FOOD],options.mapstart_x + 390, options.mapstart_y + 294, NULL, NULL);
    }
}

static void display_layer2(void)
{
    cpl.container = NULL; /* this will be set right on the fly in get_inventory_data() */

    if (GameStatus == GAME_STATUS_PLAY)
    {
        /* TODO: optimize, only call this functions when something in the inv changed */
        cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot,
                                             &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN,
                                             INVITEMYLEN);
        cpl.real_weight = cpl.window_weight;
        cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot,
                                               &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN,
                                               INVITEMBELOWYLEN);
    }

}

/* display the widgets (graphical user interface) */
static void display_layer3(void)
{
    /* process the widgets */
    if(GameStatus  == GAME_STATUS_PLAY)
    {
        process_widgets();
    }
}

static void DisplayCustomCursor(void)
{
    if(f_custom_cursor == MSCURSOR_MOVE)
    {
        /* display the cursor */
        sprite_blt(Bitmaps[BITMAP_MSCURSOR_MOVE],
                    x_custom_cursor-(Bitmaps[BITMAP_MSCURSOR_MOVE]->bitmap->w/2),
                    y_custom_cursor-(Bitmaps[BITMAP_MSCURSOR_MOVE]->bitmap->h/2),
                    NULL,
                    NULL);
    }
}

/* dialogs, highest-priority layer */
static void display_layer4(void)
{
    if (GameStatus == GAME_STATUS_PLAY)
    {
        /* we have to make sure that this two windows get closed/hidden right */
        cur_widget[IN_CONSOLE_ID].show = FALSE;
        cur_widget[IN_NUMBER_ID].show = FALSE;

        if (cpl.input_mode == INPUT_MODE_CONSOLE)
            do_console();
        else if (cpl.input_mode == INPUT_MODE_NUMBER)
            do_number();
        else if (cpl.input_mode == INPUT_MODE_GETKEY)
            do_keybind_input();
    }

    /* show main-option menu */
    if (esc_menu_flag)
    {
        show_option(Screensize.x / 2, (Screensize.y/2)-(Bitmaps[BITMAP_OPTIONS_ALPHA]->bitmap->h/2));
    }
}

int main(int argc, char *argv[])
{
	char buf[256];
	int x, y;
	int drag;
	uint32 anim_tick;
    Uint32 videoflags;
    int i, done = 0, FrameCount = 0;
    fd_set tmp_read, tmp_write, tmp_exceptions;
    int pollret, maxfd;
	struct timeval timeout;

	init_game_data();

    while (argc > 1)
    {
		--argc;

        if (strcmp(argv[argc - 1], "-port") == 0)
        {
			argServerPort = atoi(argv[argc]);
            --argc;
        }
		else if (strcmp(argv[argc - 1], "-server") == 0)
        {
			strcpy(argServerName, argv[argc]);
            --argc;
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
            LOG(LOG_MSG, tmp);
            fprintf(stderr, "%s", tmp);
            exit(1);
        }
	}

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
    {
		LOG(LOG_ERROR, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

	atexit(SDL_Quit);
	/* Start the system after starting SDL */
    SYSTEM_Start();
    list_vid_modes();

#ifdef INSTALL_OPENGL
	if (options.use_gl)
	{
		const SDL_VideoInfo* info = NULL;
		info = SDL_GetVideoInfo();

		SDL_GL_LoadLibrary(NULL);

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		options.used_video_bpp = info->vfmt->BitsPerPixel;
		LOG(LOG_MSG, "Using OpenGL: bpp:%d\n", options.used_video_bpp);
		videoflags = SDL_OPENGL;
	}
#endif

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

	if ((ScreenSurface = SDL_SetVideoMode(Screensize.x, Screensize.y, options.used_video_bpp, videoflags)) == NULL)
    {
        /* We have a problem, not supportet screensize */
        /* If we have higher resolution we try the default 800x600 */
        if (Screensize.x > 800 && Screensize.y > 600)
        {
            LOG(LOG_ERROR, "Try to set to default 800x600...\n");
            Screensize = Screendefs[0];
            options.resolution = 0;

            if ((ScreenSurface = SDL_SetVideoMode(Screensize.x, Screensize.y, options.used_video_bpp, videoflags)) == NULL)
            {
                /* Now we have a really really big problem */
                LOG(LOG_ERROR, "Couldn't set %dx%dx%d video mode: %s\n", Screensize.x, Screensize.y, options.used_video_bpp, SDL_GetError());
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

	/* add later better error handling here */
	for (i = 5; i < (int) BITMAP_MAX; i++)
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

	sound_play_music("orchestral.ogg", options.music_volume, -1, 0, MUSIC_MODE_DIRECT);
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
		SDL_Delay(25);

		/* wait for keypress */
	}

	sprintf(buf, "Welcome to Atrinik version %s", PACKAGE_VERSION);
	draw_info(buf, COLOR_HGOLD);

	draw_info("Init network...", COLOR_GREEN);

	/* log in function*/
	if (!SOCKET_InitSocket())
		exit(1);

	maxfd = csocket.fd + 1;
	LastTick = tmpGameTick = anim_tick = SDL_GetTicks();
	/* ticks since this second frame in ms */
	GameTicksSec = 0;

   	/* the one and only main loop */
   	/* TODO: frame update can be optimized. It uses some cpu time because it
	 * draws every loop some parts.*/
	while (!done)
	{
		done = Event_PollInputDevice();

#ifdef INSTALL_SOUND
		if (music_global_fade)
			sound_fadeout_music(music_new.flag);
#endif

        /* get our ticks */
        if ((LastTick - tmpGameTick) > 1000)
        {
            tmpGameTick = LastTick;
            FrameCount = 0;
            GameTicksSec = 0;
        }

        GameTicksSec = LastTick - tmpGameTick;

        if (GameStatus > GAME_STATUS_CONNECT)
        {
            if (csocket.fd == SOCKET_NO)
            {
                /* Connection closed, so we go back to INIT here */
                if (GameStatus == GAME_STATUS_PLAY)
                    GameStatus = GAME_STATUS_INIT;
                else
                    GameStatus = GAME_STATUS_START;
            }
            else
            {
                FD_ZERO(&tmp_read);
                FD_ZERO(&tmp_write);
                FD_ZERO(&tmp_exceptions);

                FD_SET((unsigned int )csocket.fd, &tmp_exceptions);
                FD_SET((unsigned int )csocket.fd, &tmp_read);
                FD_SET((unsigned int )csocket.fd, &tmp_write);

#if 0
                if (MAX_TIME != 0)
                {
                    timeout.tv_sec = MAX_TIME / 100000;
                    timeout.tv_usec = MAX_TIME % 100000;
                }
				else
				{
                    timeout.tv_sec = 0;
                    timeout.tv_usec =0;
				}
#endif

				timeout.tv_sec = 0;
				timeout.tv_usec =0;

                /* main poll point for the socket */
                if ((pollret = select(maxfd, &tmp_read, &tmp_write, &tmp_exceptions, &timeout)) == -1)
                    LOG(LOG_MSG, "Got errno %d on selectcall.\n", SOCKET_GetError());
                else if (FD_ISSET(csocket.fd, &tmp_read))
                    DoClient(&csocket);

				/* flush face request buffer */
				request_face(0, 1);
            }
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
                SDL_FillRect(ScreenSurface, NULL, 0);

			if (esc_menu_flag)
				map_udate_flag = 1;
			else if (!options.force_redraw)
            {
                if (options.doublebuf_flag)
                    map_udate_flag--;
                else
                    map_udate_flag = 0;
            }
        }

		if (GameStatus != GAME_STATUS_PLAY)
            textwin_show(539, 485);

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
			StringBlt(ScreenSurface, &SystemFont, "Transfer Character to Map...", 300, 300, COLOR_DEFAULT, NULL, NULL);

		/* If not connected, walk through connection chain and/or wait for action */
		if (GameStatus != GAME_STATUS_PLAY)
		{
			if (!game_status_chain())
			{
				LOG(LOG_ERROR, "Error connecting: GStatus: %d  SocketError: %d\n", GameStatus, SOCKET_GetError());
				exit(1);
			}
		}

		/* Show main option menu */
		if (esc_menu_flag)
			show_option(Screensize.x / 2, (Screensize.y / 2) - (Bitmaps[BITMAP_OPTIONS_ALPHA]->bitmap->h / 2));

		if (map_transfer_flag)
			StringBlt(ScreenSurface, &SystemFont, "Transfer Character to Map...", 300, 300, COLOR_DEFAULT, NULL, NULL);

		/* Show the current dragged item */
		if ((drag = draggingInvItem(DRAG_GET_STATUS)))
		{
			item *Item = NULL;

			if (drag == DRAG_IWIN_INV)
				Item = locate_item(cpl.win_inv_tag);
			else if (drag == DRAG_IWIN_BELOW)
				Item = locate_item(cpl.win_below_tag);
			else if (drag == DRAG_QUICKSLOT)
				Item = locate_item(cpl.win_quick_tag);
			else if (drag == DRAG_PDOLL)
				Item = locate_item(cpl.win_pdoll_tag);

			SDL_GetMouseState(&x, &y);

			if (drag == DRAG_QUICKSLOT_SPELL)
				sprite_blt(spell_list[quick_slots[cpl.win_quick_tag].groupNr].entry[quick_slots[cpl.win_quick_tag].classNr][quick_slots[cpl.win_quick_tag].spellNr].icon, x,y, NULL, NULL);
			else
     		  blt_inv_item_centered(Item, x, y);
		}

		if (GameStatus < GAME_STATUS_REQUEST_FILES)
			show_meta_server(start_server, metaserver_start, metaserver_sel);
	    else if (GameStatus >= GAME_STATUS_REQUEST_FILES && GameStatus < GAME_STATUS_NEW_CHAR)
			show_login_server();
		else if (GameStatus == GAME_STATUS_NEW_CHAR)
			cpl.menustatus = MENU_CREATE;

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
			DisplayCustomCursor();

		/* We count always last frame */
		FrameCount++;
		LastTick = SDL_GetTicks();

  		/* Print frame rate */
		if (options.show_frame && GameStatus == GAME_STATUS_PLAY && cpl.menustatus == MENU_NO)
		{
            sprintf(buf, "fps %d (%d) (%d %d) %s%s%s%s%s%s%s%s%s%s %d %d", ((LastTick - tmpGameTick) / FrameCount) ? 1000 / ((LastTick - tmpGameTick) / FrameCount) : 0, (LastTick - tmpGameTick) / FrameCount, GameStatus, cpl.input_mode, ScreenSurface->flags & SDL_FULLSCREEN ? "F" : "", ScreenSurface->flags & SDL_HWSURFACE ? "H" : "S", ScreenSurface->flags & SDL_HWACCEL ? "A" : "", ScreenSurface->flags & SDL_DOUBLEBUF ? "D" : "", ScreenSurface->flags & SDL_ASYNCBLIT ? "a" : "", ScreenSurface->flags & SDL_ANYFORMAT ? "f" : "", ScreenSurface->flags & SDL_HWPALETTE ? "P" : "", options.rleaccel_flag ? "R" : "", options.force_redraw ? "r" : "", options.use_rect ? "u" : "", options.used_video_bpp, options.real_video_bpp);


			StringBlt(ScreenSurface, &SystemFont, buf, cur_widget[MAPNAME_ID].x1, cur_widget[MAPNAME_ID].y1 + 12, COLOR_DEFAULT, NULL, NULL);
		}

		flip_screen();

		/* Force the thread to sleep */
		if (!options.max_speed)
			SDL_Delay(options.sleep);
	}

	save_interface_file();
    kill_widgets();
	/* Save options at exit */
	save_options_dat();
	/* We have left main loop and shut down the client */
	SOCKET_DeinitSocket();
	sound_freeall();
	sound_deinit();
	free_bitmaps();
	SYSTEM_End();

	return 0;
}

static void show_intro(char *text)
{
	char buf[256];
	int x, y;

    x = Screensize.xoff / 2;
    y = Screensize.yoff / 2;

    sprite_blt(Bitmaps[BITMAP_INTRO], x, y, NULL, NULL);

	if (text)
		StringBlt(ScreenSurface, &SystemFont, text, x + 370, y + 295, COLOR_DEFAULT, NULL, NULL);
	else
		StringBlt(ScreenSurface, &SystemFont, "** Press Key **", x + 375, y + 585, COLOR_DEFAULT, NULL, NULL);

	snprintf(buf, sizeof(buf), "v. %s", PACKAGE_VERSION);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 10, y + 585, COLOR_DEFAULT, NULL, NULL);
	flip_screen();
}


static void flip_screen(void)
{
#ifdef INSTALL_OPENGL
	if (options.use_gl)
		SDL_GL_SwapBuffers();
    else
    {
#endif

	if (options.use_rect)
		SDL_UpdateRect(ScreenSurface, 0, 0, Screensize.x, Screensize.y);
    else
		SDL_Flip(ScreenSurface);
#ifdef INSTALL_OPENGL
	}
#endif
}
