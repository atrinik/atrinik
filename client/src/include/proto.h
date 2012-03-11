#ifndef __CPROTO__
/* src/client/animations.c */
extern void read_anims(void);
extern void anims_reset(void);
/* src/client/client.c */
extern Client_Player cpl;
extern ClientSocket csocket;
extern void DoClient(void);
extern void check_animation_status(int anum);
/* src/client/cmd_aliases.c */
extern void cmd_aliases_init(void);
extern void cmd_aliases_deinit(void);
extern int cmd_aliases_handle(const char *cmd);
/* src/client/commands.c */
extern void socket_command_book(uint8 *data, size_t len, size_t pos);
extern void socket_command_setup(uint8 *data, size_t len, size_t pos);
extern void socket_command_anim(uint8 *data, size_t len, size_t pos);
extern void socket_command_image(uint8 *data, size_t len, size_t pos);
extern void socket_command_drawinfo(uint8 *data, size_t len, size_t pos);
extern void socket_command_target(uint8 *data, size_t len, size_t pos);
extern void socket_command_stats(uint8 *data, size_t len, size_t pos);
extern void send_reply(char *text);
extern void socket_command_player(uint8 *data, size_t len, size_t pos);
extern void socket_command_item(uint8 *data, size_t len, size_t pos);
extern void socket_command_item_update(uint8 *data, size_t len, size_t pos);
extern void socket_command_item_delete(uint8 *data, size_t len, size_t pos);
extern void socket_command_mapstats(uint8 *data, size_t len, size_t pos);
extern void socket_command_map(uint8 *data, size_t len, size_t pos);
extern void socket_command_version(uint8 *data, size_t len, size_t pos);
extern void socket_command_data(uint8 *data, size_t len, size_t pos);
extern void socket_command_compressed(uint8 *data, size_t len, size_t pos);
/* src/client/curl.c */
extern int curl_connect(void *c_data);
extern curl_data *curl_data_new(const char *url);
extern curl_data *curl_download_start(const char *url);
extern sint8 curl_download_finished(curl_data *data);
extern void curl_data_free(curl_data *data);
extern void curl_init(void);
extern void curl_deinit(void);
/* src/client/ignore.c */
extern void ignore_list_clear(void);
extern void ignore_list_load(void);
extern int ignore_check(const char *name, const char *type);
extern void ignore_command(const char *cmd);
/* src/client/image.c */
extern bmap_struct *bmap_find(const char *name);
extern void bmap_add(bmap_struct *bmap);
extern void read_bmaps_p0(void);
extern void read_bmaps(void);
extern void finish_face_cmd(int facenum, uint32 checksum, char *face);
extern int request_face(int pnum);
extern int get_bmap_id(char *name);
extern void face_show(SDL_Surface *surface, int x, int y, int id);
/* src/client/item.c */
extern void objects_free(object *op);
extern object *object_find_object_inv(object *op, sint32 tag);
extern object *object_find_object(object *op, sint32 tag);
extern object *object_find(sint32 tag);
extern void object_remove(object *op);
extern void object_remove_inventory(object *op);
extern object *object_create(object *env, sint32 tag, int bflag);
extern void toggle_locked(object *op);
extern void object_send_mark(object *op);
extern void ready_object(object *op);
extern void objects_deinit(void);
extern void objects_init(void);
extern void animate_objects(void);
extern void object_show_centered(object *tmp, int x, int y);
/* src/client/keybind.c */
extern keybind_struct **keybindings;
extern size_t keybindings_num;
extern void keybind_load(void);
extern void keybind_save(void);
extern void keybind_free(keybind_struct *keybind);
extern void keybind_deinit(void);
extern keybind_struct *keybind_add(SDLKey key, SDLMod mod, const char *command);
extern void keybind_edit(size_t i, SDLKey key, SDLMod mod, const char *command);
extern void keybind_remove(size_t i);
extern void keybind_repeat_toggle(size_t i);
extern char *keybind_get_key_shortcut(SDLKey key, SDLMod mod, char *buf, size_t len);
extern keybind_struct *keybind_find_by_command(const char *cmd);
extern int keybind_command_matches_event(const char *cmd, SDL_KeyboardEvent *event);
extern int keybind_command_matches_state(const char *cmd);
extern int keybind_process_event(SDL_KeyboardEvent *event);
extern void keybind_process(keybind_struct *keybind, uint8 type);
extern int keybind_process_command_up(const char *cmd);
extern void keybind_state_ensure(void);
extern int keybind_process_command(const char *cmd);
/* src/client/main.c */
extern SDL_Surface *ScreenSurface;
extern struct sockaddr_in insock;
extern server_struct *selected_server;
extern uint32 LastTick;
extern int f_custom_cursor;
extern int x_custom_cursor;
extern int y_custom_cursor;
extern int map_udate_flag;
extern int map_redraw_flag;
extern _anim_table *anim_table;
extern Animations *animations;
extern size_t animations_num;
extern struct screensize *Screensize;
extern _face_struct FaceList[32767];
extern struct msg_anim_struct msg_anim;
extern clioption_settings_struct clioption_settings;
extern void list_vid_modes(void);
extern void clioption_settings_deinit(void);
extern int main(int argc, char *argv[]);
/* src/client/menu.c */
extern int client_command_check(const char *cmd);
extern void send_command_check(const char *cmd);
/* src/client/metaserver.c */
extern void metaserver_init(void);
extern void metaserver_disable(void);
extern server_struct *server_get_id(size_t num);
extern size_t server_get_count(void);
extern int ms_connecting(int val);
extern void metaserver_clear_data(void);
extern void metaserver_add(const char *ip, int port, const char *name, int player, const char *version, const char *desc);
extern int metaserver_thread(void *dummy);
extern void metaserver_get_servers(void);
/* src/client/misc.c */
extern void browser_open(const char *url);
extern char *package_get_version_full(char *dst, size_t dstlen);
extern char *package_get_version_partial(char *dst, size_t dstlen);
extern int bmp2png(const char *path);
extern void screenshot_create(SDL_Surface *surface);
/* src/client/player.c */
extern const char *gender_noun[4];
extern const char *gender_subjective[4];
extern const char *gender_subjective_upper[4];
extern const char *gender_objective[4];
extern const char *gender_possessive[4];
extern const char *gender_reflexive[4];
extern void clear_player(void);
extern void new_player(tag_t tag, long weight, short face);
extern void client_send_apply(tag_t tag);
extern void client_send_examine(tag_t tag);
extern void client_send_move(int loc, int tag, int nrof);
extern void send_command(const char *command);
extern void init_player_data(void);
extern int gender_to_id(const char *gender);
extern void player_draw_exp_progress(SDL_Surface *surface, int x, int y, sint64 exp, uint8 level);
/* src/client/server_files.c */
extern void server_files_init(void);
extern void server_files_load(int post_load);
extern FILE *server_file_open(size_t id);
extern void server_file_save(size_t id, unsigned char *data, size_t len);
extern int server_files_updating(void);
extern void server_files_setup_add(packet_struct *packet);
extern void server_files_mark_update(size_t i);
extern void server_files_clear_update(void);
/* src/client/server_settings.c */
extern server_settings *s_settings;
extern void server_settings_init(void);
extern void server_settings_deinit(void);
/* src/client/settings.c */
extern setting_category **setting_categories;
extern size_t setting_categories_num;
extern void settings_init(void);
extern void settings_load(void);
extern void settings_save(void);
extern void settings_deinit(void);
extern void *setting_get(setting_struct *setting);
extern const char *setting_get_str(int cat, int setting);
extern sint64 setting_get_int(int cat, int setting);
extern void settings_apply(void);
extern void settings_apply_change(void);
extern void setting_set_int(int cat, int setting, sint64 val);
extern void setting_set_str(int cat, int setting, const char *val);
extern int setting_is_text(setting_struct *setting);
extern sint64 category_from_name(const char *name);
extern sint64 setting_from_name(const char *name);
/* src/client/socket.c */
extern command_buffer *command_buffer_new(size_t len, uint8 *data);
extern void command_buffer_free(command_buffer *buf);
extern void socket_send_packet(packet_struct *packet);
extern command_buffer *get_next_input_command(void);
extern void add_input_command(command_buffer *buf);
extern void socket_thread_start(void);
extern void socket_thread_stop(void);
extern int handle_socket_shutdown(void);
extern int socket_get_error(void);
extern int socket_close(struct ClientSocket *csock);
extern int socket_initialize(void);
extern void socket_deinitialize(void);
extern int socket_open(struct ClientSocket *csock, char *host, int port);
/* src/client/sound.c */
extern void sound_init(void);
extern void sound_deinit(void);
extern void sound_clear_cache(void);
extern void sound_play_effect(const char *filename, int volume);
extern int sound_play_effect_loop(const char *filename, int volume, int loop);
extern void sound_start_bg_music(const char *filename, int volume, int loop);
extern void sound_stop_bg_music(void);
extern void update_map_bg_music(const char *bg_music);
extern void sound_update_volume(void);
extern const char *sound_get_bg_music(void);
extern const char *sound_get_bg_music_basename(void);
extern uint8 sound_map_background(int new);
extern void socket_command_sound(uint8 *data, size_t len, size_t pos);
extern void sound_ambient_mapcroll(int xoff, int yoff);
extern void sound_ambient_clear(void);
extern void socket_command_sound_ambient(uint8 *data, size_t len, size_t pos);
extern void sound_pause_music(void);
extern void sound_resume_music(void);
extern int sound_playing_music(void);
/* src/client/sprite.c */
extern struct _anim *start_anim;
extern SDL_Surface *FormatHolder;
extern void sprite_init_system(void);
extern sprite_struct *sprite_load_file(char *fname, uint32 flags);
extern sprite_struct *sprite_tryload_file(char *fname, uint32 flag, SDL_RWops *rwop);
extern void sprite_free_sprite(sprite_struct *sprite);
extern void surface_show(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, SDL_Surface *src);
extern void surface_show_effects(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, SDL_Surface *src, uint8 alpha, uint32 stretch, sint16 zoom_x, sint16 zoom_y, sint16 rotate);
extern void map_sprite_show(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, sprite_struct *sprite, uint32 flags, uint8 dark_level, uint8 alpha, uint32 stretch, sint16 zoom_x, sint16 zoom_y, sint16 rotate);
extern Uint32 getpixel(SDL_Surface *surface, int x, int y);
extern void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
extern int surface_borders_get(SDL_Surface *surface, int *top, int *bottom, int *left, int *right, uint32 color);
extern struct _anim *add_anim(int type, int mapx, int mapy, int value);
extern void remove_anim(struct _anim *anim);
extern void play_anims(void);
extern int sprite_collision(int x, int y, int x2, int y2, sprite_struct *sprite1, sprite_struct *sprite2);
extern void surface_pan(SDL_Surface *surface, SDL_Rect *box);
extern void draw_frame(SDL_Surface *surface, int x, int y, int w, int h);
extern void border_create(SDL_Surface *surface, int x, int y, int w, int h, int color, int size);
extern void border_create_line(SDL_Surface *surface, int x, int y, int w, int h, uint32 color);
extern void border_create_sdl_color(SDL_Surface *surface, SDL_Rect *coords, int thickness, SDL_Color *color);
extern void border_create_color(SDL_Surface *surface, SDL_Rect *coords, int thickness, const char *color_notation);
extern void rectangle_create(SDL_Surface *surface, int x, int y, int w, int h, const char *color_notation);
/* src/client/texture.c */
extern void texture_init(void);
extern void texture_deinit(void);
extern void texture_clear_cache(void);
extern texture_struct *texture_get(int type, const char *name);
/* src/client/tilestretcher.c */
extern int add_color_to_surface(SDL_Surface *dest, Uint8 red, Uint8 green, Uint8 blue);
extern int copy_pixel_to_pixel(SDL_Surface *src, SDL_Surface *dest, int x, int y, int x2, int y2, float brightness);
extern int copy_vertical_line(SDL_Surface *src, SDL_Surface *dest, int src_x, int src_sy, int src_ey, int dest_x, int dest_sy, int dest_ey, float brightness, int extra);
extern SDL_Surface *tile_stretch(SDL_Surface *src, int n, int e, int s, int w);
/* src/client/updates.c */
extern void socket_command_file_update(uint8 *data, size_t len, size_t pos);
extern int file_updates_finished(void);
extern void file_updates_parse(void);
/* src/client/upgrader.c */
extern void upgrader_init(void);
extern char *upgrader_get_version_partial(char *dst, size_t dstlen);
/* src/client/video.c */
extern int video_get_bpp(void);
extern int video_set_size(void);
extern uint32 get_video_flags(void);
extern int video_fullscreen_toggle(SDL_Surface **surface, uint32 *flags);
/* src/client/wrapper.c */
extern void system_start(void);
extern void system_end(void);
extern void mkdir_ensure(const char *path);
extern void copy_file(const char *filename, const char *filename_out);
extern void copy_if_exists(const char *from, const char *to, const char *src, const char *dst);
extern void rmrf(const char *path);
extern void copy_rec(const char *src, const char *dst);
extern const char *get_config_dir(void);
extern void get_data_dir_file(char *buf, size_t len, const char *fname);
extern char *file_path(const char *fname, const char *mode);
extern FILE *fopen_wrapper(const char *fname, const char *mode);
extern SDL_Surface *IMG_Load_wrapper(const char *file);
extern TTF_Font *TTF_OpenFont_wrapper(const char *file, int ptsize);
/* src/events/event.c */
extern int old_mouse_y;
extern int event_dragging_check(void);
extern void event_dragging_start(int tag, int mx, int my);
extern void event_dragging_stop(void);
extern void resize_window(int width, int height);
extern int Event_PollInputDevice(void);
extern void event_push_key(SDL_EventType type, SDLKey key, SDLMod mod);
extern void event_push_key_once(SDLKey key, SDLMod mod);
/* src/events/keys.c */
extern key_struct keys[SDLK_LAST];
extern void init_keys(void);
extern void key_handle_event(SDL_KeyboardEvent *event);
/* src/events/move.c */
extern void move_keys(int num);
extern int dir_from_tile_coords(int tx, int ty);
/* src/gui/misc/effects.c */
extern void effects_init(void);
extern void effects_deinit(void);
extern void effects_reinit(void);
extern void effect_sprites_free(effect_struct *effect);
extern void effect_free(effect_struct *effect);
extern void effect_sprite_def_free(effect_sprite_def *sprite_def);
extern void effect_sprite_free(effect_sprite *sprite);
extern void effect_sprite_remove(effect_sprite *sprite);
extern void effect_sprites_play(void);
extern int effect_start(const char *name);
extern void effect_debug(const char *type);
extern void effect_stop(void);
extern uint8 effect_has_overlay(void);
extern void effect_scale(sprite_struct *sprite);
/* src/gui/misc/game_news.c */
extern void game_news_open(const char *title);
/* src/gui/misc/intro.c */
extern void intro_show(void);
extern int intro_event(SDL_Event *event);
/* src/gui/popups/book.c */
extern UT_array *book_help_history;
extern void book_name_change(const char *name, size_t len);
extern void book_load(const char *data, int len);
extern void book_redraw(void);
extern void book_add_help_history(const char *name);
/* src/gui/popups/characters.c */
extern void characters_open(void);
extern void socket_command_characters(uint8 *data, size_t len, size_t pos);
/* src/gui/popups/color_chooser.c */
extern color_picker_struct *color_chooser_open(void);
/* src/gui/popups/help.c */
extern void hfiles_deinit(void);
extern void hfiles_init(void);
extern void help_show(const char *name);
extern void help_handle_tabulator(text_input_struct *text_input);
/* src/gui/popups/interface.c */
extern void socket_command_interface(uint8 *data, size_t len, size_t pos);
extern void interface_redraw(void);
/* src/gui/popups/login.c */
extern void login_start(void);
/* src/gui/popups/region_map.c */
extern void region_map_clear(void);
extern void socket_command_region_map(uint8 *data, size_t len, size_t pos);
/* src/gui/popups/settings.c */
extern void settings_open(void);
/* src/gui/popups/settings_client.c */
extern void settings_client_open(void);
/* src/gui/popups/settings_keybinding.c */
extern void settings_keybinding_open(void);
/* src/gui/popups/updater.c */
extern void updater_open(void);
/* src/gui/toolkit/button.c */
extern void button_init(void);
extern void button_create(button_struct *button);
extern void button_set_parent(button_struct *button, int px, int py);
extern void button_show(button_struct *button, const char *text);
extern int button_event(button_struct *button, SDL_Event *event);
extern void button_tooltip(button_struct *button, int font, const char *text);
/* src/gui/toolkit/clipboard.c */
extern int clipboard_init(void);
extern int clipboard_set(const char *str);
extern char *clipboard_get(void);
/* src/gui/toolkit/color_picker.c */
extern void color_picker_create(color_picker_struct *color_picker, int size);
extern void color_picker_set_parent(color_picker_struct *color_picker, int px, int py);
extern void color_picker_set_notation(color_picker_struct *color_picker, const char *color_notation);
extern void color_picker_get_rgb(color_picker_struct *color_picker, uint8 *r, uint8 *g, uint8 *b);
extern void color_picker_show(SDL_Surface *surface, color_picker_struct *color_picker);
extern int color_picker_event(color_picker_struct *color_picker, SDL_Event *event);
extern int color_picker_mouse_over(color_picker_struct *color_picker, int mx, int my);
/* src/gui/toolkit/list.c */
extern void list_set_parent(list_struct *list, int px, int py);
extern list_struct *list_create(uint32 max_rows, uint32 cols, int spacing);
extern void list_add(list_struct *list, uint32 row, uint32 col, const char *str);
extern void list_remove_row(list_struct *list, uint32 row);
extern void list_set_column(list_struct *list, uint32 col, int width, int spacing, const char *name, int centered);
extern void list_set_font(list_struct *list, int font);
extern void list_scrollbar_enable(list_struct *list);
extern int list_need_redraw(list_struct *list);
extern void list_show(list_struct *list, int x, int y);
extern void list_clear_rows(list_struct *list);
extern void list_clear(list_struct *list);
extern void list_offsets_ensure(list_struct *list);
extern void list_remove(list_struct *list);
extern int list_handle_keyboard(list_struct *list, SDL_Event *event);
extern int list_handle_mouse(list_struct *list, SDL_Event *event);
extern int list_mouse_get_pos(list_struct *list, int mx, int my, uint32 *row, uint32 *col);
extern void list_sort(list_struct *list, int type);
extern int list_set_selected(list_struct *list, const char *str, uint32 col);
/* src/gui/toolkit/popup.c */
extern popup_struct *popup_create(const char *texture);
extern void popup_destroy(popup_struct *popup);
extern void popup_destroy_all(void);
extern void popup_render(popup_struct *popup);
extern void popup_render_all(void);
extern int popup_handle_event(SDL_Event *event);
extern popup_struct *popup_get_head(void);
extern void popup_button_set_text(popup_button *button, const char *text);
/* src/gui/toolkit/progress.c */
extern void progress_dots_create(progress_dots *progress);
extern void progress_dots_show(progress_dots *progress, SDL_Surface *surface, int x, int y);
extern int progress_dots_width(progress_dots *progress);
/* src/gui/toolkit/range_buttons.c */
extern int range_buttons_show(int x, int y, int *val, int advance);
/* src/gui/toolkit/scrollbar.c */
extern void scrollbar_init(void);
extern int scrollbar_need_redraw(scrollbar_struct *scrollbar);
extern void scrollbar_create(scrollbar_struct *scrollbar, int w, int h, uint32 *scroll_offset, uint32 *num_lines, uint32 max_lines);
extern void scrollbar_info_create(scrollbar_info_struct *info);
extern void scrollbar_scroll_to(scrollbar_struct *scrollbar, int scroll);
extern void scrollbar_scroll_adjust(scrollbar_struct *scrollbar, int adjust);
extern void scrollbar_show(scrollbar_struct *scrollbar, SDL_Surface *surface, int x, int y);
extern int scrollbar_event(scrollbar_struct *scrollbar, SDL_Event *event);
extern int scrollbar_get_width(scrollbar_struct *scrollbar);
/* src/gui/toolkit/SDL_gfx.c */
extern int fastPixelColorNolock(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color);
extern int fastPixelColorNolockNoclip(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color);
extern int fastPixelColor(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color);
extern int fastPixelRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int fastPixelRGBANolock(SDL_Surface *dst, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int _putPixelAlpha(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color, Uint8 alpha);
extern int pixelColor(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color);
extern int pixelColorNolock(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color);
extern int _filledRectAlpha(SDL_Surface *dst, Sint16 xx, Sint16 yy, Sint16 x2, Sint16 y2, Uint32 color, Uint8 alpha);
extern int filledRectAlpha(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color);
extern int _HLineAlpha(SDL_Surface *dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color);
extern int _VLineAlpha(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 y2, Uint32 color);
extern int pixelColorWeight(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color, Uint32 weight);
extern int pixelColorWeightNolock(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color, Uint32 weight);
extern int pixelRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int hlineColorStore(SDL_Surface *dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color);
extern int hlineRGBAStore(SDL_Surface *dst, Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int hlineColor(SDL_Surface *dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color);
extern int hlineRGBA(SDL_Surface *dst, Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int vlineColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 y2, Uint32 color);
extern int vlineRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int rectangleColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color);
extern int rectangleRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int roundedRectangleColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint32 color);
extern int roundedRectangleRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int roundedBoxColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint32 color);
extern int roundedBoxRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int boxColor(SDL_Surface *dst, Sint16 xx, Sint16 yy, Sint16 x2, Sint16 y2, Uint32 color);
extern int boxRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int lineColor(SDL_Surface *dst, Sint16 xx, Sint16 yy, Sint16 x2, Sint16 y2, Uint32 color);
extern int lineRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int _aalineColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color, int draw_endpoint);
extern int aalineColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color);
extern int aalineRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int circleColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Uint32 color);
extern int circleRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int arcColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color);
extern int arcRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int aacircleColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Uint32 color);
extern int aacircleRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int filledCircleColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Uint32 color);
extern int filledCircleRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int ellipseColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color);
extern int ellipseRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int aaellipseColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color);
extern int aaellipseRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int filledEllipseColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color);
extern int filledEllipseRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int _pieColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color, Uint8 filled);
extern int pieColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color);
extern int pieRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int filledPieColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color);
extern int filledPieRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int trigonColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color);
extern int trigonRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int aatrigonColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color);
extern int aatrigonRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int filledTrigonColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color);
extern int filledTrigonRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int polygonColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color);
extern int polygonRGBA(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int aapolygonColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color);
extern int aapolygonRGBA(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int _gfxPrimitivesCompareInt(const void *a, const void *b);
extern int filledPolygonColorMT(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color, int **polyInts, int *polyAllocated);
extern int filledPolygonRGBAMT(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int **polyInts, int *polyAllocated);
extern int filledPolygonColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color);
extern int filledPolygonRGBA(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int _HLineTextured(SDL_Surface *dst, Sint16 x1, Sint16 x2, Sint16 y, SDL_Surface *texture, int texture_dx, int texture_dy);
extern int texturedPolygonMT(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, SDL_Surface *texture, int texture_dx, int texture_dy, int **polyInts, int *polyAllocated);
extern int texturedPolygon(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, SDL_Surface *texture, int texture_dx, int texture_dy);
extern double _evaluateBezier(double *data, int ndata, double t);
extern int bezierColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, int s, Uint32 color);
extern int bezierRGBA(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n, int s, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern int thickLineColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 width, Uint32 color);
extern int thickLineRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint8 width, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
/* src/gui/toolkit/SDL_rotozoom.c */
extern Uint32 _colorkey(SDL_Surface *src);
extern int _shrinkSurfaceRGBA(SDL_Surface *src, SDL_Surface *dst, int factorx, int factory);
extern int _shrinkSurfaceY(SDL_Surface *src, SDL_Surface *dst, int factorx, int factory);
extern int _zoomSurfaceRGBA(SDL_Surface *src, SDL_Surface *dst, int flipx, int flipy, int smooth);
extern int _zoomSurfaceY(SDL_Surface *src, SDL_Surface *dst, int flipx, int flipy);
extern void _transformSurfaceRGBA(SDL_Surface *src, SDL_Surface *dst, int cx, int cy, int isin, int icos, int flipx, int flipy, int smooth);
extern void transformSurfaceY(SDL_Surface *src, SDL_Surface *dst, int cx, int cy, int isin, int icos, int flipx, int flipy);
extern SDL_Surface *rotateSurface90Degrees(SDL_Surface *src, int numClockwiseTurns);
extern void _rotozoomSurfaceSizeTrig(int width, int height, double angle, double zoomx, double zoomy, int *dstwidth, int *dstheight, double *canglezoom, double *sanglezoom);
extern void rotozoomSurfaceSizeXY(int width, int height, double angle, double zoomx, double zoomy, int *dstwidth, int *dstheight);
extern void rotozoomSurfaceSize(int width, int height, double angle, double zoom, int *dstwidth, int *dstheight);
extern SDL_Surface *rotozoomSurface(SDL_Surface *src, double angle, double zoom, int smooth);
extern SDL_Surface *rotozoomSurfaceXY(SDL_Surface *src, double angle, double zoomx, double zoomy, int smooth);
extern void zoomSurfaceSize(int width, int height, double zoomx, double zoomy, int *dstwidth, int *dstheight);
extern SDL_Surface *zoomSurface(SDL_Surface *src, double zoomx, double zoomy, int smooth);
extern SDL_Surface *shrinkSurface(SDL_Surface *src, int factorx, int factory);
/* src/gui/toolkit/text.c */
extern font_struct fonts[FONTS_MAX];
extern void text_init(void);
extern void text_deinit(void);
extern void text_offset_set(int x, int y);
extern void text_offset_reset(void);
extern void text_color_set(int r, int g, int b);
extern void text_set_selection(sint64 *start, sint64 *end, uint8 *started);
extern void text_set_anchor_handle(text_anchor_handle_func func);
extern const char *get_font_filename(int font);
extern int get_font_id(const char *name, size_t size);
extern char *text_strip_markup(char *buf, size_t *buf_len, uint8 do_free);
extern char *text_escape_markup(const char *buf);
extern int text_color_parse(const char *color_notation, SDL_Color *color);
extern void text_anchor_execute(text_info_struct *info);
extern void text_show_character_init(text_info_struct *info);
extern int text_show_character(int *font, int orig_font, SDL_Surface *surface, SDL_Rect *dest, const char *cp, SDL_Color *color, SDL_Color *orig_color, uint64 flags, SDL_Rect *box, int *x_adjust, text_info_struct *info);
extern int glyph_get_width(int font, char c);
extern int glyph_get_height(int font, char c);
extern void text_show(SDL_Surface *surface, int font, const char *text, int x, int y, const char *color_notation, uint64 flags, SDL_Rect *box);
extern void text_show_shadow(SDL_Surface *surface, int font, const char *text, int x, int y, const char *color_notation, const char *color_shadow_notation, uint64 flags, SDL_Rect *box);
extern void text_show_format(SDL_Surface *surface, int font, int x, int y, const char *color_notation, uint64 flags, SDL_Rect *box, const char *format, ...) __attribute__((format(printf, 8, 9)));
extern void text_show_shadow_format(SDL_Surface *surface, int font, int x, int y, const char *color_notation, const char *color_shadow_notation, uint64 flags, SDL_Rect *box, const char *format, ...) __attribute__((format(printf, 9, 10)));
extern int text_get_width(int font, const char *text, uint64 flags);
extern int text_get_height(int font, const char *text, uint64 flags);
extern void string_truncate_overflow(int font, char *text, int max_width);
extern void text_anchor_parse(text_info_struct *info, const char *text);
extern void text_enable_debug(void);
/* src/gui/toolkit/text_input.c */
extern text_input_history_struct *text_input_history_create(void);
extern void text_input_history_free(text_input_history_struct *history);
extern void text_input_create(text_input_struct *text_input);
extern void text_input_reset(text_input_struct *text_input);
extern void text_input_set_history(text_input_struct *text_input, text_input_history_struct *history);
extern void text_input_set(text_input_struct *text_input, const char *str);
extern void text_input_set_parent(text_input_struct *text_input, int px, int py);
extern int text_input_mouse_over(text_input_struct *text_input, int mx, int my);
extern void text_input_show_edit_password(text_input_struct *text_input);
extern int text_input_number_character_check(text_input_struct *text_input, char c);
extern void text_input_show(text_input_struct *text_input, SDL_Surface *surface, int x, int y);
extern void text_input_add_char(text_input_struct *text_input, char c);
extern int text_input_event(text_input_struct *text_input, SDL_Event *event);
/* src/gui/toolkit/tooltip.c */
extern void tooltip_create(int mx, int my, int font, const char *text);
extern void tooltip_multiline(int max_width);
extern void tooltip_show(void);
extern void tooltip_dismiss(void);
/* src/gui/toolkit/widget.c */
extern widgetdata *cur_widget[TOTAL_SUBWIDGETS];
extern widgetevent widget_mouse_event;
extern void toolkit_widget_init(void);
extern widgetdata *create_widget_object(int widget_subtype_id);
extern void remove_widget_object(widgetdata *widget);
extern void remove_widget_object_intern(widgetdata *widget);
extern void remove_widget_inv(widgetdata *widget);
extern void kill_widgets(void);
extern void reset_widget(const char *name);
extern void widgets_ensure_onscreen(void);
extern void kill_widget_tree(widgetdata *widget);
extern widgetdata *create_widget(int widget_id);
extern void remove_widget(widgetdata *widget);
extern void detach_widget(widgetdata *widget);
extern void toolkit_widget_deinit(void);
extern int widget_event_mousedn(int x, int y, SDL_Event *event);
extern int widget_event_mouseup(int x, int y, SDL_Event *event);
extern int widget_event_mousemv(int x, int y, SDL_Event *event);
extern int widget_event_start_move(widgetdata *widget);
extern int widget_event_move_stop(int x, int y);
extern int widget_event_respond(int x, int y);
extern widgetdata *get_widget_owner(int x, int y, widgetdata *start, widgetdata *end);
extern widgetdata *get_widget_owner_rec(int x, int y, widgetdata *widget, widgetdata *end);
extern void process_widgets(void);
extern void process_widgets_rec(widgetdata *widget);
extern void SetPriorityWidget(widgetdata *node);
extern void SetPriorityWidget_reverse(widgetdata *node);
extern void insert_widget_in_container(widgetdata *widget_container, widgetdata *widget);
extern widgetdata *get_outermost_container(widgetdata *widget);
extern widgetdata *widget_find_by_surface(SDL_Surface *surface);
extern widgetdata *widget_find_by_type(int type);
extern void move_widget(widgetdata *widget, int x, int y);
extern void move_widget_rec(widgetdata *widget, int x, int y);
extern void resize_widget(widgetdata *widget, int side, int offset);
extern void resize_widget_rec(widgetdata *widget, int x, int width, int y, int height);
extern widgetdata *add_label(const char *text, int font, const char *color);
extern widgetdata *add_texture(const char *texture);
extern widgetdata *create_menu(int x, int y, widgetdata *owner);
extern void add_menuitem(widgetdata *menu, const char *text, void (*menu_func_ptr)(widgetdata *, widgetdata *, SDL_Event *event), int menu_type, int val);
extern void add_separator(widgetdata *widget);
extern void menu_finalize(widgetdata *widget);
extern void widget_redraw_all(int widget_type_id);
extern void menu_move_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_create_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_remove_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_detach_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_all(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_applied(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_containers(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_magical(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_cursed(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_unidentified(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_locked(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_unapplied(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inv_filter_submenu(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_submenu_more(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_submenu_quickslots(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_textwin_submenu_tabs(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
/* src/gui/widgets/container.c */
extern void widget_container_init(widgetdata *widget);
/* src/gui/widgets/fps.c */
extern void widget_fps_init(widgetdata *widget);
/* src/gui/widgets/input.c */
extern void widget_input_init(widgetdata *widget);
extern int widget_input_handle_key(widgetdata *widget, SDL_Event *event);
/* src/gui/widgets/inventory.c */
extern uint64 inventory_filter;
extern void inventory_filter_set(uint64 filter);
extern void inventory_filter_toggle(uint64 filter);
extern void widget_inventory_init(widgetdata *widget);
extern uint32 widget_inventory_num_items(widgetdata *widget);
extern object *widget_inventory_get_selected(widgetdata *widget);
extern void widget_inventory_handle_arrow_key(widgetdata *widget, SDLKey key);
extern void object_show_inventory(object *tmp, int x, int y);
extern void menu_inventory_drop(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_dropall(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_get(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_getall(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_examine(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_loadtoconsole(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_mark(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_lock(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_ready(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_inventory_drag(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void widget_inventory_handle_apply(widgetdata *widget);
extern void widget_inventory_handle_examine(widgetdata *widget);
extern void widget_inventory_handle_mark(widgetdata *widget);
extern void widget_inventory_handle_lock(widgetdata *widget);
extern void widget_inventory_handle_get(widgetdata *widget);
extern void widget_inventory_handle_drop(widgetdata *widget);
extern void widget_inventory_handle_ready(widgetdata *widget);
/* src/gui/widgets/label.c */
extern void widget_label_init(widgetdata *widget);
/* src/gui/widgets/main_lvl.c */
extern void widget_main_lvl_init(widgetdata *widget);
/* src/gui/widgets/map.c */
extern _mapdata MapData;
extern _multi_part_obj MultiArchs[16];
extern void load_mapdef_dat(void);
extern void clear_map(void);
extern void display_mapscroll(int dx, int dy);
extern void update_map_name(const char *name);
extern void update_map_weather(const char *weather);
extern void init_map_data(int xl, int yl, int px, int py);
extern void adjust_tile_stretch(void);
extern void map_set_data(int x, int y, int layer, sint16 face, uint8 quick_pos, uint8 obj_flags, const char *name, const char *name_color, sint16 height, uint8 probe, sint16 zoom_x, sint16 zoom_y, sint16 align, uint8 draw_double, uint8 alpha, sint16 rotate, uint8 infravision, uint32 target_object_count, uint8 target_is_friend);
extern void map_clear_cell(int x, int y);
extern void map_set_darkness(int x, int y, uint8 darkness);
extern void map_draw_map(void);
extern void map_draw_one(int x, int y, SDL_Surface *surface);
extern void map_target_handle(uint8 is_friend);
extern const char tile_off[24][48];
extern int mouse_to_tile_coords(int mx, int my, int *tx, int *ty);
extern void widget_map_init(widgetdata *widget);
/* src/gui/widgets/mapname.c */
extern void widget_mapname_init(widgetdata *widget);
/* src/gui/widgets/menu.c */
extern void widget_highlight_menu(widgetdata *widget);
extern void widget_menu_event(widgetdata *widget, SDL_Event *event);
/* src/gui/widgets/menu_buttons.c */
extern void widget_menu_buttons_init(widgetdata *widget);
/* src/gui/widgets/mplayer.c */
extern void widget_mplayer_init(widgetdata *widget);
/* src/gui/widgets/notification.c */
extern void notification_destroy(void);
extern int notification_keybind_check(const char *cmd);
extern void socket_command_notification(uint8 *data, size_t len, size_t pos);
extern void widget_notification_init(widgetdata *widget);
/* src/gui/widgets/party.c */
extern void socket_command_party(uint8 *data, size_t len, size_t pos);
extern void widget_party_init(widgetdata *widget);
/* src/gui/widgets/playerdoll.c */
extern void player_doll_update_items(void);
extern void widget_playerdoll_init(widgetdata *widget);
/* src/gui/widgets/playerinfo.c */
extern void widget_playerinfo_init(widgetdata *widget);
/* src/gui/widgets/protections.c */
extern void widget_protections_init(widgetdata *widget);
/* src/gui/widgets/quickslots.c */
extern int quickslot_group;
extern int quickslots_pos[8][2];
extern void quickslots_init(void);
extern void quickslots_handle_key(int slot);
extern int get_quickslot(int x, int y);
extern void widget_quickslots_init(widgetdata *widget);
extern void socket_command_quickslots(uint8 *data, size_t len, size_t pos);
/* src/gui/widgets/regen.c */
extern void widget_regen_init(widgetdata *widget);
/* src/gui/widgets/skill_exp.c */
extern void widget_skill_exp_init(widgetdata *widget);
/* src/gui/widgets/skill_lvl.c */
extern void widget_skill_lvl_init(widgetdata *widget);
/* src/gui/widgets/skills.c */
extern void skills_init(void);
extern int skill_find(const char *name, size_t *id);
extern int skill_find_object(object *op, size_t *id);
extern skill_entry_struct *skill_get(size_t id);
extern void skills_update(object *op, uint8 level, sint64 exp);
extern void skills_remove(object *op);
extern void widget_skills_init(widgetdata *widget);
/* src/gui/widgets/spells.c */
extern void spells_init(void);
extern int spell_find(const char *name, size_t *spell_path, size_t *spell_id);
extern int spell_find_object(object *op, size_t *spell_path, size_t *spell_id);
extern int spell_find_path_selected(const char *name, size_t *spell_id);
extern spell_entry_struct *spell_get(size_t spell_path, size_t spell_id);
extern void spells_update(object *op, uint16 cost, uint32 path, uint32 flags, const char *msg);
extern void spells_remove(object *op);
extern void widget_spells_init(widgetdata *widget);
/* src/gui/widgets/stats.c */
extern void widget_stats_init(widgetdata *widget);
/* src/gui/widgets/texture.c */
extern void widget_texture_init(widgetdata *widget);
/* src/gui/widgets/textwin.c */
extern const char *const textwin_tab_names[];
extern void textwin_init(void);
extern void textwin_readjust(widgetdata *widget);
extern size_t textwin_tab_name_to_id(const char *name);
extern void textwin_tab_free(textwin_tab_struct *tab);
extern void textwin_tab_remove(widgetdata *widget, const char *name);
extern void textwin_tab_add(widgetdata *widget, const char *name);
extern int textwin_tab_find(widgetdata *widget, uint8 type, const char *name, size_t *id);
extern void draw_info_tab(size_t type, const char *name, const char *color, const char *str);
extern void draw_info_format(const char *color, char *format, ...) __attribute__((format(printf, 2, 3)));
extern void draw_info(const char *color, const char *str);
extern void textwin_handle_copy(widgetdata *widget);
extern void textwin_show(SDL_Surface *surface, int x, int y, int w, int h);
extern int textwin_tabs_height(widgetdata *widget);
extern void textwin_create_scrollbar(widgetdata *widget);
extern void widget_textwin_init(widgetdata *widget);
extern void menu_textwin_clear(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_textwin_copy(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_textwin_font_inc(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_textwin_font_dec(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_textwin_timestamps(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void menu_textwin_tab(widgetdata *widget, widgetdata *menuitem, SDL_Event *event);
extern void textwin_submenu_tabs(widgetdata *widget, widgetdata *menu);
/* src/toolkit/binreloc.c */
extern void toolkit_binreloc_init(void);
extern void toolkit_binreloc_deinit(void);
extern char *binreloc_find_exe(const char *default_exe);
extern char *binreloc_find_exe_dir(const char *default_dir);
extern char *binreloc_find_prefix(const char *default_prefix);
extern char *binreloc_find_bin_dir(const char *default_bin_dir);
extern char *binreloc_find_sbin_dir(const char *default_sbin_dir);
extern char *binreloc_find_data_dir(const char *default_data_dir);
extern char *binreloc_find_locale_dir(const char *default_locale_dir);
extern char *binreloc_find_lib_dir(const char *default_lib_dir);
extern char *binreloc_find_libexec_dir(const char *default_libexec_dir);
extern char *binreloc_find_etc_dir(const char *default_etc_dir);
/* src/toolkit/bzr.c */
extern void toolkit_bzr_init(void);
extern void toolkit_bzr_deinit(void);
extern int bzr_get_revision(void);
/* src/toolkit/clioptions.c */
extern void clioptions_option_config(const char *arg);
extern void toolkit_clioptions_init(void);
extern void toolkit_clioptions_deinit(void);
extern void clioptions_add(const char *longname, const char *shortname, clioptions_handler_func handle_func, uint8 argument, const char *desc_brief, const char *desc);
extern void clioptions_parse(int argc, char *argv[]);
/* src/toolkit/colorspace.c */
extern void toolkit_colorspace_init(void);
extern void toolkit_colorspace_deinit(void);
extern double colorspace_rgb_max(const double rgb[3]);
extern double colorspace_rgb_min(const double rgb[3]);
extern void colorspace_rgb2hsv(const double rgb[3], double hsv[3]);
extern void colorspace_hsv2rgb(const double hsv[3], double rgb[3]);
/* src/toolkit/console.c */
extern char *command_generator(const char *text, int state);
extern char **readline_completion(const char *text, int start, int end);
extern void toolkit_console_init(void);
extern void toolkit_console_deinit(void);
extern void console_command_add(const char *command, console_command_func handle_func, const char *desc_brief, const char *desc);
extern void console_command_handle(void);
/* src/toolkit/datetime.c */
extern void toolkit_datetime_init(void);
extern void toolkit_datetime_deinit(void);
extern time_t datetime_getutc(void);
extern time_t datetime_utctolocal(time_t t);
/* src/toolkit/logger.c */
extern void toolkit_logger_init(void);
extern void toolkit_logger_deinit(void);
extern void logger_open_log(const char *path);
extern FILE *logger_get_logfile(void);
extern void logger_set_print_func(logger_print_func func);
extern void logger_do_print(const char *str);
extern void logger_print(const char *level, const char *function, uint64 line, const char *format, ...) __attribute__((format(printf, 4, 5)));
/* src/toolkit/math.c */
extern void toolkit_math_init(void);
extern void toolkit_math_deinit(void);
extern unsigned long isqrt(unsigned long n);
extern int rndm(int min, int max);
extern int rndm_chance(uint32 n);
/* src/toolkit/memory.c */
extern void toolkit_memory_init(void);
extern void toolkit_memory_deinit(void);
extern void *memory_reallocz(void *ptr, size_t old_size, size_t new_size);
/* src/toolkit/mempool.c */
extern mempool_chunk_struct end_marker;
extern void toolkit_mempool_init(void);
extern void toolkit_mempool_deinit(void);
extern uint32 nearest_pow_two_exp(uint32 n);
extern void setup_poolfunctions(mempool_struct *pool, chunk_constructor constructor, chunk_destructor destructor);
extern mempool_struct *mempool_create(const char *description, uint32 expand, uint32 size, uint32 flags, chunk_initialisator initialisator, chunk_deinitialisator deinitialisator, chunk_constructor constructor, chunk_destructor destructor);
extern void mempool_free(mempool_struct *pool);
extern void *get_poolchunk_array_real(mempool_struct *pool, uint32 arraysize_exp);
extern void return_poolchunk_array_real(void *data, uint32 arraysize_exp, mempool_struct *pool);
/* src/toolkit/packet.c */
extern void toolkit_packet_init(void);
extern void toolkit_packet_deinit(void);
extern packet_struct *packet_new(uint8 type, size_t size, size_t expand);
extern void packet_free(packet_struct *packet);
extern void packet_compress(packet_struct *packet);
extern void packet_enable_ndelay(packet_struct *packet);
extern void packet_set_pos(packet_struct *packet, size_t pos);
extern size_t packet_get_pos(packet_struct *packet);
extern packet_struct *packet_dup(packet_struct *packet);
extern void packet_delete(packet_struct *packet, size_t pos, size_t len);
extern void packet_merge(packet_struct *src, packet_struct *dst);
extern void packet_append_uint8(packet_struct *packet, uint8 data);
extern void packet_append_sint8(packet_struct *packet, sint8 data);
extern void packet_append_uint16(packet_struct *packet, uint16 data);
extern void packet_append_sint16(packet_struct *packet, sint16 data);
extern void packet_append_uint32(packet_struct *packet, uint32 data);
extern void packet_append_sint32(packet_struct *packet, sint32 data);
extern void packet_append_uint64(packet_struct *packet, uint64 data);
extern void packet_append_sint64(packet_struct *packet, sint64 data);
extern void packet_append_data_len(packet_struct *packet, uint8 *data, size_t len);
extern void packet_append_string(packet_struct *packet, const char *data);
extern void packet_append_string_terminated(packet_struct *packet, const char *data);
extern uint8 packet_to_uint8(uint8 *data, size_t len, size_t *pos);
extern sint8 packet_to_sint8(uint8 *data, size_t len, size_t *pos);
extern uint16 packet_to_uint16(uint8 *data, size_t len, size_t *pos);
extern sint16 packet_to_sint16(uint8 *data, size_t len, size_t *pos);
extern uint32 packet_to_uint32(uint8 *data, size_t len, size_t *pos);
extern sint32 packet_to_sint32(uint8 *data, size_t len, size_t *pos);
extern uint64 packet_to_uint64(uint8 *data, size_t len, size_t *pos);
extern sint64 packet_to_sint64(uint8 *data, size_t len, size_t *pos);
extern char *packet_to_string(uint8 *data, size_t len, size_t *pos, char *dest, size_t dest_size);
extern void packet_to_stringbuffer(uint8 *data, size_t len, size_t *pos, StringBuffer *sb);
/* src/toolkit/path.c */
extern void toolkit_path_init(void);
extern void toolkit_path_deinit(void);
extern char *path_join(const char *path, const char *path2);
extern char *path_dirname(const char *path);
extern char *path_basename(const char *path);
extern char *path_normalize(const char *path);
extern void path_ensure_directories(const char *path);
extern int path_copy_file(const char *src, FILE *dst, const char *mode);
extern int path_exists(const char *path);
extern int path_touch(const char *path);
extern size_t path_size(const char *path);
extern char *path_file_contents(const char *path);
/* src/toolkit/porting.c */
extern void toolkit_porting_init(void);
extern void toolkit_porting_deinit(void);
/* src/toolkit/sha1.c */
extern void toolkit_sha1_init(void);
extern void toolkit_sha1_deinit(void);
extern void sha1_starts(sha1_context *ctx);
extern void sha1_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
extern void sha1_finish(sha1_context *ctx, unsigned char output[20]);
extern void sha1(const unsigned char *input, size_t ilen, unsigned char output[20]);
extern int sha1_file(const char *path, unsigned char output[20]);
extern void sha1_hmac_starts(sha1_context *ctx, const unsigned char *key, size_t keylen);
extern void sha1_hmac_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
extern void sha1_hmac_finish(sha1_context *ctx, unsigned char output[20]);
extern void sha1_hmac_reset(sha1_context *ctx);
extern void sha1_hmac(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[20]);
/* src/toolkit/shstr.c */
extern void toolkit_shstr_init(void);
extern void toolkit_shstr_deinit(void);
extern shstr *add_string(const char *str);
extern shstr *add_refcount(shstr *str);
extern int query_refcount(shstr *str);
extern shstr *find_string(const char *str);
extern void free_string_shared(shstr *str);
/* src/toolkit/signals.c */
extern void toolkit_signals_init(void);
extern void toolkit_signals_deinit(void);
/* src/toolkit/string.c */
extern void toolkit_string_init(void);
extern void toolkit_string_deinit(void);
extern void string_replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize);
extern void string_replace_char(char *str, const char *key, const char replacement);
extern size_t string_split(char *str, char *array[], size_t array_size, char sep);
extern void string_replace_unprintable_chars(char *buf);
extern char *string_format_number_comma(uint64 num);
extern void string_remove_markup(char *str);
extern void string_toupper(char *str);
extern void string_tolower(char *str);
extern char *string_whitespace_trim(char *str);
extern char *string_whitespace_squeeze(char *str);
extern void string_newline_to_literal(char *str);
extern const char *string_get_word(const char *str, size_t *pos, char delim, char *word, size_t wordsize);
extern void string_skip_word(const char *str, size_t *i, int dir);
extern int string_isdigit(const char *str);
extern void string_capitalize(char *str);
extern void string_title(char *str);
extern int string_startswith(const char *str, const char *cmp);
extern int string_endswith(const char *str, const char *cmp);
extern char *string_sub(const char *str, ssize_t start, ssize_t end);
extern int string_isempty(const char *str);
extern int char_contains(const char c, const char *key);
extern int string_contains(const char *str, const char *key);
extern int string_contains_other(const char *str, const char *key);
extern char *string_create_char_range(char start, char end);
extern char *string_crypt(char *str, const char *salt);
extern char *string_join(const char *delim, ...);
extern char *string_join_array(const char *delim, char **array, size_t arraysize);
extern char *string_repeat(const char *str, size_t num);
/* src/toolkit/stringbuffer.c */
extern void toolkit_stringbuffer_init(void);
extern void toolkit_stringbuffer_deinit(void);
extern StringBuffer *stringbuffer_new(void);
extern char *stringbuffer_finish(StringBuffer *sb);
extern const char *stringbuffer_finish_shared(StringBuffer *sb);
extern void stringbuffer_append_string_len(StringBuffer *sb, const char *str, size_t len);
extern void stringbuffer_append_string(StringBuffer *sb, const char *str);
extern void stringbuffer_append_printf(StringBuffer *sb, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern void stringbuffer_append_stringbuffer(StringBuffer *sb, const StringBuffer *sb2);
extern void stringbuffer_append_char(StringBuffer *sb, const char c);
extern size_t stringbuffer_length(StringBuffer *sb);
extern ssize_t stringbuffer_index(StringBuffer *sb, char c);
extern ssize_t stringbuffer_rindex(StringBuffer *sb, char c);
/* src/toolkit/toolkit.c*/
extern void toolkit_import_register(toolkit_func func);
extern int toolkit_check_imported(toolkit_func func);
extern void toolkit_deinit(void);
#endif
