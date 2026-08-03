/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Banning related functions.
 *
 * It is possible to use the /ban operator command to ban a specified player
 * or account from the game.
 *
 * Syntax for banning is: "player [account]", where player is the name of the
 * player to ban (or * to match any player) and account is the account name to
 * match (or * to match any account).
 *
 * @author Zoey Rose
 */

#ifndef __CPROTO__

#include <global.h>
#include <toolkit/string.h>
#include <ban.h>

/**
 * The ban structure.
 */
typedef struct ban {
    shstr *name; ///< Name of the banned player. Can be NULL.

    char *account; ///< Name of the banned account. Can be NULL.

    bool removed : 1; ///< If true, the ban entry is no longer valid.
} ban_t;

/**
 * Array of all the bans.
 */
static ban_t *bans = NULL;

/**
 * Number of bans.
 */
static size_t bans_num = 0;

/* Prototypes */
static void ban_save(void);
static void ban_free(void);
static void ban_entry_free(ban_t *ban);
static const char *ban_entry_save(const ban_t *ban, char *buf, size_t len);

TOOLKIT_API(IMPORTS(shstr), IMPORTS(string));

TOOLKIT_INIT_FUNC(ban) {
    char filename[HUGE_BUF];
    snprintf(VS(filename), "%s/" BANFILE, settings.datapath);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return;
    }

    char buf[HUGE_BUF];
    while (fgets(VS(buf), fp)) {
        /* Skip comments and blank lines. */
        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }

        char *end = strchr(buf, '\n');
        if (end != NULL) {
            *end = '\0';
        }

        ban_error_t rc = ban_add(buf);
        if (rc != BAN_OK) {
            LOG(ERROR, "Malformed line in bans file: %s, %s", buf, ban_strerror(rc));
        }
    }

    fclose(fp);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(ban) {
    ban_save();
    ban_free();
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Saves all the bans to the bans file.
 */
static void ban_save(void) {
    char filename[HUGE_BUF];
    snprintf(VS(filename), "%s/" BANFILE, settings.datapath);

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        LOG(ERROR, "Cannot open %s for writing.", filename);
        return;
    }

    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        char buf[HUGE_BUF];
        if (ban_entry_save(ban, VS(buf)) == NULL) {
            continue;
        }

        fprintf(fp, "%s", buf);
    }

    fclose(fp);
}

/**
 * Frees all the bans.
 */
static void ban_free(void) {
    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (!ban->removed) {
            ban_entry_free(ban);
        }
    }

    if (bans != NULL) {
        efree(bans);
        bans = NULL;
    }

    bans_num = 0;
}

/**
 * Creates a new ban entry.
 * @param name
 * Player name. Can be NULL.
 * @param account
 * Account name. Can be NULL.
 */
static void ban_entry_new(const char *name, const char *account) {
    bans = erealloc(bans, sizeof(*bans) * (bans_num + 1));
    ban_t *ban = &bans[bans_num];
    bans_num++;

    ban->name = strcmp(name, "*") == 0 ? NULL : add_string(name);
    ban->account = strcmp(account, "*") == 0 ? NULL : estrdup(account);
    ban->removed = false;
}

/**
 * Find an existing ban entry.
 * @param name
 * Player name, * for any match.
 * @param account
 * Account name, * for any match.
 * @return
 * Pointer to the found ban structure, NULL otherwise.
 */
static ban_t *ban_entry_find(const char *name, const char *account) {
    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        if (strcmp(name, "*") != 0 && (ban->name == NULL || strcmp(ban->name, name) != 0)) {
            continue;
        }

        if (strcmp(account, "*") != 0 &&
            (ban->account == NULL || strcmp(ban->account, account) != 0)) {
            continue;
        }

        return ban;
    }

    return NULL;
}

/**
 * Frees the specified ban entry.
 * @param ban
 * Ban entry to free.
 */
static void ban_entry_free(ban_t *ban) {
    if (ban->name != NULL) {
        free_string_shared(ban->name);
    }

    if (ban->account != NULL) {
        efree(ban->account);
    }

    ban->removed = true;
}

/**
 * Create a text representation of a single ban entry.
 * @param ban
 * Pointer to the ban entry.
 * @param buf
 * Where to store the text representation.
 * @param len
 * Size of 'buf'.
 * @return
 * 'buf' on success, NULL on failure.
 */
static const char *ban_entry_save(const ban_t *ban, char *buf, size_t len) {
    if (ban->name == NULL) {
        snprintf(buf, len, "*");
    } else {
        snprintf(buf, len, "\"%s\"", ban->name);
    }

    snprintfcat(buf, len, " %s\n", ban->account != NULL ? ban->account : "*");
    return buf;
}

/**
 * Parse ban command parameters into supported components.
 * @param str
 * The command parameters.
 * @param[out] name Where to store player name.
 * @param name_len
 * Size of 'name'.
 * @param[out] account Where to store account name.
 * @param account_len
 * Size of 'account'.
 * @return
 * #BAN_OK on success, one of the errors defined in #ban_error_t on
 * failure.
 */
static ban_error_t
ban_parse(const char *str, char *name, size_t name_len, char *account, size_t account_len) {
    size_t pos = 0;
    if (!string_get_word(str, &pos, ' ', name, name_len, '"')) {
        return BAN_BADSYNTAX;
    }

    if (!string_get_word(str, &pos, ' ', account, account_len, 0)) {
        snprintf(account, account_len, "*");
        return BAN_OK;
    }

    char extra[MAX_BUF];
    if (string_get_word(str, &pos, ' ', VS(extra), 0)) {
        return BAN_BADSYNTAX;
    }

    return BAN_OK;
}

/**
 * Adds a new ban.
 * @param str
 * Ban command parameters.
 * @return
 * #BAN_OK on success, one of the errors defined in #ban_error_t on
 * failure.
 */
ban_error_t ban_add(const char *str) {
    HARD_ASSERT(str != NULL);

    char name[MAX_BUF], account[MAX_BUF];
    ban_error_t rc = ban_parse(str, VS(name), VS(account));
    if (rc != BAN_OK) {
        return rc;
    }

    ban_t *ban = ban_entry_find(name, account);
    if (ban != NULL) {
        return BAN_EXIST;
    }

    ban_entry_new(name, account);
    ban_save();
    return BAN_OK;
}

/**
 * Remove an existing ban.
 * @param str
 * Ban command parameters.
 * @return
 * #BAN_OK on success, one of the errors defined in #ban_error_t on
 * failure.
 */
ban_error_t ban_remove(const char *str) {
    HARD_ASSERT(str != NULL);

    ban_t *ban;
    if (string_startswith(str, "#")) {
        if (string_isempty(str + 1)) {
            return BAN_BADID;
        }

        unsigned long value = strtoul(str + 1, NULL, 10);
        if (value == 0 || value > bans_num) {
            return BAN_BADID;
        }

        ban = &bans[value - 1];
        if (ban->removed) {
            return BAN_REMOVED;
        }
    } else {
        char name[MAX_BUF], account[MAX_BUF];
        ban_error_t rc = ban_parse(str, VS(name), VS(account));
        if (rc != BAN_OK) {
            return rc;
        }

        ban = ban_entry_find(name, account);
        if (ban == NULL) {
            return BAN_NOTEXIST;
        }
    }

    ban_entry_free(ban);
    ban_save();
    return BAN_OK;
}

/**
 * Checks whether a player or account is banned from the game.
 * @param name
 * Player name to check. Can be NULL.
 * @param account
 * Account name to check. Can be NULL.
 * @return
 * True if the connection is banned, false otherwise.
 */
bool ban_check(const char *name, const char *account) {
    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        if (ban->name != NULL && (name == NULL || strcmp(ban->name, name) != 0)) {
            continue;
        }

        if (ban->account != NULL && (account == NULL || strcmp(ban->account, account) != 0)) {
            continue;
        }

        return true;
    }

    return false;
}

/**
 * List existing bans to the specified player.
 * @param op
 * Player.
 */
void ban_list(object *op) {
    draw_info(COLOR_WHITE, op, "List of bans:");

    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        char buf[HUGE_BUF];
        if (ban_entry_save(ban, VS(buf)) == NULL) {
            continue;
        }

        draw_info_format(COLOR_WHITE, op, "#%" PRIuMAX ": %s", (uintmax_t)i, buf);
    }
}

/**
 * Removes all existing bans.
 */
void ban_reset(void) {
    ban_free();
    ban_save();
}

/**
 * Create string representation of an error code.
 * @param errnum
 * Error code. Cannot be #BAN_OK.
 * @return
 * String representation, never NULL.
 */
const char *ban_strerror(ban_error_t errnum) {
    SOFT_ASSERT_RC(errnum >= BAN_OK && errnum < BAN_MAX,
                   "unknown error",
                   "Invalid error number: %d",
                   errnum);

    switch (errnum) {
        case BAN_OK:
            return "success";

        case BAN_EXIST:
            return "specified ban already exists";

        case BAN_NOTEXIST:
            return "no such ban entry";

        case BAN_REMOVED:
            return "ban entry has been removed already";

        case BAN_BADID:
            return "invalid ban ID";

        case BAN_BADSYNTAX:
            return "invalid syntax";

        default:
            break;
    }

    return "<unreachable>";
}

#endif
