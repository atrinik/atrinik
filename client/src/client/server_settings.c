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
 * Server settings.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>

/** The server settings. */
server_settings *s_settings = NULL;

/**
 * Initialize the server settings from the srv file.
 */
void server_settings_init(void)
{
    FILE *fp = server_file_open_name(SERVER_FILE_SETTINGS);
    if (fp == NULL) {
        return;
    }

    server_settings_deinit();
    s_settings = ecalloc(1, sizeof(server_settings));

    char_struct *cur_char = NULL;
    size_t text_id = 0;

    char buf[HUGE_BUF * 4];
    uint64_t linenum = 0;

    while (fgets(VS(buf), fp) != NULL) {
        linenum++;

        char *cp = string_skip_whitespace(buf), *end = strchr(cp, '\n');
        if (end != NULL) {
            *end = '\0';
        }

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        char *key = cps[0], *value = cps[1], *error_str;

        if (strcmp(key, "char") == 0) {
            if (cur_char != NULL) {
                error_str = "superfluous char attribute";
                goto error;
            }

            s_settings->characters = ereallocz(s_settings->characters,
                    sizeof(*s_settings->characters) *
                    s_settings->num_characters,
                    sizeof(*s_settings->characters) *
                    (s_settings->num_characters + 1));
            cur_char = &s_settings->characters[s_settings->num_characters];
            s_settings->num_characters++;
            cur_char->name = estrdup(value);
        } else if (string_isempty(value)) {
            if (strcmp(key, "end") == 0) {
                if (cur_char == NULL) {
                    error_str = "superfluous end attribute";
                    goto error;
                }

                cur_char = NULL;
            }
        } else if (cur_char != NULL) {
            if (strcmp(key, "gender") == 0) {
                char *cps2[3];

                if (string_split(value, cps2, arraysize(cps2), ' ') == 3) {
                    int gender_id = gender_to_id(cps2[0]);
                    if (gender_id != -1) {
                        cur_char->gender_archetypes[gender_id] =
                                estrdup(cps2[1]);
                        cur_char->gender_faces[gender_id] = estrdup(cps2[2]);
                    }
                }
            } else if (strcmp(key, "desc") == 0) {
                cur_char->desc = estrdup(value);
            }
        } else if (strcmp(key, "level") == 0) {
            s_settings->max_level = atoi(value);
            s_settings->level_exp = emalloc(sizeof(*s_settings->level_exp) *
                    (s_settings->max_level + 2));

            for (uint32_t lev = 0; lev <= s_settings->max_level; lev++) {
                if (fgets(VS(buf), fp) == NULL) {
                    break;
                }

                s_settings->level_exp[lev] = strtoull(buf, NULL, 16);
            }

            s_settings->level_exp[s_settings->max_level + 1] = 0;
        } else if (strcmp(key, "text") == 0) {
            if (text_id == SERVER_TEXT_MAX) {
                error_str = "reached maximum amount of text entries";
                goto error;
            }

            s_settings->text[text_id] = estrdup(value);
            string_newline_to_literal(s_settings->text[text_id]);

            if (text_id == SERVER_TEXT_PROTECTION_GROUPS ||
                    text_id == SERVER_TEXT_PROTECTION_LETTERS ||
                    text_id == SERVER_TEXT_PROTECTION_FULL ||
                    text_id == SERVER_TEXT_SPELL_PATHS) {
                char **dst;
                size_t arraymax;
                if (text_id == SERVER_TEXT_PROTECTION_GROUPS) {
                    dst = s_settings->protection_groups;
                    arraymax = arraysize(s_settings->protection_groups);
                } else if (text_id == SERVER_TEXT_PROTECTION_LETTERS) {
                    dst = s_settings->protection_letters;
                    arraymax = arraysize(s_settings->protection_letters);
                } else if (text_id == SERVER_TEXT_PROTECTION_FULL) {
                    dst = s_settings->protection_full;
                    arraymax = arraysize(s_settings->protection_full);
                } else if (text_id == SERVER_TEXT_SPELL_PATHS) {
                    dst = s_settings->spell_paths;
                    arraymax = arraysize(s_settings->spell_paths);
                } else {
                    HARD_ASSERT(false);
                }

                size_t i = 0, pos = 0;
                while (string_get_word(s_settings->text[text_id], &pos, ' ',
                        VS(buf), 0)) {
                    if (i == arraymax) {
                        error_str = "reached maximum array size";
                        goto error;
                    }

                    dst[i++] = estrdup(buf);
                }
            }

            text_id++;
        }

        continue;

error:
        LOG(ERROR, "Error parsing %s, line %" PRIu64 ", %s: %s %s",
                SERVER_FILE_SETTINGS, linenum, error_str, key,
                value != NULL ? value : "");
    }

    for (size_t i = text_id; i < SERVER_TEXT_MAX; i++) {
        s_settings->text[i] = estrdup("???");
    }

    fclose(fp);
}

/**
 * Deinitialize the server settings.
 */
void server_settings_deinit(void)
{
    if (s_settings == NULL) {
        return;
    }

    if (s_settings->level_exp != NULL) {
        efree(s_settings->level_exp);
    }

    for (size_t i = 0; i < s_settings->num_characters; i++) {
        if (s_settings->characters[i].name != NULL) {
            efree(s_settings->characters[i].name);
        }

        if (s_settings->characters[i].desc != NULL) {
            efree(s_settings->characters[i].desc);
        }

        for (size_t gender = 0; gender < GENDER_MAX; gender++) {
            if (s_settings->characters[i].gender_archetypes[gender] != NULL) {
                efree(s_settings->characters[i].gender_archetypes[gender]);
            }

            if (s_settings->characters[i].gender_faces[gender] != NULL) {
                efree(s_settings->characters[i].gender_faces[gender]);
            }
        }
    }

    if (s_settings->characters != NULL) {
        efree(s_settings->characters);
    }

    for (size_t i = 0; i < SERVER_TEXT_MAX; i++) {
        if (s_settings->text[i] != NULL) {
            efree(s_settings->text[i]);
        }
    }

    for (size_t i = 0; i < arraysize(s_settings->protection_groups); i++) {
        if (s_settings->protection_groups[i] != NULL) {
            efree(s_settings->protection_groups[i]);
        }
    }

    for (size_t i = 0; i < arraysize(s_settings->protection_letters); i++) {
        if (s_settings->protection_letters[i] != NULL) {
            efree(s_settings->protection_letters[i]);
        }
    }

    for (size_t i = 0; i < arraysize(s_settings->protection_full); i++) {
        if (s_settings->protection_full[i] != NULL) {
            efree(s_settings->protection_full[i]);
        }
    }

    for (size_t i = 0; i < arraysize(s_settings->spell_paths); i++) {
        if (s_settings->spell_paths[i] != NULL) {
            efree(s_settings->spell_paths[i]);
        }
    }

    efree(s_settings);
    s_settings = NULL;
}
