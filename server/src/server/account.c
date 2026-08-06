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
 * Account system.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <server_main.h>
#include <server.h>
#include <region.h>
#include <initialization.h>
#include <account.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <arch.h>
#include <player.h>
#include <toolkit/path.h>
#include <toolkit/datetime.h>
#include <toolkit/password.h>

#include <openssl/crypto.h>

#define ACCOUNT_CHARACTERS_LIMIT 16
#define ACCOUNT_LEGACY_PASSWORD_SIZE 32
#define ACCOUNT_AUTH_WORK_BURST 8
#define ACCOUNT_AUTH_WORK_REFILL_SECONDS 2

static time_t account_auth_work_refill_time;
static unsigned int account_auth_work_tokens;

typedef struct account_struct {
    char password_record[PASSWORD_RECORD_SIZE];

    unsigned char legacy_password[ACCOUNT_LEGACY_PASSWORD_SIZE];

    unsigned char legacy_salt[ACCOUNT_LEGACY_PASSWORD_SIZE];

    bool has_legacy_pbkdf2;
    bool has_legacy_salt;

    char *password_old;

    char *last_connection_id;

    time_t last_time;

    struct {
        archetype_t *at;

        char *name;

        char *region_name;

        uint8_t level;
    } *characters;

    size_t characters_num;
} account_struct;

void account_init(void) {
    account_auth_work_refill_time = 0;
    account_auth_work_tokens = ACCOUNT_AUTH_WORK_BURST;
}

void account_deinit(void) {}

static bool account_auth_work_allowed(unsigned int cost) {
    time_t now = datetime_getutc();

    if (account_auth_work_refill_time == 0 || now < account_auth_work_refill_time) {
        account_auth_work_refill_time = now;
        account_auth_work_tokens = ACCOUNT_AUTH_WORK_BURST;
    } else {
        time_t elapsed = now - account_auth_work_refill_time;
        time_t refill = elapsed / ACCOUNT_AUTH_WORK_REFILL_SECONDS;
        if (refill >= ACCOUNT_AUTH_WORK_BURST) {
            account_auth_work_tokens = ACCOUNT_AUTH_WORK_BURST;
            account_auth_work_refill_time = now;
        } else if (refill > 0) {
            account_auth_work_tokens += (unsigned int)refill;
            account_auth_work_refill_time += refill * ACCOUNT_AUTH_WORK_REFILL_SECONDS;
        }
    }

    if (cost > account_auth_work_tokens) {
        return false;
    }

    account_auth_work_tokens -= cost;
    return true;
}

static void account_free(account_struct *account) {
    size_t i;

    free(account->last_connection_id);

    free(account->password_old);

    for (i = 0; i < account->characters_num; i++) {
        free(account->characters[i].name);
        free(account->characters[i].region_name);
    }

    free(account->characters);
    OPENSSL_cleanse(account->password_record, sizeof(account->password_record));
    OPENSSL_cleanse(account->legacy_password, sizeof(account->legacy_password));
    OPENSSL_cleanse(account->legacy_salt, sizeof(account->legacy_salt));
}

static char *account_old_crypt(char *str, const char *salt) {
#if defined(HAVE_CRYPT) && defined(HAVE_CRYPT_H)
    return crypt(str, salt);
#else
    return NULL;
#endif
}

static bool account_set_password(account_struct *account, const char *password) {
    if (!password_record_create(password, account->password_record)) {
        LOG(ERROR, "Failed to create Argon2id account password record");
        return false;
    }

    OPENSSL_cleanse(account->legacy_password, sizeof(account->legacy_password));
    OPENSSL_cleanse(account->legacy_salt, sizeof(account->legacy_salt));
    account->has_legacy_pbkdf2 = false;
    account->has_legacy_salt = false;
    free(account->password_old);
    account->password_old = NULL;
    return true;
}

static password_verify_result_t account_check_password(account_struct *account,
                                                       const char *password) {
    if (account->password_old) {
        const char *calculated = account_old_crypt((char *)password, account->password_old);
        size_t expected_length = strlen(account->password_old);
        return calculated != NULL && strlen(calculated) == expected_length &&
                       CRYPTO_memcmp(calculated, account->password_old, expected_length) == 0
                   ? PASSWORD_VERIFY_MATCH
                   : PASSWORD_VERIFY_MISMATCH;
    }

    if (account->has_legacy_pbkdf2 && account->has_legacy_salt) {
        return password_legacy_pbkdf2_verify(password,
                                             account->legacy_salt,
                                             account->legacy_password);
    }

    return password_record_verify(password, account->password_record);
}

static int account_save(account_struct *account, const char *path) {
    StringBuffer *buffer;
    char *contents;
    size_t i;

    if (!password_record_is_valid(account->password_record)) {
        LOG(BUG, "Refusing to save account with an invalid password record: %s", path);
        return 0;
    }

    buffer = stringbuffer_new();
    stringbuffer_append_printf(buffer, "password %s\n", account->password_record);
    stringbuffer_append_printf(buffer, "connection %s\n", account->last_connection_id);
    stringbuffer_append_printf(buffer, "time %" PRIu64 "\n", (uint64_t)account->last_time);

    for (i = 0; i < account->characters_num; i++) {
        stringbuffer_append_printf(buffer,
                                   "char %s:%s:%s:%d\n",
                                   account->characters[i].at->name,
                                   account->characters[i].name,
                                   account->characters[i].region_name,
                                   account->characters[i].level);
    }

    contents = stringbuffer_finish(buffer);
    bool ok = path_write_atomic(path, contents, strlen(contents), 0600);
    OPENSSL_cleanse(contents, strlen(contents));
    free(contents);
    if (!ok) {
        LOG(BUG, "Could not atomically replace account file: %s", path);
        return 0;
    }

    return 1;
}

static int account_load(account_struct *account, const char *path) {
    FILE *fp;
    char buf[MAX_BUF], *end;
    unsigned int credential_count = 0;

    fp = fopen(path, "rb");

    if (!fp) {
        LOG(BUG, "Could not open %s for reading.", path);
        return 0;
    }

    memset(account, 0, sizeof(*account));
    account->last_connection_id = xstrdup("");

    while (fgets(buf, sizeof(buf), fp)) {
        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        if (strncmp(buf, "password ", 9) == 0) {
            credential_count++;
            if (!password_record_is_valid(buf + 9)) {
                LOG(BUG, "Invalid password record in file: %s", path);
            } else {
                snprintf(VS(account->password_record), "%s", buf + 9);
            }
        } else if (strncmp(buf, "pswd ", 5) == 0) {
            size_t len;

            credential_count++;
            len = strlen(buf + 5);

            if (len == 13 || len == 40) {
                account->password_old = xstrdup(buf + 5);
            } else if (string_fromhex(buf + 5,
                                      len,
                                      account->legacy_password,
                                      ACCOUNT_LEGACY_PASSWORD_SIZE) !=
                       ACCOUNT_LEGACY_PASSWORD_SIZE) {
                LOG(BUG, "Invalid password entry in file: %s", path);
                OPENSSL_cleanse(account->legacy_password, sizeof(account->legacy_password));
            } else {
                account->has_legacy_pbkdf2 = true;
            }
        } else if (strncmp(buf, "salt ", 5) == 0) {
            if (string_fromhex(buf + 5,
                               strlen(buf + 5),
                               account->legacy_salt,
                               ACCOUNT_LEGACY_PASSWORD_SIZE) != ACCOUNT_LEGACY_PASSWORD_SIZE) {
                LOG(BUG, "Invalid salt entry in file: %s", path);
                OPENSSL_cleanse(account->legacy_salt, sizeof(account->legacy_salt));
                account->has_legacy_salt = false;
            } else {
                account->has_legacy_salt = true;
            }
        } else if (strncmp(buf, "connection ", 11) == 0) {
            free(account->last_connection_id);
            account->last_connection_id = xstrdup(buf + 11);
        } else if (strncmp(buf, "time ", 5) == 0) {
            account->last_time = atoll(buf + 5);
        } else if (strncmp(buf, "char ", 5) == 0) {
            char *cps[4];

            if (string_split(buf + 5, cps, arraysize(cps), ':') != arraysize(cps)) {
                LOG(BUG, "Invalid character entry in file: %s", path);
                continue;
            }

            account->characters = xreallocarray(account->characters,
                                                account->characters_num + 1,
                                                sizeof(*account->characters));
            account->characters[account->characters_num].at = arch_find(cps[0]);
            account->characters[account->characters_num].name = xstrdup(cps[1]);
            account->characters[account->characters_num].region_name = xstrdup(cps[2]);
            account->characters[account->characters_num].level = atoi(cps[3]);
            account->characters_num++;
        }
    }

    fclose(fp);

    if (credential_count != 1 ||
        (account->password_record[0] == '\0' && account->password_old == NULL &&
         !(account->has_legacy_pbkdf2 && account->has_legacy_salt))) {
        LOG(BUG, "Account file has no valid password record: %s", path);
        account_free(account);
        return 0;
    }

    return 1;
}

static void account_send_characters(socket_struct *ns, account_struct *account) {
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_CHARACTERS, 64, 64);

    if (account) {
        size_t i;

        packet_debug_data(packet, 0, "Account name");
        packet_writer_write_cstring(packet, ns->account);
        packet_debug_data(packet, 0, "Connection ID");
        packet_writer_write_cstring(packet, socket_get_id(ns->sc));
        packet_debug_data(packet, 0, "Previous connection ID");
        packet_writer_write_cstring(packet, account->last_connection_id);
        packet_debug_data(packet, 0, "Last time");
        packet_writer_write_uint64(packet, account->last_time);

        for (i = 0; i < account->characters_num; i++) {
            packet_debug(packet, 0, "Character #%" PRIu64 ":\n", (uint64_t)i);
            packet_debug_data(packet, 1, "Archname");
            packet_writer_write_cstring(packet, account->characters[i].at->name);
            packet_debug_data(packet, 1, "Name");
            packet_writer_write_cstring(packet, account->characters[i].name);
            packet_debug_data(packet, 1, "Region name");
            packet_writer_write_cstring(packet, account->characters[i].region_name);
            packet_debug_data(packet, 1, "Animation ID");
            packet_writer_write_uint16(packet, account->characters[i].at->clone.animation_id);
            packet_debug_data(packet, 1, "Level");
            packet_writer_write_uint8(packet, account->characters[i].level);
        }
    }

    socket_send_packet(ns, packet);
}

char *account_make_path(const char *name) {
    StringBuffer *sb;
    size_t i;
    char *cp;

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s/accounts/", settings.datapath);

    for (i = 0; i < settings.limits[ALLOWED_CHARS_ACCOUNT][0]; i++) {
        stringbuffer_append_string_len(sb, name, i + 1);
        stringbuffer_append_string(sb, "/");
    }

    stringbuffer_append_printf(sb, "%s.dat", name);
    cp = stringbuffer_finish(sb);

    return cp;
}

void account_login(socket_struct *ns, char *name, char *password) {
    account_struct account;
    char *path;

    if (ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*name == '\0' || *password == '\0' ||
        string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) ||
        string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid name and/or password.");
        account_send_characters(ns, NULL);
        return;
    }

    string_tolower(name);
    path = account_make_path(name);

    if (!path_exists(path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "No such account.");
        account_send_characters(ns, NULL);
        free(path);
        return;
    }

    if (!account_load(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Read error occurred, please contact server administrator.");
        account_send_characters(ns, NULL);
        free(path);
        return;
    }

    unsigned int auth_cost = account.password_old || account.has_legacy_pbkdf2 ||
                                     password_record_needs_rehash(account.password_record)
                                 ? 2
                                 : 1;
    if (!account_auth_work_allowed(auth_cost)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Authentication is temporarily busy; please retry shortly.");
        account_send_characters(ns, NULL);
        account_free(&account);
        free(path);
        return;
    }

    password_verify_result_t password_result = account_check_password(&account, password);
    if (password_result != PASSWORD_VERIFY_MATCH) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid password.");
        account_send_characters(ns, NULL);
        account_free(&account);
        free(path);

        ns->password_fails++;
        LOG(SYSTEM,
            "%s: Failed to provide correct password for account %s.",
            socket_get_id(ns->sc),
            name);

        if (ns->password_fails >= MAX_PASSWORD_FAILURES) {
            LOG(SYSTEM,
                "%s: Failed to provide a correct password for account %s too many times!",
                socket_get_id(ns->sc),
                name);
            draw_info_send(CHAT_TYPE_GAME,
                           NULL,
                           COLOR_RED,
                           ns,
                           "You have failed to provide a correct password too many times.");
            ns->state = ST_ZOMBIE;
        }

        return;
    }

    if ((account.password_old || account.has_legacy_pbkdf2 ||
         password_record_needs_rehash(account.password_record)) &&
        !account_set_password(&account, password)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Password upgrade failed, please contact server administrator.");
        account_free(&account);
        free(path);
        return;
    }

    ns->account = xstrdup(name);
    account_send_characters(ns, &account);

    free(account.last_connection_id);
    account.last_connection_id = xstrdup(socket_get_id(ns->sc));
    account.last_time = datetime_getutc();
    account_save(&account, path);
    account_free(&account);
    free(path);
}

void account_register(socket_struct *ns, char *name, char *password, char *password2) {
    size_t name_len, password_len;
    char *path;
    account_struct account;

    memset(&account, 0, sizeof(account));

    if (ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*name == '\0' || *password == '\0' || *password2 == '\0' ||
        string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) ||
        string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) ||
        string_contains_other(password2, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid name and/or password.");
        return;
    }

    name_len = strlen(name);
    password_len = strlen(password);

    /* Ensure the name/password lengths are within the allowed range.
     * No need to compare 'password2' length, as it needs to be the same
     * as 'password' anyway. */
    if (name_len < settings.limits[ALLOWED_CHARS_ACCOUNT][0] ||
        name_len > settings.limits[ALLOWED_CHARS_ACCOUNT][1] ||
        password_len < settings.limits[ALLOWED_CHARS_PASSWORD][0] ||
        password_len > settings.limits[ALLOWED_CHARS_PASSWORD][1]) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Invalid length for name and/or password.");
        return;
    }

    if (strcasecmp(name, ACCOUNT_TESTING_NAME) == 0) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Account name is reserved by the system.");
        return;
    }

    if (strcmp(password, password2) != 0) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "The passwords did not match.");
        return;
    }

    string_tolower(name);
    path = account_make_path(name);

    if (path_exists(path)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "That account name is already registered.");
        free(path);
        return;
    }

    if (!account_auth_work_allowed(1) || !account_set_password(&account, password)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Account creation failed, please contact server administrator.");
        account_free(&account);
        free(path);
        return;
    }
    path_ensure_directories(path);
    account.last_connection_id = xstrdup(socket_get_id(ns->sc));
    account.last_time = datetime_getutc();
    account.characters = NULL;
    account.characters_num = 0;

    if (!account_save(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Save error occurred, please contact server administrator.");
        account_free(&account);
        free(path);
        return;
    }

    ns->account = xstrdup(name);
    account_send_characters(ns, &account);
    account_free(&account);
    free(path);
}

void account_new_char(socket_struct *ns, char *name, char *archname) {
    archetype_t *at;
    char *path, *path_player;
    account_struct account;

    if (!ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*name == '\0' ||
        string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_CHARNAME])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid character name");
        return;
    }

    string_title(name);

    if (strcmp(name, PLAYER_TESTING_NAME1) == 0 || strcmp(name, PLAYER_TESTING_NAME2) == 0) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Character name is reserved by the system.");
        return;
    }

    if (player_exists(name)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Character with that name already exists.");
        return;
    }

    at = arch_find(archname);

    if (!at || at->clone.type != PLAYER) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid archname.");
        return;
    }

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Read error occurred, please contact server administrator.");
        free(path);
        return;
    }

    if (account.characters_num >= ACCOUNT_CHARACTERS_LIMIT) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "You have reached the maximum number of allowed characters per account.");
        account_free(&account);
        free(path);
        return;
    }

    path_player = player_make_path(name, "player.dat");

    if (!path_touch(path_player)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Write error occurred, please contact server administrator.");
        account_free(&account);
        free(path);
        free(path_player);
        return;
    }

    free(path_player);

    account.characters = xreallocarray(account.characters,
                                       (account.characters_num + 1),
                                       sizeof(*account.characters));
    account.characters[account.characters_num].at = at;
    account.characters[account.characters_num].name = xstrdup(name);
    account.characters[account.characters_num].region_name = xstrdup("");
    account.characters[account.characters_num].level = 1;
    account.characters_num++;

    if (!account_save(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Write error occurred, please contact server administrator.");
        account_free(&account);
        free(path);
        return;
    }

    draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_GREEN, ns, "New character created successfully.");
    account_send_characters(ns, &account);
    account_free(&account);
    free(path);
}

void account_login_char(socket_struct *ns, char *name) {
    char *path;
    account_struct account;
    size_t i;

    if (!ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        free(path);
        return;
    }

    free(path);

    for (i = 0; i < account.characters_num; i++) {
        if (strcmp(account.characters[i].name, name) == 0) {
            break;
        }
    }

    if (i == account.characters_num) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "No such character.");
        account_free(&account);
        return;
    }

    player_login(ns, name, account.characters[i].at);
    account_free(&account);
}

void account_logout_char(socket_struct *ns, player *pl) {
    char *path;
    account_struct account;
    size_t i;

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        free(path);
        return;
    }

    for (i = 0; i < account.characters_num; i++) {
        if (strcmp(account.characters[i].name, pl->ob->name) == 0) {
            free(account.characters[i].region_name);
            account.characters[i].region_name =
                xstrdup(pl->ob->map->region ? region_get_longname(pl->ob->map->region) : "???");
            string_replace_char(account.characters[i].region_name, ":", ' ');
            account.characters[i].level = pl->ob->level;
            break;
        }
    }

    account_save(&account, path);
    account_free(&account);
    free(path);
}

void account_password_change(socket_struct *ns,
                             char *password,
                             char *password_new,
                             char *password_new2) {
    size_t password_new_len;
    char *path;
    account_struct account;

    if (!ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*password == '\0' || *password_new == '\0' || *password_new2 == '\0' ||
        string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) ||
        string_contains_other(password_new, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) ||
        string_contains_other(password_new2, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid password.");
        return;
    }

    password_new_len = strlen(password_new);

    if (password_new_len < settings.limits[ALLOWED_CHARS_PASSWORD][0] ||
        password_new_len > settings.limits[ALLOWED_CHARS_PASSWORD][1]) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid length for password.");
        return;
    }

    if (strcmp(password_new, password_new2) != 0) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "The new passwords did not match.");
        return;
    }

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Read error occurred, please contact server administrator.");
        free(path);
        return;
    }

    if (!account_auth_work_allowed(2)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Authentication is temporarily busy; please retry shortly.");
        account_free(&account);
        free(path);
        return;
    }

    if (account_check_password(&account, password) != PASSWORD_VERIFY_MATCH) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid password.");
        account_free(&account);
        free(path);
        return;
    }

    if (!account_set_password(&account, password_new)) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Password change failed, please contact server administrator.");
        account_free(&account);
        free(path);
        return;
    }

    if (account_save(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_GREEN, ns, "Password changed successfully.");
    } else {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Save error occurred, please contact server administrator.");
    }

    account_free(&account);
    free(path);
}

void account_password_force(object *op, char *name, const char *password) {
    size_t password_len;
    char *path;
    account_struct account;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(name != NULL);
    HARD_ASSERT(password != NULL);

    if (*password == '\0' ||
        string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info(COLOR_RED, op, "Invalid password.");
        return;
    }

    password_len = strlen(password);

    if (password_len < settings.limits[ALLOWED_CHARS_PASSWORD][0] ||
        password_len > settings.limits[ALLOWED_CHARS_PASSWORD][1]) {
        draw_info(COLOR_RED, op, "Invalid length for password.");
        return;
    }

    string_tolower(name);
    path = account_make_path(name);

    if (!path_exists(path)) {
        draw_info(COLOR_RED, op, "No such account.");
        free(path);
        return;
    }

    if (!account_load(&account, path)) {
        draw_info(COLOR_RED,
                  op,
                  "Read error occurred, please contact server "
                  "administrator.");
        free(path);
        return;
    }

    if (!account_set_password(&account, password)) {
        draw_info(COLOR_RED,
                  op,
                  "Password change failed, please contact server "
                  "administrator.");
        account_free(&account);
        free(path);
        return;
    }

    if (account_save(&account, path)) {
        draw_info(COLOR_GREEN, op, "Password changed successfully.");
    } else {
        draw_info(COLOR_RED,
                  op,
                  "Save error occurred, please contact server "
                  "administrator.");
    }

    account_free(&account);
    free(path);
}
