#ifndef __CPROTO__
/* client/animations.c */
int read_anim_tmp();
void read_anims();

/* client/client.c */
void DoClient();
void SockList_Init(SockList *sl);
void SockList_AddChar(SockList *sl, char c);
void SockList_AddShort(SockList *sl, uint16 data);
void SockList_AddInt(SockList *sl, uint32 data);
void SockList_AddString(SockList *sl, char *data);
void SockList_AddStringTerminated(SockList *sl, char *data);
int GetInt_String(const unsigned char *data);
sint64 GetInt64_String(const unsigned char *data);
short GetShort_String(const unsigned char *data);
int cs_write_string(char *buf, size_t len);
void finish_face_cmd(int pnum, uint32 checksum, char *face);
int request_face(int pnum, int mode);
void check_animation_status(int anum);
char *adjust_string(char *buf);

/* client/commands.c */
void BookCmd(unsigned char *data, int len);
void PartyCmd(unsigned char *data, int len);
void SetupCmd(char *buf, int len);
void AddMeFail(unsigned char *data, int len);
void AddMeSuccess(unsigned char *data, int len);
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
void Map2Cmd(unsigned char *data, int len);
void MagicMapCmd(unsigned char *data, int len);
void VersionCmd(char *data);
void SendVersion();
void RequestFile(int index);
void SendAddMe();
void SkilllistCmd(char *data);
void SpelllistCmd(char *data);
void GolemCmd(unsigned char *data);
void NewCharCmd();
void DataCmd(unsigned char *data, int len);
void ShopCmd(unsigned char *data, int len);
void QuestListCmd(unsigned char *data, int len);

/* client/dialog.c */
void draw_frame(int x, int y, int w, int h);
void add_close_button(int x, int y, int menu);
int add_button(int x, int y, int id, int gfxNr, char *text, char *text_h);
int add_gr_button(int x, int y, int id, int gfxNr, const char *text, const char *text_h);
int add_rangebox(int x, int y, int id, int text_w, int text_x, const char *text, int color);
void add_value(void *value, int type, int offset, int min, int max);
void draw_tabs(const char *tabs[], int *act_tab, const char *head_text, int x, int y);

/* client/ignore.c */
void ignore_list_clear();
void ignore_list_load();
int ignore_check(char *name, char *type);
void ignore_command(char *cmd);

/* client/image.c */
void read_bmaps_p0();
void delete_bmap_tmp();
int read_bmap_tmp();
void read_bmaps();

/* client/item.c */
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

/* client/main.c */
void save_options_dat();
void open_input_mode(int maxchar);
void list_vid_modes();
int main(int argc, char *argv[]);

/* client/menu.c */
int client_command_check(char *cmd);
void blt_inventory_face_from_tag(int tag, int x, int y);
void show_menu();
void blt_window_slider(_Sprite *slider, int maxlen, int winlen, int startoff, int len, int x, int y);

/* client/metaserver.c */
void metaserver_init();
server_struct *server_get_id(size_t num);
size_t server_get_count();
int ms_connecting(int val);
void metaserver_clear_data();
void metaserver_add(const char *ip, int port, const char *name, int player, const char *version, const char *desc);
int metaserver_thread(void *dummy);
void metaserver_get_servers();

/* client/misc.c */
_bmaptype *find_bmap(char *name);
void add_bmap(_bmaptype *at);
void FreeMemory(void **p);
const char *show_input_string(const char *text, struct _Font *font, int wlen);
int isqrt(int n);
char *get_parameter_string(const char *data, int *pos);
size_t split_string(char *str, char *array[], size_t array_size, char sep);

/* client/player.c */
void clear_player();
void new_player(long tag, char *name, long weight, short face);
void new_char(_server_char *nc);
void client_send_apply(int tag);
void client_send_examine(int tag);
void client_send_move(int loc, int tag, int nrof);
void send_command(const char *command);
void CompleteCmd(unsigned char *data, int len);
void set_weight_limit(uint32 wlim);
void init_player_data();
void widget_player_data_event(widgetdata *widget, int x, int y);
void widget_show_player_data(widgetdata *widget);
void widget_player_stats(widgetdata *widget);
void widget_menubuttons(widgetdata *widget);
void widget_menubuttons_event(widgetdata *widget, int x, int y);
void widget_skillgroups(widgetdata *widget);
void widget_show_player_doll_event();
void widget_show_player_doll(widgetdata *widget);
void widget_show_main_lvl(widgetdata *widget);
void widget_show_skill_exp(widgetdata *widget);
void widget_skill_exp_event(widgetdata *widget);
void widget_show_regeneration(widgetdata *widget);
void widget_show_container(widgetdata *widget);
void widget_highlight_menu(widgetdata *widget);
void widget_menu_event(widgetdata *widget, int x, int y);
void widget_menuitem_event(widgetdata *widget, int x, int y, void (*menu_func_ptr)(widgetdata *, int, int));
void widget_show_label(widgetdata *widget);
void widget_show_bitmap(widgetdata *widget);

/* client/scripts.c */
void script_load(const char *cparams);
void script_list();
void script_process();
int script_trigger_event(const char *cmd, const uint8 *data, const int data_len, const enum CmdFormat format);
void script_send(char *params);
void script_killall();
void script_autoload();
void script_unload(const char *params);

/* client/server_settings.c */
int get_bmap_id(char *name);
void load_settings();
void read_settings();
void delete_server_chars();

/* client/socket.c */
void command_buffer_free(command_buffer *buf);
int send_command_binary(uint8 cmd, uint8 *body, unsigned int len);
int send_socklist(SockList msg);
command_buffer *get_next_input_command();
void socket_thread_start();
void socket_thread_stop();
int handle_socket_shutdown();
int socket_get_error();
int socket_close(struct ClientSocket *csock);
int socket_initialize();
void socket_deinitialize();
int socket_open(struct ClientSocket *csock, char *host, int port);

/* client/sound.c */
void sound_init();
void sound_deinit();
void sound_play_effect(const char *filename, int volume);
void sound_start_bg_music(const char *filename, int volume, int loop);
void sound_stop_bg_music();
void parse_map_bg_music(const char *bg_music);
void sound_update_volume();
void SoundCmd(uint8 *data, int len);

/* client/sprite.c */
void sprite_init_system();
_Sprite *sprite_load_file(char *fname, uint32 flags);
_Sprite *sprite_tryload_file(char *fname, uint32 flag, SDL_RWops *rwop);
void sprite_free_sprite(_Sprite *sprite);
void sprite_blt(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx);
void sprite_blt_map(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx, uint32 stretch, sint16 zoom);
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
void zoomSurfaceSize(int width, int height, double zoomx, double zoomy, int *dstwidth, int *dstheight);

/* client/tilestretcher.c */
int add_color_to_surface(SDL_Surface *dest, Uint8 red, Uint8 green, Uint8 blue);
int copy_pixel_to_pixel(SDL_Surface *src, SDL_Surface *dest, int x1, int y1, int x2, int y2, float brightness);
int copy_vertical_line(SDL_Surface *src, SDL_Surface *dest, int src_x, int src_sy, int src_ey, int dest_x, int dest_sy, int dest_ey, float brightness, int extra);
SDL_Surface *tile_stretch(SDL_Surface *src, int n, int e, int s, int w);

/* client/updates.c */
void file_updates_init();
void cmd_request_update(unsigned char *data, int len);
int file_updates_finished();
void file_updates_parse();

/* client/wrapper.c */
void LOG(LogLevel logLevel, char *format, ...) __attribute__((format(printf, 2, 3)));
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

/* events/console.c */
void key_string_event(SDL_KeyboardEvent *key);
void mouse_InputNumber();

/* events/event.c */
int draggingInvItem(int src);
void resize_window(int width, int height);
int Event_PollInputDevice();

/* events/keys.c */
int key_event(SDL_KeyboardEvent *key);
int event_poll_key(SDL_Event *event);
void cursor_keys(int num);
void key_repeat();
void check_menu_keys(int menu, int key);

/* events/macro.c */
void init_keys();
void reset_keys();
int check_menu_macros(char *text);
int check_keys_menu_status(int key);
void process_macro(_keymap macro);
void check_keys(int key);
int process_macro_keys(int id, int value);
void read_keybind_file(char *fname);
void save_keybind_file(char *fname);

/* events/move.c */
void move_keys(int num);
int dir_from_tile_coords(int tx, int ty);

/* gui/book.c */
_gui_book_struct *book_gui_load(char *data, int len);
void book_gui_show();
void book_gui_handle_mouse(int x, int y);

/* gui/connect.c */
void show_login_server();

/* gui/create_character.c */
void blit_face(int id, int x, int y);
void show_newplayer_server();

/* gui/fps.c */
void widget_show_fps(widgetdata *widget);

/* gui/help.c */
void free_help_files();
void read_help_files();
void show_help(char *helpname);

/* gui/input.c */
void widget_number_event(widgetdata *widget, int x, int y);
void widget_show_console(widgetdata *widget);
void widget_show_number(widgetdata *widget);
void do_number();
void do_keybind_input();
void do_console();

/* gui/inventory.c */
int get_inventory_data(item *op, int *ctag, int *slot, int *start, int *count, int wxlen, int wylen);
void widget_inventory_event(widgetdata *widget, int x, int y, SDL_Event event);
void widget_show_inventory_window(widgetdata *widget);
void widget_below_window_event(widgetdata *widget, int x, int y, int MEvent);
void widget_show_below_window(widgetdata *widget);
int blt_inv_item_centered(item *tmp, int x, int y);
void blt_inv_item(item *tmp, int x, int y, int nrof);
void examine_range_inv();
void examine_range_marks(int tag);

/* gui/keybind.c */
void show_keybind();

/* gui/main.c */
void show_meta_server();
void metaserver_mouse(SDL_Event *e);
int key_meta_menu(SDL_KeyboardEvent *key);

/* gui/map.c */
void load_mapdef_dat();
void widget_show_mapname(widgetdata *widget);
void clear_map();
void display_mapscroll(int dx, int dy);
void map_draw_map_clear();
void update_map_data(const char *name, char *bg_music);
void init_map_data(int xl, int yl, int px, int py);
void align_tile_stretch(int x, int y);
void adjust_tile_stretch();
void map_set_data(int x, int y, int layer, sint16 face, uint8 quick_pos, uint8 obj_flags, const char *name, uint8 name_color, sint16 height, uint8 probe, sint16 zoom, sint16 align);
void map_clear_cell(int x, int y);
void map_set_darkness(int x, int y, uint8 darkness);
void map_draw_map();
void map_draw_one(int x, int y, _Sprite *sprite);
int mouse_to_tile_coords(int mx, int my, int *tx, int *ty);

/* gui/party.c */
void switch_tabs();
void draw_party_tabs(int x, int y);
void show_party();
void gui_party_interface_mouse(SDL_Event *e);
void clear_party_interface();
_gui_party_struct *load_party_interface(char *data, int len);
int console_party();

/* gui/player_shop.c */
void widget_show_shop(widgetdata *widget);
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

/* gui/protections.c */
void widget_show_resist(widgetdata *widget);

/* gui/quickslots.c */
void quickslot_key(SDL_KeyboardEvent *key, int slot);
int get_quickslot(int x, int y);
void show_quickslots(int x, int y, int vertical_quickslot);
void widget_quickslots(widgetdata *widget);
void widget_quickslots_mouse_event(widgetdata *widget, int x, int y, int MEvent);
void update_quickslots(int del_item);
void QuickSlotCmd(unsigned char *data, int len);

/* gui/range.c */
void widget_range_event(widgetdata *widget, int x, int y, SDL_Event event, int MEvent);
void widget_show_range(widgetdata *widget);

/* gui/settings.c */
void optwin_draw_options(int x, int y);
void show_optwin();

/* gui/skill_list.c */
void show_skilllist();
void read_skills();

/* gui/spell_list.c */
void show_spelllist();
void read_spells();
int find_spell(const char *name, int *spell_group, int *spell_class, int *spell_nr);

/* gui/target.c */
void widget_event_target(widgetdata *widget, int x, int y);
void widget_show_target(widgetdata *widget);

/* gui/textwin.c */
void say_clickedKeyword(widgetdata *widget, int mouseX, int mouseY);
void draw_info_format(int flags, char *format, ...) __attribute__((format(printf, 2, 3)));
void draw_info(char *str, int flags);
void textwin_show(int x, int y);
void widget_textwin_show(widgetdata *widget);
void textwin_button_event(widgetdata *widget, SDL_Event event);
int textwin_move_event(widgetdata *widget, SDL_Event event);
void textwin_event(int e, SDL_Event *event, widgetdata *widget);
void textwin_addhistory(char *text);
void textwin_clearhistory();
void textwin_putstring(char *text);
void change_textwin_font(int font);

/* widgets/widget.c */
void init_widgets_fromCurrent();
widgetdata *create_widget_object(int widget_subtype_id);
void remove_widget_object(widgetdata *widget);
void remove_widget_object_intern(widgetdata *widget);
void remove_widget_inv(widgetdata *widget);
void init_widgets();
void kill_widgets();
void kill_widget_tree(widgetdata *widget);
widgetdata *create_widget(int widget_id);
void remove_widget(widgetdata *widget);
void detach_widget(widgetdata *widget);
void save_interface_file();
void save_interface_file_rec(widgetdata *widget, FILE *stream);
int widget_event_mousedn(int x, int y, SDL_Event *event);
int widget_event_mouseup(int x, int y, SDL_Event *event);
int widget_event_mousemv(int x, int y, SDL_Event *event);
int widget_event_start_move(widgetdata *widget, int x, int y);
int widget_event_respond(int x, int y);
int widget_event_override();
widgetdata *get_widget_owner(int x, int y, widgetdata *start, widgetdata *end);
widgetdata *get_widget_owner_rec(int x, int y, widgetdata *widget, widgetdata *end);
void process_widgets();
void process_widgets_rec(widgetdata *widget);
void SetPriorityWidget(widgetdata *node);
void insert_widget_in_container(widgetdata *widget_container, widgetdata *widget);
widgetdata *get_outermost_container(widgetdata *widget);
void move_widget(widgetdata *widget, int x, int y);
void move_widget_rec(widgetdata *widget, int x, int y);
void resize_widget(widgetdata *widget, int side, int offset);
void resize_widget_rec(widgetdata *widget, int x, int width, int y, int height);
widgetdata *add_label(char *text, _Font *font, int color);
widgetdata *add_bitmap(int bitmap_id);
widgetdata *create_menu(int x, int y, widgetdata *owner);
void add_menuitem(widgetdata *menu, char *text, void (*menu_func_ptr)(widgetdata *, int, int), int menu_type);
void add_separator(widgetdata *widget);
void widget_redraw_all(int widget_type_id);
void menu_move_widget(widgetdata *widget, int x, int y);
void menu_create_widget(widgetdata *widget, int x, int y);
void menu_remove_widget(widgetdata *widget, int x, int y);
void menu_detach_widget(widgetdata *widget, int x, int y);
void menu_set_say_filter(widgetdata *widget, int x, int y);
void menu_set_shout_filter(widgetdata *widget, int x, int y);
void menu_set_gsay_filter(widgetdata *widget, int x, int y);
void menu_set_tell_filter(widgetdata *widget, int x, int y);
void menu_set_channel_filter(widgetdata *widget, int x, int y);
void submenu_chatwindow_filters(widgetdata *widget, int x, int y);
#endif
