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
 * Socket server header file.
 */

#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <toolkit/toolkit.h>

/**
 * Function to call when a specific command type is received.
 *
 * @param cs
 * Client socket.
 * @param pl
 * Player associated with the client. May be NULL if the client has not
 * logged in yet.
 * @param data
 * Network data buffer that contains the command.
 * @param len
 * Number of bytes in the command.
 * @param pos
 * Position where to start parsing command-specific data.
 */
typedef void (
    *socket_command_func)(socket_struct *cs, player *pl, uint8_t *data, size_t len, size_t pos);

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(socket_server);

void socket_server_handle_client(player *pl);
bool socket_server_remove(socket_struct *cs);
void socket_server_process(void);
void socket_server_post_process(void);
bool socket_server_quic_info(char *host,
                             size_t host_size,
                             uint16_t *port,
                             char certificate_sha256[65]);
bool socket_server_quic_punch(const char *host, uint16_t port);
size_t socket_server_quic_candidates(socket_direct_candidate_t *candidates, size_t capacity);
bool socket_port_mapping_init(uint16_t port, char *host, size_t host_size, uint16_t *external_port);
void socket_port_mapping_process(void);
void socket_port_mapping_deinit(void);

/** Public API implemented in src/socket/image.c. */

extern int is_valid_faceset(int fsn);

extern void free_socket_images(void);

extern void read_client_images(void);

extern void
socket_command_ask_face(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void face_get_data(int face, uint8_t **ptr, uint16_t *len);

/** Public API implemented in src/socket/info.c. */

extern void draw_info_send(uint8_t type,
                           const char *name,
                           const char *color,
                           socket_struct *ns,
                           const char *buf);

extern void draw_info_full(uint8_t type,
                           const char *name,
                           const char *color,
                           StringBuffer *sb_capture,
                           object *pl,
                           const char *buf);

extern void draw_info_full_format(uint8_t type,
                                  const char *name,
                                  const char *color,
                                  StringBuffer *sb_capture,
                                  object *pl,
                                  const char *format,
                                  ...) __attribute__((format(printf, 6, 7)));

extern void
draw_info_type(uint8_t type, const char *name, const char *color, object *pl, const char *buf);

extern void draw_info_type_format(uint8_t type,
                                  const char *name,
                                  const char *color,
                                  object *pl,
                                  const char *format,
                                  ...) __attribute__((format(printf, 5, 6)));

extern void draw_info(const char *color, object *pl, const char *buf);

extern void draw_info_format(const char *color, object *pl, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

extern void draw_info_map(uint8_t type,
                          const char *name,
                          const char *color,
                          mapstruct *map,
                          int x,
                          int y,
                          int dist,
                          object *op,
                          object *op2,
                          const char *buf);

/** Public API implemented in src/socket/init.c. */

extern Socket_Info socket_info;

extern socket_struct *init_sockets;

extern bool init_connection(socket_struct *ns);

extern bool socket_prelogin_expired(const socket_struct *ns);

extern void free_all_newserver(void);

extern void free_newsocket(socket_struct *ns);

extern void init_srv_files(void);

/** Public API implemented in src/socket/item.c. */

extern unsigned int query_flags(object *op);

extern void add_object_to_packet(struct packet_struct *packet,
                                 object *op,
                                 object *pl,
                                 uint8_t apply_action,
                                 uint32_t flags,
                                 int level);

extern void esrv_draw_look(object *pl);

extern void esrv_close_container(object *pl, object *op);

extern void esrv_send_inventory(object *pl, object *op);

extern void esrv_update_item(int flags, object *op);

extern void esrv_send_item(object *op);

extern void esrv_del_item(object *op);

extern object *esrv_get_ob_from_count(object *pl, tag_t count);

extern void
socket_command_item_examine(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void send_quickslots(player *pl);

extern void
socket_command_quickslot(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_item_apply(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_item_lock(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_item_mark(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof);

/** Public API implemented in src/socket/lowlevel.c. */

extern void socket_buffer_clear(socket_struct *ns);

extern void socket_buffer_write(socket_struct *ns);

extern bool socket_buffer_can_enqueue(const socket_struct *ns, size_t bytes, bool bulk);

extern void socket_send_packet(socket_struct *ns, struct packet_struct *packet);

/** Public API implemented in src/socket/metaserver.c. */

extern void metaserver_info_update(void);

extern void metaserver_init(void);

extern void metaserver_deinit(void);

extern void metaserver_stats(char *buf, size_t size);

extern bool metaserver_rendezvous_token_parse(const char *body, size_t body_size, char token[65]);

/** Public API implemented in src/socket/assets.c. */

extern void socket_assets_init(void);

extern void socket_assets_deinit(void);

void socket_command_asset(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

/** Public API implemented in src/socket/request.c. */

extern void
socket_command_setup(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_player_cmd(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_version(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_item_move(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void esrv_update_stats(player *pl);

extern void esrv_new_player(player *pl, uint32_t weight);

extern void draw_map_text_anim(object *pl, const char *color, const char *text);

extern void draw_client_map(object *pl);

extern void
packet_writer_write_map_name(struct packet_struct *packet, object *op, object *map_info);

extern void
packet_writer_write_map_music(struct packet_struct *packet, object *op, object *map_info);

extern void
packet_writer_write_map_weather(struct packet_struct *packet, object *op, object *map_info);

extern void draw_client_map2(object *pl);

extern void send_game_time(player *recipient);

extern void
socket_command_quest_list(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_clear(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_move_path(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_fire(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_keepalive(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_move(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void send_target_command(player *pl);

extern void
socket_command_account(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void generate_quick_name(player *pl);

extern void
socket_command_target(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_talk(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_control(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

extern void
socket_command_combat(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

void socket_command_ask_resource(socket_struct *ns,
                                 player *pl,
                                 uint8_t *data,
                                 size_t len,
                                 size_t pos);

/** Public API implemented in src/socket/sounds.c. */

extern void play_sound_player_only(player *pl,
                                   int type,
                                   const char *filename,
                                   int x,
                                   int y,
                                   int loop,
                                   int volume);

extern void
play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume);

/** Public API implemented in src/socket/updates.c. */

extern void updates_init(void);

extern void
socket_command_request_update(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos);

#endif
