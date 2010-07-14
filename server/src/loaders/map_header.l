%{
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

#include <global.h>
#include <loader.h>

#define YY_DECL int map_lex_load(mapstruct *m)

static char *yval();

static char msgbuf[HUGE_BUF];
static int lex_error, msg_flag;

#define IVAL atoi(yval())

#define SET_OR_CLEAR_MAP_FLAG(m, flag, val) \
{ \
	if (val) \
	{ \
		m->map_flags |= (flag); \
	} \
	else \
	{ \
		m->map_flags &= ~(flag); \
	} \
}

#define YY_NO_INPUT
%}

S	[ \t]+.+
WS	[ \t]*

%x MESSAGE

/* Don't have to link with -lfl with this */
%option noyywrap

/* Disable unput */
%option nounput

/* Disable tt_top_state */
%option noyy_top_state

/* need yy_push_state, yy_pop_state */
%option stack

%%

%{
/* Declare some local variables */
lex_error = 0;
%}

^name{S}             m->name = strdup_local(yval());
^bg_music{S}         m->bg_music = strdup_local(yval());
^region{S}           m->region = get_region_by_name(yval());
^enter_x{S}          m->enter_x = IVAL;
^enter_y{S}          m->enter_y = IVAL;
^width{S}            m->width = IVAL;
^height{S}           m->height = IVAL;

^reset_timeout{S}    m->reset_timeout = IVAL;
^swap_time{S}        m->swap_time = IVAL;
^difficulty{S}       m->difficulty = IVAL;
^darkness{S}         {
						int v = IVAL;

						if (v < -1 || v > MAX_DARKNESS)
						{
							LOG(llevBug, "BUG: Illegal map darkness %d (must be -1 to %d, defaulting to -1).\n", v, MAX_DARKNESS);
						}
						else
						{
							set_map_darkness(m, v);
						}
                     }
^light{S}            m->light_value = IVAL;
^no_save{S}          SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_NO_SAVE, IVAL);
^no_magic{S}         SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_NOMAGIC, IVAL);
^no_priest{S}        SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_NOPRIEST, IVAL);
^no_harm{S}          SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_NOHARM, IVAL);
^no_summon{S}        SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_NOSUMMON, IVAL);
^fixed_login{S}      SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_FIXED_LOGIN, IVAL);
^player_no_save{S}   SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_PLAYER_NO_SAVE, IVAL);
^pvp{S}              SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_PVP, IVAL);
^outdoor{S}          SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_OUTDOOR, IVAL);
^unique{S}           SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_UNIQUE, IVAL);
^fixed_resettime{S}  SET_OR_CLEAR_MAP_FLAG(m, MAP_FLAG_FIXED_RTIME, IVAL);
^tile_path_[1-8]{S}  {
						int tile = atoi(yytext + 10);

						if (tile < 1 || tile > TILED_MAPS)
						{
							LOG(llevBug, "BUG: load_map_header(): Tile location %d out of bounds (%s)\n", tile, m->path);
						}
						else
						{
							char *value = yval(), path[HUGE_BUF];

							if (m->tile_path[tile - 1])
							{
								LOG(llevBug, "BUG: load_map_header(): Tile location %d duplicated (%s <-> %s)\n", tile, m->path, m->tile_path[tile - 1]);
								FREE_AND_CLEAR_HASH(m->tile_path[tile - 1]);
							}

							/* If path not absoulute, try to normalize it */
							if (check_path(value, 1) == -1)
							{
								normalize_path(m->path, value, path);

								if (check_path(path, 1) == -1)
								{
									LOG(llevBug, "BUG: load_map_header(): Can not normalize tile path %s %s %s\n", m->path, value, msgbuf);
									value = NULL;
								}
								else
								{
									value = path;
								}
							}

							/* We have a correct path to a neighbour tile */
							if (value)
							{
								mapstruct *neighbour;
								shstr *path_sh = add_string(value);

								m->tile_path[tile - 1] = path_sh;

								/* If the neighbouring map tile has been loaded, set up the map pointers */
								if ((neighbour = has_been_loaded_sh(path_sh)) && (neighbour->in_memory == MAP_IN_MEMORY || neighbour->in_memory == MAP_LOADING))
								{
									int dest_tile = map_tiled_reverse[tile - 1];

									m->tile_map[tile - 1] = neighbour;

									if (neighbour->tile_path[dest_tile] == NULL || neighbour->tile_path[dest_tile] == m->path)
									{
										neighbour->tile_map[dest_tile] = m;
									}
								}
							}
						}
                     }
^msg{WS}             {
						BEGIN(MESSAGE);
						msgbuf[0] = '\0';
						msg_flag = 0;
                     }
<MESSAGE>^endmsg{WS} {
						BEGIN(INITIAL);

						m->msg = strdup_local(msgbuf);
                     }
<MESSAGE>.*	         {
						if (msg_flag)
						{
							strcat(msgbuf, "\n");
						}

						msg_flag = 1;
						strcat(msgbuf, yytext);
					 }

<*>(^{WS}$)|\n		 {/* ignore empty lines, newlines we don't do above */}
#.*\n			     {}
^end\n               return LL_EOF;

<<EOF>>			     {
						return LL_MORE;
                     }

.*                   {
						LOG(llevBug, "BUG: Got unknown value in map header: %s\n", yytext);
						return LL_NORMAL;
                     }

%%

/**
 * Returns the next token for lex.
 *
 * Our save file syntax is very simple, so we can use a very simple
 * processing mechanism here instead using something like bison.
 *
 * This skips over the space and returns the value, or "" if no value is
 * found. */
static char *yval()
{
	static char *em = "";
	char *cp, *end;

	/* First skip over start of line, like animation or name */
	for (cp = yytext; *cp != ' '; cp++)
	{
		if (*cp == '\0')
		{
			return em;
		}
	}

	/* Skip over whitespace */
	for (; *cp == ' '; cp++)
	{
		if (*cp == '\0')
		{
			return em;
		}
	}

	/* Get last character before null and strip
	 * off trailing whitespace. */
	for (end = cp + strlen(cp) - 1; *end == ' ' || *end == 0x0A || *end == 0x0D; end--)
	{
		if (end == cp)
		{
			return em;
		}

		*end = '\0';
	}

	return cp;
}

int map_set_variable(mapstruct *m, char *buf)
{
	YY_BUFFER_STATE yybufstate, yycurbuf = YY_CURRENT_BUFFER;
	int retval;

	yy_push_state(INITIAL);
	yybufstate = yy_scan_string(buf);

	retval = map_lex_load(m);

	if (yycurbuf)
	{
		yy_switch_to_buffer(yycurbuf);
	}

	yy_delete_buffer(yybufstate);
	yy_pop_state();

	return retval;
}

/**
 * This loads the header information of the map. The header contains
 * things like difficulty, size, timeout, etc.
 * @param m Map being read.
 * @param fp File to read from.
 * @return 1 on success, 0 on failure. */
int load_map_header(mapstruct *m, FILE *fp)
{
	char inbuf[HUGE_BUF];
	YY_BUFFER_STATE yybufstate;
	int retval;

	if (!fgets(inbuf, sizeof(inbuf) - 1, fp) || strcmp(inbuf, "arch map\n"))
	{
		LOG(llevBug, "BUG: load_map_header(): First line should always be 'arch map', but it is not (%s)\n", m->path);
		return 0;
	}

	while (fgets(inbuf, sizeof(inbuf) - 1, fp))
	{
		yybufstate = yy_scan_string(inbuf);
		retval = map_lex_load(m);
		yy_delete_buffer(yybufstate);

		if (retval == LL_ERROR)
		{
			return 0;
		}
		else if (retval == LL_EOF)
		{
			return 1;
		}
	}

	return 1;
}

void save_map_header(mapstruct *m, FILE *fp, int flag)
{
	int i;

	fprintf(fp, "arch map\n");

	if (m->name)
	{
		fprintf(fp, "name %s\n", m->name);
	}

	if (m->bg_music)
	{
		fprintf(fp, "bg_music %s\n", m->bg_music);
	}

	if (m->region)
	{
		fprintf(fp, "region %s\n", m->region->name);
	}

	if (!flag)
	{
		fprintf(fp, "swap_time %d\n", m->swap_time);
	}

	if (m->reset_timeout)
	{
		fprintf(fp, "reset_timeout %d\n", m->reset_timeout);
	}

	if (MAP_FIXED_RESETTIME(m))
	{
		fprintf(fp, "fixed_resettime 1\n");
	}

	if (m->difficulty)
	{
		fprintf(fp, "difficulty %d\n", m->difficulty);
	}

	fprintf(fp, "darkness %d\n", m->darkness);
	fprintf(fp, "light %d\n", m->light_value);

	if (m->width)
	{
		fprintf(fp, "width %d\n", m->width);
	}

	if (m->height)
	{
		fprintf(fp, "height %d\n", m->height);
	}

	if (m->enter_x)
	{
		fprintf(fp, "enter_x %d\n", m->enter_x);
	}

	if (m->enter_y)
	{
		fprintf(fp, "enter_y %d\n", m->enter_y);
	}

	if (m->msg)
	{
		fprintf(fp, "msg\n%s\nendmsg\n", m->msg);
	}

	if (MAP_UNIQUE(m))
	{
		fprintf(fp, "unique 1\n");
	}

	if (MAP_OUTDOORS(m))
	{
		fprintf(fp, "outdoor 1\n");
	}

	if (MAP_NOSAVE(m))
	{
		fprintf(fp, "no_save 1\n");
	}

	if (MAP_NOMAGIC(m))
	{
		fprintf(fp, "no_magic 1\n");
	}

	if (MAP_NOPRIEST(m))
	{
		fprintf(fp, "no_priest 1\n");
	}

	if (MAP_NOHARM(m))
	{
		fprintf(fp, "no_harm 1\n");
	}

	if (MAP_NOSUMMON(m))
	{
		fprintf(fp, "no_summon 1\n");
	}

	if (MAP_FIXEDLOGIN(m))
	{
		fprintf(fp, "fixed_login 1\n");
	}

	if (MAP_PVP(m))
	{
		fprintf(fp, "pvp 1\n");
	}

	/* Save any tiling information */
	for (i = 0; i < TILED_MAPS; i++)
	{
		if (m->tile_path[i])
		{
			fprintf(fp, "tile_path_%d %s\n", i + 1, m->tile_path[i]);
		}
	}

	fprintf(fp, "end\n");
}
