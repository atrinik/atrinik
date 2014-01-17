%{
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

#include <global.h>
#include <loader.h>

#define YY_DECL int rmap_lex_read(RMParms *RP)

    static char *rmap_yval(void);

    static int rmap_lex_error;

#define IVAL    atoi(rmap_yval())
#define FVAL    atof(rmap_yval())

#define YY_NO_INPUT
    %
}

S   [ \ t]+.+
WS  [ \ t]*

%x MESSAGE

/* Don't have to link with -lfl with this */
%option noyywrap

/* Disable unput */
%option nounput

% %

%{
/* Declare some local variables */
    rmap_lex_error = 0;
    %
}

^wallstyle {S}             strncpy(RP->wallstyle, rmap_yval(), RM_SIZE); ^
floorstyle {S}            strncpy(RP->floorstyle, rmap_yval(), RM_SIZE); ^
monsterstyle {S}          strncpy(RP->monsterstyle, rmap_yval(), RM_SIZE); ^
layoutstyle {S}           strncpy(RP->layoutstyle, rmap_yval(), RM_SIZE); ^
doorstyle {S}             strncpy(RP->doorstyle, rmap_yval(), RM_SIZE); ^
decorstyle {S}            strncpy(RP->decorstyle, rmap_yval(), RM_SIZE); ^
dungeon_name {S}          strncpy(RP->dungeon_name, rmap_yval(), RM_SIZE); ^
xsize {S}                 RP->Xsize = IVAL; ^
ysize {S}                 RP->Ysize = IVAL; ^
expand2x {S}              RP->expand2x = IVAL; ^
layoutoptions1 {S}        RP->layoutoptions1 = IVAL; ^
layoutoptions2 {S}        RP->layoutoptions2 = IVAL; ^
layoutoptions3 {S}        RP->layoutoptions3 = IVAL; ^
symmetry {S}              RP->symmetry = IVAL; ^
difficulty {S}            RP->difficulty = IVAL; ^
decorchance {S}           RP->decorchance = IVAL; ^
exitstyle {S}             strncpy(RP->exitstyle, rmap_yval(), RM_SIZE); ^
dungeon_level {S}         RP->dungeon_level = IVAL; ^
dungeon_depth {S}         RP->dungeon_depth = IVAL; ^
final_map {S}             strncpy(RP->final_map, rmap_yval(), RM_SIZE); ^
orientation {S}           RP->orientation = IVAL; ^
origin_x {S}              RP->origin_x = IVAL; ^
origin_y {S}              RP->origin_y = IVAL; ^
origin_map {S}            strncpy(RP->origin_map, rmap_yval(), RM_SIZE); ^
random_seed {S}           RP->random_seed = IVAL; ^
num_monsters {S}          RP->num_monsters = IVAL; ^
darkness {S}              RP->darkness = IVAL; ^
level_increment {S}       RP->level_increment = IVAL; ^
bg_music {S}              strncpy(RP->bg_music, rmap_yval(), RM_SIZE);


<*>(^{WS} $)|\ n            { /* ignore empty lines, newlines we don't do above
                               * */                                               }
#.* \n			          {}

    <<EOF>>                   {
    /* If we got an error, return the error.  Otherwise, return that we got EOF
     * */
    if (rmap_lex_error != 0) {
        return rmap_lex_error;
    }
    else {
        return LL_EOF;
    }
    }

    .*{
    yyerror("Unrecognized string");
    rmap_lex_error = -1;
    }

    % %

/* Our save file syntax is very simple, so we can use a very simple
 * processing mechanism here instead using something like bison
 * This skips over the space and returns the value, or "" if no value
 * is found. */
    static char *rmap_yval(void)
        {
        static char *em = "";
        char *cp;

        cp = strchr(yytext,' ');

        if (cp) {
            return cp + 1;
        }
        else {
            return em;
        }
    }

    int load_parameters(FILE *fp, int bufstate, RMParms *RP)
        {
        int retval;
        char inbuf[MAX_BUF];

        if (bufstate == LO_NEWFILE || bufstate == LO_NOREAD) {
            yy_delete_buffer(YY_CURRENT_BUFFER);
            yy_switch_to_buffer(yy_create_buffer(fp, YY_BUF_SIZE));

            if (bufstate == LO_NOREAD) {
                return LL_NORMAL;
            }
        }

        if (bufstate == LO_LINEMODE) {
            YY_BUFFER_STATE yybufstate;

            while (fgets(inbuf, MAX_BUF - 3, fp)) {
                yybufstate = yy_scan_string(inbuf);
                retval = rmap_lex_read(RP);
                yy_delete_buffer(yybufstate);

                if (retval == LL_NORMAL) {
                    return retval;
                }
            }

            return LL_EOF;
        }

        retval = rmap_lex_read(RP);
        return retval;
    }


/* This takes a buffer, scans it for variables, and sets those variables
 * as appropriate in op.
 *
 * This function appears to be used in only 2 places - in crossedit to
 * override values and in c_wiz to mutate values. */
    int set_random_map_variable(RMParms *rp, const char *buf)
        {
        YY_BUFFER_STATE yybufstate;
        int retval;

        yybufstate = yy_scan_string(buf);
        retval = rmap_lex_read(rp);
        yy_delete_buffer(yybufstate);

        return retval;
    }

/**
 * Frees all memory allocated by the random map loader. */
    void free_random_map_loader(void)
        {
        yylex_destroy();
    }
