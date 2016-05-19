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
 * Various defines.
 */

#ifndef CLIENT_H
#define CLIENT_H

/* How many skill types server supports/client will get sent to it.
 * If more skills are added to server, this needs to get increased. */
#define MAX_SKILL   6

#define INPUT_MODE_NO       0
#define INPUT_MODE_CONSOLE  1
#define INPUT_MODE_NUMBER   2

#define NUM_MODE_GET  1
#define NUM_MODE_DROP 2

typedef struct Animations {
    /* 0 = all fields are invalid, 1 = anim is loaded */
    int loaded;

    /* Length of one a animation frame (num_anim / facings) */
    int frame;
    uint16_t *faces;

    /* Number of frames */
    uint8_t facings;

    /* Number of animations. Value of 2 means
     * only faces[0], [1] have meaningful values. */
    uint8_t num_animations;
    uint8_t flags;
} Animations;

typedef struct _anim_table {
    /* Length of anim_cmd data */
    size_t len;

    /* Faked animation command */
    uint8_t *anim_cmd;
} _anim_table;

/**
 * One command buffer.
 */
typedef struct command_buffer {
    /** Next command in queue. */
    struct command_buffer *next;

    /** Previous command in queue. */
    struct command_buffer *prev;

    /** Length of the data. */
    size_t len;

    /** The data. */
    uint8_t data[1];
} command_buffer;

/* ClientSocket could probably hold more of the global values - it could
 * probably hold most all socket/communication related values instead
 * of globals. */
typedef struct client_socket {
    socket_t *sc;
} client_socket_t;

/** Copies information from one color structure into another. */
#define SDL_color_copy(_color, _color2) \
    { \
        (_color)->r = (_color2)->r; \
        (_color)->g = (_color2)->g; \
        (_color)->b = (_color2)->b; \
    }

typedef struct socket_command_struct {
    void (*handle_func)(uint8_t *data, size_t len, size_t pos);
} socket_command_struct;

/**
 * @defgroup SPELL_DESC_xxx Spell flags
 * Spell flags.
 *@{*/
/** Spell is safe to cast in town. */
#define SPELL_DESC_TOWN         0x01
/** Spell is fired in a direction (bullet, bolt, ...). */
#define SPELL_DESC_DIRECTION    0x02
/** Spell can be cast on self. */
#define SPELL_DESC_SELF         0x04
/** Spell can be cast on friendly creature. */
#define SPELL_DESC_FRIENDLY     0x08
/** Spell can be cast on enemy creature. */
#define SPELL_DESC_ENEMY        0x10

/*@}*/

typedef struct clioption_settings_struct {
    char **servers;

    size_t servers_num;

    char **metaservers;

    size_t metaservers_num;

    char *connect[4];

    char *game_news_url;

    uint8_t reconnect;
} clioption_settings_struct;

#endif
