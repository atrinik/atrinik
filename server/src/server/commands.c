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
 * Commands API.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <player.h>
#include <object.h>

static command_struct *commands;

/**
 * Hash table containing all permission groups.
 */
static permission_group_struct *permission_groups;

static void commands_permission_group_free(permission_group_struct *tmp);
static void commands_permissions_read(const char *path);

TOOLKIT_API(DEPENDS(path), DEPENDS(string));

TOOLKIT_INIT_FUNC(commands)
{
    commands = NULL;
    permission_groups = NULL;

    commands_permissions_read("permissions.cfg");

    if (path_exists("permissions-custom.cfg")) {
        commands_permissions_read("permissions-custom.cfg");
    }

    /* [operator] */
    commands_add(COMMAND(arrest), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(ban), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(follow), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(freeze), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(kick), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(memfree), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(memleak), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(mod_chat), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(no_chat), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(opsay), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(password), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(resetmap), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(resetmaps), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(server_chat), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(settime), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(shutdown), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(stats), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tcl), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tgm), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tli), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tls), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tp), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tphere), 0.0, COMMAND_PERMISSION);
    commands_add(COMMAND(tsi), 0.0, COMMAND_PERMISSION);

    /* [player] */
    commands_add(COMMAND(afk), 1.0, 0);
    commands_add(COMMAND(apply), 1.0, 0);
    commands_add(COMMAND(chat), 1.0, 0);
    commands_add(COMMAND(drop), 1.0, 0);
    commands_add(COMMAND(gsay), 1.0, 0);
    commands_add(COMMAND(hiscore), 2.0, 0);
    commands_add(COMMAND(left), 1.0, 0);
    commands_add(COMMAND(me), 1.0, 0);
    commands_add(COMMAND(motd), 1.0, 0);
    commands_add(COMMAND(my), 1.0, 0);
    commands_add(COMMAND(party), 1.0, 0);
    commands_add(COMMAND(push), 1.0, 0);
    commands_add(COMMAND(rename), 1.0, 0);
    commands_add(COMMAND(reply), 1.0, 0);
    commands_add(COMMAND(right), 1.0, 0);
    commands_add(COMMAND(say), 1.0, 0);
    commands_add(COMMAND(statistics), 1.0, 0);
    commands_add(COMMAND(take), 1.0, 0);
    commands_add(COMMAND(tell), 1.0, 0);
    commands_add(COMMAND(time), 1.0, 0);
    commands_add(COMMAND(version), 1.0, 0);
    commands_add(COMMAND(whereami), 1.0, 0);
    commands_add(COMMAND(who), 1.0, 0);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(commands)
{
    command_struct *curr, *tmp;
    permission_group_struct *curr2, *tmp2;

    HASH_ITER(hh, commands, curr, tmp)
    {
        HASH_DEL(commands, curr);
        efree(curr->name);
        efree(curr);
    }

    HASH_ITER(hh, permission_groups, curr2, tmp2)
    {
        HASH_DEL(permission_groups, curr2);
        commands_permission_group_free(curr2);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Free a single permission group structure.
 * @param tmp
 * What to free.
 */
static void commands_permission_group_free(permission_group_struct *tmp)
{
    size_t i;

    efree(tmp->name);

    for (i = 0; i < tmp->cmd_permissions_num; i++) {
        efree(tmp->cmd_permissions[i]);
    }

    if (tmp->cmd_permissions) {
        efree(tmp->cmd_permissions);
    }

    efree(tmp);
}

/**
 * Add a single permission group structure to the hash table of
 * permission groups.
 * @param tmp
 * What to add.
 */
static void commands_permission_group_add(permission_group_struct *tmp)
{
    permission_group_struct *curr;

    HASH_FIND(hh, permission_groups, tmp->name, strlen(tmp->name), curr);

    /* If it already exists, remove it and free it. */
    if (curr) {
        HASH_DEL(permission_groups, curr);
        commands_permission_group_free(curr);
    }

    HASH_ADD_KEYPTR(hh, permission_groups, tmp->name, strlen(tmp->name), tmp);
}

/**
 * Read command permissions config file.
 * @param path
 * File to read.
 */
static void commands_permissions_read(const char *path)
{
    FILE *fp;
    char buf[MAX_BUF], *end;
    permission_group_struct *tmp;

    fp = fopen(path, "r");

    if (!fp) {
        LOG(BUG, "Could not open %s for reading.", path);
        return;
    }

    tmp = NULL;

    while (fgets(buf, sizeof(buf), fp)) {
        if (*buf == '\n' || *buf == '#') {
            continue;
        }

        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        if (string_startswith(buf, "[") && string_endswith(buf, "]")) {
            if (tmp) {
                commands_permission_group_add(tmp);
            }

            tmp = ecalloc(1, sizeof(*tmp));
            tmp->name = estrdup(buf);
        } else if (tmp) {
            char *cps[2];

            if (string_split(buf, cps, arraysize(cps), '=') == 2) {
                string_whitespace_trim(cps[0]);
                string_whitespace_trim(cps[1]);

                if (strcmp(cps[0], "cmd") == 0) {
                    tmp->cmd_permissions = erealloc(tmp->cmd_permissions, sizeof(*tmp->cmd_permissions) * (tmp->cmd_permissions_num + 1));
                    tmp->cmd_permissions[tmp->cmd_permissions_num] = estrdup(cps[1]);
                    tmp->cmd_permissions_num++;
                }
            }
        }
    }

    if (tmp) {
        commands_permission_group_add(tmp);
    }

    fclose(fp);
}

void commands_add(const char *name, command_func handle_func, double delay, uint64_t flags)
{
    command_struct *command;

    TOOLKIT_PROTECT();

    command = emalloc(sizeof(*command));
    command->name = estrdup(name);
    command->handle_func = handle_func;
    command->delay = delay;
    command->flags = flags;

    HASH_ADD_KEYPTR(hh, commands, command->name, strlen(command->name), command);
}

static int commands_check_permission_group(const char *name, size_t len, const char *command)
{
    size_t i;
    permission_group_struct *tmp;

    HASH_FIND(hh, permission_groups, name, len, tmp);

    if (!tmp) {
        return 0;
    }

    for (i = 0; i < tmp->cmd_permissions_num; i++) {
        if (strcmp(tmp->cmd_permissions[i], "*") == 0 || strcmp(tmp->cmd_permissions[i], command) == 0) {
            return 1;
        }
    }

    return 0;
}

int commands_check_permission(player *pl, const char *command)
{
    int i;

    TOOLKIT_PROTECT();

    if (*settings.default_permission_groups != '\0') {
        char *curr, *next;

        for (curr = settings.default_permission_groups; (curr && (next = strchr(curr, ','))) || curr; curr = next ? next + 1 : NULL) {
            if (commands_check_permission_group(curr, next ? (size_t) (next - curr) : strlen(curr), command)) {
                return 1;
            }
        }
    }

    for (i = 0; i < pl->num_cmd_permissions; i++) {
        if (!pl->cmd_permissions[i]) {
            continue;
        }

        if (string_startswith(pl->cmd_permissions[i], "[") && string_endswith(pl->cmd_permissions[i], "]") && commands_check_permission_group(pl->cmd_permissions[i], strlen(pl->cmd_permissions[i]), command)) {
            return 1;
        } else if (strcmp(pl->cmd_permissions[i], command) == 0) {
            return 1;
        }
    }

    return 0;
}

void commands_handle(object *op, char *cmd)
{
    TOOLKIT_PROTECT();

    if (cmd[0] == '/' && cmd[1] != '\0') {
        char *cp, *params;
        command_struct *command;

        cmd++;
        cp = strchr(cmd, ' ');

        if (cp) {
            cmd[cp - cmd] = '\0';
            params = cp + 1;

            if (*params == '\0') {
                params = NULL;
            }
        } else {
            params = NULL;
        }

        HASH_FIND(hh, commands, cmd, strlen(cmd), command);

        if (command) {
            if (command->flags & COMMAND_PERMISSION && !commands_check_permission(CONTR(op), cmd)) {
                draw_info(COLOR_WHITE, op, "You do not have sufficient permissions for that command.");
                return;
            }

            if (params && !(command->flags & COMMAND_ALLOW_MARKUP)) {
                string_replace_char(params, "[]", ' ');
            }

            op->speed_left -= command->delay;
            command->handle_func(op, cmd, params);
            return;
        }
    }

    draw_info_format(COLOR_WHITE, op, "'/%s' is not a valid command.", cmd);
}
