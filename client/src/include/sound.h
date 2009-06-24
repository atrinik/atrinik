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

#if !defined(__SOUND_H)
#define __SOUND_H

/* possible status of the sound system*/
typedef enum _sound_system {
	SOUND_SYSTEM_NONE,
	SOUND_SYSTEM_OFF,
	SOUND_SYSTEM_ON
}_sound_system;

/* the sound system status*/
extern _sound_system SoundSystem;


#define SOUND_NORMAL	0
#define SOUND_SPELL		1

/* music mode - controls how the music is played and started */
#define MUSIC_MODE_NORMAL 1
#define MUSIC_MODE_DIRECT 2
#define MUSIC_MODE_FORCED 4 /* thats needed for some map event sounds */

/* sound ids. */
typedef enum _sound_id {
	SOUND_EVENT01,
	SOUND_BOW01,
	SOUND_LEARNSPELL,
	SOUND_FAILSPELL,
	SOUND_FAILROD,
	SOUND_DOOR,
	SOUND_PUSHPLAYER,

	SOUND_HIT_IMPACT, /* 8 */
	SOUND_HIT_CLEAVE,
	SOUND_HIT_SLASH,
	SOUND_HIT_PIERCE,
    SOUND_HIT_BLOCK,
    SOUND_HIT_HAND,
    SOUND_MISS_MOB1,
    SOUND_MISS_MOB2,

	SOUND_PETDEAD, /* 16 */
	SOUND_PLAYERDEAD,
	SOUND_EXPLOSION00,
	SOUND_EXPLOSION01,
    SOUND_KILL,
	SOUND_PULLLEVER,
	SOUND_FALLHOLE,
	SOUND_POISON,

	SOUND_DROP, /* 24 */
	SOUND_LOSE_SOME,
	SOUND_THROW,
	SOUND_GATE_OPEN,
	SOUND_GATE_CLOSE,
	SOUND_OPEN_CONTAINER,
	SOUND_GROWL,
	SOUND_ARROW_HIT,
	SOUND_DOOR_CLOSE,
	SOUND_TELEPORT,
	SOUND_SCROLL,

	/* here we have client side sounds - add server
	 * sounds BEFORE this.
	 */
	SOUND_STEP1,
	SOUND_STEP2,
	SOUND_PRAY,
	SOUND_CONSOLE,
	SOUND_CLICKFAIL,
	SOUND_CHANGE1,
	SOUND_WARN_FOOD,
	SOUND_WARN_DRAIN,
	SOUND_WARN_STATUP,
	SOUND_WARN_STATDOWN,
	SOUND_WARN_HP,
	SOUND_WARN_HP2,
	SOUND_WEAPON_ATTACK,
	SOUND_WEAPON_HOLD,
	SOUND_GET,
	SOUND_BOOK,
	SOUND_PAGE,
	SOUND_MAX
}_sound_id1;

/* to call a spell sound here, do
 * SOUND_MAX + SOUND_MAGIC_xxx
 */
/* this enum should be same as in server */
typedef enum _spell_sound_id {
	SOUND_MAGIC_DEFAULT,
	SOUND_MAGIC_ACID,
	SOUND_MAGIC_ANIMATE,
	SOUND_MAGIC_AVATAR,
	SOUND_MAGIC_BOMB,
	SOUND_MAGIC_BULLET1,
	SOUND_MAGIC_BULLET2,
	SOUND_MAGIC_CANCEL,
	SOUND_MAGIC_COMET,
	SOUND_MAGIC_CONFUSION,
	SOUND_MAGIC_CREATE,
	SOUND_MAGIC_DARK,
	SOUND_MAGIC_DEATH,
	SOUND_MAGIC_DESTRUCTION,
	SOUND_MAGIC_ELEC,
	SOUND_MAGIC_FEAR,
	SOUND_MAGIC_FIRE,
	SOUND_MAGIC_FIREBALL1,
	SOUND_MAGIC_FIREBALL2,
	SOUND_MAGIC_HWORD,
	SOUND_MAGIC_ICE,
	SOUND_MAGIC_INVISIBLE,
	SOUND_MAGIC_INVOKE,
	SOUND_MAGIC_INVOKE2,
	SOUND_MAGIC_MAGIC,
	SOUND_MAGIC_MANABALL,
	SOUND_MAGIC_MISSILE,
	SOUND_MAGIC_MMAP,
	SOUND_MAGIC_ORB,
	SOUND_MAGIC_PARALYZE,
	SOUND_MAGIC_POISON,
	SOUND_MAGIC_PROTECTION,
	SOUND_MAGIC_RSTRIKE,
	SOUND_MAGIC_RUNE,
	SOUND_MAGIC_SBALL,
	SOUND_MAGIC_SLOW,
	SOUND_MAGIC_SNOWSTORM,
	SOUND_MAGIC_STAT,
	SOUND_MAGIC_STEAMBOLT,
	SOUND_MAGIC_SUMMON1,
	SOUND_MAGIC_SUMMON2,
	SOUND_MAGIC_SUMMON3,
	SOUND_MAGIC_TELEPORT,
	SOUND_MAGIC_TURN,
	SOUND_MAGIC_WALL,
	SOUND_MAGIC_WALL2,
	SOUND_MAGIC_WOUND,

	SPELL_SOUND_MAX
}_sound_id;


#ifdef INSTALL_SOUND
typedef struct _wave {
	Mix_Chunk *sound;
	Uint32   soundlen;		/* Length of wave data */
	int      soundpos;		/* Current play position */
} _wave;

typedef struct music_data {
	int flag;				/* if 1, struct is loaded */
	Mix_Music *data;		/* data != 0 , buffer is allocated */
	char name[256];			/* if flag = 1, this is a valid music name */
	int loop;				/* loop data for init music_play() */
	int fade;
	int vol;
}music_data;

extern _wave Sounds[];

extern music_data music;	  		 /* thats the music we just play - if NULL, no music */
extern music_data music_new;
#endif

enum {
	    SPECIAL_SOUND_FOOD,
        SPECIAL_SOUND_STATDOWN,
        SPECIAL_SOUND_STATUP,
        SPECIAL_SOUND_DRAIN,

        SPECIAL_SOUND_INIT
};

void sound_fillerup(void *unused, Uint8 *stream, int len);

void sound_init(void);
void sound_deinit(void);

void sound_loadall(void);
void sound_freeall(void);

void calculate_map_sound(int soundnr, int xoff, int yoff);
int sound_play_effect(int soundid, int pan, int vol);
void sound_play_one_repeat(int soundid, int special_id);

int sound_test_playing(int channel);

void sound_play_music(char *fname, int vol, int fade, int loop, int mode);
void sound_fadeout_music(int value);

#endif
