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
 * General convenience functions for Atrinik.
 *
 * The random functions here take luck into account when rolling random
 * dice or numbers.  This function has less of an impact the larger the
 * difference becomes in the random numbers.  IE, the effect is lessened
 * on a 1-1000 roll, vs a 1-6 roll.  This can be used by crafty programmers,
 * to specifically disable luck in certain rolls, simply by making the
 * numbers larger (ie, 1d1000 > 500 vs 1d6 > 3) */

#include <global.h>

/**
 * Roll a random number between min and max.  Uses op to determine luck,
 * and if goodbad is non-zero, luck increases the roll, if zero, it decreases.
 * Generally, op should be the player/caster/hitter requesting the roll,
 * not the recipient (ie, the poor slob getting hit). */
int random_roll(int min, int max, const object *op, int goodbad)
{
	int omin, diff, luck, base, ran;

	omin = min;
	diff = max - min + 1;
	/* d2 and d3 are corner cases */
	((diff > 2) ? (base = 20) : (base = 50));

	if (max < 1 || diff < 1)
	{
		LOG(llevBug, "BUG: Calling random_roll with min=%d max=%d\n", min, max);
		/* avoids a float exception */
		return min;
	}

	ran = RANDOM();

	if (op->type != PLAYER)
	{
		return (ran % diff) + min;
	}

	luck = op->stats.luck;

	if (RANDOM() % base < MIN(10, abs(luck)))
	{
		/* we have a winner */
		((luck > 0) ? (luck = 1) : (luck = -1));
		diff -= luck;

		/* check again */
		if (diff < 1)
		{
			return(omin);
		}

		((goodbad) ? (min += luck) : (diff));

		return (MAX(omin, MIN(max, (ran % diff) + min)));
	}

	return ((ran % diff) + min);
}

/**
 * Roll a number of dice (2d3, 4d6).  Uses op to determine luck,
 * If goodbad is non-zero, luck increases the roll, if zero, it decreases.
 * Generally, op should be the player/caster/hitter requesting the roll,
 * not the recipient (ie, the poor slob getting hit).
 * The args are num D size (ie 4d6) */
int die_roll(int num, int size, const object *op, int goodbad)
{
	int min, diff, luck, total, i, gotlucky, base;

	diff = size;
	min = 1;
	luck = total = gotlucky = 0;
	/* d2 and d3 are corner cases */
	((diff > 2) ? (base = 20) : (base = 50));

	if (size < 2 || diff < 1)
	{
		LOG(llevBug, "BUG: Calling die_roll with num=%d size=%d\n", num, size);
		/* avoids a float exception */
		return num;
	}

	if (op->type == PLAYER)
	{
		luck = op->stats.luck;
	}

	for (i = 0; i < num; i++)
	{
		if (RANDOM() % base < MIN(10, abs(luck)) && !gotlucky)
		{
			/* we have a winner */
			gotlucky++;
			((luck > 0) ? (luck = 1) : (luck = -1));
			diff -= luck;

			/* check again */
			if (diff < 1)
			{
				return num;
			}

			((goodbad) ? (min += luck) : (diff));
			total += MAX(1, MIN(size, (RANDOM() % diff) + min));
		}
		else
		{
			total += RANDOM() % size + 1;
		}
	}

	return total;
}

/**
 * Returns a number between min and max.
 *
 * It is suggested one use these functions rather than RANDOM()%, as it
 * would appear that a number of off-by-one-errors exist due to improper
 * use of %.
 *
 * This should also prevent SIGFPE. */
int rndm(int min, int max)
{
	int diff;

	diff = max - min + 1;

	if (max < 1 || diff < 1)
	{
		return min;
	}

	return (RANDOM() % diff + min);
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
 * Find a racelink.
 * @param name The name of the race to look for.
 * @return The racelink if found, NULL otherwise. */
racelink *find_racelink(const char *name)
{
	racelink *test = NULL;

	if (name && first_race)
	{
		for (test = first_race; test && test != test->next; test = test->next)
		{
			if (!test->name || !strcmp(name, test->name))
			{
				break;
			}
		}
	}

	return test;
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
	while (*ustring != '\0' && isspace(*ustring))
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
		if (*p < ' ')
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
