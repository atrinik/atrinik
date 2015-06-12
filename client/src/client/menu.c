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
 * Menu related functions. */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>

/**
 * Analyze /cmd type commands the player has typed in the console or bound to a
 * key.
 * Sort out the "client intern" commands and expand or pre process them for the
 * server.
 * @param cmd Command to check
 * @return 0 to send command to server, 1 to not send it */
int client_command_check(const char *cmd)
{
    if (cmd_aliases_handle(cmd)) {
        return 1;
    } else if (strncasecmp(cmd, "/ready_spell", 12) == 0) {
        cmd = strchr(cmd, ' ');

        if (!cmd || *++cmd == '\0') {
            draw_info(COLOR_RED, "Usage: /ready_spell <spell name>");
            return 1;
        } else {
            object *tmp;

            for (tmp = cpl.ob->inv; tmp; tmp = tmp->next) {
                if (tmp->itype == TYPE_SPELL && strncasecmp(tmp->s_name, cmd, strlen(cmd)) == 0) {
                    if (!(tmp->flags & CS_FLAG_APPLIED)) {
                        client_send_apply(tmp);
                    }

                    return 1;
                }
            }
        }

        draw_info(COLOR_RED, "Unknown spell.");
        return 1;
    } else if (strncasecmp(cmd, "/ready_skill", 12) == 0) {
        cmd = strchr(cmd, ' ');

        if (!cmd || *++cmd == '\0') {
            draw_info(COLOR_RED, "Usage: /ready_skill <skill name>");
            return 1;
        } else {
            object *tmp;

            for (tmp = cpl.ob->inv; tmp; tmp = tmp->next) {
                if (tmp->itype == TYPE_SKILL && strncasecmp(tmp->s_name, cmd, strlen(cmd)) == 0) {
                    if (!(tmp->flags & CS_FLAG_APPLIED)) {
                        client_send_apply(tmp);
                    }

                    return 1;
                }
            }
        }

        draw_info(COLOR_RED, "Unknown skill.");
        return 1;
    } else if (!strncmp(cmd, "/help", 5)) {
        cmd += 5;

        if (!cmd || *cmd == '\0') {
            help_show("main");
        } else {
            help_show(cmd + 1);
        }

        return 1;
    } else if (!strncmp(cmd, "/resetwidgets", 13)) {
        widgets_reset();
        return 1;
    } else if (!strncmp(cmd, "/effect ", 8)) {
        if (!strcmp(cmd + 8, "none")) {
            effect_stop();
            draw_info(COLOR_GREEN, "Stopped effect.");
            return 1;
        }

        if (effect_start(cmd + 8)) {
            draw_info_format(COLOR_GREEN, "Started effect %s.", cmd + 8);
        } else {
            draw_info_format(COLOR_RED, "No such effect %s.", cmd + 8);
        }

        return 1;
    } else if (!strncmp(cmd, "/d_effect ", 10)) {
        effect_debug(cmd + 10);
        return 1;
    } else if (!strncmp(cmd, "/music_pause", 12)) {
        sound_pause_music();
        return 1;
    } else if (!strncmp(cmd, "/music_resume", 13)) {
        sound_resume_music();
        return 1;
    } else if (!strncmp(cmd, "/party joinpassword ", 20)) {
        cmd += 20;

        if (cpl.partyjoin[0] != '\0') {
            char buf[MAX_BUF];

            snprintf(VS(buf), "/party join %s\t%s", cpl.partyjoin, cmd);
            send_command(buf);
        }

        return 1;
    } else if (!strncmp(cmd, "/invfilter ", 11)) {
        inventory_filter_set_names(cmd + 11);
        return 1;
    } else if (!strncasecmp(cmd, "/screenshot", 11)) {
        SDL_Surface *surface_save;

        cmd += 11;

        if (!strncasecmp(cmd, " map", 4)) {
            surface_save = cur_widget[MAP_ID]->surface;
        } else {
            surface_save = ScreenSurface;
        }

        if (!surface_save) {
            draw_info(COLOR_RED, "No surface to save.");
            return 1;
        }

        screenshot_create(surface_save);
        return 1;
    } else if (!strncasecmp(cmd, "/console-load ", 14)) {
        FILE *fp;
        char path[HUGE_BUF], buf[HUGE_BUF * 4], *cp;
        StringBuffer *sb;

        cmd += 14;

        snprintf(path, sizeof(path), "%s/.atrinik/console/%s", get_config_dir(), cmd);

        fp = fopen(path, "r");

        if (!fp) {
            draw_info_format(COLOR_RED, "Could not read %s.", path);
            return 1;
        }

        send_command("/console noinf::");

        while (fgets(buf, sizeof(buf) - 1, fp)) {
            cp = strchr(buf, '\n');

            if (cp) {
                *cp = '\0';
            }

            sb = stringbuffer_new();
            stringbuffer_append_string(sb, "/console noinf::");
            stringbuffer_append_string(sb, buf);
            cp = stringbuffer_finish(sb);
            send_command(cp);
            efree(cp);
        }

        send_command("/console noinf::");

        fclose(fp);

        return 1;
    } else if (strncasecmp(cmd, "/console-obj", 11) == 0) {
        menu_inventory_loadtoconsole(cpl.inventory_focus, NULL, NULL);
        return 1;
    } else if (strncasecmp(cmd, "/patch-obj", 11) == 0) {
        menu_inventory_patch(cpl.inventory_focus, NULL, NULL);
        return 1;
    } else if (string_startswith(cmd, "/cast ") || string_startswith(cmd, "/use_skill ")) {
        object *tmp;
        uint8_t type;

        type = string_startswith(cmd, "/cast ") ? TYPE_SPELL : TYPE_SKILL;
        cmd = strchr(cmd, ' ') + 1;

        if (string_isempty(cmd)) {
            return 1;
        }

        for (tmp = cpl.ob->inv; tmp; tmp = tmp->next) {
            if (tmp->itype == type && strncasecmp(tmp->s_name, cmd, strlen(cmd)) == 0) {
                client_send_fire(5, tmp->tag);
                return 1;
            }
        }

        draw_info_format(COLOR_RED, "Unknown %s.", type == TYPE_SPELL ? "spell" : "skill");
        return 1;
    } else if (strncasecmp(cmd, "/clearcache", 11) == 0) {
        cmd += 12;

        if (string_isempty(cmd)) {
            return 1;
        }

        if (strcasecmp(cmd, "sound") == 0) {
            sound_clear_cache();
            draw_info(COLOR_GREEN, "Sound cache cleared.");
        } else if (strcasecmp(cmd, "textures") == 0) {
            texture_reload();
            draw_info(COLOR_GREEN, "Textures reloaded.");
        }

        return 1;
    } else if (string_startswith(cmd, "/droptag ") ||
            string_startswith(cmd, "/gettag ")) {
        char *cps[3];
        unsigned long int loc, tag, num;

        if (string_split(strchr(cmd, ' ') + 1, cps, arraysize(cps), ' ') !=
                arraysize(cps)) {
            return 1;
        }

        loc = strtoul(cps[0], NULL, 10);
        tag = strtoul(cps[1], NULL, 10);
        num = strtoul(cps[2], NULL, 10);
        client_send_move(loc, tag, num);

        if (string_startswith(cmd, "/gettag ")) {
            sound_play_effect("get.ogg", 100);
        } else {
            sound_play_effect("drop.ogg", 100);
        }

        return 1;
    } else if (string_startswith(cmd, "/talk")) {
        char type[MAX_BUF], npc_name[MAX_BUF];
        size_t pos;
        uint8_t type_num;
        packet_struct *packet;

        pos = 5;

        if (!string_get_word(cmd, &pos, ' ', type, sizeof(type), 0) || string_isempty(cmd + pos)) {
            return 1;
        }

        type_num = atoi(type);

        if (type_num == CMD_TALK_NPC_NAME && (!string_get_word(cmd, &pos, ' ', npc_name, sizeof(npc_name), '"') || string_isempty(cmd + pos))) {
            return 1;
        }

        packet = packet_new(SERVER_CMD_TALK, 64, 64);
        packet_append_uint8(packet, type_num);

        if (type_num == CMD_TALK_NPC || type_num == CMD_TALK_NPC_NAME) {
            if (type_num == CMD_TALK_NPC_NAME) {
                packet_append_string_terminated(packet, npc_name);
            }

            packet_append_string_terminated(packet, cmd + pos);
        } else {
            char tag[MAX_BUF];

            if (!string_get_word(cmd, &pos, ' ', tag, sizeof(tag), 0) || string_isempty(cmd + pos)) {
                packet_free(packet);
                return 1;
            }

            packet_append_uint32(packet, atoi(tag));
            packet_append_string_terminated(packet, cmd + pos);
        }

        socket_send_packet(packet);

        return 1;
    } else if (string_startswith(cmd, "/widget_toggle")) {
        size_t pos;
        char word[MAX_BUF], *cps[2];
        int widget_id;

        pos = 14;

        while (string_get_word(cmd, &pos, ' ', word, sizeof(word), 0)) {
            if (string_split(word, cps, arraysize(cps), ':') < 1) {
                continue;
            }

            widget_id = widget_id_from_name(cps[0]);

            /* Invalid widget ID */
            if (widget_id == -1) {
                continue;
            }

            /* Redraw all or a specific one identified by its UID */
            if (cps[1] == NULL) {
                WIDGET_SHOW_TOGGLE_ALL(widget_id);
            } else {
                widgetdata *widget;

                widget = widget_find(NULL, widget_id, cps[1], NULL);

                if (widget) {
                    WIDGET_SHOW_TOGGLE(widget);
                }
            }
        }

        return 1;
    } else if (string_startswith(cmd, "/widget_focus")) {
        size_t pos;
        char word[MAX_BUF], *cps[2];
        int widget_id;

        pos = 14;

        while (string_get_word(cmd, &pos, ' ', word, sizeof(word), 0)) {
            if (string_split(word, cps, arraysize(cps), ':') < 1) {
                continue;
            }

            widget_id = widget_id_from_name(cps[0]);
            if (widget_id == -1) {
                /* Invalid widget ID */
                continue;
            }

            widget_switch_focus(widget_id, cps[1]);
        }

        return 1;
    } else if (string_startswith(cmd, "/ping")) {
        keepalive_ping_stats();
        return 1;
    } else if (string_startswith(cmd, "/region_map")) {
        region_map_open();
        return 1;
    }

    return 0;
}

/**
 * Same as send_command(), but also check client commands.
 * @param cmd Command to send. */
int send_command_check(const char *cmd)
{
    if (!client_command_check(cmd)) {
        send_command(cmd);
        return 1;
    }

    return 0;
}
