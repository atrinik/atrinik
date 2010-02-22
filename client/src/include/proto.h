/* book.c */
_gui_book_struct *book_gui_load(char *data, int len);
void book_gui_show();
void book_gui_handle_mouse(int x, int y);

/* client.c */
void DoClient(ClientSocket *csocket);
void SockList_Init(SockList *sl);
void SockList_AddChar(SockList *sl, char c);
void SockList_AddShort(SockList *sl, uint16 data);
void SockList_AddInt(SockList *sl, uint32 data);
int GetInt_String(const unsigned char *data);
short GetShort_String(const unsigned char *data);
int send_socklist(int fd, SockList msg);
int cs_write_string(int fd, char *buf, size_t len);
void finish_face_cmd(int pnum, uint32 checksum, char *face);
int request_face(int pnum, int mode);
void check_animation_status(int anum);
char *adjust_string(char *buf);

/* commands.c */
void BookCmd(unsigned char *data, int len);
void PartyCmd(unsigned char *data, int len);
void SoundCmd(unsigned char *data, int len);
void SetupCmd(char *buf, int len);
void Face1Cmd(unsigned char *data, int len);
void AddMeFail(unsigned char *data, int len);
void AddMeSuccess(unsigned char *data, int len);
void GoodbyeCmd(unsigned char *data, int len);
void AnimCmd(unsigned char *data, int len);
void ImageCmd(unsigned char *data, int len);
void SkillRdyCmd(char *data, int len);
void DrawInfoCmd(unsigned char *data);
void DrawInfoCmd2(unsigned char *data, int len);
void TargetObject(unsigned char *data, int len);
void StatsCmd(unsigned char *data, int len);
void PreParseInfoStat(char *cmd);
void handle_query(char *data);
void send_reply(char *text);
void PlayerCmd(unsigned char *data, int len);
void ItemXCmd(unsigned char *data, int len);
void ItemYCmd(unsigned char *data, int len);
void UpdateItemCmd(unsigned char *data, int len);
void DeleteItem(unsigned char *data, int len);
void DeleteInventory(unsigned char *data);
void QuickSlotCmd(char *data);
void Map2Cmd(unsigned char *data, int len);
void map_scrollCmd(char *data);
void MagicMapCmd(unsigned char *data, int len);
void VersionCmd(char *data);
void SendVersion(ClientSocket csock);
void RequestFile(ClientSocket csock, int index);
void SendAddMe(ClientSocket csock);
void SendSetFaceMode(ClientSocket csock, int mode);
void MapstatsCmd(unsigned char *data);
void SkilllistCmd(char *data);
void SpelllistCmd(char *data);
void GolemCmd(unsigned char *data);
void NewCharCmd();
void DataCmd(unsigned char *data, int len);
void ShopCmd(unsigned char *data, int len);
void QuestListCmd(unsigned char *data, int len);

/* event.c */
void init_keys();
void reset_keys();
int mouseInPlayfield(int x, int y);
int draggingInvItem(int src);
int Event_PollInputDevice();
void key_connection_event(SDL_KeyboardEvent *key);
int key_meta_menu(SDL_KeyboardEvent *key);
int check_menu_macros(char *text);
void check_keys(int key);
int process_macro_keys(int id, int value);
void read_keybind_file(char *fname);
void save_keybind_file(char *fname);
void check_menu_keys(int menu, int key);

/* ignore.c */
void ignore_list_clear();
void ignore_list_load();
int ignore_check(char *name, char *type);
void ignore_command(char *cmd);

/* inventory.c */
int get_inventory_data(item *op, int *ctag, int *slot, int *start, int *count, int wxlen, int wylen);
void widget_inventory_event(int x, int y, SDL_Event event);
void widget_show_inventory_window(int x, int y);
void widget_below_window_event(int x, int y, int MEvent);
void widget_show_below_window(item *op, int x, int y);
int blt_inv_item_centered(item *tmp, int x, int y);
void blt_inv_item(item *tmp, int x, int y, int nrof);
void examine_range_inv();
void examine_range_marks(int tag);

/* item.c */
void init_item_types();
void update_item_sort(item *it);
char *get_number(int i);
void free_all_items(item *op);
int locate_item_tag_from_nr(item *op, int nr);
item *locate_item_from_inv(item *op, sint32 tag);
item *locate_item_from_item(item *op, sint32 tag);
item *locate_item(sint32 tag);
void remove_item(item *op);
void remove_item_inventory(item *op);
item *create_new_item(item *env, sint32 tag, int bflag);
void set_item_values(item *op, char *name, sint32 weight, uint16 face, int flags, uint16 anim, uint16 animspeed, sint32 nrof, uint8 itype, uint8 stype, uint8 qual, uint8 cond, uint8 skill, uint8 level, uint8 dir);
void fire_command(char *buf);
void combat_command(char *buf);
void toggle_locked(item *op);
void send_mark_obj(item *op);
item *player_item();
void update_item(int tag, int loc, char *name, int weight, int face, int flags, int anim, int animspeed, int nrof, uint8 itype, uint8 stype, uint8 qual, uint8 cond, uint8 skill, uint8 level, uint8 direction, int bflag);
void print_inventory(item *op);
void animate_objects();

/* main.c */
void save_options_dat();
void open_input_mode(int maxchar);
void list_vid_modes();
int main(int argc, char *argv[]);

/* map.c */
void load_mapdef_dat();
void clear_map();
void display_mapscroll(int dx, int dy);
void map_draw_map_clear();
void InitMapData(char *name, int xl, int yl, int px, int py, char *bg_music);
void set_map_ext(int x, int y, int layer, int ext, int probe);
void align_tile_stretch(int x, int y);
void adjust_tile_stretch();
void set_map_face(int x, int y, int layer, int face, int pos, int ext, char *name, int name_color, sint16 height);
void display_map_clearcell(long x, long y);
void set_map_darkness(int x, int y, uint8 darkness);
void map_draw_map();
int get_tile_position(int x, int y, int *tx, int *ty);

/* menu.c */
void do_console();
int client_command_check(char *cmd);
void show_help(char *helpname);
void do_number();
void do_keybind_input();
void widget_show_resist(int x, int y);
void blt_inventory_face_from_tag(int tag, int x, int y);
void show_menu();
int init_media_tag(char *tag);
void blt_window_slider(_Sprite *slider, int maxlen, int winlen, int startoff, int len, int x, int y);
int read_anim_tmp();
void read_anims();
void read_bmaps_p0();
void delete_bmap_tmp();
int read_bmap_tmp();
void read_bmaps();
void delete_server_chars();
void load_settings();
void read_settings();
void read_spells();
void free_help_files();
void read_help_files();
void read_skills();
void widget_range_event(int x, int y, SDL_Event event, int MEvent);
void widget_number_event(int x, int y);
void widget_show_console(int x, int y);
void widget_show_number(int x, int y);
void widget_show_mapname(int x, int y);
void widget_show_range(int x, int y);
void widget_event_target(int x, int y);
void widget_show_target(int x, int y);
int get_quickslot(int x, int y);
void widget_quickslots(int x, int y);
void widget_quickslots_mouse_event(int x, int y, int MEvent);
void show_quickslots(int x, int y);
void update_quickslots(int del_item);
void widget_show_fps(int x, int y);

/* metaserver.c */
server_struct *metaserver_get_selected(int num);
void metaserver_clear_data();
void metaserver_add(const char *ip, int port, const char *name, int player, const char *version, const char *desc);
int metaserver_thread(void *dummy);
void metaserver_get_servers();

/* misc.c */
_bmaptype *find_bmap(char *name);
void add_bmap(_bmaptype *at);
void FreeMemory(void **p);
const char *show_input_string(const char *text, struct _Font *font, int wlen);
int isqrt(int n);
char *get_parameter_string(const char *data, int *pos);
size_t split_string(char *str, char *array[], size_t array_size, char sep);

/* player.c */
void clear_player();
void new_player(long tag, char *name, long weight, short face);
void new_char(_server_char *nc);
void client_send_apply(int tag);
void client_send_examine(int tag);
void client_send_move(int loc, int tag, int nrof);
void send_command(const char *command, int repeat, int must_send);
void CompleteCmd(unsigned char *data, int len);
void set_weight_limit(uint32 wlim);
void init_player_data();
void widget_player_data_event(int x, int y);
void widget_show_player_data(int x, int y);
void widget_player_stats(int x, int y);
void widget_menubuttons(int x, int y);
void widget_menubuttons_event(int x, int y);
void widget_skillgroups(int x, int y);
void widget_show_player_doll_event();
void widget_show_player_doll(int x, int y);
void widget_show_main_lvl(int x, int y);
void widget_show_skill_exp(int x, int y);
void widget_skill_exp_event();
void widget_show_regeneration(int x, int y);

/* player_shop.c */
void widget_show_shop(int x, int y);
void shop_open();
void shop_buy_item();
void initialize_shop(int shop_state);
void clear_shop(int send_to_server);
void shop_add_close_button(int x, int y);
int shop_put_item(int x, int y);
void shop_remove_item(sint32 tag);
int check_shop_keys(SDL_KeyboardEvent *key);
char *shop_show_input(char *text, struct _Font *font, int wlen, int append_underscore);
int shop_price2int(char *text);
char *shop_int2price(int value);

/* scripts.c */
void script_load(const char *cparams);
void script_list();
void script_fdset(int *maxfd, fd_set *set);
void script_process(fd_set *set);
int script_trigger_event(const char *cmd, const uint8 *data, const int data_len, const enum CmdFormat format);
void script_send(char *params);
void script_killall();
void script_autoload();
void script_unload(const char *params);

/* party.c */
void switch_tabs();
void draw_party_tabs(int x, int y);
void show_party();
void gui_party_interface_mouse(SDL_Event *e);
void clear_party_interface();
_gui_party_struct *load_party_interface(char *data, int len);
int console_party();

/* socket.c */
int socket_get_error();
int socket_read(int fd, SockList *sl, int len);
int socket_write(int fd, unsigned char *buf, int len);
int socket_initialize();
void socket_deinitialize();
void socket_close(SOCKET socket_temp);
int open_socket(SOCKET *socket_temp, struct ClientSocket *csock, char *host, int port);

/* sound.c */
void sound_init();
void sound_deinit();
void sound_loadall();
void sound_freeall();
void calculate_map_sound(int soundnr, int xoff, int yoff);
int sound_play_effect(int soundid, int pan, int vol);
void sound_play_one_repeat(int soundid, int special_id);
void sound_play_music(char *fname, int vol, int fade, int loop, int mode);
void sound_fadeout_music(int i);

/* sprite.c */
void sprite_init_system();
_Sprite *sprite_load_file(char *fname, uint32 flags);
_Sprite *sprite_tryload_file(char *fname, uint32 flag, SDL_RWops *rwop);
void sprite_free_sprite(_Sprite *sprite);
void sprite_blt(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx);
void sprite_blt_map(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx, uint32 stretch);
Uint32 getpixel(SDL_Surface *surface, int x, int y);
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
void StringBlt(SDL_Surface *surf, _Font *font, const char *text, int x, int y, int col, SDL_Rect *area, _BLTFX *bltfx);
void CreateNewFont(_Sprite *sprite, _Font *font, int xlen, int ylen, int c32len);
void show_tooltip(int mx, int my, char *text);
int get_string_pixel_length(const char *text, struct _Font *font);
int StringWidth(_Font *font, char *text);
int StringWidthOffset(_Font *font, char *text, int *line, int len);
struct _anim *add_anim(int type, int mapx, int mapy, int value);
void remove_anim(struct _anim *anim);
void play_anims();
int sprite_collision(int x1, int y1, int x2, int y2, _Sprite *sprite1, _Sprite *sprite2);
SDL_Surface *zoomSurface(SDL_Surface *src, double zoomx, double zoomy, int smooth);

/* textwin.c */
void say_clickedKeyword(int actWin, int mouseX, int mouseY);
void textwin_init();
void draw_info_format(int flags, char *format, ...);
void draw_info(char *str, int flags);
void textwin_show(int x, int y);
void widget_textwin_show(int x, int y, int actWin);
void textwin_button_event(int actWin, SDL_Event event);
int textwin_move_event(int actWin, SDL_Event event);
void textwin_event(int e, SDL_Event *event, int WidgetID);
void textwin_addhistory(char *text);
void textwin_clearhistory();
void textwin_putstring(char *text);
void change_textwin_font(int font);

/* wrapper.c */
void LOG(int logLevel, char *format, ...);
void SYSTEM_Start();
int SYSTEM_End();
char *GetBitmapDirectory();
char *GetIconDirectory();
char *GetSfxDirectory();
char *GetCacheDirectory();
char *GetGfxUserDirectory();
char *GetMediaDirectory();
char *get_word_from_string(char *str, int *pos);
uint32 get_video_flags();
char *file_path(const char *fname, const char *mode);
FILE *fopen_wrapper(const char *fname, const char *mode);
SDL_Surface *IMG_Load_wrapper(const char *file);

/* widget.c */
void init_widgets_fromCurrent();
void kill_widgets();
void save_interface_file();
int widget_event_mousedn(int x, int y, SDL_Event *event);
int widget_event_mouseup(int x, int y, SDL_Event *event);
int widget_event_mousemv(int x, int y, SDL_Event *event);
int get_widget_owner(int x, int y);
void process_widgets();
uint32 GetMouseState(int *mx, int *my, int widget_id);
void SetPriorityWidget(int nWidgetID);

/* dialog.c */
void draw_frame(int x, int y, int w, int h);
void add_close_button(int x, int y, int menu);
int add_rangebox(int x, int y, int id, int text_w, int text_x, const char *text, int color);
void add_value(void *value, int type, int offset, int min, int max);
void optwin_draw_options(int x, int y);
void show_skilllist();
void show_spelllist();
void show_optwin();
void show_keybind();
void show_newplayer_server();
void show_login_server();
void show_meta_server(server_struct *node, int metaserver_sel);
void metaserver_mouse(SDL_Event *e);

/* tilestretcher.c */
int add_color_to_surface(SDL_Surface *dest, Uint8 red, Uint8 green, Uint8 blue);
int copy_pixel_to_pixel(SDL_Surface *src, SDL_Surface *dest, int x1, int y1, int x2, int y2, float brightness);
int copy_vertical_line(SDL_Surface *src, SDL_Surface *dest, int src_x, int src_sy, int src_ey, int dest_x, int dest_sy, int dest_ey, float brightness, int extra);
SDL_Surface *tile_stretch(SDL_Surface *src, int n, int e, int s, int w);
