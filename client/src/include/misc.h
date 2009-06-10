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

#if !defined(__MISC_H)
#define __MISC_H

#define MAX_INPUT_STR 256

void FreeMemory(void **p);
char *show_input_string(char *text, struct _Font *font, int wlen);
int read_substr_char(char *srcstr, char *desstr, int *sz, char ct);
char *get_parameter_string(char *data, int *pos);
int isqrt(int n);

void * _my_malloc(size_t blen, char *info);
extern unsigned long hasharch(char *str, int tablesize);
extern _bmaptype *find_bmap(char *name);
extern void add_bmap(_bmaptype *at);

#endif
