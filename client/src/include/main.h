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
 * Header file for things that are generally used in many places. */

#ifndef MAIN_H
#define MAIN_H

#define HUGE_BUF 4096

#define SDL_DEFAULT_REPEAT_DELAY 500
#define SDL_DEFAULT_REPEAT_INTERVAL 30

/* For hash table (bmap, ...) */
#define MAXSTRING 20

/** Keymap structure */
typedef struct _keymap
{
	/** The command text, submitted to server when key is pressed. */
	char text[256];

	/** Key name. */
	char keyname[256];

	/** Scan code of key. */
	int key;

	/** If true, key will be repeated when pressed. */
	int repeatflag;
}_keymap;

/** The servers list, as given by the metaserver. */
typedef struct server_struct
{
	/** Next server in the list. */
	struct server_struct *next;

	/** IP of the server. */
	char *ip;

	/** Name of the server. */
	char *name;

	/** Server version. */
	char *version;

	/** Server description. */
	char *desc;

	/** Number of players online. */
	int player;

	/** Server port. */
	int port;
} server_struct;

/**
 * Message animation structure. Used when NDI_ANIM is passed to
 * DrawInfoCmd2(). */
typedef struct msg_anim_struct
{
	/** The message to play. */
	char message[MAX_BUF];

	/** Tick when it started. */
	uint32 tick;

	/** Flags as determined in DrawInfoCmd2(). */
	int flags;
} msg_anim_struct;

extern struct msg_anim_struct msg_anim;

#ifndef SYSPATH
#define SYSPATH "./"
#endif

#define FILE_ATRINIK_P0 "atrinik.p0"

extern Uint32 sdl_dgreen, sdl_gray1, sdl_gray2, sdl_gray3, sdl_gray4, sdl_blue1;
extern int mb_clicked;

/* IMPORTANT: datatype must also be changed in dialog.c */
typedef struct _options
{
	/* Sound */
	int sound_volume;
	int music_volume;

	/* Visual */
	int video_bpp;
	int fullscreen;
	int resolution_x;
	int resolution_y;
	int resolution;
	int textwin_alpha;
#ifdef WIDGET_SNAP
	int widget_snap;
#endif
	int mapstart_x;
	int mapstart_y;

	/* Look & Feel */
	int player_names;
	int playerdoll;
	int zoom;
	int zoom_smooth;
	int show_target_self;
	int warning_hp;
	int warning_food;
	int show_tooltips;
	int chat_timestamp;
	int chat_font_size;
	int chat_max_lines;

	/* key-infos in dialog-wins. */
	int show_d_key_infos;
	int collectAll;
	int key_repeat;

	/* Exp display */
	int expDisplay;

	/* Debug */
	int force_redraw;

	/* True: show frame rate */
	int show_frame;
	int sleep;
	int speedup;
	int max_speed;
	int auto_bpp_flag;
	int use_rect;

	/* Fullscreen Flags */
	int Full_HWSURFACE;
	int Full_SWSURFACE;
	int Full_HWACCEL;
	int Full_DOUBLEBUF;
	int Full_ANYFORMAT;
	int Full_ASYNCBLIT;
	int Full_HWPALETTE;
	int Full_RESIZABLE;
	int Full_NOFRAME;
	int Full_RLEACCEL;

	/* Windowed flags */
	int Win_HWSURFACE;
	int Win_SWSURFACE;
	int Win_HWACCEL;
	int Win_DOUBLEBUF;
	int Win_ANYFORMAT;
	int Win_ASYNCBLIT;
	int Win_HWPALETTE;
	int Win_RESIZABLE;
	int Win_NOFRAME;
	int Win_RLEACCEL;

	/* INTERNAL FLAGS - Setup depends on option settings and selected mode */

	/* We are in fullscreen mode */
	int fullscreen_flag;

	/* We doublebuf */
	int doublebuf_flag;
	int rleaccel_flag;
	int no_meta;
	Uint8 used_video_bpp;
	Uint8 real_video_bpp;
	uint32 videoflags_full;
	uint32 videoflags_win;
	int reload_gfx_user;
	int disable_updates;
}_options;

extern struct _options options;

/* Face requested from server - do it only one time */
#define FACE_REQUESTED		16

typedef struct _face_struct
{
	/* Our face data. if != null, face is loaded */
	struct _Sprite *sprite;

	/* Our face name. if != null, face is requested */
	char *name;

	/* Checksum of face */
	uint32 checksum;

	int flags;
}_face_struct;

#define NUM_STATS 7

/* This entry is unused */
#define LIST_ENTRY_UNUSED 	-1
/* Entry is used but player doesn't have it */
#define LIST_ENTRY_USED		1
/* Player knows this used entry */
#define LIST_ENTRY_KNOWN	2
#define LIST_NAME_MAX		64
#define DIALOG_LIST_ENTRY	26
#define OPTWIN_MAX_TABLEN	14

/* Skill list defines */

/* Groups of skills */
#define SKILL_LIST_MAX 7

typedef struct _skill_list_entry
{
	/* -1: entry is unused */
	int flag;

	/* Name of entry */
	char name[LIST_NAME_MAX];

	char icon_name[32];
	struct _Sprite *icon;

	/* Description (in 4 rows) */
	char desc[4][96];

	/* -1: skill has no level or exp */
	int exp_level;

	/* exp of this skill */
	sint64 exp;
}_skill_list_entry;

typedef struct _skill_list
{
	_skill_list_entry entry[DIALOG_LIST_ENTRY];
}_skill_list;

/* Bind key list defines */

/** Bindkey list max */
#define BINDKEY_LIST_MAX 10

/** Bindkey list structure */
typedef struct _bindkey_list
{
	/** Entry */
	_keymap entry[DIALOG_LIST_ENTRY];

	/** Name */
	char name[OPTWIN_MAX_TABLEN];

	/** Size */
	int size;
}_bindkey_list;

/** Dialog list structure */
typedef struct _dialog_list_set
{
	/** Group number */
	int group_nr;

	/** Entry number */
	int entry_nr;

	/** For spell-list => spell, prayer, ... */
	int class_nr;

	/** Key change */
	int key_change;
}_dialog_list_set;

/** Spell list max */
#define SPELL_LIST_MAX 20
/** Spell list classes */
#define SPELL_LIST_CLASS 2

/** Spell list entry structure */
typedef struct _spell_list_entry
{
	/** -1 - entry is unused */
	int flag;

	/** name of entry */
	char name[LIST_NAME_MAX];

	/** Icon name */
	char icon_name[128];

	/** Description. */
	char desc[HUGE_BUF];

	/** Spell's icon ID. */
	int icon;

	/** Cost of spell. */
	int cost;

	/**
	 * Contains what kind of relationship the player has with the spell's path:
	 * - a: Attuned
	 * - r: Repelled
	 * - d: Denied */
	char path;
}_spell_list_entry;

/** Spell list structure */
typedef struct _spell_list
{
	_spell_list_entry entry[SPELL_LIST_CLASS][DIALOG_LIST_ENTRY];
}_spell_list;

/** Fire mode structure */
typedef struct _fire_mode
{
	/** Item */
	int item;

	/** Amunnition */
	int amun;

	/** Spell */
	_spell_list_entry *spell;

	/** Skill */
	_skill_list_entry *skill;

	/** Name */
	char name[128];
}_fire_mode;

/** Help files structure. */
typedef struct help_files_struct
{
	/** Help name, like "main", or "apply". */
	char helpname[MAX_BUF];

	/** Help title (shown at the start of the book). */
	char title[MAX_BUF];

	/** The help message. */
	char message[HUGE_BUF * 12];

	/** Is this for DMs only? */
	int dm_only;

	/** Can the command be autocompleted using the tab key? */
	int autocomplete;

	/** Next help entry. */
	struct help_files_struct *next;
} help_files_struct;

extern help_files_struct *help_files;

typedef enum _fire_mode_id
{
	FIRE_MODE_BOW,
	FIRE_MODE_SPELL,
	FIRE_MODE_WAND,
	FIRE_MODE_SKILL,
	FIRE_MODE_THROW,
	FIRE_MODE_INIT
}_fire_mode_id;

/** If no fire item */
#define FIRE_ITEM_NO -1

/** Game statuses. */
typedef enum _game_status
{
	/** Call this add start to autoinit */
	GAME_STATUS_INIT,

	/** To to connect to meta server */
	GAME_STATUS_META,

	/** Start all up without full reset or meta calling */
	GAME_STATUS_START,

	/** We are NOT connected to anything */
	GAME_STATUS_WAITLOOP,

	/** We have a server + port, init and start */
	GAME_STATUS_STARTCONNECT,

	/** If this is set, we start connecting */
	GAME_STATUS_CONNECT,

	/** Now the steps: Connect, we send version */
	GAME_STATUS_VERSION,

	/** We ready to send setup commands */
	GAME_STATUS_SETUP,

	/** We wait for server response */
	GAME_STATUS_WAITSETUP,

	/** After we get response from setup, we request files if needed */
	GAME_STATUS_REQUEST_FILES,

	/** All setup is done, now try to enter game */
	GAME_STATUS_ADDME,

	/** Now we wait for LOGIN request of the server */
	GAME_STATUS_LOGIN,

	/** All this here is tricky */
	GAME_STATUS_NAME,

	/** Server will trigger this when asking for password */
	GAME_STATUS_PSWD,

	/** Client will then show input panel or so */
	GAME_STATUS_VERIFYPSWD,

	/** Show new char creation screen and send /nc command when finished */
	GAME_STATUS_NEW_CHAR,

	/** We simply wait for game start.
	 * Means, this is not a serial stepping here */
	GAME_STATUS_WAITFORPLAY,

	/** We are in quit menu */
	GAME_STATUS_QUIT,

	/** We play now. */
	GAME_STATUS_PLAY
} _game_status;

extern int f_custom_cursor;
extern int x_custom_cursor;
extern int y_custom_cursor;

extern _game_status GameStatus;
extern int MapStatusX;
extern int MapStatusY;
extern uint32 LastTick;
extern uint32 tmpGameTick;
extern uint32 FrameCount;
extern server_struct *selected_server;
extern int map_udate_flag, map_transfer_flag, map_redraw_flag;
extern int esc_menu_flag;
extern int esc_menu_index;

enum
{
	ESC_MENU_KEYS,
	ESC_MENU_SETTINGS,
	ESC_MENU_LOGOUT,
	ESC_MENU_BACK,

	/* Last index */
	ESC_MENU_INDEX
};

/* With this, we overrule bitmap loading params.
 * For example, we need for fonts an attached palette, and not the native vid mode */

/** Surface must stay in palette mode, not in vid mode */
#define SURFACE_FLAG_PALETTE 	1
/** Use this when you want a colkey in a true color picture - color should be 0 */
#define SURFACE_FLAG_COLKEY_16M 2
#define SURFACE_FLAG_DISPLAYFORMAT 4

/** Types of pictures. */
typedef enum _pic_type
{
	PIC_TYPE_DEFAULT, PIC_TYPE_PALETTE, PIC_TYPE_TRANS
} _pic_type;

/** Bitmap name structure */
typedef struct _bitmap_name
{
	/** Name */
	char *name;

	/** Type */
	_pic_type type;
} _bitmap_name;

typedef enum _bitmap_index
{
	BITMAP_PALETTE,
	BITMAP_FONT1,
	BITMAP_FONT6x3OUT,
	BITMAP_BIGFONT,
	BITMAP_FONT1OUT,
	BITMAP_FONTMEDIUM,
	BITMAP_INTRO,

	/* Progres bar */
	BITMAP_PROGRESS,
	BITMAP_PROGRESS_BACK,

	BITMAP_DOLL,

	/* blacktile for map */
	BITMAP_BLACKTILE,
	BITMAP_TEXTWIN,
	BITMAP_LOGIN_INP,
	BITMAP_INVSLOT,

	/* Status bars */
	BITMAP_HP,
	BITMAP_SP,
	BITMAP_GRACE,
	BITMAP_FOOD,
	BITMAP_HP_BACK,
	BITMAP_SP_BACK,
	BITMAP_GRACE_BACK,
	BITMAP_FOOD_BACK,

	BITMAP_APPLY,
	BITMAP_UNPAID,
	BITMAP_CURSED,
	BITMAP_DAMNED,
	BITMAP_LOCK,
	BITMAP_MAGIC,

	BITMAP_RANGE,
	BITMAP_RANGE_MARKER,
	BITMAP_RANGE_CTRL,
	BITMAP_RANGE_CTRL_NO,
	BITMAP_RANGE_SKILL,
	BITMAP_RANGE_SKILL_NO,
	BITMAP_RANGE_THROW,
	BITMAP_RANGE_THROW_NO,
	BITMAP_RANGE_TOOL,
	BITMAP_RANGE_TOOL_NO,
	BITMAP_RANGE_WIZARD,
	BITMAP_RANGE_WIZARD_NO,
	BITMAP_RANGE_PRIEST,
	BITMAP_RANGE_PRIEST_NO,

	BITMAP_CMARK_START,
	BITMAP_CMARK_END,
	BITMAP_CMARK_MIDDLE,

	BITMAP_TWIN_SCROLL,
	BITMAP_INV_SCROLL,
	BITMAP_BELOW_SCROLL,

	BITMAP_NUMBER,
	BITMAP_INVSLOT_U,

	BITMAP_DEATH,
	BITMAP_SLEEP,
	BITMAP_CONFUSE,
	BITMAP_PARALYZE,
	BITMAP_SCARED,
	BITMAP_BLIND,

	BITMAP_ENEMY1,
	BITMAP_ENEMY2,
	BITMAP_PROBE,

	BITMAP_QUICKSLOTS,
	BITMAP_QUICKSLOTSV,
	BITMAP_INVENTORY,
	BITMAP_INV_BG,

	BITMAP_EXP_BORDER,
	BITMAP_EXP_SLIDER,
	BITMAP_EXP_BUBBLE1,
	BITMAP_EXP_BUBBLE2,

	BITMAP_STATS_BG,
	BITMAP_BELOW,
	BITMAP_FLINE,

	BITMAP_TARGET_ATTACK,
	BITMAP_TARGET_TALK,
	BITMAP_TARGET_NORMAL,

	BITMAP_LOADING,
	BITMAP_WARN_HP,
	BITMAP_WARN_FOOD,
	BITMAP_LOGO270,

	BITMAP_DIALOG_BG,
	BITMAP_DIALOG_TITLE_OPTIONS,
	BITMAP_DIALOG_TITLE_KEYBIND,
	BITMAP_DIALOG_TITLE_SKILL,
	BITMAP_DIALOG_TITLE_SPELL,
	BITMAP_DIALOG_TITLE_CREATION,
	BITMAP_DIALOG_TITLE_LOGIN,
	BITMAP_DIALOG_TITLE_SERVER,
	BITMAP_DIALOG_TITLE_PARTY,
	BITMAP_DIALOG_BUTTON_UP,
	BITMAP_DIALOG_BUTTON_DOWN,
	BITMAP_DIALOG_TAB_START,
	BITMAP_DIALOG_TAB,
	BITMAP_DIALOG_TAB_STOP,
	BITMAP_DIALOG_TAB_SEL,
	BITMAP_DIALOG_CHECKER,
	BITMAP_DIALOG_RANGE_OFF,
	BITMAP_DIALOG_RANGE_L,
	BITMAP_DIALOG_RANGE_R,

	BITMAP_TARGET_HP,
	BITMAP_TARGET_HP_B,

	BITMAP_TEXTWIN_MASK,
	BITMAP_SLIDER_UP,
	BITMAP_SLIDER_DOWN,
	BITMAP_SLIDER,

	BITMAP_EXP_SKILL_BORDER,
	BITMAP_EXP_SKILL_LINE,
	BITMAP_EXP_SKILL_BUBBLE,

	BITMAP_OPTIONS_HEAD,
	BITMAP_OPTIONS_KEYS,
	BITMAP_OPTIONS_SETTINGS,
	BITMAP_OPTIONS_LOGOUT,
	BITMAP_OPTIONS_BACK,
	BITMAP_OPTIONS_MARK_LEFT,
	BITMAP_OPTIONS_MARK_RIGHT,
	BITMAP_OPTIONS_ALPHA,

	BITMAP_PENTAGRAM,
	BITMAP_BUTTONQ_UP,
	BITMAP_BUTTONQ_DOWN,
	BITMAP_NCHAR_MARKER,

	BITMAP_TRAPPED,
	BITMAP_PRAY,
	BITMAP_WAND,
	BITMAP_JOURNAL,
	BITMAP_SLIDER_LONG,
	BITMAP_INVSLOT_MARKED,
	BITMAP_MSCURSOR_MOVE,
	BITMAP_RESIST_BG,
	BITMAP_MAIN_LVL_BG,
	BITMAP_SKILL_EXP_BG,
	BITMAP_REGEN_BG,
	BITMAP_SKILL_LVL_BG,
	BITMAP_MENU_BUTTONS,
	BITMAP_PLAYER_INFO,
	BITMAP_TARGET_BG,
	BITMAP_TEXTINPUT,
	BITMAP_SHOP,
	BITMAP_SHOP_INPUT,

	BITMAP_SQUARE_HIGHLIGHT,
	BITMAP_SERVERS_BG,
	BITMAP_SERVERS_BG_OVER,
	BITMAP_NEWS_BG,
	BITMAP_EYES,
	BITMAP_POPUP,
	BITMAP_ARROW_UP,
	BITMAP_ARROW_UP2,
	BITMAP_ARROW_DOWN,
	BITMAP_ARROW_DOWN2,
	BITMAP_BUTTON_ROUND,
	BITMAP_BUTTON_ROUND_DOWN,

	BITMAP_INIT
}_bitmap_index;

/* For custom cursors */
enum
{
	MSCURSOR_MOVE = 1
};

extern struct gui_book_struct *gui_interface_book;
extern struct gui_party_struct *gui_interface_party;

extern struct _Font MediumFont;

/* Our text char string */
extern char InputString[MAX_INPUT_STRING];

/* Input lines history buffer */
extern char InputHistory[MAX_HISTORY_LINES][MAX_INPUT_STRING];
extern int HistoryPos;
extern int CurrentCursorPos;

/* Nr. of char in string and max chars. */
extern int InputCount, InputMax;

/* If true keyboard and game is in input string mode */
extern int InputStringFlag;

/* If true, we had entered some in text mode and it's ready */
extern int InputStringEndFlag;
extern int InputStringEscFlag;

/* Range table */
extern struct _fire_mode fire_mode_tab[FIRE_MODE_INIT];
extern int RangeFireMode;

extern int ToggleScreenFlag;

extern struct _Sprite *Bitmaps[];

/* Face data */
extern _face_struct FaceList[MAX_FACE_TILES];

/* Bigger font */
extern struct _Font BigFont;
/* Our main font */
extern struct _Font SystemFont;
extern struct _Font SystemFontOut;
/* 6x3 mini font */
extern struct _Font Font6x3Out;

/* Our main bla and so on surface */
extern SDL_Surface *ScreenSurface;
extern SDL_Surface *ScreenSurfaceMap;

/* Server's attributes */
extern struct sockaddr_in insock;


#endif
