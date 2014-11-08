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
 * Server settings.
 *
 * @author Alex Tokar */

#include <global.h>

/** Server settings. */
server_settings *s_settings = NULL;

/**
 * Initialize the server settings from the srv file. */
void server_settings_init(void)
{
    FILE *fp;
    char buf[HUGE_BUF * 4], *cp;
    int line = 0;
    char_struct *cur_char = NULL;
    size_t text_id = 0, i;

    fp = server_file_open_name(SERVER_FILE_SETTINGS);

    if (!fp) {
        return;
    }

    server_settings_deinit();
    s_settings = ecalloc(1, sizeof(server_settings));

    while (fgets(buf, sizeof(buf) - 1, fp)) {
        line++;

        if (*buf == '#') {
            continue;
        }

        cp = strrchr(buf, '\n');

        /* Eliminate newline. */
        if (cp) {
            *cp = '\0';
        }

        if (*buf == '\0') {
            continue;
        }

        /* Parse the command. Unknown commands will be silently ignored. */
        if (!strncmp(buf, "char ", 5)) {
            s_settings->characters = memory_reallocz(s_settings->characters, sizeof(*s_settings->characters) * s_settings->num_characters, sizeof(*s_settings->characters) * (s_settings->num_characters + 1));
            cur_char = &s_settings->characters[s_settings->num_characters];
            cur_char->name = estrdup(buf + 5);
        }
        else if (!strncmp(buf, "gender ", 7)) {
            char gender[MAX_BUF], arch[MAX_BUF], face[MAX_BUF];
            int gender_id;

            if (sscanf(buf + 7, "%s %s %s", gender, arch, face) == 3) {
                gender_id = gender_to_id(gender);
                cur_char->gender_archetypes[gender_id] = estrdup(arch);
                cur_char->gender_faces[gender_id] = estrdup(face);
            }
        }
        else if (!strncmp(buf, "desc ", 5)) {
            cur_char->desc = estrdup(buf + 5);
        }
        else if (!strcmp(buf, "end")) {
            s_settings->num_characters++;
        }
        else if (!strncmp(buf, "level ", 6)) {
            uint32 lev;

            s_settings->max_level = atoi(buf + 6);
            s_settings->level_exp = malloc(sizeof(*s_settings->level_exp) * (s_settings->max_level + 2));

            for (lev = 0; lev <= s_settings->max_level; lev++) {
                if (!fgets(buf, sizeof(buf) - 1, fp)) {
                    break;
                }

                s_settings->level_exp[lev] = strtoull(buf, NULL, 16);
            }

            s_settings->level_exp[lev] = 0;
        }
        else if (!strncmp(buf, "text ", 5)) {
            if (text_id < SERVER_TEXT_MAX) {
                size_t j = 0;

                s_settings->text[text_id] = estrdup(buf + 5);
                string_newline_to_literal(s_settings->text[text_id]);

                if (text_id == SERVER_TEXT_PROTECTION_GROUPS) {
                    cp = strtok(s_settings->text[text_id], " ");

                    while (cp) {
                        snprintf(s_settings->protection_groups[j],
                                sizeof(s_settings->protection_groups[j]), "%s",
                                cp);
                        j++;
                        cp = strtok(NULL, " ");
                    }
                }
                else if (text_id == SERVER_TEXT_PROTECTION_LETTERS) {
                    cp = strtok(s_settings->text[text_id], " ");

                    while (cp) {
                        strncpy(s_settings->protection_letters[j], cp, sizeof(*s_settings->protection_letters) - 1);
                        s_settings->protection_letters[j][sizeof(*s_settings->protection_letters) - 1] = '\0';
                        j++;
                        cp = strtok(NULL, " ");
                    }
                }
                else if (text_id == SERVER_TEXT_PROTECTION_FULL) {
                    cp = strtok(s_settings->text[text_id], " ");

                    while (cp) {
                        strncpy(s_settings->protection_full[j], cp, sizeof(*s_settings->protection_full) - 1);
                        s_settings->protection_full[j][sizeof(*s_settings->protection_full) - 1] = '\0';
                        j++;
                        cp = strtok(NULL, " ");
                    }
                }
                else if (text_id == SERVER_TEXT_SPELL_PATHS) {
                    cp = strtok(s_settings->text[text_id], " ");

                    while (cp) {
                        strncpy(s_settings->spell_paths[j], cp, sizeof(*s_settings->spell_paths) - 1);
                        s_settings->spell_paths[j][sizeof(*s_settings->spell_paths) - 1] = '\0';
                        j++;
                        cp = strtok(NULL, " ");
                    }
                }

                text_id++;
            }
            else {
                logger_print(LOG(BUG), "Error in settings file, more text entries than allowed on line %d.", line);
            }
        }
    }

    for (i = text_id; i < SERVER_TEXT_MAX; i++) {
        s_settings->text[i] = estrdup("???");
    }

    fclose(fp);
}

/**
 * Deinitialize the server settings. */
void server_settings_deinit(void)
{
    size_t i, gender;

    if (!s_settings) {
        return;
    }

    efree(s_settings->level_exp);

    for (i = 0; i < s_settings->num_characters; i++) {
        efree(s_settings->characters[i].name);
        efree(s_settings->characters[i].desc);

        for (gender = 0; gender < GENDER_MAX; gender++) {
            if (s_settings->characters[i].gender_archetypes[gender]) {
                efree(s_settings->characters[i].gender_archetypes[gender]);
                efree(s_settings->characters[i].gender_faces[gender]);
            }
        }
    }

    for (i = 0; i < SERVER_TEXT_MAX; i++) {
        efree(s_settings->text[i]);
    }

    efree(s_settings->characters);
    efree(s_settings);
    s_settings = NULL;
}
