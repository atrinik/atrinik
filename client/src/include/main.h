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

#if !defined(__MAIN_H)
#define __MAIN_H


typedef struct _server_char {
	struct _server_char *next;
	struct _server_char *prev;
	int pic_id;
	char *name; /* race name: human, elf */
	char *desc[4]; /* 4 description strings */
	int bar[3];
	int bar_add[3];
	int gender[4]; /* male, female, neutrum, herm. */
	int gender_selected;
	char *char_arch[4]; /* 4 description strings */
	int face_id[4];
	int stat_points; /* points which can be added to char stats */
	int stats[7];
	int stats_min[7];
	int stats_max[7];
}_server_char;

extern _server_char *first_server_char;
extern _server_char new_character; /* if we login as new char, thats the values of it we set */

#define SKIN_POS_QUICKSLOT_X 518
#define SKIN_POS_QUICKSLOT_Y 109

#define HUGE_BUF 1024

#define SDL_DEFAULT_REPEAT_DELAY        500
#define SDL_DEFAULT_REPEAT_INTERVAL     30

#define MAXHASHSTRING 20 /* for hash table (bmap, ...) */

#define BMAPTABLE 5003 /* prime nubmer for hash table */
/* struct for out bmap data */
typedef struct _bmaptype {
	char *name;
	int num;
	int len;
	int pos;
	unsigned int crc;
}_bmaptype;

extern _bmaptype *bmap_table[BMAPTABLE];

typedef struct _keymap
{
	char text[256];/*the command text, submited to server when key pressed*/
	char keyname[256];
	int key;/*scancode of key*/
	int repeatflag;/*if true, key will be repeated when pressed*/
	int mode;/*the send mode OR the menu id*/
	int menu_mode;
}_keymap;

typedef struct _server
{
        struct _server *next;	/* go on in list. NULL: no following this node*/
        char *nameip;
        char *version;
        char *desc1;
        char *desc2;
        char *desc3;
        char *desc4;
        int player;
        int port;
} _server;

#define MAX_BMAPTYPE_TABLE 10000

typedef struct	_bmaptype_table {
	char *name;
	int pos;
	int len;
	unsigned int crc;
}_bmaptype_table;

_bmaptype_table bmaptype_table[MAX_BMAPTYPE_TABLE];

extern int bmaptype_table_size;

#define FILE_ATRINIK_P0 "./atrinik.p0"
#define FILE_BMAPS_P0 "./bmaps.p0"
#define FILE_BMAPS_TMP "./srv_files/bmaps.tmp"
#define FILE_ANIMS_TMP "./srv_files/anims.tmp"

#define FILE_CLIENT_SPELLS "./srv_files/client_spells"
#define FILE_CLIENT_SKILLS "./srv_files/client_skills"
#define FILE_CLIENT_SETTINGS "./srv_files/client_settings"
#define FILE_CLIENT_BMAPS "./srv_files/client_bmap"
#define FILE_CLIENT_ANIMS "./srv_files/client_anims"
#define FILE_CLIENT_HFILES "./srv_files/help_files"

enum {
	SRV_CLIENT_SKILLS,
	SRV_CLIENT_SPELLS,
	SRV_CLIENT_SETTINGS,
	SRV_CLIENT_ANIMS,
	SRV_CLIENT_BMAPS,
	SRV_CLIENT_HFILES,
	SRV_CLIENT_FILES /* last index */
};

enum {
	SRV_CLIENT_STATUS_OK,
	SRV_CLIENT_STATUS_UPDATE,
};

#define	SRV_CLIENT_FLAG_BMAP 1
#define SRV_CLIENT_FLAG_ANIM 2
#define SRV_CLIENT_FLAG_SETTING 4
#define	SRV_CLIENT_FLAG_SKILL 8
#define	SRV_CLIENT_FLAG_SPELL 16
#define SRV_CLIENT_FLAG_HFILES 32

typedef struct _srv_client_files {
	int status;						/* is set from setup exchange */
	int len;
	uint32 crc;
	int server_len;
	uint32 server_crc;

}_srv_client_files;

extern _srv_client_files srv_client_files[SRV_CLIENT_FILES];
extern 	Uint32 sdl_dgreen,sdl_gray1, sdl_gray2, sdl_gray3, sdl_gray4, sdl_blue1;
extern int mb_clicked;

#define MAXMETAWINDOW 14		/* count max. shown server in meta window*/

#define MAXFACES 4

#define MAX_GROUP_MEMBER 8      /* max members in a daimonin group (shown in client) */

/* IMPORTANT: datatype it must also be changed in dialog.c */
typedef struct _options {
   /* Sound */
   int sound_volume;
   int music_volume;

   /* Server */
   char metaserver[256];
   int metaserver_port;

   /* Visual */
   int video_bpp;
   int fullscreen;
   Boolean use_TextwinSplit;
   Boolean use_TextwinAlpha;
   int textwin_alpha;

   /* Look & Feel */
   int player_names;
   int show_target_self;
   int warning_hp;
   int warning_food;
   Boolean gfx_statusbars;
   Boolean show_tooltips;
   Boolean show_d_key_infos; /* key-infos in dialog-wins. */
   Boolean collectAll;

   /* Debug */
   Boolean force_redraw;
   Boolean show_frame;         /* true: show frame rate */
   Boolean use_gl;
   int sleep;
	int speedup;
   Boolean max_speed;
   Boolean auto_bpp_flag;
   Boolean use_rect;

   /* Fullscreen Flags */
   Boolean Full_HWSURFACE;
   Boolean Full_SWSURFACE;
   Boolean Full_HWACCEL;
   Boolean Full_DOUBLEBUF;
   Boolean Full_ANYFORMAT;
   Boolean Full_ASYNCBLIT;
   Boolean Full_HWPALETTE;
   Boolean Full_RESIZABLE;
   Boolean Full_NOFRAME;
   Boolean Full_RLEACCEL;

   /* Windowed flags */
   Boolean Win_HWSURFACE;
   Boolean Win_SWSURFACE;
   Boolean Win_HWACCEL;
   Boolean Win_DOUBLEBUF;
   Boolean Win_ANYFORMAT;
   Boolean Win_ASYNCBLIT;
   Boolean Win_HWPALETTE;
   Boolean Win_RESIZABLE;
   Boolean Win_NOFRAME;
   Boolean Win_RLEACCEL;

    /* INTERN FLAGS - Setup depends on option settings and selected mode */
   Boolean fullscreen_flag;      /* we are in fullscreen mode */
   Boolean doublebuf_flag;       /* we doublebuf */
   Boolean rleaccel_flag;
   int no_meta;
   Uint8 used_video_bpp;
   Uint8 real_video_bpp;
   uint32 videoflags_full;
   uint32 videoflags_win;
}_options;

extern struct _options options;

#define FACE_FLAG_NO		0
#define FACE_FLAG_DOUBLE	1		/* this is a double wall type */
#define FACE_FLAG_UP		2		/* this is a upper part of something */
#define FACE_FLAG_D1		4		/* this is a x1x object (animation or direction) */
#define FACE_FLAG_D3		8		/* this is a x3x object (animation or direction) */
#define FACE_REQUESTED		16		/* face requested from server - do it only one time */

typedef struct _face_struct
{
	struct _Sprite *sprite; /* our face data. if != null, face is loaded*/
	char *name;				/* our face name. if != null, face is requested*/
	uint32 checksum;		/* checksum of face */
	int flags;
}_face_struct;

#define GOLEM_CTR_RELEASE  0
#define GOLEM_CTR_ADD	   1


#define LIST_ENTRY_UNUSED -1 /* this entry is unused */
#define LIST_ENTRY_USED    1 /* entry is used but player don't have it */
#define LIST_ENTRY_KNOWN   2 /* player know this used entry */
#define LIST_NAME_MAX 64
#define DIALOG_LIST_ENTRY 26
#define OPTWIN_MAX_TABLEN 14

/* skill list defines */
#define SKILL_LIST_MAX 7        /* groups of skills */

typedef struct _skill_list_entry {
    int flag;                   /* -1: entry is unused */
    char name[LIST_NAME_MAX];   /* name of entry */
    char icon_name[32];
    struct _Sprite *icon;
    char desc[4][96];               /* description (in 4 rows) */
    int exp_level;              /* -1: skill has no level or exp */
    int exp;                    /* exp of this skill */
}_skill_list_entry;

typedef struct _skill_list {
    _skill_list_entry entry[DIALOG_LIST_ENTRY];
}_skill_list;

/* bind key list defines */
#define BINDKEY_LIST_MAX 10        /* groups of keys */

typedef struct _bindkey_list {
	_keymap entry[DIALOG_LIST_ENTRY];
	char name[OPTWIN_MAX_TABLEN];
	int size;
}_bindkey_list;

typedef struct _dialog_list_set {
	int group_nr;
	int entry_nr;
	int class_nr;   /* for spell-list => spell, prayer, ... */
	int key_change;
}_dialog_list_set;

/* spell list defines */
#define SPELL_LIST_MAX 20        /* groups of spells */
#define SPELL_LIST_CLASS 2

typedef struct _spell_list_entry {
	int flag;           /* -1: entry is unused */
	char name[LIST_NAME_MAX];      /* name of entry */
	char icon_name[32];
	struct _Sprite *icon;
	char desc[4][96];               /* description (in 4 rows) */
}_spell_list_entry;

typedef struct _spell_list {
	_spell_list_entry entry[SPELL_LIST_CLASS][DIALOG_LIST_ENTRY];
}_spell_list;


typedef struct _fire_mode {
    int item;
    int amun;
    _spell_list_entry *spell;
    _skill_list_entry *skill;
	char name[128];
}_fire_mode;

typedef enum _fire_mode_id {
	FIRE_MODE_BOW,
	FIRE_MODE_SPELL,
	FIRE_MODE_WAND,
	FIRE_MODE_SKILL,
	FIRE_MODE_THROW,
	FIRE_MODE_SUMMON,
	FIRE_MODE_INIT
}_fire_mode_id;

#define FIRE_ITEM_NO (-1)

typedef enum _game_status
{
	GAME_STATUS_INIT, /* cal this add start to autoinit */
	GAME_STATUS_META,/* to to connect to meta server */
	GAME_STATUS_START,/* start all up without full reset or meta calling*/
	GAME_STATUS_WAITLOOP,/* we are NOT connected to anything*/
	GAME_STATUS_STARTCONNECT,/* we have a server+port, init and start*/
	GAME_STATUS_CONNECT,/* if this is set, we start connecting*/
	GAME_STATUS_VERSION, /* now the steps: Connect, we send version*/
	GAME_STATUS_WAITVERSION, /* wait for response... add up in version cmd*/
	GAME_STATUS_SETUP, /* we ready to send setup commands*/
	GAME_STATUS_WAITSETUP,/* we wait for server response*/
	GAME_STATUS_REQUEST_FILES, /* after we get response from setup, we request files if needed */
	GAME_STATUS_ADDME,   /* all setup is done, now try to enter game!*/
	GAME_STATUS_LOGIN,       /* now we wait for LOGIN request of the server*/
	GAME_STATUS_NAME,          /* all this here is tricky*/
	GAME_STATUS_PSWD,          /* server will trigger this when asking for*/
	GAME_STATUS_VERIFYPSWD,    /* client will then show input panel or so*/
	GAME_STATUS_NEW_CHAR,	   /* show new char creation screen and send /nc command when finished */
	GAME_STATUS_WAITFORPLAY,	/* we simply wait for game start */
        /* means, this is not a serial stepping here*/
	GAME_STATUS_QUIT,   /* we are in quit menu*/
	GAME_STATUS_PLAY,   /* we play now!!*/
} _game_status;

extern int f_custom_cursor;
extern int x_custom_cursor;
extern int y_custom_cursor;

extern int debug_layer[MAXFACES];

extern int music_global_fade; /* global flag for polling music fade out */

extern _game_status GameStatus;		/* THE game status 2*/
extern int MapStatusX;				/* map x,y len */
extern int MapStatusY;

extern uint32 LastTick;			/* system time counter in ms since prg start */

extern char ServerName[];	/* name of the server we want connect */
extern int ServerPort;			/* port addr */

extern int map_udate_flag, map_transfer_flag;
extern uint32 GameTicksSec;		/* ticks since this second frame in ms */
extern int metaserver_start, metaserver_sel,metaserver_count;

extern int GameStatusVersionFlag;
extern int GameStatusVersionOKFlag;

extern int request_file_chain;
extern int request_file_flags;

extern int esc_menu_flag;
extern int esc_menu_index;

enum {
	ESC_MENU_KEYS,
	ESC_MENU_SETTINGS,
	ESC_MENU_LOGOUT,
	ESC_MENU_BACK,

	ESC_MENU_INDEX /* last index */
};

/* with this, we overrule bitmap loading params*/
/* for example, we need for fonts a attached palette, and not the native vid mode*/
#define SURFACE_FLAG_PALETTE 1		/* surface must stay in palette mode, not in vid mode*/
#define SURFACE_FLAG_COLKEY_16M 2   /* use this when you want a colkey in a true color picture - color should be 0 */

typedef enum _bitmap_index {
  BITMAP_PALETTE,
  BITMAP_FONT1,
  BITMAP_FONT6x3OUT,
  BITMAP_BIGFONT,
  BITMAP_FONT1OUT,
  BITMAP_FONTMEDIUM,
  BITMAP_INTRO,
  BITMAP_DOLL,
  BITMAP_BLACKTILE, /* blacktile for map*/
  BITMAP_TEXTWIN,
  BITMAP_LOGIN_INP,
  BITMAP_INVSLOT,

   /* Status bars */
   BITMAP_TESTTUBES,
   BITMAP_HP,
   BITMAP_SP,
   BITMAP_GRACE,
   BITMAP_FOOD,
   BITMAP_HP_BACK,
   BITMAP_SP_BACK,
   BITMAP_GRACE_BACK,
   BITMAP_FOOD_BACK,
   BITMAP_HP_BACK2,
   BITMAP_SP_BACK2,
   BITMAP_GRACE_BACK2,
   BITMAP_FOOD_BACK2,

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
  BITMAP_INVENTORY,
  BITMAP_GROUP,
  BITMAP_EXP_BORDER,
  BITMAP_EXP_SLIDER,
  BITMAP_EXP_BUBBLE1,
  BITMAP_EXP_BUBBLE2,
  BITMAP_STATS,
  BITMAP_BUFFSPOT,
  BITMAP_TEXTSPOT,
  BITMAP_PDOLL2,
  BITMAP_PDOLL2_SPOT,
  BITMAP_CLEAR_SPOT,
  BITMAP_BORDER1,
  BITMAP_BORDER2,
  BITMAP_BORDER3,
  BITMAP_BORDER4,
  BITMAP_BORDER5,
  BITMAP_BORDER6,
  BITMAP_PANEL_P1,
  BITMAP_GROUP_SPOT,
  BITMAP_TARGET_SPOT,
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
  BITMAP_TEXTWIN_BLANK,
  BITMAP_SLIDER_UP,
	BITMAP_SLIDER_DOWN,
	BITMAP_SLIDER,
	BITMAP_GROUP_CLEAR,
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
	BITMAP_TRAPED,
    BITMAP_PRAY,
    BITMAP_WAND,
	BITMAP_JOURNAL,
	BITMAP_INIT

}_bitmap_index;

/* for custom cursors */
enum {
    MSCURSOR_MOVE = 1,
};

extern struct gui_book_struct    *gui_interface_book;

extern struct _Font         MediumFont;
extern char InputString[MAX_INPUT_STRING];			/* our text char string*/
extern char InputHistory[MAX_HISTORY_LINES][MAX_INPUT_STRING];  /* input lines history buffer */
extern int HistoryPos;
extern int CurrentCursorPos;
extern int InputCount, InputMax;					/* nr. of char in string and max chars. */
extern Boolean InputStringFlag;	/* if true keyboard and game is in input str mode*/
extern Boolean InputStringEndFlag;	/* if true, we had entered some in text mode and its ready*/
extern Boolean InputStringEscFlag;

extern struct _fire_mode fire_mode_tab[FIRE_MODE_INIT]; /* range table */
extern int RangeFireMode;

extern int ToggleScreenFlag;

extern struct _Sprite *Bitmaps[];

extern _face_struct FaceList[MAX_FACE_TILES];	/* face data */

extern struct _Font BigFont;			/* bigger font */
extern struct _Font SystemFont;			/* our main font*/
extern struct _Font SystemFontOut;			/* our main font*/
extern struct _Font Font6x3Out;			/* 6x3 mini font */
extern SDL_Surface *ScreenSurface;      /* our main bla and so on surface */
extern struct sockaddr_in insock;       /* Server's attributes*/
extern int SocketStatusErrorNr; /* if an socket error, this is it */

extern int main ( int argc, char *argv[] );
extern void open_input_mode(int maxchar);
extern void add_metaserver_data(char *server, int port, int player, char *ver, char *desc1, char *desc2, char *desc3, char *desc4);
extern void clear_metaserver_data(void);
extern void get_meta_server_data(int num, char *server, int *port);
extern void free_faces(void);
extern void load_options_dat(void);
extern void save_options_dat(void);
#endif

