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
 * Handles command aliases system.
 *
 * @author Alex Tokar */

#include <global.h>
#include <toolkit_string.h>

/**
 * One command alias. */
typedef struct cmd_alias_struct {
    /**
     * Name of the command alias. */
    char *name;

    /**
     * What to execute when there is an argument for the command. */
    char *arg;

    /**
     * What to execute when there isn't an argument for the command, or
     * if there is but cmd_alias_struct::arg is not set. */
    char *noarg;

    /**
     * Hash handle. */
    UT_hash_handle hh;
} cmd_alias_struct;

/**
 * All the possible command aliases. */
static cmd_alias_struct *cmd_aliases = NULL;

/**
 * Load command aliases file.
 * @param path Where to load the file from. */
static void cmd_aliases_load(const char *path)
{
    FILE *fp = fopen_wrapper(path, "r");
    if (fp == NULL) {
        return;
    }

    cmd_alias_struct *cmd_alias = NULL;

    char buf[HUGE_BUF];
    uint64_t linenum = 0;

    while (fgets(VS(buf), fp)) {
        linenum++;

        if (*buf == '#' || *buf == '\n') {
            continue;
        }

        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        const char *key = cp, *value = NULL, *error_str;

        if (string_startswith(cp, "[") && string_endswith(cp, "]")) {
            if (cmd_alias != NULL) {
                HASH_ADD_KEYPTR(hh, cmd_aliases, cmd_alias->name,
                        strlen(cmd_alias->name), cmd_alias);
            }

            cmd_alias = ecalloc(1, sizeof(*cmd_alias));
            cmd_alias->name = string_sub(cp, 1, -1);

            if (string_isempty(cmd_alias->name)) {
                error_str = "empty command alias name";
                goto error;
            }

            continue;
        }

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), '=') != 2) {
            error_str = "malformed line";
            goto error;
        }

        string_whitespace_trim(cps[0]);
        string_whitespace_trim(cps[1]);
        key = cps[0];
        value = cps[1];

        if (string_isempty(key)) {
            error_str = "empty key";
            goto error;
        } else if (string_isempty(value)) {
            error_str = "empty value";
            goto error;
        }

        if (cmd_alias == NULL) {
            error_str = "expected command alias definition";
            goto error;
        } else if (strcmp(key, "arg") == 0) {
            cmd_alias->arg = estrdup(value);
        } else if (strcmp(key, "noarg") == 0) {
            cmd_alias->noarg = estrdup(value);
        } else {
            error_str = "unknown attribute";
            goto error;
        }

        continue;

error:
        LOG(ERROR, "Error parsing %s, line %" PRIu64 ", %s: %s%s%s", path,
                linenum, error_str, key, value != NULL ? " = " : "",
                value != NULL ? value : "");
        exit(1);
    }

    if (cmd_alias != NULL) {
        HASH_ADD_KEYPTR(hh, cmd_aliases, cmd_alias->name,
                strlen(cmd_alias->name), cmd_alias);
    }

    fclose(fp);
}

/**
 * Initialize the command aliases system. */
void cmd_aliases_init(void)
{
    cmd_aliases_load("data/cmd_aliases.cfg");
    cmd_aliases_load("settings/cmd_aliases.cfg");
}

/**
 * Deinitialize the command aliases system. */
void cmd_aliases_deinit(void)
{
    cmd_alias_struct *curr, *tmp;

    HASH_ITER(hh, cmd_aliases, curr, tmp)
    {
        HASH_DEL(cmd_aliases, curr);

        efree(curr->name);

        if (curr->arg) {
            efree(curr->arg);
        }

        if (curr->noarg) {
            efree(curr->noarg);
        }

        efree(curr);
    }
}

/**
 * Execute the specified command alias.
 * @param cmd What to execute.
 * @param params Parameters passed by the player. NULL if none. */
static void cmd_aliases_execute(const char *cmd, const char *params)
{
    char word[MAX_BUF], *cp, *func_end;
    StringBuffer *sb;
    size_t pos;

    pos = 0;
    sb = stringbuffer_new();

    while (string_get_word(cmd, &pos, ' ', word, sizeof(word), 0)) {
        if (stringbuffer_length(sb)) {
            stringbuffer_append_string(sb, " ");
        }

        func_end = strchr(word, '>');

        if (string_startswith(word, "<") && func_end) {
            char *func, *cps[2];

            func = string_sub(word, 1, func_end - word);

            if (string_split(func, cps, arraysize(cps), ':') == 2) {
                if (strcmp(cps[0], "get") == 0) {
                    char *str, *cps2[2];

                    if (string_split(cps[1], cps2, arraysize(cps2), ';') < 1) {
                        continue;
                    }

                    if (strcmp(cps2[0], "arg") == 0) {
                        str = estrdup(params ? params : "");
                    } else if (strcmp(cps2[0], "mplayer") == 0) {
                        if (sound_map_background(-1) && sound_playing_music()) {
                            str = estrdup(sound_get_bg_music_basename());
                        } else {
                            str = estrdup("nothing");
                        }
                    } else {
                        str = estrdup("???");
                    }

                    if (cps2[1]) {
                        if (strcmp(cps2[1], "upper") == 0) {
                            string_toupper(str);
                        } else if (strcmp(cps2[1], "lower") == 0) {
                            string_tolower(str);
                        } else if (strcmp(cps2[1], "capitalize") == 0) {
                            string_capitalize(str);
                        } else if (strcmp(cps2[1], "titlecase") == 0) {
                            string_title(str);
                        }
                    }

                    stringbuffer_append_string(sb, str);
                    efree(str);
                } else if (strcmp(cps[0], "gender") == 0) {
                    if (strcmp(cps[1], "possessive") == 0) {
                        stringbuffer_append_string(sb, gender_possessive[cpl.gender]);
                    } else if (strcmp(cps[1], "reflexive") == 0) {
                        stringbuffer_append_string(sb, gender_reflexive[cpl.gender]);
                    } else if (strcmp(cps[1], "subjective") == 0) {
                        stringbuffer_append_string(sb, gender_subjective[cpl.gender]);
                    }
                } else if (strcmp(cps[0], "choice") == 0) {
                    UT_array *strs;
                    char *s, **p;
                    size_t idx;

                    utarray_new(strs, &ut_str_icd);

                    s = strtok(cps[1], ",");

                    while (s) {
                        utarray_push_back(strs, &s);
                        s = strtok(NULL, ",");
                    }

                    idx = rndm(1, utarray_len(strs)) - 1;
                    p = (char **) utarray_eltptr(strs, idx);

                    if (p) {
                        stringbuffer_append_string(sb, *p);
                    }

                    utarray_free(strs);
                } else if (strcmp(cps[0], "rndm") == 0) {
                    int min, max;

                    if (sscanf(cps[1], "%d-%d", &min, &max) == 2) {
                        stringbuffer_append_printf(sb, "%d", rndm(min, max));
                    }
                }
            }

            efree(func);

            stringbuffer_append_string(sb, func_end + 1);
        } else {
            stringbuffer_append_string(sb, word);
        }
    }

    cp = stringbuffer_finish(sb);
    send_command(cp);
    efree(cp);
}

/**
 * Try to handle player's command.
 * @param cmd Command to handle.
 * @return 1 if it was handled, 0 otherwise. */
int cmd_aliases_handle(const char *cmd)
{
    if (cmd[0] == '/' && cmd[1] != '\0') {
        char *cp;
        size_t cmd_len;
        const char *params;
        cmd_alias_struct *cmd_alias;

        cmd++;
        cp = strchr(cmd, ' ');

        if (cp) {
            cmd_len = cp - cmd;
            params = cp + 1;

            if (*params == '\0') {
                params = NULL;
            }
        } else {
            cmd_len = strlen(cmd);
            params = NULL;
        }

        HASH_FIND(hh, cmd_aliases, cmd, cmd_len, cmd_alias);

        if (cmd_alias) {
            if (params && cmd_alias->arg) {
                cmd_aliases_execute(cmd_alias->arg, params);
            } else if (cmd_alias->noarg) {
                cmd_aliases_execute(cmd_alias->noarg, params);
            }

            return 1;
        }
    }

    return 0;
}
