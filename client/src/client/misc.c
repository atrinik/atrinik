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
 * Miscellaneous functions. */

#include <include.h>

/**
 * Hash string to find it in the bitmap table.
 * @param str The string to hash.
 * @param tablesize Table size.
 * @return Hashed string. */
static unsigned long hashbmap(const char *str, int tablesize)
{
	unsigned long hash = 0;
	int i = 0;
	unsigned int rot = 0;
	const char *p;

	for (p = str; i < MAXHASHSTRING && *p; p++, i++)
	{
		hash ^= (unsigned long) *p << rot;
		rot += 2;

		if (rot >= (sizeof(unsigned long) - sizeof(char)) * CHAR_BIT)
		{
			rot = 0;
		}
	}

	return (hash % tablesize);
}

/**
 * Find a bitmap by name.
 * @param name The bitmap name to find
 * @return NULL if not found, pointer to the bmap otherwise. */
_bmaptype *find_bmap(char *name)
{
	_bmaptype *at;
	unsigned long index;

	if (name == NULL)
	{
		return NULL;
	}

	index = hashbmap(name, BMAPTABLE);

	for (; ;)
	{
		at = bmap_table[index];

		/* Not in our bmap list */
		if (at == NULL)
		{
			return NULL;
		}

		if (!strcmp(at->name, name))
		{
			return at;
		}

		if (++index >= BMAPTABLE)
		{
			index = 0;
		}
	}
}

/**
 * Add a bitmap to the bitmap table.
 * @param at The bitmap to add. */
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
		{
			index = 0;
		}

		if (index == org_index)
		{
			LOG(LOG_ERROR, "ERROR: add_bmap(): bmaptable too small for %s\n", at->name);
			return;
		}
	}
}

/**
 * Free memory pointed to by p.
 * @param p The memory to free. */
void FreeMemory(void **p)
{
	if (p == NULL)
		return;

	if (*p != NULL)
		free(*p);

	*p = NULL;
}

/**
 * Show string that is being inputted.
 * @param text String to show.
 * @param font Font to use.
 * @param wlen Maximum length of the string.
 * @return String to show, based on the cursor position. */
const char *show_input_string(const char *text, struct _Font *font, int wlen)
{
	int i, len;
	size_t j;
	static char buf[MAX_INPUT_STR];

	strcpy(buf, text);

	len = strlen(buf);

	while (len >= CurrentCursorPos)
	{
		buf[len + 1] = buf[len];
		len--;
	}

	buf[CurrentCursorPos] = '_';

	for (len = 25, i = CurrentCursorPos; i >= 0; i--)
	{
		if (!buf[i])
		{
			continue;
		}

		if (len + font->c[(int) (buf[i])].w + font->char_offset >= wlen)
		{
			i--;
			break;
		}

		len += font->c[(int) (buf[i])].w + font->char_offset;
	}

	len -= 25;

	for (j = CurrentCursorPos; j <= strlen(buf); j++)
	{
		if (len + font->c[(int) (buf[j])].w + font->char_offset >= wlen)
		{
			break;
		}

		len += font->c[(int) (buf[j])].w + font->char_offset;
	}

	buf[j] = '\0';

	return &buf[++i];
}

/**
 * Computes the square root.
 * Based on (n+1)^2 = n^2 + 2n + 1
 * given that   1^2 = 1, then
 *              2^2 = 1 + (2 + 1) = 1 + 3 = 4
 *              3^2 = 4 + (4 + 1) = 4 + 5 = 1 + 3 + 5 = 9
 *              4^2 = 9 + (6 + 1) = 9 + 7 = 1 + 3 + 5 + 7 = 16
 *              ...
 * In other words, a square number can be express as the sum of the
 * series n^2 = 1 + 3 + ... + (2n-1)
 * @param n Number of which to compute the root.
 * @return Square root. */
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
 * Looks for <b>"xxxx"</b> string (including the quotes) in data, and
 * returns the string without the quotes. Updates pos appropriately.
 * @param data The data to look for in.
 * @param[out] pos Position.
 * @return The string without the quotes. */
char *get_parameter_string(const char *data, int *pos)
{
	char *start_ptr, *end_ptr;
	static char buf[4024];

	start_ptr = strchr(data + *pos, '"');

	if (!start_ptr)
	{
		return NULL;
	}

	end_ptr = strchr(++start_ptr, '"');

	if (!end_ptr)
	{
		return NULL;
	}

	strncpy(buf, start_ptr, end_ptr - start_ptr);
	buf[end_ptr - start_ptr] = '\0';

	*pos += ++end_ptr - (data + *pos);

	return buf;
}

/**
 * Splits a string delimited by passed in sep value into characters into an array of strings.
 * @param str The string to be split; will be modified.
 * @param array The string array; will be filled with pointers into str.
 * @param array_size The number of elements in array; if <code>str</code> contains more fields
 * excess fields are not split but included into the last element.
 * @param sep Seperator to use.
 * @return The number of elements found; always less or equal to <code>array_size</code>. */
size_t split_string(char *str, char *array[], size_t array_size, char sep)
{
	char *p;
	size_t pos;

	if (array_size <= 0)
	{
		return 0;
	}

	if (*str == '\0')
	{
		array[0] = str;
		return 1;
	}

	pos = 0;
	p = str;

	while (pos < array_size)
	{
		array[pos++] = p;

		while (*p != '\0' && *p != sep)
		{
			p++;
		}

		if (pos >= array_size)
		{
			break;
		}

		if (*p != sep)
		{
			break;
		}

		*p++ = '\0';
	}

	return pos;
}
