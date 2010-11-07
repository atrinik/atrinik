#ifndef __CPROTO__
/* client/animations.c */
void read_anims();
void anims_reset();

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
char *GetString_String(uint8 *data, int *pos, char *dest, size_t dest_size);
int cs_write_string(char *buf, size_t len);
void check_animation_status(int anum);

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
void NewCharCmd();
void DataCmd(unsigned char *data, int len);
void ShopCmd(unsigned char *data, int len);
void QuestListCmd(unsigned char *data, int len);

/* client/curl.c */
int curl_connect(void *c_data);
curl_data *curl_data_new(const char *url);
curl_data *curl_download_start(const char *url);
sint8 curl_download_finished(curl_data *data);
void curl_data_free(curl_data *data);
void curl_init();
void curl_deinit();

/* client/dialog.c */
void draw_frame(SDL_Surface *surface, int x, int y, int w, int h);
void add_close_button(int x, int y, int menu);
int add_button(int x, int y, int id, int gfxNr, char *text, char *text_h);
int add_gr_button(int x, int y, int id, int gfxNr, const char *text, const char *text_h);
void add_value(void *value, int type, int offset, int min, int max);
void draw_tabs(const char *tabs[], int *act_tab, const char *head_text, int x, int y);

/* client/ignore.c */
void ignore_list_clear();
void ignore_list_load();
int ignore_check(char *name, char *type);
void ignore_command(char *cmd);

/* client/image.c */
bmap_struct *bmap_find(const char *name);
void bmap_add(bmap_struct *bmap);
void read_bmaps_p0();
void read_bmaps();
void finish_face_cmd(int pnum, uint32 checksum, char *face);
int request_face(int pnum);
int get_bmap_id(char *name);
void blit_face(int id, int x, int y);

/* client/item.c */
void objects_free(object *op);
object *object_find_object_inv(object *op, sint32 tag);
object *object_find_object(object *op, sint32 tag);
object *object_find(sint32 tag);
void object_remove(object *op);
void object_remove_inventory(object *op);
object *object_create(object *env, sint32 tag, int bflag);
void object_set_values(object *op, const char *name, sint32 weight, uint16 face, int flags, uint16 anim, uint16 animspeed, sint32 nrof, uint8 itype, uint8 stype, uint8 qual, uint8 cond, uint8 skill, uint8 level, uint8 dir);
void toggle_locked(object *op);
void object_send_mark(object *op);
void objects_init();
void update_object(int tag, int loc, const char *name, int weight, int face, int flags, int anim, int animspeed, int nrof, uint8 itype, uint8 stype, uint8 qual, uint8 cond, uint8 skill, uint8 level, uint8 direction, int bflag);
void animate_objects();

/* client/main.c */
void save_options_dat();
void free_bitmaps();
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
unsigned long isqrt(unsigned long n);
size_t split_string(char *str, char *array[], size_t array_size, char sep);
void *reallocz(void *ptr, size_t old_size, size_t new_size);
void convert_newline(char *str);
void browser_open(const char *url);

/* client/player.c */
void clear_player();
void new_player(long tag, long weight, short face);
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
int gender_to_id(const char *gender);

/* client/scripts.c */
void script_load(const char *cparams);
void script_list();
void script_process();
int script_trigger_event(const char *cmd, const uint8 *data, const int data_len, const enum CmdFormat format);
void script_send(char *params);
void script_killall();
void script_autoload();
void script_unload(const char *params);

/* client/server_files.c */
void server_files_init();
void server_files_load();
FILE *server_file_open(size_t id);
void server_file_save(size_t id, unsigned char *data, size_t len);
int server_files_updating();
void server_files_setup_add(char *buf, size_t buf_size);
int server_files_parse_setup(const char *cmd, const char *param);

/* client/server_settings.c */
void server_settings_init();
void server_settings_deinit();

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
void surface_pan(SDL_Surface *surface, SDL_Rect *box);

/* client/tilestretcher.c */
int add_color_to_surface(SDL_Surface *dest, Uint8 red, Uint8 green, Uint8 blue);
int copy_pixel_to_pixel(SDL_Surface *src, SDL_Surface *dest, int x1, int y1, int x2, int y2, float brightness);
int copy_vertical_line(SDL_Surface *src, SDL_Surface *dest, int src_x, int src_sy, int src_ey, int dest_x, int dest_sy, int dest_ey, float brightness, int extra);
SDL_Surface *tile_stretch(SDL_Surface *src, int n, int e, int s, int w);

/* client/updates.c */
void cmd_request_update(unsigned char *data, int len);
int file_updates_finished();
void file_updates_parse();

/* client/wrapper.c */
void LOG(LogLevel logLevel, char *format, ...) __attribute__((format(printf, 2, 3)));
void system_start();
void system_end();
char *get_word_from_string(char *str, int *pos);
uint32 get_video_flags();
char *file_path(const char *fname, const char *mode);
FILE *fopen_wrapper(const char *fname, const char *mode);
SDL_Surface *IMG_Load_wrapper(const char *file);

/* events/console.c */
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
void book_name_change(const char *name, size_t len);
void book_load(const char *data, int len);
void book_show();
void book_handle_key(SDLKey key);
void book_handle_event(SDL_Event *event);

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
void inventory_filter_set(uint64 filter);
void inventory_filter_toggle(uint64 filter);
int get_inventory_data(object *op, int *ctag, int *slot, int *start, int *count, int wxlen, int wylen);
void widget_inventory_event(widgetdata *widget, int x, int y, SDL_Event event);
void widget_show_inventory_window(widgetdata *widget);
void widget_below_window_event(widgetdata *widget, int x, int y, int MEvent);
void widget_show_below_window(widgetdata *widget);
int blt_inv_item_centered(object *tmp, int x, int y);
void blt_inv_item(object *tmp, int x, int y, int nrof);
void examine_range_inv();
void examine_range_marks(int tag);

/* gui/keybind.c */
void show_keybind();

/* gui/main.c */
void show_meta_server();

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
void map_set_data(int x, int y, int layer, sint16 face, uint8 quick_pos, uint8 obj_flags, const char *name, uint8 name_color, sint16 height, uint8 probe, sint16 zoom, sint16 align, uint8 draw_double, uint8 alpha);
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
void shop_object_remove(sint32 tag);
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

/* gui/region_map.c */
void RegionMapCmd(uint8 *data, int len);
void region_map_handle_key(SDLKey key);
void region_map_handle_event(SDL_Event *event);
void region_map_show();

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
void textwin_scroll_adjust(widgetdata *widget);
void draw_info_format(int flags, char *format, ...) __attribute__((format(printf, 2, 3)));
void draw_info(const char *str, int flags);
void textwin_show(int x, int y, int w, int h);
void widget_textwin_show(widgetdata *widget);
void textwin_button_event(widgetdata *widget, SDL_Event event);
int textwin_move_event(widgetdata *widget, SDL_Event event);
void textwin_event(int e, SDL_Event *event, widgetdata *widget);
void change_textwin_font(int font);

/* toolkit/button.c */
int button_show(int bitmap_id, int bitmap_id_over, int bitmap_id_clicked, int x, int y, const char *text, int font, SDL_Color color, SDL_Color color_shadow, SDL_Color color_over, SDL_Color color_over_shadow);

/* toolkit/list.c */
list_struct *list_get_focused();
void list_set_focus(list_struct *list);
list_struct *list_create(uint32 id, int x, int y, uint32 max_rows, uint32 cols, int spacing);
void list_add(list_struct *list, uint32 row, uint32 col, const char *str);
void list_set_column(list_struct *list, uint32 col, int width, int spacing, const char *name, int centered);
void list_set_font(list_struct *list, int font);
void list_show(list_struct *list);
void list_remove(list_struct *list);
void list_remove_all();
int lists_handle_keyboard(SDL_KeyboardEvent *event);
void list_handle_mouse(list_struct *list, int mx, int my, SDL_Event *event);
int lists_handle_mouse(int mx, int my, SDL_Event *event);
void lists_handle_resize(int y_offset);
list_struct *list_exists(uint32 id);

/* toolkit/popup.c */
popup_struct *popup_create(int bitmap_id);
void popup_destroy_visible();
int popup_overlay_need_update(popup_struct *popup);
void popup_draw();
int popup_handle_event(SDL_Event *event);
popup_struct *popup_get_visible();

/* toolkit/range_buttons.c */
int range_buttons_show(int x, int y, int *val, int advance);

/* toolkit/scroll_buttons.c */
void scroll_buttons_show(SDL_Surface *surface, int x, int y, int *pos, int max_pos, int advance, SDL_Rect *box);

/* toolkit/text.c */
void text_init();
void text_deinit();
int blt_character(int *font, int orig_font, SDL_Surface *surface, SDL_Rect *dest, const char *cp, SDL_Color *color, SDL_Color *orig_color, uint64 flags, SDL_Rect *box);
int glyph_get_width(int font, char c);
void string_blt(SDL_Surface *surface, int font, const char *text, int x, int y, SDL_Color color, uint64 flags, SDL_Rect *box);
void string_blt_shadow(SDL_Surface *surface, int font, const char *text, int x, int y, SDL_Color color, SDL_Color color_shadow, uint64 flags, SDL_Rect *box);
void string_blt_format(SDL_Surface *surface, int font, int x, int y, SDL_Color color, uint64 flags, SDL_Rect *box, const char *text, ...) __attribute__((format(printf, 8, 9)));
void string_blt_shadow_format(SDL_Surface *surface, int font, int x, int y, SDL_Color color, SDL_Color color_shadow, uint64 flags, SDL_Rect *box, const char *text, ...) __attribute__((format(printf, 9, 10)));
int string_get_width(int font, const char *text, uint64 flags);
void text_enable_debug();

/* toolkit/text_input.c */
int text_input_center_offset();
void text_input_draw_background(SDL_Surface *surface, int x, int y, int bitmap);
void text_input_draw_text(SDL_Surface *surface, int x, int y, int font, const char *text, SDL_Color color, uint64 flags, int bitmap, SDL_Rect *box);
void text_input_show(SDL_Surface *surface, int x, int y, int font, const char *text, SDL_Color color, uint64 flags, int bitmap, SDL_Rect *box);
void text_input_clear();
void text_input_open(int maxchar);
void text_input_history_clear();
void text_input_add_string(const char *text);
int text_input_handle(SDL_KeyboardEvent *key);
const char *show_input_string(const char *text, struct _Font *font, int wlen);

/* toolkit/tooltip.c */
void tooltip_create(int mx, int my, int font, const char *text);
void tooltip_show();

/* toolkit/widget.c */
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
widgetdata *widget_find_by_surface(SDL_Surface *surface);
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
void menu_inv_filter_all();
void menu_inv_filter_applied();
void menu_inv_filter_containers();
void menu_inv_filter_magical();
void menu_inv_filter_cursed();
void menu_inv_filter_unidentified();
void menu_inv_filter_locked();
void menu_inv_filter_unapplied();
#endif
