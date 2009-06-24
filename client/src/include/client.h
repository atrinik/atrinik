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

#include "include.h"

#define STRINGCOMMAND 0
#define MAX_BUF 256
#define BIG_BUF 1024

/* How many skill types server supports/client will get sent to it.
 * If more skills are added to server, this needs to get increased.
 */
#define MAX_SKILL   6

/* defines for the ext map command */
#define FFLAG_SLEEP     0x01        /* object sleeps */
#define FFLAG_CONFUSED  0x02        /* object is confused */
#define FFLAG_PARALYZED 0x04        /* object is paralyzed */
#define FFLAG_SCARED    0x08        /* object is scared    */
#define FFLAG_BLINDED   0x10        /* object is blinded */
#define FFLAG_INVISIBLE 0x20        /* object is invisible (but when send, player can see it) */
#define FFLAG_ETHEREAL  0x40        /* object is etheral - but when send, object can be seen */
#define FFLAG_PROBE		0x80        /* object is target of player */

#define INPUT_MODE_NO      0
#define INPUT_MODE_CONSOLE 1
#define INPUT_MODE_NUMBER  4
#define INPUT_MODE_GETKEY  8

#define NUM_MODE_GET  1
#define NUM_MODE_DROP 2

/* Values for send_command option */

#define SC_NORMAL 0
#define SC_FIRERUN 1
#define SC_ALWAYS 2

typedef struct _screensize
{
    int x;
    int y;
    int xoff;
    int yoff;
} _screensize;

extern _screensize Screensize;

extern _screensize Screendefs[16];

typedef struct Animations {
	int loaded;			/* 0= all fields are invalid, 1= anim is loaded */
	int frame;			/* length of one a animation frame (num_anim/facings) */
    uint16  *faces;
	uint8  facings;		/* number of frames */
    uint8   num_animations; /* number of animations.  Value of 2 means
			     * only faces[0],[1] have meaningfull values.
			     */
    uint8  flags;
} Animations;

typedef struct _anim_table {
	int len;			/* len of anim_cmd data */
	char *anim_cmd;		/* faked animation command */
}_anim_table;

extern _anim_table anim_table[MAXANIM]; /* the stored "anim commands" we created out of anims.tmp */
extern Animations animations[MAXANIM];

/* Contains the base information we use to make up a packet we want to send. */
typedef struct SockList {
    int len;
    unsigned char *buf;
} SockList;

/* ClientSocket could probably hold more of the global values - it could
 * probably hold most all socket/communication related values instead
 * of globals.
 */
typedef struct ClientSocket {
    SOCKET	fd;						/* typedef your socket type to SOCKET */
    SockList	inbuf;
    int	cs_version, sc_version;	/* Server versions of these */
    /* These are used for the newer 'windowing' method of commands -
     * number of last command sent, number of received confirmation
     */
    int command_sent, command_received;
    /* Time (in ms) players commands currently take to execute */
    int command_time;

} ClientSocket;

extern ClientSocket csocket;

extern int port_num,
	basenrofpixmaps;	/* needed so that we know where to
				 * start when creating the additional
				 * images in x11.c
				 */

extern char *server, *client_libdir,*image_file;

typedef enum Input_State {Playing, Reply_One, Reply_Many,
	Configure_Keys, Command_Mode, Metaserver_Select} Input_State;

typedef enum rangetype {
  range_bottom = -1, range_none = 0, range_bow = 1, range_magic = 2,
  range_wand = 3, range_rod = 4, range_scroll = 5, range_horn = 6,
  range_steal = 7,
  range_size = 8
} rangetype;

typedef struct Stat_struct {
    sint8 Str,Dex,Con,Wis,Cha,Int,Pow;
	sint16 wc,ac;	    /* Weapon Class and Armour Class */
    sint8 level;
    sint16 hp;		    /* Hit Points. */
    sint16 maxhp;
    sint16 sp;		    /* Spell points.  Used to cast spells. */
    sint16 maxsp;	    /* Max spell points. */
    sint16 grace;	    /* Spell points.  Used to cast spells. */
    sint16 maxgrace;	    /* Max spell points. */
    sint32 exp;		    /* Experience.  Killers gain 1/10. */
    sint16 food;	    /* How much food in stomach.  0 = starved. */
    sint16 dam;		    /* How much damage this object does when hitting */
    sint32 speed;	    /* Gets converted to a float for display*/
    int weapon_sp;	    /* Gets converted to a float for display */
    uint16 flags;	    /* contains fire on/run on flags */
    sint16 protection[20];	    /* Resistant values */
    uint32 protection_change:1; /* Resistant value has changed */
    sint16 skill_level[MAX_SKILL];  /* Level and experience totals for */
    sint32 skill_exp[MAX_SKILL];    /* skills */
} Stats;

typedef enum _inventory_win {
	IWIN_BELOW, IWIN_INV
}_inventory_win;


typedef struct Player_Struct {
    item	*ob;		/* Player object */
    item	*below;		/* Items below the player (pl.below->inv) */
	item	*sack;		/* inventory of a open container */

    item	*container;	/* ptr to open container */
	sint32	container_tag;	/* tag of the container */
    item	*ranges[range_size];	/* Object that is used for that */


    uint32 weight_limit;
    uint32	count;		/* Repeat count on command */
	int		target_mode;
	int		target_code;
	int		target_color;
	int inventory_win;  /* inventory windows */
	int menustatus;
    int loc;
    int tag;
    int nrof;
    int skill_g;            /* skill group and entry of ready skill */
    int skill_e;

    int warn_hp;

	int win_inv_slot;
	int win_inv_tag;
	int win_quick_tag;
	int win_pdoll_tag;
	int win_inv_start;
	int win_inv_count;
    int win_inv_ctag;

    int win_below_slot;
    int win_below_tag;
    int win_below_start;
    int win_below_count;
    int win_below_ctag;

	int  input_mode;	/* mode: no, console(textstring), numinput */
	int		nummode;

	float gen_hp;			/* hp, mana and grace reg. */
	float gen_sp;
	float gen_grace;

	float action_timer; /* skill cooldown time */

    uint32	no_echo:1;	/* If TRUE, don't echo keystrokes */
    uint32	fire_on:1;	/* True if fire key is pressed = action key (ALT;CTRL)*/
    uint32	run_on:1;	/* True if run key is on = action key (ALT;CTRL)*/
    uint32	resize_twin:1;
    uint32	resize_twin_marker:1;
	uint32	firekey_on:1;	/* True if fire key is pressed = permanent mode*/
	uint32	runkey_on:1;	/* sic! */
    uint32	echo_bindings:1;/* If true, echo the command that the key */

	float window_weight;
	float real_weight;


    uint16	count_left;	/* count for commands */
    uint16	mmapx, mmapy;	/* size of magic map */
    uint16	pmapx, pmapy;	/* Where the player is on the magic map */
    uint16	mapxres,mapyres;/* resolution to draw on the magic map */

    Boolean warn_statdown;
    Boolean warn_statup;
    Boolean warn_drain;
    Stats	stats;		/* Player stats */
    Input_State input_state;	/* What the input state is */
    rangetype	shoottype;	/* What type of range attack player has */

    uint8	*magicmap;	/* Magic map data */
    uint8	showmagic;	/* If 0, show normal map, otherwise, show
				 * magic map.
				 */
    uint8	command_window;	/* How many outstanding commands to allow */
    uint8	ready_spell;	/* Index to spell that is readied */
				/* player knows */
    uint8	map_x, map_y;	/* These are offset values.  See object.c */


	char	target_hp;	/* hp of our target in % */
    char	last_command[MAX_BUF];	/* Last command entered */
    char	input_text[MAX_BUF];	/* keys typed (for long commands) */
    char	name[40];	/* name and password.  Only used while */
    char	password[40];	/* logging in. */
    char	spells[255][40];	/* List of all the spells the */
    char	target_name[MAX_BUF];	/* Rank & Name of char*/
    char	num_text[300];
    char	skill_name[128];
	char    rankandname[MAX_BUF];
    char	pname[MAX_BUF];	/* Name of char*/
    char	title[MAX_BUF];	/* Race & Profession of character */
    char	rank[MAX_BUF];	/* rank */
    char	race[MAX_BUF];	/* alignment */
    char	godname[MAX_BUF];	/* alignment */
    char	alignment[MAX_BUF];	/* alignment */
    char	gender[MAX_BUF];	/* Gender */
    char	range[MAX_BUF];	/* Range attack chosen */
	char    partyname[MAX_BUF];
} Client_Player;

extern Client_Player cpl;		/* Player object. */
extern char *skill_names[MAX_SKILL];


/* To handle XPM display mode, #ifdef Xpm_Pix are only used in areas
 * that make XPM function calls, or areas where using certain display
 * methods is a lot more efficient.
 *
 * Xpm_Display can only be set if Xpm_Pix is defined.  Thus, a lot
 * of the #ifdefs can be removed - those functions will never be called,
 * or values used, because Display_Mode will never be set to Xpm_Display
 */

typedef enum Display_Mode {Pix_Display, Xpm_Display, Png_Display}
	Display_Mode;

extern Display_Mode display_mode;

extern int nosound, updatekeycodes;

/* WE need to declare most of the structs before we can include this */

/* move this to hardware depend if needed
#include <proto.h>
extern int errno;
*/

/* translation of the STAT_RES names into printable names,
 * in matching order.
 */
#define NUM_RESISTS 18
extern char *resists_name[NUM_RESISTS];

/* Map size the client will request the map to be.  Bigger it is,
 * more memory it will use
 */

#define CS_QUERY_YESNO	0x1	/* Yes/no question */
#define CS_QUERY_SINGLECHAR 0x2	/* Single character response expected */
#define CS_QUERY_HIDEINPUT 0x4	/* Hide input being entered */

#define CS_SAY_NORMAL	0x1	/* Normal say command */
#define CS_SAY_SHOUT	0x2	/* Text is shouted. */
#define CS_SAY_GSAY	0x4	/* Text is group say command */

/* These are multiplication values that should be used when changing
 * floats to ints, and vice version.  MULTI is integer representatin
 * (float to int), MULTF is float, for going from int to float.
 */
#define FLOAT_MULTI	100000
#define FLOAT_MULTF	100000.0

/* ID's for the various stats that get sent across. */

#define CS_STAT_HP	 1
#define CS_STAT_MAXHP	 2
#define CS_STAT_SP	 3
#define CS_STAT_MAXSP	 4
#define CS_STAT_STR	 5
#define CS_STAT_INT	 6
#define CS_STAT_WIS	 7
#define CS_STAT_DEX	 8
#define CS_STAT_CON	 9
#define CS_STAT_CHA	10
#define CS_STAT_EXP	11
#define CS_STAT_LEVEL	12
#define CS_STAT_WC	13
#define CS_STAT_AC	14
#define CS_STAT_DAM	15
#define CS_STAT_ARMOUR	16
#define CS_STAT_SPEED	17
#define CS_STAT_FOOD	18
#define CS_STAT_WEAP_SP 19
#define CS_STAT_RANGE	20
#define CS_STAT_TITLE	21
#define CS_STAT_POW	22
#define CS_STAT_GRACE	23
#define CS_STAT_MAXGRACE	24
#define CS_STAT_FLAGS	25
#define CS_STAT_WEIGHT_LIM	26
#define CS_STAT_EXT_TITLE 27

#define CS_STAT_REG_HP 28
#define CS_STAT_REG_MANA 29
#define CS_STAT_REG_GRACE 30
#define CS_STAT_TARGET_HP 31

/* Start & end of resistances, inclusive. */
#define CS_STAT_RESIST_START	100
#define CS_STAT_RESIST_END	117
#define CS_STAT_RES_PHYS	100
#define CS_STAT_RES_MAG		101
#define CS_STAT_RES_FIRE	102
#define CS_STAT_RES_ELEC	103
#define CS_STAT_RES_COLD	104
#define CS_STAT_RES_CONF	105
#define CS_STAT_RES_ACID	106
#define CS_STAT_RES_DRAIN	107
#define CS_STAT_RES_GHOSTHIT	108
#define CS_STAT_RES_POISON	109
#define CS_STAT_RES_SLOW	110
#define CS_STAT_RES_PARA	111
#define CS_STAT_TURN_UNDEAD	112
#define CS_STAT_RES_FEAR	113
#define CS_STAT_RES_DEPLETE	114
#define CS_STAT_RES_DEATH	115
#define CS_STAT_RES_HOLYWORD	116
#define CS_STAT_RES_BLIND	117

#define CS_STAT_ACTION_TIME   36

/* Start & end of skill experience + skill level, inclusive. */
#define CS_STAT_SKILLEXP_START 118
#define CS_STAT_SKILLEXP_END 129
#define CS_STAT_SKILLEXP_AGILITY 118
#define CS_STAT_SKILLEXP_AGLEVEL 119
#define CS_STAT_SKILLEXP_PERSONAL 120
#define CS_STAT_SKILLEXP_PELEVEL 121
#define CS_STAT_SKILLEXP_MENTAL 122
#define CS_STAT_SKILLEXP_MELEVEL 123
#define CS_STAT_SKILLEXP_PHYSIQUE 124
#define CS_STAT_SKILLEXP_PHLEVEL 125
#define CS_STAT_SKILLEXP_MAGIC 126
#define CS_STAT_SKILLEXP_MALEVEL 127
#define CS_STAT_SKILLEXP_WISDOM 128
#define CS_STAT_SKILLEXP_WILEVEL 129

#define CS_STAT_PROT_START	130
#define CS_STAT_PROT_END	149

#define CS_STAT_PROT_HIT	130
#define CS_STAT_PROT_SLASH	131
#define CS_STAT_PROT_CLEAVE	132
#define CS_STAT_PROT_PIERCE	133
#define CS_STAT_PROT_WMAGIC	134

#define CS_STAT_PROT_FIRE	135
#define CS_STAT_PROT_COLD	136
#define CS_STAT_PROT_ELEC	137
#define CS_STAT_PROT_POISON	138
#define CS_STAT_PROT_ACID	139

#define CS_STAT_PROT_MAGIC	140
#define CS_STAT_PROT_MIND	141
#define CS_STAT_PROT_BODY	142
#define CS_STAT_PROT_PSIONIC	143
#define CS_STAT_PROT_ENERGY	144

#define CS_STAT_PROT_NETHER	145
#define CS_STAT_PROT_CHAOS	146
#define CS_STAT_PROT_DEATH	147
#define CS_STAT_PROT_HOLY	148
#define CS_STAT_PROT_CORRUPT	149

/* These are used with CS_STAT_FLAGS above to communicate S->C what the
 * server thinks the fireon & runon states are.
 */
#define SF_FIREON           1
#define SF_RUNON            2
#define SF_BLIND            4
#define SF_XRAYS            8
#define SF_INFRAVISION      16

#define NDI_SAY		0x0100	/* its a say command */
#define NDI_SHOUT	0x0200
#define NDI_TELL	0x0400
#define NDI_PLAYER	0x0800 /* this comes from a player */
#define NDI_SYSTEM	0x01000 /* if this is set, its a "system" message */

#define NDI_UNIQUE	0x10000	/* Print this out immediately, don't buffer */
#define NDI_ALL		0x20000	/* Inform all players of this message */

/* Flags for the item command */
enum {a_none, a_readied, a_wielded, a_worn, a_active, a_applied};
#define F_APPLIED       0x000F
#define F_LOCATION      0x00F0
#define F_UNPAID        0x0200
#define F_MAGIC         0x0400
#define F_CURSED        0x0800
#define F_DAMNED        0x1000
#define F_OPEN          0x2000
#define F_NOPICK        0x4000
#define F_LOCKED        0x8000
#define F_TRAPED        0x10000

#define CF_FACE_NONE	0
#define CF_FACE_BITMAP	1
#define CF_FACE_XPM	2
#define CF_FACE_PNG	3
#define CF_FACE_CACHE	0x10

/* Used in the new_face structure on the magicmap field.  Low bits
 * are color informatin.  For now, only high bit information we need
 * is floor information.
 */
#define FACE_FLOOR	0x80
#define FACE_WALL	0x40	/* Or'd into the color value by the server
				 * right before sending.
				 */
#define FACE_COLOR_MASK	0xf

#define UPD_LOCATION	0x01
#define UPD_FLAGS	0x02
#define UPD_WEIGHT	0x04
#define UPD_FACE	0x08
#define UPD_NAME	0x10
#define UPD_ANIM	0x20
#define UPD_ANIMSPEED	0x40
#define UPD_NROF	0x80
#define UPD_DIRECTION	0x100

#define SOUND_NORMAL	0
#define SOUND_SPELL	1

#define COLOR_DEFAULT 0 /* white */
#define COLOR_WHITE  0
#define COLOR_ORANGE 1
#define COLOR_LBLUE  2 /* navy... */
#define COLOR_RED	3
#define COLOR_GREEN 4
#define COLOR_BLUE  5
#define COLOR_GREY  6
#define COLOR_YELLOW  7
#define COLOR_DK_NAVY  8


#define COLOR_HGOLD 64 /* client only colors */
#define COLOR_DGOLD	65
#define COLOR_DBROWN  44

#define COLOR_BLACK 255
extern void DoClient ( ClientSocket *csocket );
extern void SockList_Init ( SockList *sl );
extern void SockList_AddChar ( SockList *sl, char c );
extern void SockList_AddShort ( SockList *sl, uint16 data );
extern void SockList_AddInt ( SockList *sl, uint32 data );
extern int GetInt_String ( unsigned char *data );
extern short GetShort_String ( unsigned char *data );
extern int send_socklist ( int fd, SockList msg );
extern int cs_write_string ( int fd, char *buf, int len );
extern void finish_face_cmd(int pnum, uint32 checksum, char *face);
extern int request_face(int num, int mode);
extern void check_animation_status(int anum);
extern char *adjust_string(char *buf);
