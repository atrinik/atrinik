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
 * Banning related functions.
 *
 * It is possible to use the /ban operator command to ban a specified player,
 * account or IP from the game.
 *
 * Syntax for banning is: "player account[ address/plen]", where player is name
 * of the player to ban (or * to match any player), account name to match (or *
 * to match any account), address is an IP address (IPv4, or IPv6 if the server
 * supports it) and plen is the subnet to match.
 *
 * @author Alex Tokar
 */

#ifndef __CPROTO__

#include <global.h>
#include <toolkit_string.h>
#include <ban.h>

/**
 * The ban structure.
 */
typedef struct ban {
    shstr *name; ///< Name of the banned player. Can be NULL.

    shstr *account; ///< Name of the banned account. Can be NULL.

    struct sockaddr_storage addr; ///< IP address.

    unsigned short plen; ///< Prefix length (the subnet).

    bool removed:1; ///< If true, the ban entry is no longer valid.
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

TOOLKIT_API(DEPENDS(socket), IMPORTS(shstr), IMPORTS(string));

TOOLKIT_INIT_FUNC(ban)
{
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
            LOG(ERROR, "Malformed line in bans file: %s, %s", buf,
                    ban_strerror(rc));
        }
    }

    fclose(fp);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(ban)
{
    ban_save();
    ban_free();
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Saves all the bans to the bans file.
 */
static void ban_save(void)
{
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

        char address[MAX_BUF];
        if (ban->plen != 0) {
            if (!socket_addr2host(&ban->addr, VS(address))) {
                LOG(ERROR, "Failed to convert banned IP address");
                continue;
            }

            snprintfcat(VS(address), "/%u", ban->plen);
        } else {
            address[0] = '\0';
        }

        fprintf(fp, "%s %s %s\n", ban->name != NULL ? ban->name : "*",
                ban->account != NULL ? ban->account : "*", address);
    }

    fclose(fp);
}

static void ban_free(void)
{
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

static void ban_entry_new(const char *name, const char *account,
        const struct sockaddr_storage *addr, unsigned short plen)
{
    bans = erealloc(bans, sizeof(*bans) * (bans_num + 1));
    ban_t *ban = &bans[bans_num];
    bans_num++;

    ban->name = strcmp(name, "*") == 0 ? NULL : add_string(name);
    ban->account = strcmp(account, "*") == 0 ? NULL : add_string(account);
    memcpy(&ban->addr, addr, sizeof(ban->addr));
    ban->plen = plen;
    ban->removed = false;
}

static ban_t *ban_entry_find(const char *name, const char *account,
        const struct sockaddr_storage *addr, unsigned short plen)
{
    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        if (ban->name != NULL && strcmp(name, "*") != 0 &&
                strcmp(ban->name, name) != 0) {
            continue;
        }

        if (ban->account != NULL && strcmp(account, "*") != 0 &&
                strcmp(ban->account, account) != 0) {
            continue;
        }

        if (memcmp(&ban->addr, addr, sizeof(ban->addr)) != 0) {
            continue;
        }

        if (ban->plen != plen) {
            continue;
        }

        return ban;
    }

    return NULL;
}

static void ban_entry_free(ban_t *ban)
{
    if (ban->name != NULL) {
        free_string_shared(ban->name);
    }

    if (ban->account != NULL) {
        free_string_shared(ban->account);
    }

    ban->removed = true;
}

static ban_error_t ban_parse(const char *str, char *name, size_t name_len,
        char *account, size_t account_len, struct sockaddr_storage *addr,
        unsigned short *plen)
{
    size_t pos = 0;
    if (!string_get_word(str, &pos, ' ', name, name_len, '"')) {
        return BAN_BADSYNTAX;
    }

    memset(addr, 0, sizeof(*addr));
    *plen = 0;

    if (!string_get_word(str, &pos, ' ', account, account_len, 0)) {
        snprintf(account, account_len, "*");
        return BAN_OK;
    }

    char address[MAX_BUF];
    if (!string_get_word(str, &pos, '/', VS(address), 0)) {
        return BAN_OK;
    }

    if (!socket_host2addr(address, addr)) {
        return BAN_BADIP;
    }

    char subnet[MAX_BUF];
    unsigned short max_plen = socket_addr_plen(addr);
    if (string_get_word(str, &pos, ' ', VS(subnet), 0)) {
        int val = atoi(subnet);
        if (val <= 0 || val > max_plen) {
            return BAN_BADPLEN;
        }

        *plen = (unsigned short) val;
    } else {
        *plen = max_plen;
    }

    return BAN_OK;
}

ban_error_t ban_add(const char *str)
{
    HARD_ASSERT(str != NULL);

    char name[MAX_BUF], account[MAX_BUF];
    struct sockaddr_storage addr;
    unsigned short plen;
    ban_error_t rc = ban_parse(str, VS(name), VS(account), &addr, &plen);
    if (rc != BAN_OK) {
        return rc;
    }

    ban_t *ban = ban_entry_find(name, account, &addr, plen);
    if (ban != NULL) {
        return BAN_EXIST;
    }

    ban_entry_new(name, account, &addr, plen);
    ban_save();
    return BAN_OK;
}

ban_error_t ban_remove(const char *str)
{
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
        struct sockaddr_storage addr;
        unsigned short plen;
        ban_error_t rc = ban_parse(str, VS(name), VS(account), &addr, &plen);
        if (rc != BAN_OK) {
            return rc;
        }

        ban = ban_entry_find(name, account, &addr, plen);
        if (ban == NULL) {
            return BAN_NOTEXIST;
        }
    }

    ban_entry_free(ban);
    ban_save();
    return BAN_OK;
}

bool ban_check(socket_struct *ns, const char *name)
{
    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        bool got_one = false;

        if (name != NULL && ban->name != NULL &&
                !(got_one = (strcmp(ban->name, name) == 0))) {
            continue;
        }

        if (ns->account != NULL && ban->account != NULL &&
                !(got_one = (ban->account == ns->account))) {
            continue;
        }

        if (ban->plen != 0 && !(got_one =
                (socket_cmp_addr(ns->sc, &ban->addr, ban->plen) == 0))) {

            continue;
        }

        return got_one;
    }

    return false;
}

void ban_list(object *op)
{
    draw_info(COLOR_WHITE, op, "List of bans:");

    for (size_t i = 0; i < bans_num; i++) {
        ban_t *ban = &bans[i];
        if (ban->removed) {
            continue;
        }

        char address[MAX_BUF];
        if (ban->plen != 0) {
            if (!socket_addr2host(&ban->addr, VS(address))) {
                LOG(ERROR, "Failed to convert banned IP address");
                continue;
            }

            snprintfcat(VS(address), "/%u", ban->plen);
        } else {
            address[0] = '\0';
        }

        draw_info_format(COLOR_WHITE, op, "#%" PRIuMAX ": %s %s %s",
                (uintmax_t) i,
                ban->name != NULL ? ban->name : "*",
                ban->account != NULL ? ban->account : "*",
                address);
    }
}

void ban_reset(void)
{
    ban_free();
    ban_save();
}

const char *ban_strerror(ban_error_t errnum)
{
    SOFT_ASSERT_RC(errnum > 0 && errnum < BAN_MAX, "unknown error",
            "Invalid error number: %d", errnum);

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

    case BAN_BADIP:
        return "IP address in invalid format";

    case BAN_BADPLEN:
        return "invalid subnet";

    case BAN_BADSYNTAX:
        return "invalid syntax";

    case BAN_MAX:
        break;
    }

    return "<unreachable>";
}

#endif
