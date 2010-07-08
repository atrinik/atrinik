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
 * General convenience functions for Atrinik. */

#include <global.h>

/**
 * Calculates a random number between min and max.
 *
 * It is suggested one uses this function rather than RANDOM()%, as it
 * would appear that a number of off-by-one-errors exist due to improper
 * use of %.
 *
 * This should also prevent SIGFPE.
 * @param min Starting range.
 * @param max Ending range.
 * @return The random number. */
int rndm(int min, int max)
{
	if (max < 1 || max - min + 1 < 1)
	{
		LOG(llevBug, "BUG: Calling rndm() with min=%d max=%d\n", min, max);
		return min;
	}

	return min + RANDOM() / (RAND_MAX / (max - min + 1) + 1);
}

/**
 * Calculates a chance of 1 in 'n'.
 * @param n Number.
 * @return 1 if the chance of 1/n was successful, 0 otherwise. */
int rndm_chance(uint32 n)
{
	if (!n)
	{
		LOG(llevBug, "BUG: Calling rndm_chance() with n=0.\n");
		return 0;
	}

	return (uint32) RANDOM() < (RAND_MAX + 1U) / n;
}

/**
 * Return the number of the spell that whose name matches the passed
 * string argument.
 * @param spname Name of the spell to look up.
 * @return -1 if no such spell name match is found, the spell ID
 * otherwise. */
int look_up_spell_name(const char *spname)
{
	int i;

	for (i = 0; i < NROFREALSPELLS; i++)
	{
		if (strcmp(spname, spells[i].name) == 0)
		{
			return i;
		}
	}

	return -1;
}

/**
 * Replace in string src all occurrences of key by replacement. The resulting
 * string is put into result; at most resultsize characters (including the
 * terminating null character) will be written to result. */
void replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize)
{
	size_t resultlen, keylen;

	/* special case to prevent infinite loop if key == replacement == "" */
	if (strcmp(key, replacement) == 0)
	{
		snprintf(result, resultsize, "%s", src);
		return;
	}

	keylen = strlen(key);
	resultlen = 0;

	while (*src != '\0' && resultlen + 1 < resultsize)
	{
		if (strncmp(src, key, keylen) == 0)
		{
			snprintf(result + resultlen, resultsize - resultlen, "%s", replacement);
			resultlen += strlen(result + resultlen);
			src += keylen;
		}
		else
		{
			result[resultlen++] = *src++;
		}
	}

	result[resultlen] = '\0';
}

/**
 * Checks for a legal string by first trimming left whitespace and then
 * checking if there is anything left.
 * @param ustring The string to clean up.
 * @return Cleaned up string, or NULL if the cleaned up string doesn't
 * have any characters left. */
char *cleanup_string(char *ustring)
{
	/* Trim all left whitespace */
	while (*ustring != '\0' && (isspace(*ustring) || !isprint(*ustring)))
	{
		ustring++;
	}

	/* This happens when whitespace only string was submitted. */
	if (!ustring || *ustring == '\0')
	{
		return NULL;
	}

	return ustring;
}

/**
 * Returns a single word from a string, free from left and right
 * whitespace.
 * @param str The string.
 * @param pos Position in string.
 * @return The word, NULL if there is no word left in str. */
char *get_word_from_string(char *str, int *pos)
{
	/* this is used for controled input which never should bigger than this */
	static char buf[HUGE_BUF];
	int i = 0;

	buf[0] = '\0';

	while (*(str + (*pos)) != '\0' && (!isalnum(*(str + (*pos))) && !isalpha(*(str + (*pos)))))
	{
		(*pos)++;
	}

	/* Nothing left. */
	if (*(str + (*pos)) == '\0')
	{
		return NULL;
	}

	/* Copy until end of string or whitespace */
	while (*(str + (*pos)) != '\0' && (isalnum(*(str + (*pos))) || isalpha(*(str + (*pos)))))
	{
		buf[i++] = *(str + (*pos)++);
	}

	buf[i] = '\0';
	return buf;
}

/**
 * Adjusts a player name like "xxXxx " to "Xxxxx".
 * @param name Player name to adjust. */
void adjust_player_name(char *name)
{
	char *tmp = name;

	if (!tmp || *tmp == '\0')
	{
		return;
	}

	*tmp = toupper(*tmp);

	while (*(++tmp) != '\0')
	{
		*tmp = tolower(*tmp);
	}

	/* Trim the right whitespace */
	while (tmp >= name && *(tmp - 1) == ' ')
	{
		*(--tmp) = '\0';
	}
}

/**
 * Replaces any unprintable character in the given buffer with a space.
 * @param buf The buffer to modify. */
void replace_unprintable_chars(char *buf)
{
	char *p;

	for (p = buf; *p != '\0'; p++)
	{
		if (*p < ' ' || *p > '~')
		{
			*p = ' ';
		}
	}
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

/**
 * Returns a random direction (1..8).
 * @return The random direction. */
int get_random_dir()
{
	return rndm(1, 8);
}

/**
 * Returns a random direction (1..8) similar to a given direction.
 * @param dir The exact direction.
 * @return The randomized direction. */
int get_randomized_dir(int dir)
{
	return absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
}

/**
 * We don't want to exceed the buffer size of buf1 by adding on buf2!
 * @param buf1
 * @param buf2
 * Buffers we plan on concatening. Can be NULL.
 * @param bufsize Size of buf1. Can be 0.
 * @return 1 if overflow will occur, 0 otherwise. */
int buf_overflow(const char *buf1, const char *buf2, size_t bufsize)
{
	size_t len1 = 0, len2 = 0;

	if (buf1)
	{
		len1 = strlen(buf1);
	}

	if (buf2)
	{
		len2 = strlen(buf2);
	}

	if ((len1 + len2) >= bufsize)
	{
		return 1;
	}

	return 0;
}

/**
 * This function does three things:
 * -# Controls that we have a legal string; if not, return NULL
 * -# Removes all left whitespace (if all whitespace return NULL)
 * -# Change and/or process all control characters like '^', '~', etc.
 * @param ustring The string to cleanup
 * @return Cleaned up string, or NULL */
char *cleanup_chat_string(char *ustring)
{
	int i;

	if (!ustring)
	{
		return NULL;
	}

	/* This happens when whitespace only string was submitted. */
	if (!ustring || *ustring == '\0')
	{
		return NULL;
	}

	/* Now clear all special characters. */
	for (i = 0; *(ustring + i) != '\0'; i++)
	{
		if (*(ustring + i) == '~' || *(ustring + i) == '^' || *(ustring + i) == '|' || *(ustring + i) < ' ' || *(ustring + i) > '~')
		{
			*(ustring + i) = ' ';
		}
	}

	/* Kill all whitespace. */
	while (*ustring != '\0' && isspace(*ustring))
	{
		ustring++;
	}

	return ustring;
}

/**
 * Adds thousand separators to a given number.
 * @param num Number.
 * @return Thousands-separated string. */
char *format_number_comma(uint64 num)
{
	static char retbuf[4 * (sizeof(uint64) * CHAR_BIT + 2) / 3 / 3 + 1];
	char *buf;
	int i = 0;

	buf = &retbuf[sizeof(retbuf) - 1];
	*buf = '\0';

	do
	{
		if (i % 3 == 0 && i != 0)
		{
			*--buf = ',';
		}

		*--buf = '0' + num % 10;
		num /= 10;
		i++;
	}
	while (num != 0);

	return buf;
}

/**
 * Copy a file.
 * @param filename Source file.
 * @param fpout Where to copy to. */
void copy_file(const char *filename, FILE *fpout)
{
	FILE *fp;
	char buf[MAX_BUF];

	fp = fopen(filename, "r");

	if (!fp)
	{
		LOG(llevBug, "BUG: copy_file(): Failed to open '%s'.\n", filename);
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		fputs(buf, fpout);
	}

	fclose(fp);
}

/**
 * Replaces "\n" by a newline char.
 *
 * Since we are replacing 2 chars by 1, no overflow should happen.
 * @param line Text to replace into. */
void convert_newline(char *str)
{
	char *next, buf[MAX_BUF];

	while ((next = strstr(str, "\\n")))
	{
		*next = '\n';
		*(next + 1) = '\0';
		snprintf(buf, sizeof(buf), "%s%s", str, next + 2);
		strcpy(str, buf);
	}
}
