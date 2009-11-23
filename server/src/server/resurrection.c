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

/*  the contents of this file were create solely by peterm@soda.berkeley.edu
    all of the above disclaimers apply.  */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#ifdef NO_ERRNO_H
extern int errno;
#else
#   include <errno.h>
#endif

#ifdef sequent
/* stoopid sequent includes don't do this like they should */
extern char * sys_errlist[];
extern int sys_nerr;
#endif
extern char **classname;

void dead_player(object *op)
{
	(void) op;

	/* As we now use SQLite database, this won't work. */
#if 0
	char filename[MAX_BUF];
	char newname[MAX_BUF];
	char path[MAX_BUF];

	/*  set up our paths/strings...  */
	sprintf(path,"%s/%s/%s/%s", settings.localdir, settings.playerdir, op->name, op->name);

	strcpy(filename, path);
	strcat(filename, ".pl");
	strcpy(newname, filename);
	strcat(newname, ".dead");

	if (!rename(filename, newname))
		LOG(llevBug, "BUG: dead_player(): rename error (%s) (%s)\n", filename, newname);
#endif
}

int resurrection_fails(int levelcaster, int leveldead)
{
	int chance = 9;
	/* scheme:  equal in level, 50% success.
	 * +5 % for each level below, -5% for each level above.
	 * minimum 20% */
	chance += levelcaster - leveldead;

	if (chance < 4)
		chance = 4;

	if (chance > rndm(0, 19))
		return 0;

	return 1;
}


/*  name of the person to resurrect and which spell was used
 * to resurrect  */
int resurrect_player(object *op, char *playername, int rspell)
{
	(void) op;
	(void) playername;
	(void) rspell;

	/* Again, because of SQLite this won't work. */
#if 0
	FILE *deadplayer, *liveplayer;

	char oldname[MAX_BUF];
	char newname[MAX_BUF];
	char path[MAX_BUF];
	char buf[MAX_BUF];
	char buf2[MAX_BUF];

	static char *races[]= {"barbarian", "cleric", "elf", "human", "mage", "ninja", "priest", "swashbuckler", "thief", "viking", "warrior", "wizard"};

	long int exp;
	int Con;

	/* set up our paths/strings...  */
	sprintf(path, "%s/%s/%s/%s", settings.localdir, settings.playerdir, playername, playername);

	strcpy(newname, path);
	strcat(newname, ".pl");

	strcpy(oldname, newname);
	strcat(oldname, ".dead");

	if (!(deadplayer = fopen(oldname, "r")))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "The soul of %s cannot be reached.", playername);
		return 0;
	}

	if (!access(newname, 0))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "The soul of %s has already been reborn!", playername);
		fclose(deadplayer);
		return 0;
	}

	if (!(liveplayer = fopen(newname, "w")))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "The soul of %s cannot be re-embodied at the moment.", playername);
		LOG(llevBug, "BUG: ressurect_player(): Cannot write player file %s!\n", newname);
		fclose(deadplayer);
		return 0;
	}

	while (!feof(deadplayer))
	{
		if (fgets(buf, 255, deadplayer) == NULL)
			continue;
		sscanf(buf, "%s", buf2);
		if (!(strcmp(buf2, "exp")))
		{
			sscanf(buf, "%s %ld", buf2, &exp);
			switch (rspell)
			{
				case SP_RAISE_DEAD:
					exp -= exp / 5;
					break;

				case SP_RESURRECTION:
					exp -= exp / 10;
					break;

				case SP_REINCARNATION:
					exp -= exp / 20;
					break;
			}
			sprintf(buf, "exp %ld\n", exp);
		}

		if (!(strcmp(buf2, "Con")))
		{
			sscanf(buf, "%s %d", buf2, &Con);
			switch (rspell)
			{
				case SP_RAISE_DEAD:
					Con -= 2;
					break;

				case SP_RESURRECTION:
					Con -= 1;
					break;
			}
			sprintf(buf, "Con %d\n", Con);
		}

		if (rspell == SP_REINCARNATION)
		{
			if (!(strcmp(buf2, "race")))
				sprintf(buf, "race %s\n", races[rndm(1, 12)]);
		}

		fputs(buf, liveplayer);
	}
	fclose(liveplayer);
	fclose(deadplayer);
	unlink(oldname);
	new_draw_info_format(NDI_UNIQUE, 0, op, "%s lives again!", playername);
#endif

	return 1;
}

void dead_character(char *name)
{
	(void) name;

#if 0
	char buf[MAX_BUF];
	char buf2[MAX_BUF];

	sprintf(buf, "%s/%s/%s/%s.pl", settings.localdir, settings.playerdir, name, name);
	/*  peterm:  create a .dead filename....  ***.pl.dead  */
	strcpy(buf2, buf);
	strcat(buf, ".dead");
	if (rename(buf2, buf) == -1)
		LOG(llevDebug, "DEBUG: dead_character(): Crossfire character rename to dead: %s --", buf);
#endif
}

int dead_player_exists(char *name)
{
	(void) name;

#if 0
	char buf[MAX_BUF];

	sprintf(buf, "%s/%s/%s/%s", settings.localdir, settings.playerdir, name, name);
	strcat(buf, ".pl.dead");

	return !(access(buf, 0));
#endif
	return 0;
}
