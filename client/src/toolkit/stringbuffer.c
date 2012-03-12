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

/**
 * @file
 * Implements a general string buffer: it builds a string by
 * concatenating. It allocates enough memory to hold the whole string;
 * there is no upper limit for the total string length.
 *
 * Usage is:
 * @code
 * StringBuffer *sb = stringbuffer_new();
 * stringbuffer_append_string(sb, "abc");
 * stringbuffer_append_string(sb, "def");
 * ... more calls to stringbuffer_append_xxx()
 * char *str = stringbuffer_finish(sb);
 * ... use str
 * free(str);
 * @endcode
 *
 * No function ever fails. In case not enough memory is available, the
 * program exits. */

#include <global.h>
#include <stdarg.h>

static void stringbuffer_ensure(StringBuffer *sb, size_t len);

/**
 * Initialize the stringbuffer API.
 * @internal */
void toolkit_stringbuffer_init(void)
{
	TOOLKIT_INIT_FUNC_START(stringbuffer)
	{
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the stringbuffer API.
 * @internal */
void toolkit_stringbuffer_deinit(void)
{
}

/**
 * Create a new string buffer.
 * @return The newly allocated string buffer. */
StringBuffer *stringbuffer_new(void)
{
	StringBuffer *sb = malloc(sizeof(StringBuffer));

	if (!sb)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(1);
	}

	sb->size = MAX_BUF;
	sb->buf = malloc(sb->size);
	sb->pos = 0;
	return sb;
}

/**
 * Deallocate the string buffer instance and return the string.
 *
 * The passed string buffer must not be accessed afterwards.
 * @param sb The string buffer to deallocate.
 * @return The result string; to free it, call free() on it. */
char *stringbuffer_finish(StringBuffer *sb)
{
	char *result;

	sb->buf[sb->pos] = '\0';
	result = sb->buf;
	free(sb);
	return result;
}

/**
 * Deallocate the string buffer instance and return the string as a shared
 * string.
 *
 * The passed string buffer must not be accessed afterwards.
 * @param sb The string buffer to deallocate.
 * @return The result shared string; to free it, use
 * FREE_AND_CLEAR_HASH(). */
const char *stringbuffer_finish_shared(StringBuffer *sb)
{
	char *str = stringbuffer_finish(sb);
	const char *result = add_string(str);

	free(str);
	return result;
}

/**
 * Append a string of the specified length to a string buffer instance.
 * @param sb The string buffer to modify.
 * @param str The string to append.
 * @param len Length of the string. */
void stringbuffer_append_string_len(StringBuffer *sb, const char *str, size_t len)
{
	stringbuffer_ensure(sb, len + 1);
	memcpy(sb->buf + sb->pos, str, len);
	sb->pos += len;
}

/**
 * Append a string to a string buffer instance.
 * @param sb The string buffer to modify.
 * @param str The string to append. */
void stringbuffer_append_string(StringBuffer *sb, const char *str)
{
	stringbuffer_append_string_len(sb, str, strlen(str));
}

/**
 * Append a formatted string to a string buffer instance.
 * @param sb The string buffer to modify.
 * @param format The format string to append. */
void stringbuffer_append_printf(StringBuffer *sb, const char *format, ...)
{
	size_t size = MAX_BUF;

	for (; ;)
	{
		int n;
		va_list arg;

		stringbuffer_ensure(sb, size);

		va_start(arg, format);
		n = vsnprintf(sb->buf + sb->pos, size, format, arg);
		va_end(arg);

		if (n > -1 && (size_t) n < size)
		{
			sb->pos += (size_t) n;
			break;
		}

		/* Precisely what is needed */
		if (n > -1)
		{
			size = n + 1;
		}
		/* Twice the old size */
		else
		{
			size *= 2;
		}
	}
}

/**
 * Append the contents of a string buffer instance to another string
 * buffer instance.
 * @param sb The string buffer to modify.
 * @param sb2 The string buffer to append; it must be different from sb. */
void stringbuffer_append_stringbuffer(StringBuffer *sb, const StringBuffer *sb2)
{
	stringbuffer_ensure(sb, sb2->pos + 1);
	memcpy(sb->buf + sb->pos, sb2->buf, sb2->pos);
	sb->pos += sb2->pos;
}

/**
 * Append a single character to the specified string buffer instance.
 * @param sb The string buffer to modify.
 * @param c The character to append. */
void stringbuffer_append_char(StringBuffer *sb, const char c)
{
	stringbuffer_ensure(sb, 1 + 1);
	sb->buf[sb->pos++] = c;
}

/**
 * Make sure that at least len bytes are available in the passed string
 * buffer.
 * @param sb The string buffer to modify.
 * @param len The number of bytes to allocate. */
static void stringbuffer_ensure(StringBuffer *sb, size_t len)
{
	char *tmp;
	size_t new_size;

	if (sb->pos + len <= sb->size)
	{
		return;
	}

	new_size = sb->pos + len + MAX_BUF;
	tmp = realloc(sb->buf, new_size);

	if (tmp == NULL)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(1);
	}

	sb->buf = tmp;
	sb->size = new_size;
}

/**
 * Return the current length of the buffer.
 * @param sb The string buffer to check.
 * @return Current length of 'sb'. */
size_t stringbuffer_length(StringBuffer *sb)
{
	return sb->pos;
}

/**
 * Find character 'c' in the specified StringBuffer instance, searching
 * left-to-right.
 * @param sb The StringBuffer instance to search in.
 * @param c Character to look for.
 * @return Index in the StringBuffer's buffer, -1 if the character was
 * not found. */
ssize_t stringbuffer_index(StringBuffer *sb, char c)
{
	size_t i;

	for (i = 0; i < sb->pos; i++)
	{
		if (sb->buf[i] == c)
		{
			return i;
		}
	}

	return -1;
}

/**
 * Find character 'c' in the specified StringBuffer instance, searching
 * right-to-left.
 * @param sb The StringBuffer instance to search in.
 * @param c Character to look for.
 * @return Index in the StringBuffer's buffer, -1 if the character was
 * not found. */
ssize_t stringbuffer_rindex(StringBuffer *sb, char c)
{
	size_t i;

	for (i = sb->pos; i > 0; i--)
	{
		if (sb->buf[i - 1] == c)
		{
			return i - 1;
		}
	}

	return -1;
}
