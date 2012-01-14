/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * String API. */

#include <global.h>

/**
 * Initialize the string API.
 * @internal */
void toolkit_string_init(void)
{
	TOOLKIT_INIT_FUNC_START(string)
	{
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the string API.
 * @internal */
void toolkit_string_deinit(void)
{
}

/**
 * Replace in string src all occurrences of key by replacement. The resulting
 * string is put into result; at most resultsize characters (including the
 * terminating null character) will be written to result. */
void string_replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize)
{
	size_t resultlen, keylen;

	/* Special case to prevent infinite loop if key == replacement == "" */
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
 * Splits a string delimited by passed in sep value into characters into an array of strings.
 * @param str The string to be split; will be modified.
 * @param array The string array; will be filled with pointers into str.
 * @param array_size The number of elements in array; if <code>str</code> contains more fields
 * excess fields are not split but included into the last element.
 * @param sep Separator to use.
 * @return The number of elements found; always less or equal to <code>array_size</code>. */
size_t string_split(char *str, char *array[], size_t array_size, char sep)
{
	char *p;
	size_t pos;

	for (pos = 0; pos < array_size; pos++)
	{
		array[pos] = NULL;
	}

	if (!str || *str == '\0' || array_size <= 0)
	{
		return 0;
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
 * Replaces any unprintable character in the given buffer with a space.
 * @param buf The buffer to modify. */
void string_replace_unprintable_chars(char *buf)
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
 * Adds thousand separators to a given number.
 * @param num Number.
 * @return Thousands-separated string. */
char *string_format_number_comma(uint64 num)
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
 * Strips markup from a string.
 *
 * Replaces '<' characters with a space, effectively disabling any markup
 * (note that entities such as &lt; are still allowed).
 * @param str The string. */
void string_remove_markup(char *str)
{
	char *cp;

	for (cp = str; *cp != '\0'; cp++)
	{
		if (*cp == '<')
		{
			*cp = ' ';
		}
	}
}

/**
 * Transforms a string to uppercase, in-place.
 * @param str String to transform, will be modified. */
void string_toupper(char *str)
{
	while (*str != '\0')
	{
		*str = toupper(*str);
		str++;
	}
}

/**
 * Transforms a string to lowercase, in-place.
 * @param str String to transform, will be modified. */
void string_tolower(char *str)
{
	while (*str != '\0')
	{
		*str = tolower(*str);
		str++;
	}
}

/**
 * Trim left and right whitespace in string.
 *
 * @note Does in-place modification.
 * @param str String to trim.
 * @return 'str'. */
char *string_whitespace_trim(char *str)
{
	char *cp;
	size_t len;

	cp = str;
	len = strlen(cp);

	while (isspace(cp[len - 1]))
	{
		cp[--len] = '\0';
	}

	while (isspace(*cp))
	{
		cp++;
		len--;
	}

	memmove(str, cp, len + 1);

	return str;
}

/**
 * Remove extraneous whitespace in a string.
 *
 * @note Does in-place modification.
 * @param str The string.
 * @return 'str'. */
char *string_whitespace_squeeze(char *str)
{
	size_t r, w;

	for (r = 0, w = 0; str[r] != '\0'; r++)
	{
		if (isspace(str[r]))
		{
			if (!w || !isspace(str[w - 1]))
			{
				str[w++] = ' ';
			}
		}
		else
		{
			str[w++] = str[r];
		}
	}

	str[w] = '\0';

	return str;
}

/**
 * Replaces "\n" by a newline char.
 *
 * Since we are replacing 2 chars by 1, no overflow should happen.
 * @param line Text to replace into. */
void string_newline_to_literal(char *str)
{
	char *next;

	while ((next = strstr(str, "\\n")))
	{
		*next = '\n';
		memmove(next + 1, next + 2, strlen(next) - 1);
	}
}

/**
 * Returns a single word from a string, free from left and right
 * whitespace.
 *
 * Effectively allows looping through all the words in a string.
 * @param str The string.
 * @param[out] pos Position in string.
 * @param word Where to store the word.
 * @param wordsize Size of 'word'.
 * @return 'word', NULL if 'word' is empty. */
const char *string_get_word(const char *str, size_t *pos, char *word, size_t wordsize)
{
	size_t i;

	i = 0;
	str += (*pos);

	while (*str != '\0' && !isalnum(*str))
	{
		(*pos)++;
	}

	while (*str != '\0' && isalnum(*str))
	{
		if (i < wordsize - 1)
		{
			word[i++] = *str;
		}

		(*pos)++;
	}

	word[i] = '\0';

	return *word == '\0' ? NULL : word;
}

/**
 * Checks if string is a digit.
 * @return 1 if the string is a digit, 0 otherwise. */
int string_isdigit(const char *str)
{
	if (*str == '-')
	{
		str++;
	}

	while (*str != '\0')
	{
		if (!isdigit(*str))
		{
			return 0;
		}
	}

	return 1;
}

/**
 * Capitalize a string, transforming the starting letter (if any) into
 * its uppercase version, and all following letters into their lowercase
 * versions.
 * @param str String to capitalize. */
void string_capitalize(char *str)
{
	if (!str || *str == '\0')
	{
		return;
	}

	*str = toupper(*str);
	str++;

	while (*str != '\0')
	{
		*str = tolower(*str);
		str++;
	}
}

int string_startswith(const char *str, const char *cmp)
{
	if (!str || !cmp)
	{
		return 0;
	}

	if (strncmp(str, cmp, strlen(cmp)) == 0)
	{
		return 1;
	}

	return 0;
}

int string_endswith(const char *str, const char *cmp)
{
	ssize_t len;

	if (!str || !cmp)
	{
		return 0;
	}

	len = strlen(str) - strlen(cmp);
	str += MAX(0, len);

	if (str && strcmp(str, cmp) == 0)
	{
		return 1;
	}

	return 0;
}

char *string_sub(const char *str, ssize_t start, ssize_t end)
{
	size_t n, max;

	if (end < 0)
	{
		end = strlen(str) + end;
	}
	else if (start < 0)
	{
		end = strlen(str);
		start = end + start;
	}

	if (!(str + start) || end - start < 0)
	{
		return strdup("");
	}

	str += start;
	max = strlen(str);
	n = MIN(max, (size_t) (end - start));

	return strndup(str, n);
}
