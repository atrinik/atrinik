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

#include <include.h>

/**
 * @file
 * Miscellaneous functions */

/**
 * Hash bmap string
 * @param str The string
 * @param tablesize Table size
 * @return Hashed long */
static unsigned long hashbmap(char *str, int tablesize)
{
	unsigned long hash = 0;
	int i = 0, rot = 0;
	char *p;

	for (p = str; i < MAXHASHSTRING && *p; p++, i++)
	{
		hash ^= (unsigned long) *p << rot;
		rot += 2;

		if (rot >= ((int) sizeof(long) - (int) sizeof(char)) * 8)
			rot = 0;
	}

	return (hash % tablesize);
}

/**
 * Find a bmap by name.
 * @param name The bmap name to find
 * @return Null if not found, pointer to the bmap otherwise */
_bmaptype *find_bmap(char *name)
{
	_bmaptype *at;
	unsigned long index;

	if (name == NULL)
		return (_bmaptype *) NULL;

	index = hashbmap(name, BMAPTABLE);

	for (; ;)
	{
		at = bmap_table[index];

		/* Not in our bmap list */
		if (at == NULL)
			return NULL;

		if (!strcmp(at->name, name))
			return at;

		if (++index >= BMAPTABLE)
			index = 0;
	}
}

/**
 * Add a bmap to the bmap table.
 * @param at The bmap */
void add_bmap(_bmaptype *at)
{
	int index = hashbmap(at->name, BMAPTABLE), org_index = index;

	for (; ;)
	{
		if (bmap_table[index] && !strcmp(bmap_table[index]->name, at->name))
		{
			LOG(LOG_ERROR, "ERROR: add_bmap(): Double use of bmap name %s\n", at->name);
		}

		if (bmap_table[index] == NULL)
		{
			bmap_table[index] = at;
			return;
		}

		if (++index == BMAPTABLE)
			index = 0;

		if (index == org_index)
		{
			LOG(LOG_ERROR, "ERROR: add_bmap(): bmaptable too small for %s\n", at->name);
			return;
		}
	}
}

/**
 * Free memory pointed to by p.
 * @param p The memory to free */
void FreeMemory(void **p)
{
	if (p == NULL)
		return;

	if (*p != NULL)
		free(*p);

	*p = NULL;
}

char *show_input_string(char *text, struct _Font *font, int wlen)
{
	int i, j,len;
	static char buf[MAX_INPUT_STR];

	strcpy(buf, text);

	len = strlen(buf);

	while (len >= CurrentCursorPos)
	{
		buf[len + 1] = buf[len];
		len--;
	}

	buf[CurrentCursorPos] = '_';

	for (len = 25,i = CurrentCursorPos; i >= 0; i--)
	{
		if (!buf[i])
			continue;

		if (len + font->c[(int) (buf[i])].w + font->char_offset >= wlen)
		{
			i--;

			break;
		}

		len += font->c[(int) (buf[i])].w + font->char_offset;
	}

	len -= 25;

	for (j = CurrentCursorPos; j <= (int) strlen(buf); j++)
	{
		if (len + font->c[(int) (buf[j])].w + font->char_offset >= wlen)
		{
			break;
		}

		len += font->c[(int) (buf[j])].w + font->char_offset;
	}

	buf[j] = 0;

	return &buf[++i];
}

int read_substr_char(char *srcstr, char *desstr, int *sz, char ct)
{
	unsigned char c;
	int s = 0;

	desstr[0] = 0;

	for (; s < 1023 ;)
	{
		/* Get character */
		c = *(desstr + s++) = *(srcstr + *sz);

		if (c == 0x0d)
		{
			continue;
		}

		if (c == 0)
		{
			return -1;
		}

		/* if it END or WHITESPACE..*/
		if (c == 0x0a || c == ct)
		{
			/* have a single word! (or not...)*/
			break;
		}

		/* point to next source char */
		(*sz)++;
	}

	/* terminate all times with 0, */
	*(desstr+(--s)) = 0;

	/*point to next source charakter return(s);*/
	(*sz)++;

	return s;
}

/* Based on (n+1)^2 = n^2 + 2n + 1
 * given that	1^2 = 1, then
 *		2^2 = 1 + (2 + 1) = 1 + 3 = 4
 * 		3^2 = 4 + (4 + 1) = 4 + 5 = 1 + 3 + 5 = 9
 * 		4^2 = 9 + (6 + 1) = 9 + 7 = 1 + 3 + 5 + 7 = 16
 *		...
 * In other words, a square number can be express as the sum of the
 * series n^2 = 1 + 3 + ... + (2n-1) */
int isqrt(int n)
{
	int result, sum, prev;

	result = 0;
	prev = sum = 1;

	while (sum <= n)
	{
		prev += 2;
		sum += prev;
		++result;
	}

	return result;
}

/**
 * This function gets a ="xxxxxx" string from a line.
 * It removes the =" and the last " and returns the
 * string in a static buffer.
 * @param data The data to parse
 * @param pos Position
 * @return The static string buffer */
char *get_parameter_string(char *data, int *pos)
{
	char *start_ptr, *end_ptr;
	static char buf[4024];

	/* We assume a " after the =... Don't be shy, we search for a '"' */
	start_ptr = strchr(data + *pos, '"');

	/* Error */
	if (!start_ptr)
		return NULL;

	end_ptr = strchr(++start_ptr, '"');

	/* Error */
	if (!end_ptr)
		return NULL;

	strncpy(buf, start_ptr, end_ptr - start_ptr);
	buf[end_ptr - start_ptr] = 0;

	*pos += ++end_ptr - (data + *pos);

	return buf;
}
