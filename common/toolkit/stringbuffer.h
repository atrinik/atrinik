/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Header file for stringbuffer.c.
 */

#ifndef TOOLKIT_STRINGBUFFER_H
#define TOOLKIT_STRINGBUFFER_H

#include "toolkit.h"
#include "stringbuffer_dec.h"

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(stringbuffer);

StringBuffer *stringbuffer_new(void);
void stringbuffer_free(StringBuffer *sb);
char *stringbuffer_finish(StringBuffer *sb);
const char *stringbuffer_finish_shared(StringBuffer *sb);
void stringbuffer_append_string_len(StringBuffer *sb, const char *str,
        size_t len);
void stringbuffer_append_string(StringBuffer *sb, const char *str);
void stringbuffer_append_printf(StringBuffer *sb, const char *format,
        ...) __attribute__((format(printf, 2, 3)));
void stringbuffer_append_stringbuffer(StringBuffer *sb,
        const StringBuffer *sb2);
void stringbuffer_append_char(StringBuffer *sb, const char c);
const char *stringbuffer_data(StringBuffer *sb);
size_t stringbuffer_length(StringBuffer *sb);
void stringbuffer_seek(StringBuffer *sb, const size_t pos);
ssize_t stringbuffer_index(StringBuffer *sb, char c);
ssize_t stringbuffer_rindex(StringBuffer *sb, char c);
char *stringbuffer_sub(StringBuffer *sb, ssize_t start, ssize_t end);

#endif
