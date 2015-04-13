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
 * Account system.
 *
 * @author Alex Tokar */

#include <global.h>
#include <packet.h>

#define ACCOUNT_CHARACTERS_LIMIT 16
#define ACCOUNT_PASSWORD_SIZE 32
#define ACCOUNT_PASSWORD_ITERATIONS 4096

typedef struct account_struct {
    unsigned char password[ACCOUNT_PASSWORD_SIZE];

    unsigned char salt[ACCOUNT_PASSWORD_SIZE];

    char *password_old;

    char *last_host;

    time_t last_time;

    struct {
        archetype *at;

        char *name;

        char *region_name;

        uint8 level;
    } *characters;

    size_t characters_num;
} account_struct;

void account_init(void)
{
}

void account_deinit(void)
{
}

static void account_free(account_struct *account)
{
    size_t i;

    if (account->last_host) {
        efree(account->last_host);
    }

    if (account->password_old) {
        efree(account->password_old);
    }

    for (i = 0; i < account->characters_num; i++) {
        efree(account->characters[i].name);
    }

    if (account->characters) {
        efree(account->characters);
    }
}

static char *account_old_crypt(char *str, const char *salt)
{
#if defined(HAVE_CRYPT) && defined(HAVE_CRYPT_H)
    return crypt(str, salt);
#else
    return str;
#endif
}

static void account_set_password(account_struct *account, const char *password)
{
    size_t i;

    /* Create a truly random 256-bit salt. */
    for (i = 0; i < ACCOUNT_PASSWORD_SIZE; i++) {
        account->salt[i] = rndm(1, 256) - 1;
    }

    PKCS5_PBKDF2_HMAC_SHA2((unsigned char *) password, strlen(password), account->salt, ACCOUNT_PASSWORD_SIZE, ACCOUNT_PASSWORD_ITERATIONS, ACCOUNT_PASSWORD_SIZE, account->password);
}

static int account_check_password(account_struct *account, char *password)
{
    unsigned char output[ACCOUNT_PASSWORD_SIZE];

    if (account->password_old) {
        return strcmp(account_old_crypt(password, account->password_old), account->password_old) == 0;
    }

    PKCS5_PBKDF2_HMAC_SHA2((unsigned char *) password, strlen(password), account->salt, ACCOUNT_PASSWORD_SIZE, ACCOUNT_PASSWORD_ITERATIONS, ACCOUNT_PASSWORD_SIZE, output);

    return memcmp(account->password, output, sizeof(output)) == 0;
}

static int account_save(account_struct *account, const char *path)
{
    FILE *fp;
    char hex[ACCOUNT_PASSWORD_SIZE * 2 + 1];
    size_t i;

    fp = fopen(path, "w");

    if (!fp) {
        logger_print(LOG(BUG), "Could not open %s for writing.", path);
        return 0;
    }

    if (string_tohex(account->password, ACCOUNT_PASSWORD_SIZE, hex, sizeof(hex),
            false) == sizeof(hex) - 1) {
        fprintf(fp, "pswd %s\n", hex);
    }

    if (string_tohex(account->salt, ACCOUNT_PASSWORD_SIZE, hex, sizeof(hex),
            false) == sizeof(hex) - 1) {
        fprintf(fp, "salt %s\n", hex);
    }

    fprintf(fp, "host %s\n", account->last_host);
    fprintf(fp, "time %"FMT64U "\n", (uint64) account->last_time);

    for (i = 0; i < account->characters_num; i++) {
        fprintf(fp, "char %s:%s:%s:%d\n", account->characters[i].at->name, account->characters[i].name, account->characters[i].region_name, account->characters[i].level);
    }

    fclose(fp);

    return 1;
}

static int account_load(account_struct *account, const char *path)
{
    FILE *fp;
    char buf[MAX_BUF], *end;

    fp = fopen(path, "rb");

    if (!fp) {
        logger_print(LOG(BUG), "Could not open %s for reading.", path);
        return 0;
    }

    memset(account, 0, sizeof(*account));

    while (fgets(buf, sizeof(buf), fp)) {
        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        if (strncmp(buf, "pswd ", 5) == 0) {
            size_t len;

            len = strlen(buf + 5);

            if (len == 13 || len == 40) {
                account->password_old = estrdup(buf + 5);
            } else if (string_fromhex(buf + 5, len, account->password, ACCOUNT_PASSWORD_SIZE) != ACCOUNT_PASSWORD_SIZE) {
                logger_print(LOG(BUG), "Invalid password entry in file: %s", path);
                memset(account->password, 0, sizeof(account->password));
            }
        } else if (strncmp(buf, "salt ", 5) == 0) {
            if (string_fromhex(buf + 5, strlen(buf + 5), account->salt, ACCOUNT_PASSWORD_SIZE) != ACCOUNT_PASSWORD_SIZE) {
                logger_print(LOG(BUG), "Invalid salt entry in file: %s", path);
                memset(account->salt, 0, sizeof(account->salt));
            }
        } else if (strncmp(buf, "host ", 5) == 0) {
            account->last_host = estrdup(buf + 5);
        } else if (strncmp(buf, "time ", 5) == 0) {
            account->last_time = atoll(buf + 5);
        } else if (strncmp(buf, "char ", 5) == 0) {
            char *cps[4];

            if (string_split(buf + 5, cps, arraysize(cps), ':') != arraysize(cps)) {
                logger_print(LOG(BUG), "Invalid character entry in file: %s", path);
                continue;
            }

            account->characters = erealloc(account->characters, sizeof(*account->characters) * (account->characters_num + 1));
            account->characters[account->characters_num].at = find_archetype(cps[0]);
            account->characters[account->characters_num].name = estrdup(cps[1]);
            account->characters[account->characters_num].region_name = estrdup(cps[2]);
            account->characters[account->characters_num].level = atoi(cps[3]);
            account->characters_num++;
        }
    }

    fclose(fp);

    return 1;
}

static void account_send_characters(socket_struct *ns, account_struct *account)
{
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_CHARACTERS, 64, 64);

    if (account) {
        size_t i;

        packet_append_string_terminated(packet, ns->account);
        packet_debug(packet, 0, "Account name: %s\n", ns->account);
        packet_append_string_terminated(packet, ns->host);
        packet_debug(packet, 0, "Hostname: %s\n", ns->host);
        packet_append_string_terminated(packet, account->last_host);
        packet_debug(packet, 0, "Last hostname: %s\n", account->last_host);
        packet_append_uint64(packet, account->last_time);
        packet_debug(packet, 0, "Last time: %" FMT64U "\n",
                (uint64) account->last_time);

        for (i = 0; i < account->characters_num; i++) {
            packet_debug(packet, 0, "Character #%d:\n", i);
            packet_append_string_terminated(packet,
                    account->characters[i].at->name);
            packet_debug(packet, 1, "Archname: %s\n",
                    account->characters[i].at->name);
            packet_append_string_terminated(packet,
                    account->characters[i].name);
            packet_debug(packet, 1, "Name: %s\n",
                    account->characters[i].name);
            packet_append_string_terminated(packet,
                    account->characters[i].region_name);
            packet_debug(packet, 1, "Region name: %s\n",
                    account->characters[i].region_name);
            packet_append_uint16(packet,
                    account->characters[i].at->clone.animation_id);
            packet_debug(packet, 1, "Animation ID: %d\n",
                    account->characters[i].at->clone.animation_id);
            packet_append_uint8(packet, account->characters[i].level);
            packet_debug(packet, 1, "Level: %d\n",
                    account->characters[i].level);
        }
    }

    socket_send_packet(ns, packet);
}

char *account_make_path(const char *name)
{
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

void account_login(socket_struct *ns, char *name, char *password)
{
    account_struct account;
    char *path;

    if (ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*name == '\0' || *password == '\0' || string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) || string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid name and/or password.");
        account_send_characters(ns, NULL);
        return;
    }

    string_tolower(name);
    path = account_make_path(name);

    if (!path_exists(path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "No such account.");
        account_send_characters(ns, NULL);
        efree(path);
        return;
    }

    if (!account_load(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Read error occurred, please contact server administrator.");
        account_send_characters(ns, NULL);
        efree(path);
        return;
    }

    if (!account_check_password(&account, password)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid password.");
        account_send_characters(ns, NULL);
        account_free(&account);
        efree(path);

        ns->password_fails++;
        logger_print(LOG(SYSTEM), "%s: Failed to provide correct password for account %s.", ns->host, name);

        if (ns->password_fails >= MAX_PASSWORD_FAILURES) {
            logger_print(LOG(SYSTEM), "%s: Failed to provide a correct password for account %s too many times!", ns->host, name);
            draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "You have failed to provide a correct password too many times.");
            ns->state = ST_ZOMBIE;
        }

        return;
    }

    if (account.password_old) {
        account_set_password(&account, password);
    }

    ns->account = estrdup(name);
    account_send_characters(ns, &account);

    efree(account.last_host);
    account.last_host = estrdup(ns->host);
    account.last_time = datetime_getutc();
    account_save(&account, path);
    account_free(&account);
    efree(path);
}

void account_register(socket_struct *ns, char *name, char *password, char *password2)
{
    size_t name_len, password_len;
    char *path;
    account_struct account;

    if (ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*name == '\0' || *password == '\0' || *password2 == '\0' || string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) || string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) || string_contains_other(password2, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid name and/or password.");
        return;
    }

    name_len = strlen(name);
    password_len = strlen(password);

    /* Ensure the name/password lengths are within the allowed range.
     * No need to compare 'password2' length, as it needs to be the same
     * as 'password' anyway. */
    if (name_len < settings.limits[ALLOWED_CHARS_ACCOUNT][0] || name_len > settings.limits[ALLOWED_CHARS_ACCOUNT][1] || password_len < settings.limits[ALLOWED_CHARS_PASSWORD][0] || password_len > settings.limits[ALLOWED_CHARS_PASSWORD][1]) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid length for name and/or password.");
        return;
    }

    if (strcmp(password, password2) != 0) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "The passwords did not match.");
        return;
    }

    string_tolower(name);
    path = account_make_path(name);

    if (path_exists(path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "That account name is already registered.");
        efree(path);
        return;
    }

    path_ensure_directories(path);

    account_set_password(&account, password);
    account.last_host = ns->host;
    account.last_time = datetime_getutc();
    account.characters = NULL;
    account.characters_num = 0;

    if (!account_save(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Save error occurred, please contact server administrator.");
        efree(path);
        return;
    }

    ns->account = estrdup(name);
    account_send_characters(ns, &account);
    efree(path);
}

void account_new_char(socket_struct *ns, char *name, char *archname)
{
    archetype *at;
    char *path, *path_player;
    account_struct account;

    if (!ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*name == '\0' || string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_CHARNAME])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid character name");
        return;
    }

    string_title(name);

    if (player_exists(name)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Character with that name already exists.");
        return;
    }

    at = find_archetype(archname);

    if (!at || at->clone.type != PLAYER) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid archname.");
        return;
    }

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Read error occurred, please contact server administrator.");
        efree(path);
        return;
    }

    if (account.characters_num >= ACCOUNT_CHARACTERS_LIMIT) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "You have reached the maximum number of allowed characters per account.");
        account_free(&account);
        efree(path);
        return;
    }

    path_player = player_make_path(name, "player.dat");

    if (!path_touch(path_player)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Write error occurred, please contact server administrator.");
        account_free(&account);
        efree(path);
        efree(path_player);
        return;
    }

    efree(path_player);

    account.characters = erealloc(account.characters, sizeof(*account.characters) * (account.characters_num + 1));
    account.characters[account.characters_num].at = at;
    account.characters[account.characters_num].name = estrdup(name);
    account.characters[account.characters_num].region_name = estrdup("");
    account.characters[account.characters_num].level = 1;
    account.characters_num++;

    if (!account_save(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Write error occurred, please contact server administrator.");
        account_free(&account);
        efree(path);
        return;
    }

    draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_GREEN, ns, "New character created successfully.");
    account_send_characters(ns, &account);
    account_free(&account);
    efree(path);
}

void account_login_char(socket_struct *ns, char *name)
{
    char *path;
    account_struct account;
    size_t i;

    if (!ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        efree(path);
        return;
    }

    efree(path);

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

void account_logout_char(socket_struct *ns, player *pl)
{
    char *path;
    account_struct account;
    size_t i;

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        efree(path);
        return;
    }

    for (i = 0; i < account.characters_num; i++) {
        if (strcmp(account.characters[i].name, pl->ob->name) == 0) {
            efree(account.characters[i].region_name);
            account.characters[i].region_name = estrdup(pl->ob->map->region ? region_get_longname(pl->ob->map->region) : "???");
            string_replace_char(account.characters[i].region_name, ":", ' ');
            account.characters[i].level = pl->ob->level;
            break;
        }
    }

    account_save(&account, path);
    account_free(&account);
    efree(path);
}

void account_password_change(socket_struct *ns, char *password, char *password_new, char *password_new2)
{
    size_t password_new_len;
    char *path;
    account_struct account;

    if (!ns->account) {
        ns->state = ST_DEAD;
        return;
    }

    if (*password == '\0' || *password_new == '\0' || *password_new2 == '\0' || string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) || string_contains_other(password_new, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) || string_contains_other(password_new2, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid password.");
        return;
    }

    password_new_len = strlen(password_new);

    if (password_new_len < settings.limits[ALLOWED_CHARS_PASSWORD][0] || password_new_len > settings.limits[ALLOWED_CHARS_PASSWORD][1]) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid length for password.");
        return;
    }

    if (strcmp(password_new, password_new2) != 0) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "The new passwords did not match.");
        return;
    }

    path = account_make_path(ns->account);

    if (!account_load(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Read error occurred, please contact server administrator.");
        efree(path);
        return;
    }

    if (!account_check_password(&account, password)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Invalid password.");
        account_free(&account);
        efree(path);
        return;
    }

    account_set_password(&account, password_new);

    if (account_save(&account, path)) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_GREEN, ns, "Password changed successfully.");
    } else {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Save error occurred, please contact server administrator.");
    }

    account_free(&account);
    efree(path);
}

void account_password_force(object *op, char *name, const char *password)
{
    size_t password_len;
    char *path;
    account_struct account;

    assert(op != NULL);
    assert(name != NULL);
    assert(password != NULL);

    if (*password == '\0' || string_contains_other(password,
            settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
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
        efree(path);
        return;
    }

    if (!account_load(&account, path)) {
        draw_info(COLOR_RED, op, "Read error occurred, please contact server "
                "administrator.");
        efree(path);
        return;
    }

    account_set_password(&account, password);

    if (account_save(&account, path)) {
        draw_info(COLOR_GREEN, op, "Password changed successfully.");
    } else {
        draw_info(COLOR_RED, op, "Save error occurred, please contact server "
                "administrator.");
    }

    account_free(&account);
    efree(path);
}
