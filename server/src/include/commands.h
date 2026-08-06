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
 * Commands header file.
 *
 * @author Zoey Rose
 */

#ifndef COMMANDS_H
#define COMMANDS_H

/**
 * Format for a command handler function.
 * @param op
 * The player.
 * @param command
 * The command's name.
 * @param params
 * Optional arguments for the command.
 */
typedef void (*command_func)(object *op, const char *command, char *params);

/**
 * A single command.
 */
typedef struct command_struct {
    /**
     * Name of the command.
     */
    char *name;

    /**
     * Handler function.
     */
    command_func handle_func;

    /**
     * Time the player must wait before doing another command.
     */
    double delay;

    /**
     * A combination of @ref COMMAND_xxx.
     */
    uint64_t flags;

    /**
     * Hash handle.
     */
    UT_hash_handle hh;
} command_struct;

/**
 * A single permission group.
 */
typedef struct permission_group_struct {
    /**
     * Name, eg, '[OP]'.
     */
    char *name;

    /**
     * The command permissions for this group.
     */
    char **cmd_permissions;

    /**
     * Number of command permissions.
     */
    size_t cmd_permissions_num;

    /**
     * Hash handle.
     */
    UT_hash_handle hh;
} permission_group_struct;

/**
 * @defgroup COMMAND_xxx Command flags
 * Command flags.
 *@{*/
/**
 * The command requires a permission.
 */
#define COMMAND_PERMISSION 1
/**
 * Markup can be used in this command.
 */
#define COMMAND_ALLOW_MARKUP 2
/*@}*/

/**
 * Shortcut macro for commands_add().
 */
#define COMMAND(__name) #__name, command_##__name

/**
 * Execute the specific command.
 */
#define COMMAND_EXECUTE(__op, __command, __params) \
    command_##__command((__op), #__command, (__params))

/** Public API implemented in src/commands/permission/arrest.c. */

extern void command_arrest(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/ban.c. */

extern void command_ban(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/config.c. */

extern void command_config(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/follow.c. */

extern void command_follow(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/freeze.c. */

extern void command_freeze(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/kick.c. */

extern void command_kick(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/memfree.c. */

extern void command_memfree(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/memleak.c. */

extern void command_memleak(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/mod_chat.c. */

extern void command_mod_chat(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/no_chat.c. */

extern void command_no_chat(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/opsay.c. */

extern void command_opsay(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/password.c. */

extern void command_password(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/resetmap.c. */

extern void command_resetmap(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/resetmaps.c. */

extern void command_resetmaps(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/server_chat.c. */

extern void command_server_chat(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/settime.c. */

extern void command_settime(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/shutdown.c. */

extern void command_shutdown(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/stats.c. */

extern void command_stats(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tcl.c. */

extern void command_tcl(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tgm.c. */

extern void command_tgm(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tli.c. */

extern void command_tli(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tls.c. */

extern void command_tls(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tp.c. */

extern void command_tp(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tphere.c. */

extern void command_tphere(object *op, const char *command, char *params);

/** Public API implemented in src/commands/permission/tsi.c. */

extern void command_tsi(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/afk.c. */

extern void command_afk(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/apply.c. */

extern void command_apply(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/chat.c. */

extern void command_chat(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/drop.c. */

extern void command_drop(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/gsay.c. */

extern void command_gsay(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/hiscore.c. */

extern void command_hiscore(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/left.c. */

extern void command_left(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/me.c. */

extern void command_me(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/motd.c. */

extern void command_motd(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/my.c. */

extern void command_my(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/party.c. */

extern void command_party(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/push.c. */

extern void command_push(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/rename.c. */

extern void command_rename(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/reply.c. */

extern void command_reply(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/right.c. */

extern void command_right(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/say.c. */

extern void command_say(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/statistics.c. */

extern void command_statistics(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/take.c. */

extern void command_take(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/tell.c. */

extern void command_tell(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/time.c. */

extern void command_time(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/version.c. */

extern void command_version(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/whereami.c. */

extern void command_whereami(object *op, const char *command, char *params);

/** Public API implemented in src/commands/player/who.c. */

extern void command_who(object *op, const char *command, char *params);

/** Public API implemented in src/server/commands.c. */

extern void toolkit_commands_init(void);

extern void toolkit_commands_deinit(void);

extern void commands_add(const char *name, command_func handle_func, double delay, uint64_t flags);

extern int commands_check_permission(player *pl, const char *command);

extern void commands_handle(object *op, char *cmd);

#endif
