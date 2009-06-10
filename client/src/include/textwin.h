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

#if !defined(__TEXTWIN_H)
#define __TEXTWIN_H

#define TEXT_WIN_MAX 250
#define MAX_KEYWORD_LEN 256

enum {TW_MIX, TW_MSG, TW_CHAT, TW_SUM}; /* windows */
enum {TW_CHECK_BUT_DOWN, TW_CHECK_BUT_UP, TW_CHECK_MOVE}; /* events */
enum {TW_HL_NONE, TW_HL_UP, TW_ABOVE, TW_HL_SLIDER, TW_UNDER, TW_HL_DOWN};
enum {TW_ACTWIN=0x0f, TW_SCROLL=0x10, TW_RESIZE=0x20}; /* flags */


typedef struct _text_buf {
	char buf[128];	 /* text */
	int channel;		 /* which channel */
	int flags;			 /* some flags */
	int color;			 /* color of text */
	int key_clipped; /* 1= key begin in row before 2= no key end */
}_text_buf;

typedef struct _textwin_set {
  int x,y;            /* startpos of the window */
  int size;           /* number or printed textlines */
  int scroll;         /* scroll offset */
  int top_drawLine;   /* first printed textline */
  int bot_drawLine;   /* last printed textline */
  int act_bufsize;    /* 0 ... TEXTWIN_MAX */
  int slider_h;       /* height of the scrollbar-slider  */
  int slider_y;       /* start pos of the scrollbar-slider */
  int highlight;      /* which part to highlight */
  _text_buf text[TEXT_WIN_MAX];
}_textwin_set;

extern _textwin_set txtwin[TW_SUM];
extern int  textwin_flags;
extern void textwin_event(int e, SDL_Event *event);
extern void textwin_show(int x, int y);
extern void textwin_init();
extern void draw_info( char *str, int color );
extern void textwin_addhistory(char* text);
extern void textwin_clearhistory();
extern void textwin_putstring(char* text);
#endif

