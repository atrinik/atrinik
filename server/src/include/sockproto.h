/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

/* image.c */
int is_valid_faceset(int fsn);
void free_socket_images(void);
void read_client_images(void);
void SetFaceMode(char *buf, int len, NewSocket *ns);
void SendFaceCmd(char *buff, int len, NewSocket *ns);
int esrv_send_face(NewSocket *ns, short face_num, int nocache);
void send_image_info(NewSocket *ns, char *params);
void send_image_sums(NewSocket *ns, char *params);

/* info.c */
void new_draw_info(int flags, int pri, object *pl, const char *buf);
void new_draw_info_format(int flags, int pri, object *pl, char *format, ...);
void new_info_map(int color, mapstruct *map, int x, int y, int dist, const char *str);
void new_info_map_except(int color, mapstruct *map, int x, int y, int dist, object *op1, object *op, const char *str);
void send_socket_message(int flags, NewSocket *ns, const char *buf);

/* init.c */
void InitConnection(NewSocket *ns, uint32 from);
void init_ericserver(void);
void free_all_newserver(void);
void free_newsocket(NewSocket *ns);
void init_srv_files(void);
void send_srv_file(NewSocket *ns, int id);

/* item.c */
unsigned int query_flags(object *op);
void esrv_draw_look(object *pl);
void esrv_close_container(object *op);
void esrv_send_inventory(object *pl, object *op);
void esrv_update_item(int flags, object *pl, object *op);
void esrv_send_item(object *pl, object *op);
void esrv_del_item(player *pl, int tag, object *cont);
object *esrv_get_ob_from_count(object *pl, tag_t count);
void ExamineCmd(char *buf, int len, player *pl);
void QuickSlotCmd(char *buf, int len, player *pl);
void ApplyCmd(char *buf, int len, player *pl);
void LockItem(uint8 *data, int len, player *pl);
void MarkItem(uint8 *data, int len, player *pl);
void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof);

/* loop.c */
void RequestInfo(char *buf, int len, NewSocket *ns);
void HandleClient(NewSocket *ns, player *pl);
void watchdog();
void doeric_server(void);
void doeric_server_write(void);

/* lowlevel.c */
void SockList_AddString(SockList *sl, char *data);
int SockList_ReadPacket(int fd, SockList *sl, int len);
void write_socket_buffer(NewSocket *ns);
void Write_To_Socket(NewSocket *ns, unsigned char *buf, int len);
void Send_With_Handling(NewSocket *ns, SockList *msg);
void Write_String_To_Socket(NewSocket *ns, char cmd, char *buf, int len);
void write_cs_stats(void);

/* metaserver.c */
void metaserver_init(void);
void metaserver_update(void);

/* request.c */
void SetUp(char *buf, int len, NewSocket *ns);
void AddMeCmd(char *buf, int len, NewSocket *ns);
void PlayerCmd(uint8 *buf, int len, player *pl);
void ReplyCmd(char *buf, int len, player *pl);
void RequestFileCmd(char *buf, int len, NewSocket *ns);
void VersionCmd(char *buf, int len, NewSocket *ns);
void SetSound(char *buf, int len, NewSocket *ns);
void MapRedrawCmd(char *buff, int len, player *pl);
void MapNewmapCmd(player *pl);
void MoveCmd(char *buf, int len, player *pl);
void send_query(NewSocket *ns, uint8 flags, char *text);
void esrv_update_skills(player *pl);
void esrv_update_stats(player *pl);
void esrv_new_player(player *pl, uint32 weight);
void draw_client_map(object *pl);
void draw_client_map2(object *pl);
void esrv_map_scroll(NewSocket *ns, int dx, int dy);
void send_plugin_custom_message(object *pl, char cmd, char *buf);
void ShopCmd(char *buf, int len, player *pl);
void QuestListCmd(char *data, int len, player *pl);

/* sounds.c */
void play_sound_player_only(player *pl, int sound_num, int sound_type, int x, int y);
void play_sound_map(mapstruct *map, int x, int y, int sound_num, int sound_type);
